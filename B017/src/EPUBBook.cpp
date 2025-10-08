#include "EPUBBook.h"

namespace fs = std::filesystem;


bool is_xhtml(const std::wstring& file_path)
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

static void gumbo_serialize(const GumboNode* node, std::string& out)
{
    if (node->type == GUMBO_NODE_TEXT)
    {
        out.append(node->v.text.text);
        return;
    }
    if (node->type != GUMBO_NODE_ELEMENT) return;

    const GumboElement& elem = node->v.element;
    out.push_back('<');
    out.append(gumbo_normalized_tagname(elem.tag));

    // 属性
    for (unsigned int i = 0; i < elem.attributes.length; ++i)
    {
        auto* attr = static_cast<GumboAttribute*>(elem.attributes.data[i]);
        out.append(" ").append(attr->name).append("=\"")
            .append(attr->value).append("\"");
    }

    if (elem.tag == GUMBO_TAG_IMG || elem.tag == GUMBO_TAG_BR)
    {
        // 自闭合
        out.append(" />");
    }
    else
    {
        out.push_back('>');
        for (unsigned int i = 0; i < elem.children.length; ++i)
            gumbo_serialize(static_cast<GumboNode*>(elem.children.data[i]), out);
        out.append("</").append(gumbo_normalized_tagname(elem.tag)).push_back('>');
    }
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
        //SetStatus(STATUSBAR_INFO, txt.c_str());
        OutputDebugStringW(txt.c_str());
        return false;
    }
    mz_zip_reader_end(&zip);           // 1. 先关闭旧 zip
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, w2a(epub_path).c_str(), 0))
    {
        std::wstring txt = L"[EPUBBook] zip 打开失败: " + std::to_wstring(mz_zip_get_last_error(&zip)) + L"\n";
        //SetStatus(STATUSBAR_INFO, txt.c_str());
        OutputDebugStringW(txt.c_str());
        return false;
    }

    m_current_book_path = epub_path;
    parse_ocf_();
    parse_opf_();
    parse_toc_();
    //if (g_cfg.enableEPUBFonts) { build_epub_font_index(); }

    LoadToc();
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
    //wchar_t out[INTERNET_MAX_URL_LENGTH];
    //DWORD len = INTERNET_MAX_URL_LENGTH;
    //if (SUCCEEDED(UrlCanonicalizeW(in.c_str(), out, &len, URL_UNESCAPE)))
    //    return std::wstring(out, len);
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






//
//void EPUBBook::build_epub_font_index()
//{
//
//    // 1. 创建临时目录
//    static std::wstring tempDir = make_temp_dir();
//
//    // 2. 正则
//    const std::wregex rx_face(LR"(@font-face\s*\{([^}]*)\})", std::regex::icase);
//    const std::wregex rx_fam(LR"(font-family\s*:\s*['"]?([^;'"}]+)['"]?)", std::regex::icase);
//    const std::wregex rx_url(LR"(url\s*\(\s*['"]?([^)'"]+)['"]?\s*\))", std::regex::icase);
//    const std::wregex rx_loc(LR"(local\s*\(\s*['"]?([^)'"]+)['"]?\s*\))", std::regex::icase);
//    const std::wregex rx_w(LR"(font-weight\s*:\s*(\d+|bold))", std::regex::icase);
//    const std::wregex rx_i(LR"(font-style\s*:\s*(italic|oblique))", std::regex::icase);
//
//    // 3. 遍历所有 CSS
//    for (const auto& item : ocf_pkg_.manifest)
//    {
//        if (item.media_type != L"text/css") continue;
//
//        MemFile cssFile = get_binary(L"", item.href);
//        std::wstring css_dir = fs::path(item.href).parent_path().generic_wstring();
//        if (cssFile.data.empty()) continue;
//
//        std::wstring css = a2w({ (char*)cssFile.data.data(), cssFile.data.size() });
//
//        for (std::wsregex_iterator it(css.begin(), css.end(), rx_face), end; it != end; ++it)
//        {
//            std::wstring block = it->str();
//            std::wsmatch m;
//
//            std::wstring family;
//            std::vector<std::wstring> paths;   // 可能多个 src
//            int weight = 400;
//            bool italic = false;
//
//            // family
//            if (std::regex_search(block, m, rx_fam)) family = m[1];
//
//            // weight / style
//            if (std::regex_search(block, m, rx_w))
//                weight = (m[1] == L"bold" || m[1] == L"700") ? 700 : std::stoi(m[1]);
//            if (std::regex_search(block, m, rx_i)) italic = true;
//
//            // 解析 src 中所有 url(...) 
//            for (std::wsregex_iterator srcIt(block.begin(), block.end(), rx_url), srcEnd; srcIt != srcEnd; ++srcIt)
//            {
//                std::wstring url = (*srcIt)[1];
//
//                // 跳过网络字体
//                if (url.starts_with(L"http://") || url.starts_with(L"https://"))
//                    continue;
//
//                // 去掉 query/fragment
//                if (auto pos = url.find(L'?'); pos != std::wstring::npos) url.erase(pos);
//                if (auto pos = url.find(L'#'); pos != std::wstring::npos) url.erase(pos);
//
//                // 保留扩展名
//                std::wstring ext = L".ttf";
//                if (auto dot = url.rfind(L'.'); dot != std::wstring::npos)
//                {
//                    ext = url.substr(dot);
//                    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
//                    static const std::unordered_set<std::wstring> ok{ L".ttf", L".otf", L".woff", L".woff2", L".ttc" };
//                    if (!ok.contains(ext)) ext = L".ttf";
//                }
//
//                // 解压
//                MemFile fontFile = get_binary(css_dir, url);
//                if (fontFile.data.empty()) continue;
//
//                std::wstring hashHex = blake3_hex(fontFile.data);   // 32 字节 → 64 字符
//                std::wstring tempFont = tempDir + hashHex + ext;    // 例如：a1b2c3...ff.woff2
//                // 2. 如果文件已存在，直接记录路径，不再写盘
//                if (GetFileAttributesW(tempFont.c_str()) != INVALID_FILE_ATTRIBUTES)
//                {
//                    paths.push_back(tempFont);   // 已缓存
//                    continue;
//                }
//
//                HANDLE h = CreateFileW(tempFont.c_str(), GENERIC_WRITE, 0, nullptr,
//                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
//                DWORD written = 0;
//                WriteFile(h, fontFile.data.data(), (DWORD)fontFile.data.size(), &written, nullptr);
//                CloseHandle(h);
//
//                paths.push_back(tempFont);
//            }
//
//            // local(...)
//            for (std::wsregex_iterator locIt(block.begin(), block.end(), rx_loc), locEnd; locEnd != locIt; ++locIt)
//            {
//                paths.push_back((*locIt)[1]);
//            }
//
//            if (family.empty() || paths.empty()) continue;
//
//            FontKey key{ family, weight, italic, 0 };
//            m_fontBin[key] = std::move(paths);
//        }
//    }
//}








/* ---------- 实现 ---------- */

std::string EPUBBook::extract_anchor(const char* href)
{
    if (!href) return "";
    const char* p = std::strrchr(href, '#');
    return p ? (p + 1) : "";

}
litehtml::element::ptr EPUBBook::find_link_in_chain(litehtml::element::ptr start)
{
    for (auto cur = start; cur; cur = cur->parent())
    {
        const char* tag = cur->get_tagName();
        if (std::strcmp(tag, "p") == 0) break;
        if (std::strcmp(tag, "a") == 0) return cur;
    }
    return nullptr;
}

bool EPUBBook::skip_attr(const std::string& val)
{
    if (val.empty()) return true;
    if (val == "0")  return true;
    return std::all_of(val.begin(), val.end(),
        [](unsigned char c) { return std::isspace(c); });
}

std::string EPUBBook::get_html(litehtml::element::ptr el)
{
    if (!el) return "";
    std::string out;
    out += "<";
    out += el->get_tagName();

    // 常见属性名，按需增删
    static const char* attr_names[] = {
        "id","class","style","title","alt","href","src","type","name","value"
    };

    for (const char* name : attr_names)
    {
        const char* val = el->get_attr(name);
        if (val && !skip_attr(val))
        {
            out += " ";
            out += name;
            out += "=\"";
            out += val;
            out += "\"";
        }
    }

    out += ">";

    for (auto child : el->children())
    {
        if (child->is_text())
        {
            std::string txt;
            child->get_text(txt);
            out += txt;
        }
        else
        {
            out += get_html(child);
        }
    }

    out += "</";
    out += el->get_tagName();
    out += ">";
    return out;
}


// 从一段 HTML 中提取第一个 <img> 的 outerHTML；没有则返回空串
static std::string extract_first_img(const std::string& html)
{
    GumboOutput* out = gumbo_parse(html.c_str());
    std::string result;

    // 深度优先找 <img>
    std::function<void(const GumboNode*)> dfs = [&](const GumboNode* node)
        {
            if (!result.empty()) return;           // 已找到
            if (node->type != GUMBO_NODE_ELEMENT) return;

            const GumboElement& elem = node->v.element;
            if (elem.tag == GUMBO_TAG_IMG)
            {
                gumbo_serialize(node, result);
                return;
            }
            for (unsigned int i = 0; i < elem.children.length; ++i)
                dfs(static_cast<GumboNode*>(elem.children.data[i]));
        };
    dfs(out->root);

    gumbo_destroy_output(&kGumboDefaultOptions, out);
    return result;
}
// 3. 核心函数：用 select_one 找 id，再向上找 <p>
std::string EPUBBook::html_of_anchor_paragraph(litehtml::document* doc, const std::string& anchorId)
{
    if (anchorId.empty()) return "";
    // 构造 CSS 选择器
    std::string sel = "[id=\"" + anchorId + "\"]";
    //std::string sel = "#" + anchorId;
    auto target = doc->root()->select_one(sel);
    if (!target) return "";

    // 向上找最近的 <p>
    auto p = target;
    while (p)
    {
        if (std::strcmp(p->get_tagName(), "figure") == 0 || std::strcmp(p->get_tagName(), "img") == 0) { break; }
        const char* cls = p->get_attr("class");
        if (!cls ||
            (std::strcmp(cls, "duokan-footnote-item") != 0 &&
                std::strcmp(cls, "fig") != 0 &&
                std::strcmp(cls, "figimage") != 0 &&
                std::strcmp(cls, "figure") != 0 &&
                std::strcmp(cls, "reflist") != 0 &&
                std::strcmp(cls, "illustype_image") != 0))
        {
            p = p->parent();
        }
        else
        {
            break;          // 找到目标 class
        }
    }

    if (!p) return "";          // 兜底：直接返回自身

    std::string inner = get_html(p);

    // 用 gumbo 处理
    std::string imgOnly = extract_first_img(inner);
    if (!imgOnly.empty())
    {
        return "<style>img{display:block;width:100%;height:auto;}a{color:#3182ce;text-decoration:none;} </style>" + imgOnly;
    }
    else
    {
        return "<style>img{display:block;width:100%;height:auto;}a{color:#3182ce;text-decoration:none;} </style>" + inner;
    }
}


std::string EPUBBook::get_html_of_image(litehtml::element::ptr start)
{
    if (!start && std::strcmp(start->get_tagName(), "img") != 0) { return ""; }
    std::string inner = get_html(start);
    return "<style>img{display:block;width:100%;height:auto;}</style>" + inner;

}
void EPUBBook::show_imageview(const litehtml::element::ptr& el)
{
    //std::string html = get_html_of_image(el);
    //if (html.empty()) { return; }
    //POINT pt;
    //GetCursorPos(&pt);   // pt.x, pt.y 为屏幕坐标
    ////ScreenToClient(g_hView, &pt);   // 现在 pt.x, pt.y 是相对于窗口客户区的坐标


    //g_cImage->m_doc = litehtml::document::createFromString(
    //    { html.c_str(), litehtml::encoding::utf_8 }, g_cImage.get());
    //int width = g_cfg.tooltip_width;
    //g_cImage->m_doc->render(width);

    //int height = g_cImage->m_doc->height();
    //auto tip_x = pt.x - width / 2;
    //auto tip_y = pt.y - height / 2;



    //DWORD style = GetWindowLong(g_hImageview, GWL_STYLE);
    //DWORD exStyle = GetWindowLong(g_hImageview, GWL_EXSTYLE);
    //UINT dpi = GetDpiForWindow(g_hImageview);
    //RECT r{ 0, 0, width, height };
    //AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
    //g_cImage->resize(width, height);
    //SetWindowPos(g_hImageview, HWND_TOPMOST,
    //    tip_x, tip_y,
    //    r.right - r.left, r.bottom - r.top,
    //    SWP_SHOWWINDOW | SWP_NOACTIVATE);

    //g_imageviewRenderW = width;

    //InvalidateRect(g_hImageview, nullptr, true);
}
void EPUBBook::show_tooltip(const std::string txt, int width)
{
    //auto html = txt;
    ////OutputDebugStringA(("[show_tooltip] " + std::to_string(x) + " " + std::to_string(y) + "\n").c_str());
    //if (html.empty()) { return; }

    ////if (g_vd && !g_vd->m_blocks.empty())
    ////{
    ////    html = "<html>" + g_vd->m_blocks.back().head + "<body>" + html + "</body></html>";
    ////}
    //POINT pt;
    //GetCursorPos(&pt);




    //g_cTooltip->m_doc = litehtml::document::createFromString(
    //    { html.c_str(), litehtml::encoding::utf_8 }, g_cTooltip.get());

    //g_cTooltip->m_doc->render(width);
    //int height = g_cTooltip->m_doc->height();


    //int tip_x = pt.x - width / 2;
    //int tip_y = pt.y - height - 20;
    //if (tip_y < 0) { tip_y = pt.y + 20; }
    //DWORD style = GetWindowLong(g_hTooltip, GWL_STYLE);
    //DWORD exStyle = GetWindowLong(g_hTooltip, GWL_EXSTYLE);
    //UINT dpi = GetDpiForWindow(g_hTooltip);
    //RECT r{ 0, 0, width, height };
    //AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
    //g_cTooltip->resize(width, height);
    //SetWindowPos(g_hTooltip, HWND_TOPMOST,
    //    tip_x, tip_y,
    //    r.right - r.left, r.bottom - r.top,
    //    SWP_SHOWWINDOW | SWP_NOACTIVATE);



    //InvalidateRect(g_hTooltip, nullptr, true);

}
void EPUBBook::hide_imageview()
{

    //if (g_hImageview && IsWindowVisible(g_hImageview))
    //{
    //    ShowWindow(g_hImageview, SW_HIDE);
    //    if (g_cImage && g_cImage->m_doc)
    //    {
    //        g_cImage->m_doc.reset();
    //    }

    //}
}
void EPUBBook::hide_tooltip()
{

    //if (g_hTooltip && IsWindowVisible(g_hTooltip))
    //{
    //    ShowWindow(g_hTooltip, SW_HIDE);

    //}
}



// 把单个 element 序列化成 HTML
static void element_to_html(const litehtml::element::ptr& el,
    std::string& out)
{
    if (!el) return;

    if (el->is_text())
    {
        // 文本节点
        std::string txt;
        el->get_text(txt);
        out += txt;
        return;
    }

    // 开始标签
    out += "<";
    out += el->get_tagName();

    // 属性
    auto attrs = el->dump_get_attrs();
    for (const auto& [name, val] : attrs)
    {
        out += " " + name + "=\"" + val + "\"";
    }

    if (el->children().empty())
    {
        // 自闭合
        out += " />";
    }
    else
    {
        out += ">";

        // 子节点
        for (const auto& child : el->children())
            element_to_html(child, out);

        // 结束标签
        out += "</";
        out += el->get_tagName();
        out += ">";
    }
}

// 根据锚点 id 返回对应元素的 HTML
std::string EPUBBook::get_anchor_html(litehtml::document* doc,
    const std::string& anchor)
{
    if (!doc || anchor.empty()) return {};

    litehtml::element::ptr root = doc->root();
    if (!root) return {};

    litehtml::element::ptr el =
        root->select_one(("#" + anchor).c_str());
    if (!el) return {};

    std::string html;
    element_to_html(el, html);
    return html;
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


void EPUBBook::LoadToc()
{
    //if (g_toc)
    //{
    //    g_toc->Load(ocf_pkg_);                 // 代替 EPUBBook::LoadToc()
    //}
}

ZipFileProvider::ZipFileProvider() {}
ZipFileProvider::~ZipFileProvider()
{
    mz_zip_reader_end(&m_zip);           // 1. 先关闭旧 zip
}
bool ZipFileProvider::load(const std::wstring& file_path)
{
    namespace fs = std::filesystem;
    if (!fs::exists(file_path))
    {
        OutputDebugStringW(L"[ZipProvider] 文件不存在\n");
        return false;
    }


    mz_zip_reader_end(&m_zip);           // 1. 先关闭旧 zip
    memset(&m_zip, 0, sizeof(m_zip));

    if (!mz_zip_reader_init_file(&m_zip, w2a(file_path).c_str(), 0))
    {
        OutputDebugStringW((L"[ZipProvider] zip 打开失败：" +
            std::to_wstring(mz_zip_get_last_error(&m_zip)) + L"\n").c_str());
        return false;
    }

    return true;
}
MemFile ZipFileProvider::get(std::wstring path)
{
    MemFile mf;
    std::string narrow_name = w2a(path);
    size_t uncomp_size = 0;
    void* p = mz_zip_reader_extract_file_to_heap(
        const_cast<mz_zip_archive*>(&m_zip),
        narrow_name.c_str(),
        &uncomp_size, 0);

    if (p) {
        mf.data.assign(static_cast<uint8_t*>(p),
            static_cast<uint8_t*>(p) + uncomp_size);
        mz_free(p);
    }
    return mf;
}

MemFile LocalFileProvider::get(std::wstring path)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    // 文件不存在或非普通文件
    if (!fs::is_regular_file(path, ec) || ec)
        return {};

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};

    file.seekg(0, std::ios::end);
    const auto len = static_cast<size_t>(file.tellg());
    file.seekg(0);

    MemFile mf;
    mf.data.resize(len);
    file.read(reinterpret_cast<char*>(mf.data.data()), len);

    if (!file)        // 读取失败
        return {};

    return mf;        // NRVO / move
}

EPUBParser::EPUBParser() {}
EPUBParser::~EPUBParser()
{
    m_fp.reset();
}
bool EPUBParser::load(std::shared_ptr<IFileProvider> fp)
{
    m_fp = fp;
    parse_ocf();
    parse_opf();
    parse_toc();
    return true;
}

bool EPUBParser::parse_ocf()
{
    m_ocf_pkg = {};  // 清空
    auto mf = m_fp->get(L"META-INF/container.xml");
    if (mf.data.empty()) return false;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(mf.begin(), mf.size()) != tinyxml2::XML_SUCCESS) { return false; }

    auto* rootfile = doc.FirstChildElement("container")
        ? doc.FirstChildElement("container")->FirstChildElement("rootfiles")
        : nullptr;
    rootfile = rootfile ? rootfile->FirstChildElement("rootfile") : nullptr;
    if (!rootfile || !rootfile->Attribute("full-path")) { return false; }

    m_ocf_pkg.rootfile = a2w(rootfile->Attribute("full-path"));
    m_ocf_pkg.opf_dir = m_ocf_pkg.rootfile.substr(0, m_ocf_pkg.rootfile.find_last_of(L'/') + 1);
    return true;
}
bool EPUBParser::parse_opf()
{
    auto opf = m_fp->get(m_ocf_pkg.rootfile.c_str());
    std::string xml(opf.begin(), opf.begin() + opf.size());
    if (opf.data.empty()) return false;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return false;

    auto* manifest = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("manifest")
        : nullptr;

    for (auto* it = manifest ? manifest->FirstChildElement("item") : nullptr;
        it; it = it->NextSiblingElement("item"))
    {
        OCFItem item;
        item.id = a2w(it->Attribute("id") ? it->Attribute("id") : "");
        item.href = a2w(it->Attribute("href") ? it->Attribute("href") : "");
        item.media_type = a2w(it->Attribute("media-type") ? it->Attribute("media-type") : "");
        item.properties = a2w(it->Attribute("properties") ? it->Attribute("properties") : "");


        // 只在 href 非空时拼绝对路径
        if (!item.href.empty())
            item.href = m_fp->find(item.href);

        m_ocf_pkg.manifest.emplace_back(std::move(item));
    }

    // spine
    auto* spine = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("spine")
        : nullptr;
    // 先把 manifest 做成 id -> href 的映射
    std::unordered_map<std::wstring, std::wstring> id2href;
    for (const auto& m : m_ocf_pkg.manifest)
        id2href[m.id] = m.href;

    // 再解析 spine
    for (auto* it = spine ? spine->FirstChildElement("itemref") : nullptr;
        it; it = it->NextSiblingElement("itemref")) {

        OCFRef ref;
        ref.idref = a2w(it->Attribute("idref") ? it->Attribute("idref") : "");
        ref.href = id2href[ref.idref];   // 直接填进去
        ref.linear = a2w(it->Attribute("linear") ? it->Attribute("linear") : "yes");
        m_ocf_pkg.spine.emplace_back(std::move(ref));
    }
    // meta
    auto* meta = doc.RootElement()
        ? doc.RootElement()->FirstChildElement("metadata")
        : nullptr;
    for (auto* it = meta ? meta->FirstChildElement() : nullptr;
        it; it = it->NextSiblingElement()) {
        m_ocf_pkg.meta[a2w(it->Name())] = a2w(it->GetText() ? it->GetText() : "");
    }
    return true;
}
bool EPUBParser::parse_toc()
{
    std::wstring toc_path;
    for (const auto& it : m_ocf_pkg.manifest)
    {
        if (it.properties.find(L"nav") != std::wstring::npos ||
            it.id.find(L"ncx") != std::wstring::npos)
        {
            toc_path = it.href;
            break;
        }
    }
    if (toc_path.empty()) return false;

    m_ocf_pkg.toc_path = toc_path;
    auto toc = m_fp->get(toc_path.c_str());
    if (toc.data.empty()) return false;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(toc.begin(), toc.size()) != tinyxml2::XML_SUCCESS) return false;

    bool is_nav = is_xhtml(toc_path);
    std::string opf_dir = w2a(m_ocf_pkg.opf_dir);

    m_ocf_pkg.toc.clear();

    if (is_nav)
    {
        auto* body = doc.FirstChildElement("html")
            ? doc.FirstChildElement("html")->FirstChildElement("body")
            : nullptr;
        if (!body) return false;

        for (auto* nav = body->FirstChildElement("nav");
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
    return true;
}

void EPUBParser::parse_ncx_points(tinyxml2::XMLElement* navPoint, int level,
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
            np.href = m_fp->find(np.href);
        np.order = level;               // 层级深度
        out.emplace_back(std::move(np));

        // 递归子 <navPoint>
        parse_ncx_points(pt->FirstChildElement("navPoint"), level + 1, opf_dir, out);
    }
}

void EPUBParser::parse_nav_list(tinyxml2::XMLElement* ol, int level,
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
            np.href = m_fp->find(np.href);
        np.order = level;               // 层级深度
        out.emplace_back(std::move(np));

        // 递归子 <ol>
        if (auto* sub = li->FirstChildElement("ol"))
            parse_nav_list(sub, level + 1, opf_dir, out);
    }
}

std::wstring EPUBParser::extract_text(const tinyxml2::XMLElement* a)
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




// 2. 在 EPUBBook.cpp 里写
void EPUBBook::delayed_show_tooltip(std::string txt, unsigned width, unsigned delayMs)
{
    //if (m_tooltipTimer)
    //{
    //    timeKillEvent(m_tooltipTimer);
    //    m_tooltipTimer = 0;
    //}

    //auto* p = new TooltipPayload{ std::move(txt), width };

    //// 3. 用 + 号把 lambda 转成 C 函数指针
    //using CB = void CALLBACK(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    //static CB* const proc = +[](UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR)
    //    {
    //        auto* t = reinterpret_cast<TooltipPayload*>(dwUser);
    //        //show_tooltip(std::move(t->html), std::move(t->width));
    //        delete t;
    //    };

    //m_tooltipTimer = timeSetEvent(delayMs, 1, proc,
    //    reinterpret_cast<DWORD_PTR>(p), TIME_ONESHOT);
}

void EPUBBook::cancel_delayed_tooltip()
{
    //if (m_tooltipTimer)
    //{
    //    timeKillEvent(m_tooltipTimer);
    //    m_tooltipTimer = 0;
    //}
    //hide_tooltip();   // 立即隐藏已显示的 tooltip
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