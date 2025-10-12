#include "PreprocessHTML.h"

void preprocess_js(std::string& html)
{
    if (!g_cfg.enableJS) {
        // 1) 删除 <script ...>...</script>
        static const std::regex reScriptPair(
            R"(<\s*script\b[^>]*>.*?</script>)",
            std::regex::icase | std::regex::optimize | std::regex::nosubs);

        // 2) 删除自闭合 <script ... />
        static const std::regex reScriptSelf(
            R"(<\s*script\b[^>]*\/>)",
            std::regex::icase | std::regex::optimize | std::regex::nosubs);

        html = std::regex_replace(html, reScriptPair, "");
        html = std::regex_replace(html, reScriptSelf, "");
    }
    else
    {
        std::regex  scRe(R"(<script\b([^>]*)\bsrc\s*=\s*["']([^"']*)["']([^>]*)/\s*>)",
            std::regex::icase);
        std::string out;
        out.reserve(html.size());

        std::sregex_iterator it(html.begin(), html.end(), scRe);
        std::sregex_iterator end;
        size_t last = 0;

        for (; it != end; ++it)
        {
            const std::smatch& m = *it;

            // 2.1 读文件
            std::string src = m[2].str();

            MemFile mf = g_book->get_binary(g_book->get_current_dir(), a2w(src));
            std::string code;
            if (!mf.data.empty())
                code.assign(reinterpret_cast<const char*>(mf.data.data()),
                    mf.data.size());

            // 2.2 去掉 src 属性
            std::string attrs = m[1].str() + m[3].str();
            attrs = std::regex_replace(attrs,
                std::regex(R"(\s*\bsrc\s*=\s*["'][^"']*["'])", std::regex::icase), "");

            // 2.3 拼成对标签
            out.append(html, last, m.position() - last);
            out += "<script" + attrs + ">" + code + "</script>";
            last = m.position() + m.length();
        }
        out.append(html, last, std::string::npos);
        html.swap(out);
    }
}

namespace fs = std::filesystem;

inline std::string blade16(std::string_view data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());

    std::array<uint8_t, 16> out;
    blake3_hasher_finalize(&hasher, out.data(), out.size());

    char hex[33];
    for (size_t i = 0; i < out.size(); ++i)
        std::sprintf(hex + i * 2, "%02x", out[i]);
    return std::string(hex, 32);   // 32 个十六进制字符
}

static std::wstring blake3_hex(const std::vector<uint8_t>& data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());

    std::array<uint8_t, BLAKE3_OUT_LEN> hash;          // 32 字节
    blake3_hasher_finalize(&hasher, hash.data(), hash.size());

    std::wostringstream oss;
    for (uint8_t b : hash)
        oss << std::hex << std::setw(2) << std::setfill(L'0') << (b & 0xFF);
    return oss.str();                                  // 64 个十六进制字符
}
static void save_image(const ImageFrame& img, const std::filesystem::path& bmpPath)
{
    const int w = img.width;
    const int h = img.height;
    const int rowBytes = w * 4;
    const int imgSize = rowBytes * h;
    const int fileSize = 54 + imgSize;   // 54 = 14 + 40

    std::ofstream ofs(bmpPath, std::ios::binary);
    if (ofs)
    {
        // BITMAPFILEHEADER (14 bytes)
        uint16_t bfType = 0x4D42;        // 'BM'
        uint32_t bfSize = fileSize;
        uint32_t bfOffBits = 54;
        ofs.write(reinterpret_cast<const char*>(&bfType), 2);
        ofs.write(reinterpret_cast<const char*>(&bfSize), 4);
        ofs.seekp(4, std::ios::cur);     // skip reserved
        ofs.write(reinterpret_cast<const char*>(&bfOffBits), 4);

        // BITMAPINFOHEADER (40 bytes)
        uint32_t biSize = 40;
        int32_t  biWidth = w;
        int32_t  biHeight = -h;          // top-down
        uint16_t biPlanes = 1;
        uint16_t biBitCount = 32;
        uint32_t biCompression = 0;
        uint32_t biSizeImage = imgSize;
        ofs.write(reinterpret_cast<const char*>(&biSize), 4);
        ofs.write(reinterpret_cast<const char*>(&biWidth), 4);
        ofs.write(reinterpret_cast<const char*>(&biHeight), 4);
        ofs.write(reinterpret_cast<const char*>(&biPlanes), 2);
        ofs.write(reinterpret_cast<const char*>(&biBitCount), 2);
        ofs.write(reinterpret_cast<const char*>(&biCompression), 4);
        ofs.seekp(20, std::ios::cur);    // skip rest (zeros)

        // pixel data
        ofs.write(reinterpret_cast<const char*>(img.rgba.data()), imgSize);
    }
}


// ------------------------------------------------
// 主流程：SVG 内 <image> 零拷贝替换
// ------------------------------------------------

void replace_svg_with_img(std::string& html,
    const fs::path& tempDir)
{
    fs::create_directories(tempDir);

    /* ---------- 1. 解析 ---------- */
    GumboOutput* output = gumbo_parse(html.c_str());

    /* ---------- 2. 收集 <svg> 节点 ---------- */
    struct SvgNode {
        size_t start;   // <svg ...> 的起始偏移
        size_t end;     // </svg> 的结束偏移
    };
    std::vector<SvgNode> svgs;

    std::function<void(GumboNode*)> walk = [&](GumboNode* node)
        {
            if (node->type != GUMBO_NODE_ELEMENT) return;
            GumboElement& el = node->v.element;
            if (el.tag == GUMBO_TAG_SVG)
            {
                size_t start = el.start_pos.offset;
                size_t end = el.end_pos.offset + el.original_end_tag.length;
                svgs.push_back({ start, end });
            }
            for (unsigned i = 0; i < el.children.length; ++i)
                walk(static_cast<GumboNode*>(el.children.data[i]));
        };
    walk(output->root);

    /* ---------- 3. 从后往前替换 ---------- */

    for (auto it = svgs.rbegin(); it != svgs.rend(); ++it)
    {
        std::string svgBlock = html.substr(it->start, it->end - it->start);
        std::string hash = blade16(svgBlock);
        if (!g_cMain) continue;
        if (!g_cMain->isImageCached(hash))
        {

            // ------------------------------------------------
            std::regex imgRe("<(image|img)\\b[^>]*\\b(href|xlink:href)\\s*=\\s*\"([^\"]+)\"",
                std::regex::icase);
            std::string patchedSvg = svgBlock;
            std::smatch m;
            std::string::const_iterator search(patchedSvg.cbegin());

            while (std::regex_search(search, patchedSvg.cend(), m, imgRe))
            {
                std::string imgRel = m[3].str();          // zip 内路径
                std::wstring wRel = a2w(imgRel);

                MemFile mf = g_book->get_binary(g_book->get_current_dir(), wRel);
                if (!mf.data.empty())
                {
                    // 1. 根据扩展名决定 MIME
                    fs::path p(imgRel);
                    std::string mime = "image/png";
                    if (p.extension() == ".jpg" || p.extension() == ".jpeg")
                        mime = "image/jpeg";

                    // 2. 编码 base64
                    std::string b64 = base64_encode(mf.data);

                    // 3. 生成 data URI
                    std::string dataUri = "data:" + mime + ";base64," + b64;

                    // 4. 替换 href
                    patchedSvg.replace(m.position(3), m.length(3), dataUri);
                    search = patchedSvg.cbegin() + m.position() + dataUri.size();
                }
                else
                {
                    // 读不到就保持原路径
                    search = m[0].second;
                }
            }

            g_cMain->addImageCache(hash, patchedSvg);
            g_cImage->m_img_cache[hash] = g_cMain->m_img_cache[hash];
            g_cTooltip->m_img_cache[hash] = g_cMain->m_img_cache[hash];

        }
        std::ostringstream imgTag;
        imgTag << R"(<img src=")" << hash << R"(")";
        imgTag << " display=\"block\" ";
        imgTag << " width=\"100%\" ";
        imgTag << " width=\"auto\" ";
        imgTag << " />";

        html.replace(it->start, it->end - it->start, imgTag.str());
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
}


void replace_math_with_svg(std::string& html) {
    GumboOutput* output = gumbo_parse(html.c_str());

    /* 收集所有 <math> 或 <m:math> 节点 */
    struct MathNode {
        GumboElement* el;
        size_t start;
        size_t end;
    };
    std::vector<MathNode> mathNodes;
    std::function<void(GumboNode*)> walk = [&](GumboNode* node) {
        if (node->type == GUMBO_NODE_ELEMENT) {
            GumboElement& el = node->v.element;

            // 新的检测逻辑
            bool isMathElement = false;

            // 方法1：检查原始标签名
            if (el.original_tag.data) {
                // 获取完整的原始标签名（如"math"或"m:math"）
                std::string originalTag(el.original_tag.data, el.original_tag.length);

                // 转换为小写统一比较
                std::transform(originalTag.begin(), originalTag.end(), originalTag.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                // 检查是否是math标签（支持带命名空间）
                size_t mathPos = originalTag.find("math");
                if (mathPos != std::string::npos) {
                    // 确保"math"是标签名的最后部分
                    if (mathPos + 4 == originalTag.length()) {
                        isMathElement = true;
                    }
                    // 或者前面是命名空间分隔符
                    else if (mathPos > 0 && originalTag[mathPos - 1] == ':') {
                        isMathElement = true;
                    }
                }
            }

            // 方法2：补充检查标准math标签
            if (!isMathElement && el.tag == GUMBO_TAG_MATH) {
                isMathElement = true;
            }

            if (isMathElement) {
                mathNodes.push_back({
                    &el,
                    el.start_pos.offset,
                    el.end_pos.offset + el.original_end_tag.length
                    });
            }

            // 递归处理子节点
            for (unsigned i = 0; i < el.children.length; ++i) {
                walk(static_cast<GumboNode*>(el.children.data[i]));
            }
        }
        };
    walk(output->root);

    /* 从后往前替换，避免字节偏移失效 */
    for (auto it = mathNodes.rbegin(); it != mathNodes.rend(); ++it) {
        /* 1. 取 MathML 原文 */
        std::string mathml = html.substr(it->start, it->end - it->start);

        // 处理带命名空间的标签（如 <m:math> → <math>）
        if (mathml.find("m:math") != std::string::npos) {
            boost::replace_all(mathml, "m:math", "math");
            boost::replace_all(mathml, "m:", ""); // 移除其他命名空间前缀（如 m:mrow）
        }

        size_t altimgPos = mathml.find("altimg=\"");
        if (altimgPos != std::string::npos) {
            // 提取 altimg 属性值
            size_t valueStart = altimgPos + 8; // 跳过 "altimg=\""
            size_t valueEnd = mathml.find('"', valueStart);
            if (valueEnd != std::string::npos) {
                std::string altimgSrc = mathml.substr(valueStart, valueEnd - valueStart);

                // 直接构建 img 标签
                std::string imgTag = R"(<img class="math-png" src=")" + altimgSrc + R"(" alt="math" />)";
                html.replace(it->start, it->end - it->start, imgTag);
                continue; // 跳过后续转换流程
            }
        }

        std::string hash = blade16(mathml);
        if (!g_cMain) continue;

        if (!g_cMain->isImageCached(hash)) {
            /* 2. LaTeX → KaTeX → SVG（你原来的逻辑） */
            MathML2SVG& m2s = MathML2SVG::instance();
            std::string svg = m2s.convert(mathml);
            if (svg.empty()) continue;

            g_cMain->addImageCache(hash, svg);
            g_cImage->m_img_cache[hash] = g_cMain->m_img_cache[hash];
            g_cTooltip->m_img_cache[hash] = g_cMain->m_img_cache[hash];
        }

        std::string imgTag = R"(<img class="math-png" src=")" + hash + R"(" alt="math" />)";

        /* 7. 替换原 <math> 标签 */
        html.replace(it->start, it->end - it->start, imgTag);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
}
//
//// --------------------------------------------------
//// 通用 HTML 预处理
//// --------------------------------------------------
//void replace_math_with_svg(std::string& html)
//{
//    GumboOutput* output = gumbo_parse(html.c_str());
//
//    /* 收集所有 <math> 节点 */
//    struct MathNode {
//        GumboElement* el;
//        size_t start;
//        size_t end;
//    };
//    std::vector<MathNode> mathNodes;
//    std::function<void(GumboNode*)> walk = [&](GumboNode* node) {
//        if (node->type == GUMBO_NODE_ELEMENT) {
//            GumboElement& el = node->v.element;
//            if (el.tag == GUMBO_TAG_MATH) {
//                mathNodes.push_back({ &el,
//                                      el.start_pos.offset,
//                                      el.end_pos.offset + el.original_end_tag.length});
//            }
//            for (unsigned i = 0; i < el.children.length; ++i)
//                walk(static_cast<GumboNode*>(el.children.data[i]));
//        }
//        };
//    walk(output->root);
//
//    /* 从后往前替换，避免字节偏移失效 */
//    //std::string patched = html;
//    for (auto it = mathNodes.rbegin(); it != mathNodes.rend(); ++it) {
//        /* 1. 取 MathML 原文 */
//        std::string mathml = html.substr(it->start, it->end - it->start);
//        size_t altimgPos = mathml.find("altimg=\"");
//        if (altimgPos != std::string::npos) {
//            // 提取 altimg 属性值
//            size_t valueStart = altimgPos + 8; // 跳过 "altimg=\""
//            size_t valueEnd = mathml.find('"', valueStart);
//            if (valueEnd != std::string::npos) {
//                std::string altimgSrc = mathml.substr(valueStart, valueEnd - valueStart);
//
//                // 直接构建 img 标签
//                std::string imgTag = R"(<img class="math-png" src=")" + altimgSrc + R"(" alt="math"></img>)";
//                html.replace(it->start, it->end - it->start, imgTag);
//                continue; // 跳过后续转换流程
//            }
//        }
//        std::string hash = blade16(mathml);
//        if (!g_cMain)continue;
//        if(!g_cMain->isImageCached(hash))
//        {
//            /* 2. LaTeX → KaTeX → SVG（你原来的逻辑） */
//            MathML2SVG& m2s = MathML2SVG::instance();
//            std::string svg = m2s.convert(mathml);
//            if (svg.empty()) continue;
//            g_cMain->addImageCache(hash, svg);
//            g_cImage->m_img_cache[hash] = g_cMain->m_img_cache[hash];
//            g_cTooltip->m_img_cache[hash] = g_cMain->m_img_cache[hash];
//        }
//        std::string imgTag;
//        imgTag =  R"(<img class="math-png" src=")" + hash
//            + R"(" alt="math" />)";
//
//        /* 7. 替换原 <math> 标签 */
//        html.replace(it->start, it->end - it->start, imgTag);
//    }
//
//    gumbo_destroy_output(&kGumboDefaultOptions, output);

//}
//


struct HtmlFeatureFlags {
    bool has_svg = false;
    bool has_math = false;
    bool has_script = false;
    bool all() const { return has_svg && has_math && has_script; }
};

inline HtmlFeatureFlags detect_html_features(const std::string& html) noexcept {
    HtmlFeatureFlags f;
    if (html.empty()) return f;

    // 统一转换为小写（仅需一次）
    std::string lower_html;
    lower_html.reserve(html.size());
    for (char c : html) {
        lower_html.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // 直接搜索子字符串（不关心标签完整性）
    f.has_svg = (lower_html.find("<svg") != std::string::npos);
    f.has_math = (lower_html.find("<math") != std::string::npos) ||
        (lower_html.find("<m:math") != std::string::npos);
    f.has_script = (lower_html.find("<script") != std::string::npos);

    return f;
}
//struct HtmlFeatureFlags {
//    bool has_svg = false;
//    bool has_math = false;
//    bool has_script = false;
//    bool all() const { return has_svg && has_math && has_script; }
//};
//
//inline HtmlFeatureFlags detect_html_features(const std::string& html) noexcept
//{
//    HtmlFeatureFlags f;
//    const char* s = html.data();
//    const char* end = s + html.size();
//
//    while (s < end - 6)   // 最短 "<svg" 4 字节，留余量
//    {
//        if (*s == '<')
//        {
//            ++s;
//            // 跳过空白： <  svg  或 <  script
//            while (s < end && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
//                ++s;
//
//            if (s + 3 <= end) {
//                char c0 = static_cast<char>(std::tolower(*s));
//                char c1 = static_cast<char>(std::tolower(*(s + 1)));
//                char c2 = static_cast<char>(std::tolower(*(s + 2)));
//
//                if (c0 == 's' && c1 == 'v' && c2 == 'g') { f.has_svg = true; }
//                else if (c0 == 'm' && c1 == 'a' && c2 == 't') { f.has_math = true; }
//                else if (c0 == 's' && c1 == 'c' && c2 == 'r') { f.has_script = true; }
//
//                if (f.all()) break;   // 提前终止
//            }
//        }
//        ++s;
//    }
//    return f;
//}
//



void PreprocessHTML(std::string& html)
{

    auto flags = detect_html_features(html);
    if (flags.has_math) replace_math_with_svg(html);

    html = std::regex_replace(
        html,
        std::regex(R"(<([a-zA-Z][a-zA-Z0-9]*)\b([^>]*?)/\s*>)", std::regex::icase),
        "<$1$2></$1>");

    if (flags.has_script)preprocess_js(html);


    if (flags.has_svg)
    {
        std::wstring dir = make_temp_dir();
        replace_svg_with_img(html, dir);
    }

}

