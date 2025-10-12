#include "EPUBBook.h"

namespace fs = std::filesystem;

EPUBBook::EPUBBook()
{

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

    //if (g_cfg.enableEPUBFonts) { build_epub_font_index(); }

    LoadToc();
    return true;
}
std::wstring EPUBBook::get_current_dir()
{
    return fs::path(m_current_html_path).parent_path();
}






std::wstring EPUBBook::get_book_path()
{
    return m_current_book_path;
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






EPUBBook::~EPUBBook() {


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

//MemFile EPUBBook::get_binary(std::wstring base_url, std::wstring url)
//{
//    auto path = resolve_path(base_url, url);
//    std::error_code ec;  // 存储错误码
//    MemFile mf{};
//    if (fs::exists(path, ec))
//    {
//        size_t sz = fs::file_size(path, ec);
//        if (ec) return mf;                       // 文件不存在
//        std::ifstream ifs(path, std::ios::binary);
//        if (!ifs) return mf;
//
//        std::vector<uint8_t> buf(sz);
//        ifs.read(reinterpret_cast<char*>(buf.data()), sz);
//
//        mf.data = std::move(buf);
//    }
//    else
//    {
//        mf = read_zip(path);
//    }
//    return mf;
//}

