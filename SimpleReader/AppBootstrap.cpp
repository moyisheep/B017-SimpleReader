#include "AppBootstrap.h"

#include "AppSettings.h"
#include "TocPanel.h"
#include "VirtualDoc.h"
#include "SimpleContainer.h"
#include "ScrollBar.h"


// 注册 Cambria Math（常规字重，非粗非斜）
bool lunasvgRegisterCambriaMath()
{

    wchar_t fontDir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, fontDir)))
        return false;

    wchar_t fullPath[MAX_PATH]{};
    PathCombineW(fullPath, fontDir, L"cambria.ttc");   // Cambria Math 在 .ttc 里

    char utf8Path[MAX_PATH * 3]{};
    WideCharToMultiByte(CP_UTF8, 0, fullPath, -1, utf8Path, sizeof(utf8Path), nullptr, nullptr);

    // 把 Cambria Math 的 Regular face 注册为 "Cambria Math"
    return lunasvg_add_font_face_from_file("Cambria Math", false, false, utf8Path);

}



AppBootstrap::AppBootstrap() {
    //make_tooltip_backend();
    if (lunasvgRegisterCambriaMath()) { OutputDebugStringW(L"[lunasvg] 注册字体成功： Cambria Math\n"); }
    if (g_cfg.enableJS) { enableJS(); }

    if (!g_book) { g_book = std::make_unique<EPUBBook>(); }

    if (!g_vd) { g_vd = std::make_unique<VirtualDoc>(); }

    if (!g_recorder)
    {
        fs::path dbPath = documents_dir() / g_cfg.appName / "data";
        g_recorder = std::make_unique<ReadingRecorder>(dbPath);
        if (g_recorder)
        {
            auto& settings = g_recorder->m_setting_record;
            g_cfg.enableClickPreview = settings.enableClickPreview;
            g_cfg.enableEPUBFonts = settings.enableLoadEPUBFonts;
            g_cfg.enableFontRealtimePreview = settings.enableFontRealtimePreview;
            g_cfg.enableHoverPreview = settings.enableHoverPreview;
            g_cfg.enableScrollAnimation = settings.enableScrollAnimation;
            g_cfg.displayFrameRate = settings.displayFrameRate;
            g_cfg.displayScrollBar = settings.displayScrollBar;
            g_cfg.displayStatusBar = settings.displayStatusBar;
            g_cfg.displayTOC = settings.displayTOC;
            CheckAllMenuItem();
            g_globalCSS = g_cfg.enableGlobalCSS ? get_global_css() : "";
        }
    }

    if (!g_toc)
    {
        g_toc = std::make_unique<TocPanel>();
        g_toc->GetWindow(g_hToc);
        // 绑定目录点击 -> 章节跳转
        g_toc->SetOnNavigate([](const std::string& href) {
            g_vd->OnTreeSelChanged(href.c_str());
            });
    }
    if (!g_cMain) { g_cMain = std::make_unique<SimpleContainer>(10, 10, g_hView); }

    if (!g_cTooltip) { g_cTooltip = std::make_unique<SimpleContainer>(10, 10, g_hTooltip); }

    if (!g_cImage) { g_cImage = std::make_unique<SimpleContainer>(10, 10, g_hImageview); }

    if (!g_scrollbar)
    {
        g_scrollbar = std::make_unique<ScrollBarEx>();
        g_scrollbar->GetWindow(g_hViewScroll);
    }

    if (!g_cHome)
    {
        g_cHome = std::make_unique<SimpleContainer>(10, 10, g_hHomepage);
        fs::path html_path = exe_dir() / "res" / "homepage.html";
        auto html = read_file(html_path);
        if (html.empty()) { OutputDebugStringA("[AppBootstrap] html is null!"); return; }
        std::string time_txt = "";
        if (g_recorder)
        {
            int64_t seconds = g_recorder->getTotalTime();
            time_txt = w2a(seconds2string(seconds));
        }
        boost::algorithm::replace_first(html, "[ID_READING_TIME]", time_txt);
        g_cHome->m_doc = litehtml::document::createFromString({ html.c_str(), litehtml::encoding::utf_8 }, g_cHome.get());
        if (!g_cHome->m_doc) { OutputDebugStringA("[AppBootstrap] g_cHome->m_doc is null!"); return; }
    }

    //BuildSplashWithText();
}

AppBootstrap::~AppBootstrap() {

}




void AppBootstrap::enableJS()
{
    //if (!m_jsrt) m_jsrt = std::make_unique<js_runtime>(g_doc.get());
    //if (!m_jsrt->switch_engine("duktape"))
    //    OutputDebugStringA("[Duktape] Duktape init failed\n");
    //else {
    //    OutputDebugStringA("[Duktape] Duktape init OK\n");
    //    m_jsrt->set_logger(OutputDebugStringA);
    //    m_jsrt->eval("console.log('hello from duktape\n');");
    //}
}

void AppBootstrap::disableJS()
{
    //m_jsrt.reset();   // 直接销毁即可，js_runtime 会负责 shutdown
}

void AppBootstrap::run_pending_scripts()
{
    //    if (!m_jsrt) return;          // 没有 JS 引擎就跳过
    //    for (const auto& script : m_pending_scripts)
    //    {
    //        litehtml::string code;
    //        script.el->get_text(code);  // 取出 <script> 里的纯文本
    //        if (!code.empty())
    //            m_jsrt->eval(code, "<script>");  // 交给 QuickJS / Duktape / V8
    //    }
    //    m_pending_scripts.clear();    // 执行完清空
}

void AppBootstrap::bind_host_objects()
{
    //if (!m_jsrt) return;
   // m_jsrt->bind_document(g_doc.get());   // js_runtime 内部会转发到当前引擎
}





/* ---------- 实现 ---------- */

std::string AppBootstrap::extract_anchor(const char* href)
{
    if (!href) return "";
    const char* p = std::strrchr(href, '#');
    return p ? (p + 1) : "";

}
litehtml::element::ptr AppBootstrap::find_link_in_chain(litehtml::element::ptr start)
{
    for (auto cur = start; cur; cur = cur->parent())
    {
        const char* tag = cur->get_tagName();
        if (std::strcmp(tag, "p") == 0) break;
        if (std::strcmp(tag, "a") == 0) return cur;
    }
    return nullptr;
}

bool AppBootstrap::skip_attr(const std::string& val)
{
    if (val.empty()) return true;
    if (val == "0")  return true;
    return std::all_of(val.begin(), val.end(),
        [](unsigned char c) { return std::isspace(c); });
}

std::string AppBootstrap::get_html(litehtml::element::ptr el)
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
std::string AppBootstrap::html_of_anchor_paragraph(litehtml::document* doc, const std::string& anchorId)
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


std::string AppBootstrap::get_html_of_image(litehtml::element::ptr start)
{
    if (!start && std::strcmp(start->get_tagName(), "img") != 0) { return ""; }
    std::string inner = get_html(start);
    return "<style>img{display:block;width:100%;height:auto;}</style>" + inner;

}
void AppBootstrap::show_imageview(const litehtml::element::ptr& el)
{
    std::string html = get_html_of_image(el);
    if (html.empty()) { return; }
    POINT pt;
    GetCursorPos(&pt);   // pt.x, pt.y 为屏幕坐标
    //ScreenToClient(g_hView, &pt);   // 现在 pt.x, pt.y 是相对于窗口客户区的坐标


    g_cImage->m_doc = litehtml::document::createFromString(
        { html.c_str(), litehtml::encoding::utf_8 }, g_cImage.get());
    int width = g_cfg.tooltip_width;
    g_cImage->m_doc->render(width);

    int height = g_cImage->m_doc->height();
    auto tip_x = pt.x - width / 2;
    auto tip_y = pt.y - height / 2;



    DWORD style = GetWindowLong(g_hImageview, GWL_STYLE);
    DWORD exStyle = GetWindowLong(g_hImageview, GWL_EXSTYLE);
    UINT dpi = GetDpiForWindow(g_hImageview);
    RECT r{ 0, 0, width, height };
    AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
    g_cImage->resize(width, height);
    SetWindowPos(g_hImageview, HWND_TOPMOST,
        tip_x, tip_y,
        r.right - r.left, r.bottom - r.top,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);

    g_imageviewRenderW = width;

    InvalidateRect(g_hImageview, nullptr, true);
}
void AppBootstrap::show_tooltip(const std::string txt, int width)
{
    auto html = txt;
    //OutputDebugStringA(("[show_tooltip] " + std::to_string(x) + " " + std::to_string(y) + "\n").c_str());
    if (html.empty()) { return; }

    //if (g_vd && !g_vd->m_blocks.empty())
    //{
    //    html = "<html>" + g_vd->m_blocks.back().head + "<body>" + html + "</body></html>";
    //}
    POINT pt;
    GetCursorPos(&pt);




    g_cTooltip->m_doc = litehtml::document::createFromString(
        { html.c_str(), litehtml::encoding::utf_8 }, g_cTooltip.get());

    g_cTooltip->m_doc->render(width);
    int height = g_cTooltip->m_doc->height();


    int tip_x = pt.x - width / 2;
    int tip_y = pt.y - height - 20;
    if (tip_y < 0) { tip_y = pt.y + 20; }
    DWORD style = GetWindowLong(g_hTooltip, GWL_STYLE);
    DWORD exStyle = GetWindowLong(g_hTooltip, GWL_EXSTYLE);
    UINT dpi = GetDpiForWindow(g_hTooltip);
    RECT r{ 0, 0, width, height };
    AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
    g_cTooltip->resize(width, height);
    SetWindowPos(g_hTooltip, HWND_TOPMOST,
        tip_x, tip_y,
        r.right - r.left, r.bottom - r.top,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);



    InvalidateRect(g_hTooltip, nullptr, true);

}
void AppBootstrap::hide_imageview()
{

    if (g_hImageview && IsWindowVisible(g_hImageview))
    {
        ShowWindow(g_hImageview, SW_HIDE);
        if (g_cImage && g_cImage->m_doc)
        {
            g_cImage->m_doc.reset();
        }

    }
}
void AppBootstrap::hide_tooltip()
{

    if (g_hTooltip && IsWindowVisible(g_hTooltip))
    {
        ShowWindow(g_hTooltip, SW_HIDE);

    }
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
std::string AppBootstrap::get_anchor_html(litehtml::document* doc,
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






// 2. 在 EPUBBook.cpp 里写
void AppBootstrap::delayed_show_tooltip(std::string txt, unsigned width, unsigned delayMs)
{
    if (m_tooltipTimer)
    {
        timeKillEvent(m_tooltipTimer);
        m_tooltipTimer = 0;
    }

    auto* p = new TooltipPayload{ std::move(txt), width };

    // 3. 用 + 号把 lambda 转成 C 函数指针
    using CB = void CALLBACK(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    static CB* const proc = +[](UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR)
    {
        auto* t = reinterpret_cast<TooltipPayload*>(dwUser);
        g_bootstrap->show_tooltip(std::move(t->html), std::move(t->width));
        delete t;
    };

    m_tooltipTimer = timeSetEvent(delayMs, 1, proc,
        reinterpret_cast<DWORD_PTR>(p), TIME_ONESHOT);
}

void AppBootstrap::cancel_delayed_tooltip()
{
    if (m_tooltipTimer)
    {
        timeKillEvent(m_tooltipTimer);
        m_tooltipTimer = 0;
    }
    hide_tooltip();   // 立即隐藏已显示的 tooltip
}



void AppBootstrap::copy_to_clipboard(HWND hwnd, std::wstring txt)
{
    if (txt.empty()) return;

    const size_t bufSize = (txt.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bufSize)) {
        if (wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem))) {
            // 直接复制整个字符串内容
            wcscpy_s(dst, txt.size() + 1, txt.c_str());
            GlobalUnlock(hMem);

            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                SetClipboardData(CF_UNICODETEXT, hMem);
                CloseClipboard();
            }
            else {
                GlobalFree(hMem);
            }
        }
        else {
            GlobalFree(hMem);
        }
    }
}


