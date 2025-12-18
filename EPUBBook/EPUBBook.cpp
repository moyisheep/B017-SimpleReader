#include "EPUBBook.h"
namespace fs = std::filesystem;
// ---------- 工具 ----------
static std::string w2a(const std::wstring& s)
{
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len - 1, 0);                 // 去掉末尾 '\0'
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

static std::wstring a2w(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len - 1, 0);                // 去掉末尾 '\0'
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    return out;
}
bool EPUBBook::is_xhtml(const std::wstring& file_path)
{
    auto dot = file_path.rfind(L'.');
    if (dot == std::wstring::npos) return false;

    std::wstring ext = file_path.substr(dot + 1);

    // 1. 小写
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    // 2. 去掉控制字符
    ext.erase(std::remove_if(ext.begin(), ext.end(),
        [](wchar_t c) { return c < 32 || c > 126; }),
        ext.end());

    // 3. 比较
    return ext == L"xhtml" || ext == L"html";
}



void EPUBBook::load_all_fonts() {

    //FontKey key{ L"serif", 400, false, 0 };
    //m_fontBin[key] = { g_cfg.default_serif };
    //key = { L"sans-serif", 400, false, 0 };
    //m_fontBin[key] = { g_cfg.default_sans_serif };
    //key = { L"monospace", 400, false, 0 };
    //m_fontBin[key] = { g_cfg.default_monospace };


}

std::wstring EPUBBook::blake3_hex(const std::vector<uint8_t>& data)
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

void EPUBBook::build_epub_font_index(std::wstring tempDir)
{


    // 2. 正则
    const std::wregex rx_face(LR"(@font-face\s*\{([^}]*)\})", std::regex::icase);
    const std::wregex rx_fam(LR"(font-family\s*:\s*['"]?([^;'"}]+)['"]?)", std::regex::icase);
    const std::wregex rx_url(LR"(url\s*\(\s*['"]?([^)'"]+)['"]?\s*\))", std::regex::icase);
    const std::wregex rx_loc(LR"(local\s*\(\s*['"]?([^)'"]+)['"]?\s*\))", std::regex::icase);
    const std::wregex rx_w(LR"(font-weight\s*:\s*(\d+|bold))", std::regex::icase);
    const std::wregex rx_i(LR"(font-style\s*:\s*(italic|oblique))", std::regex::icase);

    // 3. 遍历所有 CSS
    for (const auto& item : ocf_pkg_.manifest)
    {
        if (item.media_type != L"text/css") continue;

        MemFile cssFile = get_binary(L"", item.href);
        std::wstring css_dir = fs::path(item.href).parent_path().generic_wstring();
        if (cssFile.data.empty()) continue;

        std::wstring css = a2w({ (char*)cssFile.data.data(), cssFile.data.size() });

        for (std::wsregex_iterator it(css.begin(), css.end(), rx_face), end; it != end; ++it)
        {
            std::wstring block = it->str();
            std::wsmatch m;

            std::wstring family;
            std::vector<std::wstring> paths;   // 可能多个 src
            int weight = 400;
            bool italic = false;

            // family
            if (std::regex_search(block, m, rx_fam)) family = m[1];

            // weight / style
            if (std::regex_search(block, m, rx_w))
                weight = (m[1] == L"bold" || m[1] == L"700") ? 700 : std::stoi(m[1]);
            if (std::regex_search(block, m, rx_i)) italic = true;

            // 解析 src 中所有 url(...) 
            for (std::wsregex_iterator srcIt(block.begin(), block.end(), rx_url), srcEnd; srcIt != srcEnd; ++srcIt)
            {
                std::wstring url = (*srcIt)[1];

                // 跳过网络字体
                if (url.starts_with(L"http://") || url.starts_with(L"https://"))
                    continue;

                // 去掉 query/fragment
                if (auto pos = url.find(L'?'); pos != std::wstring::npos) url.erase(pos);
                if (auto pos = url.find(L'#'); pos != std::wstring::npos) url.erase(pos);

                // 保留扩展名
                std::wstring ext = L".ttf";
                if (auto dot = url.rfind(L'.'); dot != std::wstring::npos)
                {
                    ext = url.substr(dot);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                    static const std::unordered_set<std::wstring> ok{ L".ttf", L".otf", L".woff", L".woff2", L".ttc" };
                    if (!ok.contains(ext)) ext = L".ttf";
                }

                // 解压
                MemFile fontFile = get_binary(css_dir, url);
                if (fontFile.data.empty()) continue;

                std::wstring hashHex = blake3_hex(fontFile.data);   // 32 字节 → 64 字符
                std::wstring tempFont = tempDir + hashHex + ext;    // 例如：a1b2c3...ff.woff2
                // 2. 如果文件已存在，直接记录路径，不再写盘
                if (GetFileAttributesW(tempFont.c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    paths.push_back(tempFont);   // 已缓存
                    continue;
                }

                HANDLE h = CreateFileW(tempFont.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                DWORD written = 0;
                WriteFile(h, fontFile.data.data(), (DWORD)fontFile.data.size(), &written, nullptr);
                CloseHandle(h);

                paths.push_back(tempFont);
            }

            // local(...)
            for (std::wsregex_iterator locIt(block.begin(), block.end(), rx_loc), locEnd; locEnd != locIt; ++locIt)
            {
                paths.push_back((*locIt)[1]);
            }

            if (family.empty() || paths.empty()) continue;

            FontKey key{ family, weight, italic, 0 };
            m_fontBin[key] = std::move(paths);
        }
    }
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

        np.label = txt ? extract_text(txt) : L"";
        np.href = a2w(con && con->Attribute("src") ? con->Attribute("src") : "");
        if (!np.href.empty())
            np.href = resolve_path(fs::path(ocf_pkg_.toc_path).parent_path(), np.href);
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
        np.href = a2w(a->Attribute("href") ? a->Attribute("href") : "");
        if (!np.href.empty())
            np.href = resolve_path(fs::path(ocf_pkg_.toc_path).parent_path(), np.href);
        np.order = level;               // 层级深度
        out.emplace_back(std::move(np));

        // 递归子 <ol>
        if (auto* sub = li->FirstChildElement("ol"))
            parse_nav_list(sub, level + 1, opf_dir, out);
    }
}

std::wstring EPUBBook::extract_text(const tinyxml2::XMLElement* a)
{
    if (!a) return L"";

    // 1. 拿到 <a> 的完整 XML 字符串
    tinyxml2::XMLPrinter printer;
    a->Accept(&printer);
    std::string xml = printer.CStr();   // "<a ...><span ...>I</span>: The Meadow</a>"

    // 2. 去掉最外层 <a ...> 和 </a>
    size_t start = xml.find('>') + 1;
    size_t end = xml.rfind('<');
    if (start == std::string::npos || end == std::string::npos || end <= start)
        return L"";

    std::string inner = xml.substr(start, end - start);   // "<span ...>I</span>: The Meadow"

    // 3. 简单剥掉所有标签（正则或手写）
    std::regex tag_re("<[^>]*>");
    std::string plain = std::regex_replace(inner, tag_re, "");

    return a2w(plain);   // "I: The Meadow"
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

    ocf_pkg_ = {};
    m_fontBin.clear();
    m_current_book_path = L"";
    m_current_html_path = L"";

}

std::wstring EPUBBook::get_chapter_name_by_id(int spine_id)
{
    // 从给定 spine_id 开始，依次递减查找
    for (int id = spine_id; id >= 0; --id)
    {
        // 1. 取出 spine 对应的 ref
        if (id >= static_cast<int>(ocf_pkg_.spine.size()))
            continue;

        std::wstring href = ocf_pkg_.spine[id].href;


        if (href.empty())
            continue;

        // 3. 去掉锚点
        size_t pos = href.find(L'#');
        if (pos != std::wstring::npos)
            href = href.substr(0, pos);

        // 4. 与 toc 中的 href比对（同样去掉锚点）
        for (const auto& nav : ocf_pkg_.toc)
        {
            std::wstring nav_href = nav.href;
            pos = nav_href.find(L'#');
            if (pos != std::wstring::npos)
                nav_href = nav_href.substr(0, pos);

            if (nav_href == href)
                return nav.label;
        }
    }

    // 遍历到 id=0 仍未找到
    return L"";
}

std::string EPUBBook::get_title()
{
    auto titIt = ocf_pkg_.meta.find(L"dc:title");
    return titIt != ocf_pkg_.meta.end() ? w2a(titIt->second) : "";
}

std::string EPUBBook::get_author()
{
    auto titIt = ocf_pkg_.meta.find(L"dc:creator");
    return titIt != ocf_pkg_.meta.end() ? w2a(titIt->second) : "";
}




MemFile EPUBBook::get_binary(std::wstring base_url, std::wstring url)
{
    auto path = resolve_path(base_url, url);
    std::error_code ec;  // 存储错误码
    MemFile mf{};
    if (fs::exists(path, ec))
    {
        size_t sz = fs::file_size(path, ec);
        if (ec) return mf;                       // 文件不存在
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return mf;

        std::vector<uint8_t> buf(sz);
        ifs.read(reinterpret_cast<char*>(buf.data()), sz);

        mf.data = std::move(buf);
    }
    else
    {
        mf = read_zip(path);
    }
    return mf;
}

bool EPUBBook::is_toc_item(int spine_id)
{
    if (spine_id < 0 || spine_id >= ocf_pkg_.spine.size()) { return false; }
    for (auto& it : ocf_pkg_.toc)
    {
        if (it.href == ocf_pkg_.spine[spine_id].href)
        {
            return true;
        }
    }
    return false;
}


MemFile EPUBBook::read_zip(std::wstring file_name) {
    MemFile mf{};
    if (file_name.empty()) { return mf; }

    auto it = m_cache.find(file_name);
    if (it != m_cache.end()) { return it->second; }
    // 2.1 先按给定宽路径找
    std::string narrow_name = w2a(file_name);
    size_t uncomp_size = 0;
    void* p = mz_zip_reader_extract_file_to_heap(
        const_cast<mz_zip_archive*>(&zip),
        narrow_name.c_str(),
        &uncomp_size, 0);


    if (p) {
        mf.data.assign(static_cast<uint8_t*>(p),
            static_cast<uint8_t*>(p) + uncomp_size);
        mz_free(p);
        m_cache.emplace(file_name, mf);
    }
    return mf;
}

std::string EPUBBook::load_html(const std::wstring& path)
{
    MemFile mf = read_zip(path);
    if (mf.data.empty()) return {};
    m_current_html_path = path;
    return std::string(mf.begin(), mf.size());
}

bool EPUBBook::load(const std::wstring& epub_path) {

    if (!fs::exists(epub_path))
    {
        std::wstring txt = L"[EPUBBook] 文件不存在: " + epub_path + L"\n";

        OutputDebugStringW(txt.c_str());
        return false;
    }
    mz_zip_reader_end(&zip);           // 1. 先关闭旧 zip
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, w2a(epub_path).c_str(), 0))
    {
        std::wstring txt = L"[EPUBBook] zip 打开失败: " + std::to_wstring(mz_zip_get_last_error(&zip)) + L"\n";

        OutputDebugStringW(txt.c_str());
        return false;
    }

    m_current_book_path = epub_path;
    parse_ocf_();
    parse_opf_();
    parse_toc_();


    return true;
}
std::wstring EPUBBook::get_current_dir()
{
    return fs::path(m_current_html_path).parent_path();
}

// -------------- 实现（直接粘到 EPUBBook 末尾即可） --------------
void EPUBBook::parse_ocf_() {
    ocf_pkg_ = {};  // 清空
    auto container = read_zip(L"META-INF/container.xml");
    if (container.data.empty()) return;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(container.begin(), container.size()) != tinyxml2::XML_SUCCESS) return;

    auto* rootfile = doc.FirstChildElement("container")
        ? doc.FirstChildElement("container")->FirstChildElement("rootfiles")
        : nullptr;
    rootfile = rootfile ? rootfile->FirstChildElement("rootfile") : nullptr;
    if (!rootfile || !rootfile->Attribute("full-path")) return;

    ocf_pkg_.rootfile = a2w(rootfile->Attribute("full-path"));
    ocf_pkg_.opf_dir = ocf_pkg_.rootfile.substr(0, ocf_pkg_.rootfile.find_last_of(L'/') + 1);

}
std::wstring EPUBBook::url_decode(const std::wstring& in)
{
    wchar_t out[2048];
    DWORD len = 2048;
    if (SUCCEEDED(UrlCanonicalizeW(in.c_str(), out, &len, URL_UNESCAPE)))
        return std::wstring(out, len);
    return in;
}


std::wstring EPUBBook::resolve_path(std::wstring base_url, std::wstring href)
{
    fs::path p = fs::path(base_url) / url_decode(href);
    return p.lexically_normal().generic_wstring();
}
std::wstring EPUBBook::get_book_path()
{
    return m_current_book_path;
}
void EPUBBook::parse_opf_() {
    auto opf = read_zip(ocf_pkg_.rootfile.c_str());
    std::string xml(opf.begin(), opf.begin() + opf.size());
    if (opf.data.empty()) return;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return;

    auto* man = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("manifest")
        : nullptr;

    for (auto* it = man ? man->FirstChildElement("item") : nullptr;
        it; it = it->NextSiblingElement("item"))
    {
        OCFItem item;
        item.id = a2w(it->Attribute("id") ? it->Attribute("id") : "");
        item.href = a2w(it->Attribute("href") ? it->Attribute("href") : "");
        item.media_type = a2w(it->Attribute("media-type") ? it->Attribute("media-type") : "");
        item.properties = a2w(it->Attribute("properties") ? it->Attribute("properties") : "");


        // 只在 href 非空时拼绝对路径
        if (!item.href.empty())
            item.href = resolve_path(ocf_pkg_.opf_dir, item.href);

        ocf_pkg_.manifest.emplace_back(std::move(item));
    }

    // spine
    auto* spine = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("spine")
        : nullptr;
    // 先把 manifest 做成 id -> href 的映射
    std::unordered_map<std::wstring, std::wstring> id2href;
    for (const auto& m : ocf_pkg_.manifest)
        id2href[m.id] = m.href;

    // 再解析 spine
    for (auto* it = spine ? spine->FirstChildElement("itemref") : nullptr;
        it; it = it->NextSiblingElement("itemref")) {

        OCFRef ref;
        ref.idref = a2w(it->Attribute("idref") ? it->Attribute("idref") : "");
        ref.href = id2href[ref.idref];   // 直接填进去
        ref.linear = a2w(it->Attribute("linear") ? it->Attribute("linear") : "yes");
        ocf_pkg_.spine.emplace_back(std::move(ref));
    }
    // meta
    auto* meta = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("metadata")
        : nullptr;
    for (auto* it = meta ? meta->FirstChildElement() : nullptr;
        it; it = it->NextSiblingElement()) {
        ocf_pkg_.meta[a2w(it->Name())] = a2w(it->GetText() ? it->GetText() : "");
    }
}


void EPUBBook::parse_toc_()
{
    std::wstring toc_path;
    for (const auto& it : ocf_pkg_.manifest)
    {
        if (it.properties.find(L"nav") != std::wstring::npos ||
            it.id.find(L"ncx") != std::wstring::npos)
        {
            toc_path = it.href;
            break;
        }
    }
    if (toc_path.empty())
    {
        for (const auto& it : ocf_pkg_.manifest)
        {
            if (it.id.find(L"toc") != std::wstring::npos)
            {
                toc_path = it.href;
                break;
            }
        }
        if (toc_path.empty()) { return; }
    }

    ocf_pkg_.toc_path = toc_path;
    auto toc = read_zip(toc_path.c_str());
    if (toc.data.empty()) return;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(toc.begin(), toc.size()) != tinyxml2::XML_SUCCESS) return;

    bool is_nav = is_xhtml(toc_path);
    std::string opf_dir = w2a(ocf_pkg_.opf_dir);

    ocf_pkg_.toc.clear();

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
                parse_nav_list(nav->FirstChildElement("ol"), 0, opf_dir, ocf_pkg_.toc);
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
            parse_ncx_points(navMap->FirstChildElement("navPoint"), 0, opf_dir, ocf_pkg_.toc);
    }
}

