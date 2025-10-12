#include "AppBootstrap.h"

AppBootstrap::AppBootstrap() {
    //make_tooltip_backend();
    if (lunasvgRegisterCambriaMath()) { OutputDebugStringW(L"[lunasvg] 注册字体成功： Cambria Math\n"); }
    if (g_cfg.enableJS) { enableJS(); }

    if (!g_book) { g_book = std::make_unique<EPUBBook>(); }

    if (!g_vd) { g_vd = std::make_unique<VirtualDoc>(); }

    if (!g_recorder)
    {

        g_recorder = std::make_unique<ReadingRecorder>(documents_dir());
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
        g_toc->SetOnNavigate([](const std::wstring& href) {
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







