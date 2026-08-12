#include "MainWindow.h"

#include "ViewWindow.h"
#include "Tooltip.h"
#include "TocPanel.h"
#include "HomePage.h"
#include "ScrollBar.h"
#include "ImageView.h"
#include "SimpleContainer.h"
#include "VirtualDoc.h"
#include "AppStates.h"
#include "MOBIBook.h"
#include "FontPanel.h"

void register_main_class()
{
    WNDCLASSEX w{ sizeof(WNDCLASSEX) };
    w.style = CS_HREDRAW | CS_VREDRAW;   // 关键
    w.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MY_APPLICATION));
    w.hIconSm = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MY_APPLICATION)); // 小图标
    w.lpfnWndProc = WndProc;
    w.hInstance = g_hInst;
    w.hCursor = LoadCursor(nullptr, IDC_ARROW);
    w.lpszClassName = MAIN_CLASS;
    w.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    w.lpszMenuName = nullptr;   // ← 必须为空
    RegisterClassEx(&w);
}


// ---------- 窗口 ----------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {

        register_view_class();

        register_tooltip_class();
        register_imageview_class();
        register_scrollbar_class();
        register_toc_class();
        register_homepage_class();
        // 放在主窗口 CreateWindow 之后
        g_hStatus = CreateWindowEx(
            0, STATUSCLASSNAME, L"就绪",
            WS_CHILD | SBARS_SIZEGRIP,
            0, 0, 0, 0,           // 位置和大小由 WM_SIZE 调整
            hwnd, nullptr, g_hInst, nullptr);

        // 2. 创建
        g_hToc = CreateWindowExW(
            WS_EX_COMPOSITED,          // 双缓冲
            TOC_CLASS,                 // 用注册的类名
            nullptr,
            WS_CHILD | WS_BORDER,
            0, 0, 200, 600,
            hwnd, (HMENU)100, g_hInst, nullptr);



        g_hView = CreateWindowExW(
            0, VIEW_CLASS, nullptr,
            WS_CHILD | WS_CLIPSIBLINGS,
            0, 0, 1, 1,
            hwnd, (HMENU)101, g_hInst, nullptr);

        g_hTooltip = CreateWindowExW(
            WS_EX_COMPOSITED,
            TOOLTIP_CLASS, nullptr,
            WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_BORDER,
            0, 0, 300, 200,
            hwnd, nullptr, g_hInst, nullptr);


        g_hImageview = CreateWindowExW(
            WS_EX_COMPOSITED,
            IMAGEVIEW_CLASS, nullptr,
            WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_BORDER,
            0, 0, 300, 200,
            hwnd, nullptr, g_hInst, nullptr);


        g_hViewScroll = CreateWindowExW(0, SCROLLBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0, hwnd, nullptr,
            g_hInst, nullptr);

        g_hHomepage = CreateWindowExW(0, HOMEPAGE_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0, hwnd, nullptr,
            g_hInst, nullptr);

        g_bootstrap = std::make_unique<AppBootstrap>();

        // =====初始化隐藏=====


        DragAcceptFiles(hwnd, TRUE);

        SendMessage(g_hWnd, WM_SIZE, 0, 0);

        SetForegroundWindow(hwnd);          // 关键：把输入焦点抢过来
        return 0;
    }
    case WM_DROPFILES:
    {
        wchar_t file[MAX_PATH]{};
        DragQueryFileW((HDROP)wp, 0, file, MAX_PATH);
        DragFinish((HDROP)wp);
        PostMessage(hwnd, WM_EPUB_OPEN, 0, (LPARAM)DupPath(file));
        return 0;
    }

    case WM_COPYDATA:
    {
        PCOPYDATASTRUCT p = (PCOPYDATASTRUCT)lp;
        if (p->dwData == WM_EPUB_OPEN && p->lpData)
        {
            PostMessage(hwnd, WM_EPUB_OPEN, 0,
                (LPARAM)DupPath((const wchar_t*)p->lpData));
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        RECT rc; GetClientRect(hwnd, &rc);
        float page = rc.bottom - rc.top;
        float line = g_cMain->m_line_height;

        float delta = 0.f;
        switch (wp)
        {
        case VK_UP:     delta = -line;  break;
        case VK_DOWN:   delta = line;  break;
        case VK_PRIOR:  delta = -page;  break;
        case VK_NEXT:   delta = page;  break;
        default:        return DefWindowProc(hwnd, msg, wp, lp);
        }

        // 原子读-改-写
        float old = g_offsetY.load(std::memory_order_relaxed);
        float desired;
        do {
            desired = std::clamp(old + delta,
                -1.0f,
                std::max(0.0f, g_vd->m_height - page));
        } while (!g_offsetY.compare_exchange_weak(old, desired,
            std::memory_order_relaxed,
            std::memory_order_relaxed));

        UpdateCache();
        //InvalidateRect(g_hView, nullptr, FALSE);
        ScrollWindowEx(hwnd, 0, delta, NULL, NULL, NULL, NULL, SW_INVALIDATE);

        return 0;
    }

    case WM_CONTEXTMENU:
    {
        HWND hwndFrom = (HWND)wp;
        if (hwndFrom == g_hView || hwndFrom == g_hToc)
        {
            // 1. 取出屏幕坐标
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);

            // 2. 如果是由键盘触发（x == -1 && y == -1），
            //    把菜单放到窗口左上角
            if (x == -1 && y == -1)
            {
                RECT rc;
                GetWindowRect(hwndFrom, &rc);
                x = rc.left + 10;
                y = rc.top + 10;
            }

            // 3. 弹出菜单
            HMENU hPopup = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_POPUP));
            HMENU hSub = GetSubMenu(hPopup, 0);
            CheckMenuItem(hSub, IDM_TOGGLE_TOC_WINDOW,
                g_cfg.displayTOC ? MF_CHECKED : MF_UNCHECKED);
            EnableMenuItem(hSub, ID_CHOOSE_FONT, g_cfg.enableCustomFont ? MF_ENABLED : MF_DISABLED);

            TrackPopupMenuEx(
                hSub,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                x, y,
                g_hWnd,
                nullptr
            );

            DestroyMenu(hPopup);
        }
        break;
    }
    // ---------- WM_EPUB_OPEN ----------
    case WM_EPUB_OPEN:
    {
        const wchar_t* file = (const wchar_t*)lp;
        const wchar_t* ext = wcsrchr(file, L'.');

        if (ext)
        {
            if (std::wstring(ext) == std::wstring(L".epub"))
            {
                g_book = std::make_shared<EPUBBook>();
            }
            else if (std::wstring(ext) == std::wstring(L".mobi"))
            {
                g_book = std::make_shared<mobi::MobiBook>();
            }
            //else if (std::wstring(ext) == std::wstring(L".djvu"))
            //{
            //    g_book = std::make_shared<DjVuBook>();
            //}
            else
            {
                SetStatus(STATUSBAR_INFO, L"不是有效的 epub 文件");
                OutputDebugStringW(L"不是有效的 epub 文件\n");
                CoTaskMemFree((void*)file);   // 释放堆拷贝
                return 0;
            }
        }

        // 如果上一次任务还在跑，直接忽略（或加入队列）
        if (g_parse_task.valid() &&
            g_parse_task.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            SetStatus(STATUSBAR_INFO, L"正在加载其他文件，请稍候…");
            OutputDebugStringW(L"正在加载其他文件，请稍候…\n");
            CoTaskMemFree((void*)file);
            return 0;
        }

        // 清理旧数据

        g_cMain->clear();
        g_cImage->clear();
        g_cTooltip->clear();
        g_book->clear();
        g_toc->clear();
        g_vd->clear();
        InvalidateRect(g_hView, nullptr, TRUE);
        InvalidateRect(g_hToc, nullptr, TRUE);

        // 启动新任务
        SetStatus(STATUSBAR_INFO, L"正在加载...");
        g_parse_task = std::async(std::launch::async, [file] {
            try
            {
                if (!fs::exists(file))
                {
                    std::string txt = "[EPUBBook] 文件不存在: " + w2a(file) + "\n";
                    OutputDebugStringA(txt.c_str());

                    return;
                }

                if (!g_book || !g_book->load(w2a(file)))
                {
                    std::string txt = "[EPUBBook] 打开失败: " + w2a(file) + "\n";
                    OutputDebugStringA(txt.c_str());
                    return;
                }
                if (g_cfg.enableEPUBFonts && g_cMain) { g_cMain->build_font_index(make_temp_dir()); }
                if (g_toc)
                {
                    g_toc->Load(g_book->get_ocf_package());                 // 代替 EPUBBook::LoadToc()
                }
                PostMessage(g_hWnd, WM_EPUB_PARSED, 0, 0);
            }
            catch (const std::exception& e)
            {
                auto* buf = DupPath(a2w(e.what()).c_str());
                PostMessage(g_hWnd, WM_LOAD_ERROR, 0, (LPARAM)buf);
            }
            catch (...)
            {
                PostMessage(g_hWnd, WM_LOAD_ERROR, 0,
                    (LPARAM)DupPath(L"未知错误"));
            }
            CoTaskMemFree((void*)file);   // 任务结束释放路径
            });
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_EPUB_PARSED: {

        if (!g_states.isLoaded) {
            g_states.isLoaded = true;
        }
        if (!g_framerateTimer && g_cfg.displayFrameRate)
        {
            g_framerateTimer = timeSetEvent(1000, 0, OnFrameRateTimer, 0, TIME_PERIODIC);
        }
        g_vd->clear();
        g_recorder->flush();
        g_recorder->openBook(g_book->get_book_path());

        if (g_cMain)
        {
            g_cMain->set_book(g_book);
            g_cMain->build_font_index(make_temp_dir());
        }
        DumpBookRecord();


        // 更新设置
        auto& record = g_recorder->m_book_record;
        auto spine_id = record.lastSpineId;
        g_offsetY.store(record.lastOffset, std::memory_order_relaxed);
        g_cfg.font_size = record.fontSize > 0 ? record.fontSize : g_cfg.default_font_size;
        g_cfg.line_height = record.lineHeightMul > 0 ? record.lineHeightMul : g_cfg.default_line_height;
        g_cfg.document_width = record.docWidth > 0 ? record.docWidth : g_cfg.default_document_width;

        int spine_size = g_book->get_spine().size();
        SendMessage(g_hViewScroll, SBM_SETSPINECOUNT, spine_size, 0);

        g_cfg.enableCSS = record.enableCSS;
        CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CSS,
            MF_BYCOMMAND | (g_cfg.enableCSS ? MF_CHECKED : MF_UNCHECKED));

        g_cfg.enableGlobalCSS = record.enableGlobalCSS;
        CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_GLOBAL_CSS,
            MF_BYCOMMAND | (g_cfg.enableGlobalCSS ? MF_CHECKED : MF_UNCHECKED));

        g_cfg.enableCustomFont = record.enableCustomFont;
        CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CUSTOM_FONT,
            MF_BYCOMMAND | (g_cfg.enableCustomFont ? MF_CHECKED : MF_UNCHECKED));

        g_cfg.font_name = record.fontName;
        g_vd->load_book();
        g_vd->load_html(g_book->get_spine()[spine_id].href);

        UpdateCache();          // 复用前面给出的 UpdateCache()


        PostMessage(hwnd, WM_SIZE, 0, 0);

        SetStatus(STATUSBAR_INFO, L"加载完成");
        SetForegroundWindow(hwnd);          // 关键：把输入焦点抢过来
        return 0;
    }


    case WM_SIZE:
    {

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        const int cx = rcClient.right;
        int cyClient = rcClient.bottom;
        if (g_states.isLoaded)
        {

            ShowWindow(g_hHomepage, SW_HIDE);
            ShowWindow(g_hImageview, SW_HIDE);
            ShowWindow(g_hTooltip, SW_HIDE);

            /* 1. 工具栏高度 */
            int cyTB = 0;

            /* 2. 状态栏高度 */
            int cySB = 0;
            if (g_cfg.displayStatusBar && g_hStatus)
            {
                ShowWindow(g_hStatus, SW_SHOW);
                SendMessage(g_hStatus, WM_SIZE, 0, 0);
                RECT rcSB{};
                GetWindowRect(g_hStatus, &rcSB);
                cySB = rcSB.bottom - rcSB.top;
            }
            else if (g_hStatus)
            {
                ShowWindow(g_hStatus, SW_HIDE);
            }

            /* 3. 剩余可用高度 */
            const int cy = cyClient - cyTB - cySB;

            /* 4. 目录宽度 & 竖线 */


            // 最终宽度：显示 TOC 时取 g_splitX 与 idealTocW 的较大值
            const int tocW = g_cfg.displayTOC ? g_splitX : 0;
            ShowWindow(g_hToc, g_cfg.displayTOC ? SW_SHOW : SW_HIDE);


            /* 5. 滚动条宽度 */
            const int sbW = g_cfg.displayScrollBar ? 20 : 0;
            ShowWindow(g_hViewScroll, g_cfg.displayScrollBar ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hView, SW_SHOW);
            /* 6. 摆放子窗口（Y 起点统一为 cyTB） */

        /* 5. 用 SetWindowPos 摆位置，禁止立即重绘、禁止改 Z 序 */
            SetWindowPos(g_hToc, NULL, 0, cyTB, tocW, cy,
                SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);

            SetWindowPos(g_hView, NULL, tocW, cyTB, cx - tocW - sbW, cy,
                SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(g_hViewScroll, NULL, cx - sbW, cyTB, sbW, cy,
                SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);

            /* 6. 统一重绘整个父窗口及其子窗口 */
            RedrawWindow(hwnd, NULL, NULL,
                RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        else
        {

            ShowWindow(g_hStatus, SW_HIDE);

            ShowWindow(g_hToc, SW_HIDE);
            ShowWindow(g_hViewScroll, SW_HIDE);
            ShowWindow(g_hView, SW_HIDE);
            ShowWindow(g_hImageview, SW_HIDE);
            ShowWindow(g_hTooltip, SW_HIDE);



            ShowWindow(g_hHomepage, SW_SHOW);
            MoveWindow(g_hHomepage, 0, 0, cx, cyClient, TRUE);

        }

        return 0;
    }
    case WM_LOAD_ERROR:
    {
        wchar_t* msg = (wchar_t*)lp;
        SetStatus(STATUSBAR_INFO, msg);
        OutputDebugStringW(msg);
        OutputDebugStringW(L"\n");
        CoTaskMemFree(msg);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (LOWORD(lp) >= g_splitX && LOWORD(lp) <= g_splitX + 2)
        {
            SetCapture(hwnd); g_dragging = true;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (g_dragging)
        {
            int x = LOWORD(lp);
            if (x < 50) x = 50; if (x > 400) x = 400;
            g_splitX = x;
            PostMessage(hwnd, WM_SIZE, 0, 0);
        }
        break;
    }
    case WM_LBUTTONUP:
    {
        if (g_dragging) { ReleaseCapture(); g_dragging = false; }
        break;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lp) == HTCLIENT)
        {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            if (pt.x >= g_splitX && pt.x <= g_splitX + 2)
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE)); return TRUE;
            }
        }
        if (HWND(wp) == g_hView)   // 确保是客户区
        {
            if (g_cMain)
            {
                SetCursor(LoadCursor(nullptr, g_cMain->m_currentCursor));
                return TRUE;                  // 已设置光标
            }
        }
        break;
    }
    case WM_EPUB_CACHE_UPDATED:
    {
        if (g_vd->m_isReloading.exchange(false)) {
            g_offsetY.store(g_vd->m_percent * g_vd->m_height,
                std::memory_order_relaxed);
        }

        float delta = static_cast<float>(lp);
        // 原子 += delta
        float old = g_offsetY.load(std::memory_order_relaxed);
        float desired;
        do {
            desired = old + delta;
        } while (!g_offsetY.compare_exchange_weak(old, desired,
            std::memory_order_relaxed,
            std::memory_order_relaxed));

        g_cMain->m_doc = std::move(g_vd->m_doc);


        if (g_vd->m_isAnchor.exchange(false)) {
            std::string cssSel = "[id=\"" + g_vd->m_anchor_id + "\"]";
            if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
                g_offsetY.store(el->get_placement().y,
                    std::memory_order_relaxed);
            }
        }

        UpdateCache();

        InvalidateRect(g_hView, nullptr, TRUE);
        return 0;
    }
    case WM_DESTROY: {
        timeKillEvent(g_framerateTimer);
        timeKillEvent(g_scrollTimer);
        g_recorder->flush();

        PostQuitMessage(0);
        return 0;
    }


    case WM_COMMAND: {
        switch (LOWORD(wp))
        {
        case IDM_TOGGLE_CSS:
            g_cfg.enableCSS = !g_cfg.enableCSS;
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CSS,
                MF_BYCOMMAND | (g_cfg.enableCSS ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_JS:
            g_cfg.enableJS = !g_cfg.enableJS;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_JS,
                MF_BYCOMMAND | (g_cfg.enableJS ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_GLOBAL_CSS:
            g_cfg.enableGlobalCSS = !g_cfg.enableGlobalCSS;          // 切换状态
            g_cfg.enableGlobalCSS ? g_globalCSS = get_global_css() : g_globalCSS = "";
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_GLOBAL_CSS,
                MF_BYCOMMAND | (g_cfg.enableGlobalCSS ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_EPUB_FONTS:
            g_cfg.enableEPUBFonts = !g_cfg.enableEPUBFonts;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_EPUB_FONTS,
                MF_BYCOMMAND | (g_cfg.enableEPUBFonts ? MF_CHECKED : MF_UNCHECKED));
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_cMain && g_cMain->m_fontBin.empty()) { g_cMain->build_font_index(make_temp_dir()); }
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_HOVER_PREVIEW:
            g_cfg.enableHoverPreview = !g_cfg.enableHoverPreview;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_HOVER_PREVIEW,
                MF_BYCOMMAND | (g_cfg.enableHoverPreview ? MF_CHECKED : MF_UNCHECKED));
            break;
        case IDM_TOGGLE_CLICK_PREVIEW:
            g_cfg.enableClickPreview = !g_cfg.enableClickPreview;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CLICK_PREVIEW,
                MF_BYCOMMAND | (g_cfg.enableClickPreview ? MF_CHECKED : MF_UNCHECKED));
            break;
        case IDM_TOGGLE_CUSTOM_FONT:
            g_cfg.enableCustomFont = !g_cfg.enableCustomFont;          // 切换状态
            EnableMenuItem(GetMenu(g_hWnd), ID_CHOOSE_FONT, g_cfg.enableCustomFont ? MF_ENABLED : MF_DISABLED);
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CUSTOM_FONT,
                MF_BYCOMMAND | (g_cfg.enableCustomFont ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;
        case IDM_TOGGLE_SCROLL_ANIMATION:
            g_cfg.enableScrollAnimation = !g_cfg.enableScrollAnimation;          // 切换状态
            timeKillEvent(g_scrollTimer);
            g_scrollTimer = 0;
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_SCROLL_ANIMATION,
                MF_BYCOMMAND | (g_cfg.enableScrollAnimation ? MF_CHECKED : MF_UNCHECKED));
            break;

        case IDM_TOGGLE_FRAME_RATE:
            g_cfg.displayFrameRate = !g_cfg.displayFrameRate;          // 切换状态
            if (g_cfg.displayFrameRate)
            {
                timeKillEvent(g_framerateTimer);
                g_framerateTimer = timeSetEvent(1000, 0, OnFrameRateTimer, 0, TIME_PERIODIC);
            }
            else
            {
                g_statusBuf.clear();
                timeKillEvent(g_framerateTimer);
                g_framerateTimer = 0;
            }

            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_FRAME_RATE,
                MF_BYCOMMAND | (g_cfg.displayFrameRate ? MF_CHECKED : MF_UNCHECKED));
            break;
        case IDM_TOGGLE_TOC_WINDOW:

            g_cfg.displayTOC = !g_cfg.displayTOC;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_TOC_WINDOW,
                MF_BYCOMMAND | (g_cfg.displayTOC ? MF_CHECKED : MF_UNCHECKED));

            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            break;
        case IDM_TOGGLE_SCROLLBAR_WINDOW:
        {
            g_cfg.displayScrollBar = !g_cfg.displayScrollBar;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_SCROLLBAR_WINDOW,
                MF_BYCOMMAND | (g_cfg.displayScrollBar ? MF_CHECKED : MF_UNCHECKED));

            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            break;
        }
        case IDM_TOGGLE_STATUS_WINDOW:

            g_cfg.displayStatusBar = !g_cfg.displayStatusBar;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_STATUS_WINDOW,
                MF_BYCOMMAND | (g_cfg.displayStatusBar ? MF_CHECKED : MF_UNCHECKED));


            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            break;

        case ID_EPUB_RELOAD:

            if (g_vd && g_states.isLoaded) { g_vd->reload(); }
            break;
        case ID_CHOOSE_FONT:
        {
            auto original = g_cfg.font_name;
            int idx = (int)DialogBoxParam(g_hInst,
                MAKEINTRESOURCE(IDD_FONTDLG),
                hwnd,
                FontDlgProc,
                0);

            if (idx <= 0)
            {
                g_cfg.font_name = original;
                if (g_vd) { g_vd->reload(); }
            }

            break;
        }
        case ID_FONT_BIGGER:        // Ctrl + '+'
            g_cfg.font_size = std::min(g_cfg.font_size + 1.0f, 72.0f);   // 上限 72
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_vd) { g_vd->reload(); }   // 重新加载并排版
            break;

        case ID_FONT_SMALLER:       // Ctrl + '-'
            g_cfg.font_size = std::max(g_cfg.font_size - 1.0f, 8.0f);    // 下限 8
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_FONT_RESET:         // Ctrl + '0'
            g_cfg.font_size = g_cfg.default_font_size;   // 默认字号
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_LINE_HEIGHT_UP:     // Ctrl + Shift + '+'
            g_cfg.line_height = std::min(g_cfg.line_height + 0.1f, 3.0f);
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_LINE_HEIGHT_DOWN:   // Ctrl + Shift + '-'
            g_cfg.line_height = std::max(g_cfg.line_height - 0.1f, 1.0f);
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_LINE_HEIGHT_RESET:  // Ctrl + Shift + '0'
            g_cfg.line_height = g_cfg.default_line_height;  // 默认行高
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_WIDTH_BIGGER:       // Alt + '→'
            g_cfg.document_width = std::min(g_cfg.document_width + 50.0f, 2000.0f);
            if (g_vd) { g_vd->reload(); }   // 仅重新排版即可
            break;

        case ID_WIDTH_SMALLER:      // Alt + '←'
            g_cfg.document_width = std::max(g_cfg.document_width - 50.0f, 300.0f);
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_WIDTH_RESET:        // Alt + '0'
            g_cfg.document_width = g_cfg.default_document_width;   // 默认宽度
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_EDIT_COPY:
        {
            if (IsMouseOverWindow(g_hView)) {
                // 鼠标在主窗口上
                g_bootstrap->copy_to_clipboard(g_hWnd, g_cMain->m_sel_text);
            }

            if (IsMouseOverWindow(g_hToc)) {
                // 鼠标在目录窗口上
                g_bootstrap->copy_to_clipboard(g_hWnd, g_toc->m_sel_text);
            }

            break;
        }
        case ID_FILE_OPEN:
        {
            OpenEpubWithDialog(hwnd);   // 就是之前那段 IFileOpenDialog 代码
            break;
        }
        case ID_FILE_EXIT:
            PostQuitMessage(0);
            break;
        case ID_ZOOM_BIGGER:
            g_cMain->m_zoom_factor = std::min(g_cMain->m_zoom_factor + 0.1f, 5.0f);
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_ZOOM_SMALLER:
            g_cMain->m_zoom_factor = std::max(g_cMain->m_zoom_factor - 0.1f, 0.25f);
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_ZOOM_RESET:
            g_cMain->m_zoom_factor = 1.0f;
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_BACKGROUND_COLOR_DEFAULT:
            g_cfg.background_color = g_cfg.default_background_color;
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_BACKGROUND_COLOR_BEIGE:
            g_cfg.background_color = { 246.0f / 255.0f, 243.0f / 255.0f, 233.0f / 255.0f, 1.0f };
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_RESET_ALL:
            g_cMain->m_zoom_factor = 1.0f;
            g_cfg.font_name = g_cfg.default_font_name;
            g_cfg.font_size = g_cfg.default_font_size;
            g_cfg.document_width = g_cfg.default_document_width;
            g_cfg.line_height = g_cfg.default_line_height;
            g_cfg.background_color = g_cfg.default_background_color;
            g_cfg.displayFrameRate = true;
            g_cfg.displayScrollBar = true;
            g_cfg.displayStatusBar = true;
            g_cfg.displayTOC = true;
            g_cfg.enableClickPreview = true;
            g_cfg.enableCSS = true;
            g_cfg.enableCustomFont = false;
            g_cfg.enableEPUBFonts = true;
            g_cfg.enableFontRealtimePreview = true;
            g_cfg.enableGlobalCSS = true;
            g_cfg.enableHoverPreview = true;
            g_cfg.enableScrollAnimation = false;
            CheckAllMenuItem();
            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            if (g_vd) { g_vd->reload(); }
            break;
        }
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);

}