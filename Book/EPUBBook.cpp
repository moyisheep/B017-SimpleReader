#include "EPUBBook.h"
namespace fs = std::filesystem;
// ---------- 工具 ----------
//static std::string w2a(const std::wstring& s)
//{
//    if (s.empty()) return {};
//    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
//    std::string out(len - 1, 0);                 // 去掉末尾 '\0'
//    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, nullptr, nullptr);
//    return out;
//}
//
//static std::wstring a2w(const std::string& s)
//{
//    if (s.empty()) return {};
//    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
//    std::wstring out(len - 1, 0);                // 去掉末尾 '\0'
//    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
//    return out;
//}

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <gumbo.h>

namespace HtmlWordCounter {

    // ============================================================
    // 模块 1: 文本提取与清理 (保持不变)
    // ============================================================
    void extract_text_from_node(GumboNode* node, std::string& output) {
        if (node->type == GUMBO_NODE_TEXT) {
            output.append(node->v.text.text);
        }
        else if (node->type == GUMBO_NODE_ELEMENT) {
            if (node->v.element.tag == GUMBO_TAG_SCRIPT || node->v.element.tag == GUMBO_TAG_STYLE) {
                return;
            }
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                extract_text_from_node(static_cast<GumboNode*>(children->data[i]), output);
            }
        }
    }

    std::string clean_text(const std::string& raw_text) {
        std::string result;
        result.reserve(raw_text.size());
        bool in_whitespace = false;
        for (char c : raw_text) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!in_whitespace) { result += ' '; in_whitespace = true; }
            }
            else {
                result += c; in_whitespace = false;
            }
        }
        // 去除首尾空格
        if (!result.empty() && result.back() == ' ') result.pop_back();
        if (!result.empty() && result.front() == ' ') result.erase(0, 1);
        return result;
    }

    // ============================================================
    // 模块 2: Unicode 辅助工具 (多语言支持核心)
    // ============================================================

    // 获取 UTF-8 字符的字节长度
    size_t get_utf8_char_len(unsigned char first_byte) {
        if ((first_byte & 0x80) == 0x00) return 1;       // 0xxxxxxx (ASCII)
        if ((first_byte & 0xE0) == 0xC0) return 2;       // 110xxxxx
        if ((first_byte & 0xF0) == 0xE0) return 3;       // 1110xxxx
        if ((first_byte & 0xF8) == 0xF0) return 4;       // 11110xxx
        return 1; // 非法字符按单字节处理
    }

    // 判断一个 UTF-8 字符是否是 "单词字符" (字母或数字)
    // 注意：简单的实现只覆盖 ASCII 字母。为了支持多语言单词（如 Café），理想情况下应使用 ICU 库。
    // 这里为了轻量化，假设 ASCII 范围内的符号为分隔符，汉字等由 CJK 判断处理。
    bool is_word_char(const std::string& text, size_t pos, size_t char_len) {
        unsigned char c = text[pos];
        // ASCII 字母数字
        if (char_len == 1 && std::isalnum(c)) return true;
        return false;
    }

    // 判断是否是 CJK (中日韩) 字符
    // 范围覆盖常用汉字、日文假名、韩文谚文
    bool is_cjk_char(const std::string& text, size_t pos, size_t char_len) {
        if (char_len < 3) return false; // CJK 字符通常是 3 字节 (也有扩展区是 4 字节)

        unsigned char b1 = text[pos];
        unsigned char b2 = text[pos + 1];
        unsigned char b3 = text[pos + 2];

        // CJK Unified Ideographs (4E00-9FFF) -> E4 B8 80 - E9 BF BF
        // 这是一个简化判断，覆盖了绝大多数字符
        if (b1 == 0xE4 && b2 >= 0xB8) return true;
        if (b1 >= 0xE5 && b1 <= 0xE9) return true;

        // 日文假名等 (3040-30FF) -> E3 81 80 - E3 8F BF
        if (b1 == 0xE3 && b2 >= 0x81 && b2 <= 0x8F) return true;

        // 韩文 Hangul Syllables (AC00-D7AF) -> EA B0 80 - ED 9E AF
        if (b1 == 0xEA && b2 >= 0xB0) return true;
        if (b1 >= 0xEB && b1 <= 0xED) return true;

        return false;
    }

    // ============================================================
    // 模块 3: 多语言单词计数器
    // 逻辑：
    // 1. 如果是汉字 -> 直接计数 +1 (字=词)
    // 2. 如果是空格/标点 -> 结束当前单词
    // 3. 如果是英文字母 -> 累积，直到遇到分隔符，计数 +1
    // ============================================================
    size_t count_multilingual_words(const std::string& text) {
        if (text.empty()) return 0;

        size_t word_count = 0;
        bool in_word = false; // 标记是否正在处理英文单词

        for (size_t i = 0; i < text.size(); ) {
            unsigned char lead_byte = text[i];
            size_t char_len = get_utf8_char_len(lead_byte);

            if (i + char_len > text.size()) break; // 防止越界

            // 1. 检查 CJK 字符 (每个字算一个词)
            if (is_cjk_char(text, i, char_len)) {
                // 如果之前在统计英文单词，先结算那个单词
                if (in_word) {
                    word_count++;
                    in_word = false;
                }
                word_count++; // 汉字直接计数
            }
            // 2. 检查 ASCII 单词字符 (英文字母/数字)
            else if (char_len == 1 && std::isalnum(lead_byte)) {
                if (!in_word) {
                    in_word = true; // 开始一个新单词
                }
                // 否则继续在单词中，不做操作
            }
            // 3. 分隔符 (空格、标点符号、其他符号)
            else {
                if (in_word) {
                    word_count++;
                    in_word = false;
                }
                // 分隔符本身不计数
            }

            i += char_len;
        }

        // 处理字符串末尾可能残留的英文单词
        if (in_word) {
            word_count++;
        }

        return word_count;
    }

    // ============================================================
    // 主接口
    // ============================================================
    size_t count_words(const std::string& html_content) {
        GumboOutput* output = gumbo_parse(html_content.c_str());
        if (!output || !output->root) return 0;

        std::string raw_text;
        extract_text_from_node(output->root, raw_text);

        std::string clean_str = clean_text(raw_text);

        gumbo_destroy_output(&kGumboDefaultOptions, output);

        return count_multilingual_words(clean_str);
    }

} // namespace HtmlWordCounter


bool EPUBBook::is_xhtml(const std::string& file_path)
{
    auto dot = file_path.rfind('.');
    if (dot == std::string::npos) return false;

    std::string ext = file_path.substr(dot + 1);

    // 1. 小写
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    // 2. 去掉控制字符
    ext.erase(std::remove_if(ext.begin(), ext.end(),
        [](wchar_t c) { return c < 32 || c > 126; }),
        ext.end());

    // 3. 比较
    return ext == "xhtml" || ext == "html";
}









void EPUBBook::parse_ncx_points(tinyxml2::XMLElement* navPoint, int level,
    const std::string& opf_dir,
    std::vector<OCFNavPoint>& out)
{
    if (!navPoint) return;
    for (auto* pt = navPoint; pt; pt = pt->NextSiblingElement("navPoint"))
    {
        auto* lbl = pt->FirstChildElement("navLabel");
        auto* txt = lbl ? lbl->FirstChildElement("text") : nullptr;
        auto* con = pt->FirstChildElement("content");

        OCFNavPoint np;

        np.label = txt ? extract_text(txt) : "";
        np.href = con && con->Attribute("src") ? con->Attribute("src") : "";
        if (!np.href.empty())
            np.href = resolve_path(fs::path(m_ocf_pkg.toc_path).parent_path().generic_string(), np.href);
        np.order = level;               // 层级深度
        out.emplace_back(std::move(np));

        // 递归子 <navPoint>
        parse_ncx_points(pt->FirstChildElement("navPoint"), level + 1, opf_dir, out);
    }
}

void EPUBBook::parse_nav_list(tinyxml2::XMLElement* ol, int level,
    const std::string& opf_dir,
    std::vector<OCFNavPoint>& out)
{
    if (!ol) return;
    for (auto* li = ol->FirstChildElement("li"); li; li = li->NextSiblingElement("li"))
    {
        auto* a = li->FirstChildElement("a");
        if (!a) continue;

        OCFNavPoint np;
        np.label = extract_text(a);
        np.href = a->Attribute("href") ? a->Attribute("href") : "";
        if (!np.href.empty())
            np.href = resolve_path(fs::path(m_ocf_pkg.toc_path).parent_path().generic_string(), np.href);
        np.order = level;               // 层级深度
        out.emplace_back(std::move(np));

        // 递归子 <ol>
        if (auto* sub = li->FirstChildElement("ol"))
            parse_nav_list(sub, level + 1, opf_dir, out);
    }
}

std::string EPUBBook::extract_text(const tinyxml2::XMLElement* a)
{
    if (!a) return "";

    // 1. 拿到 <a> 的完整 XML 字符串
    tinyxml2::XMLPrinter printer;
    a->Accept(&printer);
    std::string xml = printer.CStr();   // "<a ...><span ...>I</span>: The Meadow</a>"

    // 2. 去掉最外层 <a ...> 和 </a>
    size_t start = xml.find('>') + 1;
    size_t end = xml.rfind('<');
    if (start == std::string::npos || end == std::string::npos || end <= start)
        return "";

    std::string inner = xml.substr(start, end - start);   // "<span ...>I</span>: The Meadow"

    // 3. 简单剥掉所有标签（正则或手写）
    std::regex tag_re("<[^>]*>");
    std::string plain = std::regex_replace(inner, tag_re, "");

    return plain;   // "I: The Meadow"
}



EPUBBook::~EPUBBook() {
    mz_zip_reader_end(&zip);

    //for (const auto& path : g_tempFontFiles)
    //{
    //    RemoveFontResourceExW(path.c_str(), FR_PRIVATE, 0);
    //    DeleteFileW(path.c_str());
    //}
    //g_tempFontFiles.clear();
}

void EPUBBook::clear()
{
    mz_zip_reader_end(&zip);
    m_cache.clear();

    m_ocf_pkg = {};

    m_current_book_path = "";
    m_current_html_path = "";

}

std::string EPUBBook::get_chapter_name_by_id(int spine_id)
{
    // 从给定 spine_id 开始，依次递减查找
    for (int id = spine_id; id >= 0; --id)
    {
        // 1. 取出 spine 对应的 ref
        if (id >= static_cast<int>(m_ocf_pkg.spine.size()))
            continue;

        std::string href = m_ocf_pkg.spine[id].href;


        if (href.empty())
            continue;

        // 3. 去掉锚点
        size_t pos = href.find('#');
        if (pos != std::string::npos)
            href = href.substr(0, pos);

        // 4. 与 toc 中的 href比对（同样去掉锚点）
        for (const auto& nav : m_ocf_pkg.toc)
        {
            std::string nav_href = nav.href;
            pos = nav_href.find('#');
            if (pos != std::string::npos)
                nav_href = nav_href.substr(0, pos);

            if (nav_href == href)
                return nav.label;
        }
    }

    // 遍历到 id=0 仍未找到
    return "";
}

std::string EPUBBook::get_title()
{
    auto titIt = m_ocf_pkg.meta.find("dc:title");
    return titIt != m_ocf_pkg.meta.end() ? titIt->second : "";
}

std::string EPUBBook::get_author()
{
    auto titIt = m_ocf_pkg.meta.find("dc:creator");
    return titIt != m_ocf_pkg.meta.end() ? titIt->second : "";
}

std::string EPUBBook::get_version()
{
    return m_ocf_pkg.version;
}

std::vector<OCFRef>& EPUBBook::get_spine()
{
    return m_ocf_pkg.spine;
}

OCFPackage& EPUBBook::get_ocf_package()
{
    return m_ocf_pkg;
}

size_t EPUBBook::get_total_words()
{
    size_t total_words = 0;
    for (auto& spine: m_ocf_pkg.spine)
    {
        auto html = get_string(spine.href);
        total_words += HtmlWordCounter::count_words(html);
    }

    return total_words;
}


bool EPUBBook::has_script()
{
    for (auto& m:m_ocf_pkg.manifest)
    {
        if (m.href.ends_with(".js")) { return true; }
    }
    return false;
}

bool EPUBBook::has_font()
{
    for (auto& m : m_ocf_pkg.manifest)
    {
        if (m.href.ends_with(".ttf") || 
            m.href.ends_with(".otf") ||
            m.href.ends_with(".woff") ||
            m.href.ends_with(".woff2")) { return true; }
    }
    return false;
}

bool EPUBBook::has_css()
{
    for (auto& m : m_ocf_pkg.manifest)
    {
        if (m.href.ends_with(".css")) { return true; }
    }
    return false;
}




std::vector<uint8_t> EPUBBook::get_binary(std::string base_url, std::string url)
{
    auto path = resolve_path(base_url, url);

    return read_zip(path);
}

//bool EPUBBook::is_toc_item(int spine_id)
//{
//    if (spine_id < 0 || spine_id >= m_ocf_pkg.spine.size()) { return false; }
//    for (auto& it : m_ocf_pkg.toc)
//    {
//        if (it.href == m_ocf_pkg.spine[spine_id].href)
//        {
//            return true;
//        }
//    }
//    return false;
//}


std::vector<uint8_t> EPUBBook::read_zip(std::string file_name) {
    std::vector<uint8_t> mf{};
    if (file_name.empty()) { return mf; }

    auto it = m_cache.find(file_name);
    if (it != m_cache.end()) { return it->second; }
  

    size_t uncomp_size = 0;
    void* p = mz_zip_reader_extract_file_to_heap(
        const_cast<mz_zip_archive*>(&zip),
        file_name.c_str(),
        &uncomp_size, 0);


    if (p) {
        mf.assign(static_cast<uint8_t*>(p),
            static_cast<uint8_t*>(p) + uncomp_size);
        mz_free(p);
        m_cache.emplace(file_name, mf);
    }
    return mf;
}

std::string EPUBBook::get_string(const std::string& path)
{
    auto mf = read_zip(path);
    if (mf.empty()) return {};
    m_current_html_path = path;
    return std::string(reinterpret_cast<const char*>(mf.data()), mf.size());
}

bool EPUBBook::load(const std::string& epub_path) 
{


    mz_zip_reader_end(&zip);           // 1. 先关闭旧 zip
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, epub_path.c_str(), 0)) {
        //mz_zip_error err = mz_zip_get_last_error(&zip);
        //std::string err_msg;

        //switch (err) {
        //case MZ_ZIP_NO_ERROR: err_msg = "无错误"; break;
        //case MZ_ZIP_UNDEFINED_ERROR: err_msg = "未定义错误"; break;
        //case MZ_ZIP_TOO_MANY_FILES: err_msg = "文件太多"; break;
        //case MZ_ZIP_FILE_TOO_LARGE: err_msg = "文件太大"; break;
        //case MZ_ZIP_UNSUPPORTED_METHOD: err_msg = "不支持的压缩方法"; break;
        //case MZ_ZIP_UNSUPPORTED_ENCRYPTION: err_msg = "不支持的加密"; break;
        //case MZ_ZIP_UNSUPPORTED_FEATURE: err_msg = "不支持的功能"; break;
        //case MZ_ZIP_FAILED_FINDING_CENTRAL_DIR: err_msg = "找不到中央目录"; break;
        //case MZ_ZIP_NOT_AN_ARCHIVE: err_msg = "不是ZIP文件"; break;
        //case MZ_ZIP_INVALID_HEADER_OR_CORRUPTED: err_msg = "无效头或文件损坏"; break;
        //case MZ_ZIP_UNSUPPORTED_MULTIDISK: err_msg = "不支持多磁盘归档"; break;
        //case MZ_ZIP_DECOMPRESSION_FAILED: err_msg = "解压失败"; break;
        //case MZ_ZIP_COMPRESSION_FAILED: err_msg = "压缩失败"; break;
        //case MZ_ZIP_UNEXPECTED_DECOMPRESSED_SIZE: err_msg = "解压大小不符"; break;
        //case MZ_ZIP_CRC_CHECK_FAILED: err_msg = "CRC校验失败"; break;
        //case MZ_ZIP_UNSUPPORTED_CDIR_SIZE: err_msg = "不支持的中央目录大小"; break;
        //case MZ_ZIP_ALLOC_FAILED: err_msg = "内存分配失败"; break;
        //case MZ_ZIP_FILE_OPEN_FAILED: err_msg = "文件打开失败"; break;
        //case MZ_ZIP_FILE_CREATE_FAILED: err_msg = "文件创建失败"; break;
        //case MZ_ZIP_FILE_WRITE_FAILED: err_msg = "文件写入失败"; break;
        //case MZ_ZIP_FILE_READ_FAILED: err_msg = "文件读取失败"; break;
        //case MZ_ZIP_FILE_CLOSE_FAILED: err_msg = "文件关闭失败"; break;
        //case MZ_ZIP_FILE_SEEK_FAILED: err_msg = "文件寻址失败"; break;
        //case MZ_ZIP_FILE_STAT_FAILED: err_msg = "文件状态获取失败"; break;
        //case MZ_ZIP_INVALID_PARAMETER: err_msg = "无效参数"; break;
        //case MZ_ZIP_INVALID_FILENAME: err_msg = "无效文件名"; break;
        //case MZ_ZIP_BUF_TOO_SMALL: err_msg = "缓冲区太小"; break;
        //case MZ_ZIP_INTERNAL_ERROR: err_msg = "内部错误"; break;
        //case MZ_ZIP_FILE_NOT_FOUND: err_msg = "文件未找到"; break;
        //case MZ_ZIP_ARCHIVE_TOO_LARGE: err_msg = "归档文件太大"; break;
        //case MZ_ZIP_VALIDATION_FAILED: err_msg = "验证失败"; break;
        //case MZ_ZIP_WRITE_CALLBACK_FAILED: err_msg = "写入回调失败"; break;
        //default: err_msg = "未知错误";
        //}

        //std::string txt = "[EPUBBook] zip 打开失败: 错误码 " + std::to_string(err) +
        //    " (" + err_msg + "), 文件: " + epub_path + "\n";
        //OutputDebugStringA(txt.c_str());

        return false;
    }

    m_current_book_path = epub_path;
    parse_ocf();
    parse_opf();
    parse_toc();


    return true;
}
std::string EPUBBook::get_current_dir()
{
    return fs::path(m_current_html_path).parent_path().generic_string();
}


//std::string EPUBBook::url_decode(const std::string& in)
//{
//    char out[2048];
//    DWORD len = 2048;
//    if (SUCCEEDED(UrlCanonicalizeA(in.c_str(), out, &len, URL_UNESCAPE)))
//        return std::string(out, len);
//    return in;
//}

std::string EPUBBook::url_decode(const std::string& in) {
    std::string result;
    result.reserve(in.size());

    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            // 快速十六进制转换
            auto hexToChar = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return -1; // 无效字符
            };

            int high = hexToChar(in[i + 1]);
            int low = hexToChar(in[i + 2]);

            if (high != -1 && low != -1) {
                result += static_cast<char>((high << 4) | low);
                i += 2;
                continue;
            }
        }
        else if (in[i] == '+') {
            result += ' ';
            continue;
        }

        result += in[i];
    }

    return result;
}
std::string EPUBBook::resolve_path(std::string base_url, std::string href)
{
    fs::path p = fs::path(base_url) / url_decode(href);
    return p.lexically_normal().generic_string();
}
std::string EPUBBook::get_book_path()
{
    return m_current_book_path;
}

// -------------- 实现（直接粘到 EPUBBook 末尾即可） --------------
void EPUBBook::parse_ocf() {
    m_ocf_pkg = {};  // 清空
    auto container = read_zip("META-INF/container.xml");
    if (container.empty()) return;

    tinyxml2::XMLDocument doc;
    if (doc.Parse((char*)container.data(), container.size()) != tinyxml2::XML_SUCCESS) return;

    auto* rootfile = doc.FirstChildElement("container")
        ? doc.FirstChildElement("container")->FirstChildElement("rootfiles")
        : nullptr;
    rootfile = rootfile ? rootfile->FirstChildElement("rootfile") : nullptr;
    if (!rootfile || !rootfile->Attribute("full-path")) return;

    m_ocf_pkg.rootfile = rootfile->Attribute("full-path");
    m_ocf_pkg.opf_dir = m_ocf_pkg.rootfile.substr(0, m_ocf_pkg.rootfile.find_last_of('/') + 1);

}


void EPUBBook::parse_opf() {
    auto opf = read_zip(m_ocf_pkg.rootfile.c_str());
    std::string xml(opf.begin(), opf.begin() + opf.size());
    if (opf.empty()) return;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return;

    // 获取EPUB版本号
    auto* pkg = doc.RootElement();
    if (pkg) {
        m_ocf_pkg.version = pkg->Attribute("version") ? pkg->Attribute("version") : "";
    }

    auto* man = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("manifest")
        : nullptr;

    for (auto* it = man ? man->FirstChildElement("item") : nullptr;
        it; it = it->NextSiblingElement("item"))
    {
        OCFItem item;
        item.id = it->Attribute("id") ? it->Attribute("id") : "";
        item.href = it->Attribute("href") ? it->Attribute("href") : "";
        item.media_type = it->Attribute("media-type") ? it->Attribute("media-type") : "";
        item.properties = it->Attribute("properties") ? it->Attribute("properties") : "";


        // 只在 href 非空时拼绝对路径
        if (!item.href.empty())
            item.href = resolve_path(m_ocf_pkg.opf_dir, item.href);

        m_ocf_pkg.manifest.emplace_back(std::move(item));
    }

    // spine
    auto* spine = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("spine")
        : nullptr;
    // 先把 manifest 做成 id -> href 的映射
    std::unordered_map<std::string, std::string> id2href;
    for (const auto& m : m_ocf_pkg.manifest)
        id2href[m.id] = m.href;

    // 再解析 spine
    for (auto* it = spine ? spine->FirstChildElement("itemref") : nullptr;
        it; it = it->NextSiblingElement("itemref")) {

        OCFRef ref;
        ref.idref = it->Attribute("idref") ? it->Attribute("idref") : "";
        ref.href = id2href[ref.idref];   // 直接填进去
        ref.linear = it->Attribute("linear") ? it->Attribute("linear") : "yes";
        m_ocf_pkg.spine.emplace_back(std::move(ref));
    }
    // meta
    auto* meta = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("metadata")
        : nullptr;
    for (auto* it = meta ? meta->FirstChildElement() : nullptr;
        it; it = it->NextSiblingElement()) {
        m_ocf_pkg.meta[it->Name()] = it->GetText() ? it->GetText() : "";
    }
}


void EPUBBook::parse_toc()
{
    std::string toc_path;
    for (const auto& it : m_ocf_pkg.manifest)
    {
        if (it.properties.find("nav") != std::string::npos ||
            it.id.find("ncx") != std::string::npos)
        {
            toc_path = it.href;
            break;
        }
    }
    if (toc_path.empty())
    {
        for (const auto& it : m_ocf_pkg.manifest)
        {
            if (it.id.find("toc") != std::string::npos)
            {
                toc_path = it.href;
                break;
            }
        }
        if (toc_path.empty()) { return; }
    }

    m_ocf_pkg.toc_path = toc_path;
    auto toc = read_zip(toc_path.c_str());
    if (toc.empty()) return;

    tinyxml2::XMLDocument doc;
    if (doc.Parse((char*)toc.data(), toc.size()) != tinyxml2::XML_SUCCESS) return;

    bool is_nav = is_xhtml(toc_path);
    std::string opf_dir = m_ocf_pkg.opf_dir;

    m_ocf_pkg.toc.clear();

    if (is_nav)
    {
        auto* body = doc.FirstChildElement("html")
            ? doc.FirstChildElement("html")->FirstChildElement("body")
            : nullptr;
        if (!body) return;

        for (auto* nav = body->FirstChildElement("nav") ? body->FirstChildElement("nav") : body->FirstChild()->FirstChildElement("nav");
            nav;
            nav = nav->NextSiblingElement("nav"))
        {
            const char* type = nav->Attribute("epub:type");
            if (type && std::string(type) == "toc")
            {
                parse_nav_list(nav->FirstChildElement("ol"), 0, opf_dir, m_ocf_pkg.toc);
                break;   // 找到就停
            }
        }
    }
    else // NCX
    {
        auto* navMap = doc.RootElement()
            ? doc.RootElement()->FirstChildElement("navMap")
            : nullptr;
        if (navMap)
            parse_ncx_points(navMap->FirstChildElement("navPoint"), 0, opf_dir, m_ocf_pkg.toc);
    }
}

