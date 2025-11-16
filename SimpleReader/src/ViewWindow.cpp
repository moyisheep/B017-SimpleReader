#include "ViewWindow.h"
const wchar_t VIEW_CLASS[] = L"ViewClass";
void register_view_class()
{
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;   // 关键
    wc.lpfnWndProc = ViewWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = VIEW_CLASS;
    RegisterClassW(&wc);
}

LRESULT CALLBACK ViewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        if (g_cMain)
        {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);   // ← 这才是客户区
            g_cMain->resize(rcClient.right, rcClient.bottom);
            //UpdateCache();
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //if (!g_cMain || !g_cMain->m_doc) { return 0; }

        //POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //g_cMain->on_lbutton_down(pt.x/g_cMain->m_zoom_factor, pt.y/g_cMain->m_zoom_factor);
        //convert_coordinate(pt);
        //litehtml::position::vector redraw_boxes;
        //g_cMain->m_doc->on_lbutton_down(pt.x, pt.y, 0, 0, redraw_boxes);
        //if (!redraw_boxes.empty()) {
        //    InvalidateRect(hwnd, nullptr, false);
        //}
        if (g_vd) { g_vd->on_lbutton_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }

        //if (g_cMain && g_cMain->m_doc)
        //{
        //    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //    g_cMain->on_lbutton_up();
        //    convert_coordinate(pt);

        //    litehtml::position::vector redraw_boxes;
        //    g_cMain->m_doc->on_lbutton_up(pt.x, pt.y, 0, 0, redraw_boxes);
        //    if (!redraw_boxes.empty()) {
        //        InvalidateRect(hwnd, nullptr, false);
        //    }

        //}
        if (g_vd) { g_vd->on_lbutton_up(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
    {
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //if (g_cMain)
        //{
        //    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //    g_cMain->on_lbutton_dblclk(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        //    InvalidateRect(hwnd, nullptr, false);
        //}
        if (g_vd) { g_vd->on_lbutton_dblclk(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //if (g_cMain && g_cMain->m_doc)
        //{
        //    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //    g_cMain->on_mouse_move(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        //    convert_coordinate(pt);



        //    litehtml::position::vector redraw_boxes;
        //    g_cMain->m_doc->on_mouse_over(pt.x, pt.y, 0, 0, redraw_boxes);
        //    if (!redraw_boxes.empty()) {
        //        InvalidateRect(hwnd, nullptr, false);
        //    }

        //}
        if (g_vd) { g_vd->on_mouse_move(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }


    case WM_EPUB_ANCHOR:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }

        if (!g_cMain->m_doc) { return 0; }
        wchar_t* sel = reinterpret_cast<wchar_t*>(wp);
        if (sel) {
            std::string cssSel = "[id=\"" + w2a(sel) + "\"]";
            if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
                g_offsetY.store(el->get_placement().y, std::memory_order_relaxed);
            }
            free(sel);          // 对应 _wcsdup
        }
        //UpdateCache();
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(g_hView);

        return 0;
    }
    case WM_EPUB_NAVIGATE:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }

        wchar_t* url = reinterpret_cast<wchar_t*>(wp);
        g_vd->OnTreeSelChanged(url);  // 现在安全地在主线程执行
        free(url);


        return 0;
    }

    case WM_MOUSELEAVE:
    {
        //if(g_cMain && g_cMain->m_doc)
        //{
        //    litehtml::position::vector redraw_boxes;
        //    g_cMain->m_doc->on_mouse_leave(redraw_boxes);
        //    if (!redraw_boxes.empty()) {
        //        InvalidateRect(hwnd, nullptr, false);
        //    }
        //}
        if (g_vd) { g_vd->on_mouse_leave(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }

        return 0;
    }
    case WM_MBUTTONDOWN:  // 鼠标中键按下
    {
        // 检测Ctrl键是否按下
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            // Ctrl+中键被按下
            g_cMain->m_zoom_factor = 1.0f;
            // UpdateCache();
             // 3. 重绘
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;  // 已处理该消息
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        //if (g_cMain) { g_cMain->clear_selection(); }
        //if (GetKeyState(VK_CONTROL) & 0x8000)
        //{
        //    int delta = GET_WHEEL_DELTA_WPARAM(wp);   // ±120
        //    float factor = (delta > 0) ? 1.1f : 0.9f;     // 放大 / 缩小系数

        //    // 2. 更新全局缩放
        //    g_cMain->m_zoom_factor = std::clamp(g_cMain->m_zoom_factor * factor, 0.25f, 5.0f);
        //    UpdateCache();
        //    // 3. 重绘
        //    InvalidateRect(hwnd, NULL, FALSE);
        //
        //    return 0;   // 已处理，不再传递
        //}

        if (g_vd) { g_vd->on_mouse_wheel(GET_WHEEL_DELTA_WPARAM(wp)); }
        //RECT rc;
        //GetClientRect(hwnd, &rc);
        //float h = float(rc.bottom - rc.top);

        //int zDelta = GET_WHEEL_DELTA_WPARAM(wp);
        //// 每格 3 行 → 每行像素 * 3
        //float pxPerLine = g_cfg.font_size * g_cfg.line_height ;
        //float pxDelta = -zDelta / 120.0f * pxPerLine * 3.0f;   // 负号：上滚为负

        //if(g_cfg.enableScrollAnimation)
        //{
        //    // 累加速度，而不是直接改目标
        //    g_velocity.fetch_add(pxDelta * 12.0f, std::memory_order_relaxed);

        //    // 启动 1 kHz 高精度定时器
        //    if (g_scrollTimer == 0) {
        //        g_scrollTimer = timeSetEvent(1, 0, OnScrollTimer, 0, TIME_PERIODIC);
        //    }
        //}
        //else
        //{
        //    float cur = g_offsetY.load(std::memory_order_relaxed);
        //    cur = std::clamp(cur + pxDelta, -h/2.0f, std::max(g_vd->m_height - h / 2.0f, 0.0f));
        //    g_offsetY.store(cur, std::memory_order_relaxed);
        //}

        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //litehtml::position::vector redraw_box;
        //if (g_cMain && g_cMain->m_doc)
        //{
        //    g_cMain->m_doc->on_mouse_leave(redraw_box);

        //}
        //UpdateCache();
        //InvalidateRect(hwnd, nullptr, FALSE);

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);

        //if (g_cMain  && g_cMain->m_doc )
        //{
        //    g_frame_count += 1;
        //    OutputDebugStringA("[View] WM_PAINT\n");
        //    RECT rc;
        //    GetClientRect(g_hView, &rc);
        //    int x = g_center_offset;
        //    int y = -g_offsetY.load(std::memory_order_relaxed);
        //    float w = g_cfg.document_width;
        //    float h = rc.bottom - rc.top;
        //    litehtml::position clip(x, 0, w, h/g_cMain->m_zoom_factor);
        //    g_cMain->present(x, y, &clip);

        //}
        RECT rc;
        GetClientRect(g_hView, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        if (g_vd && width > 0 && height > 0)
        {

            g_vd->present(width, height);

        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
