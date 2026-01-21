#include "HtmlWindow.h"

void HtmlWindow::GetWindow(HWND hwnd)
{
}

void HtmlWindow::OpenHtml(const std::string& path)
{
}

void HtmlWindow::SetHtml(const std::string& html)
{
}

void HtmlWindow::convert_coordinate(POINT& pt)
{
    if (m_container)
    {
        pt.x = pt.x / g_cMain->m_zoom_factor - g_center_offset;
        pt.y = pt.y / g_cMain->m_zoom_factor + g_offsetY.load(std::memory_order_relaxed); ;
    }

}

LRESULT CALLBACK HtmlWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HtmlWindow* self = (HtmlWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_SIZE: self->OnSize(); return 0;
    case WM_PAINT:          self->OnPaint(); return 0;
    case WM_LBUTTONDOWN:  self->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSEMOVE:      self->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSELEAVE: self->OnMouseLeave(); return 0;
    case WM_MOUSEWHEEL: self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp)); return 0;
    case WM_LBUTTONUP:    self->OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_RBUTTONUP:    self->OnRButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_ERASEBKGND: self->OnEraseBackground(); return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void HtmlWindow::OnSize() 
{
    if (m_hwnd)
    {
        RECT rcClient;
        GetClientRect(m_hwnd, &rcClient);   // ← 这才是客户区
        m_container->resize(rcClient.right, rcClient.bottom);
        UpdateCache();
    }
}

void HtmlWindow::OnLButtonDown(int x, int y)
{
    if (m_container && m_doc) 
    { 
        POINT pt{ x, y };
        m_container->on_lbutton_down(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        convert_coordinate(pt);
        litehtml::position::vector redraw_boxes;
        m_doc->on_lbutton_down(pt.x, pt.y, 0, 0, redraw_boxes);
        float offset = g_offsetY.load(std::memory_order_relaxed);
        if (!redraw_boxes.empty()) {
            for (auto& box : redraw_boxes)
            {
                RECT rc{ g_center_offset + box.left(), box.top() - offset, g_center_offset + box.right(), box.bottom() - offset };
                InvalidateRect(m_hwnd, &rc, false);
            }
        }
    }


}

void HtmlWindow::OnLButtonUp(int x, int y) 
{
    if (!g_tickTimer)
    {
        g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
    }

    if (m_container && m_doc)
    {
        POINT pt{ x, y };
        m_container->on_lbutton_up();
        convert_coordinate(pt);

        litehtml::position::vector redraw_boxes;
        m_doc->on_lbutton_up(pt.x, pt.y, 0, 0, redraw_boxes);
        float offset = g_offsetY.load(std::memory_order_relaxed);
        if (!redraw_boxes.empty()) {
            for (auto& box : redraw_boxes)
            {

                RECT rc{ g_center_offset + box.left(), box.top() - offset, g_center_offset + box.right(), box.bottom() - offset };
                InvalidateRect(m_hwnd, &rc, false);
            }
        }

    }
}
void HtmlWindow::OnLButtonDoubleClick(int x, int y)
{
    if (!g_tickTimer)
    {
        g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
    }
    if (m_container)
    {
        POINT pt{ x, y };
        m_container->on_lbutton_dblclk(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        InvalidateRect(m_hwnd, nullptr, false);
    }
}

void HtmlWindow::OnMouseMove(int x, int y)
{
    // 更新阅读记录
    if (!g_tickTimer)
    {
        g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
    }
    if (m_container && m_doc)
    {
        POINT pt{ x, y};
        m_container->on_mouse_move(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        convert_coordinate(pt);



        litehtml::position::vector redraw_boxes;
        m_doc->on_mouse_over(pt.x, pt.y, 0, 0, redraw_boxes);
        float offset = g_offsetY.load(std::memory_order_relaxed);
        if (!redraw_boxes.empty()) {
            for (auto& box : redraw_boxes)
            {

                RECT rc{ g_center_offset + box.left(), box.top() - offset, g_center_offset + box.right(), box.bottom() - offset };
                InvalidateRect(m_hwnd, &rc, false);
            }
        }

    }
}

//  WM_EPUB_ANCHOR:
void HtmlWindow::OnAnchor() 
{
    // 更新阅读记录
    if (!g_tickTimer)
    {
        g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
    }

    if (m_doc)
    { 
        wchar_t* sel = reinterpret_cast<wchar_t*>(wp);
        if (sel) {
            std::string cssSel = "[id=\"" + w2a(sel) + "\"]";
            if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
                g_offsetY.store(el->get_placement().y, std::memory_order_relaxed);
            }
            free(sel);          // 对应 _wcsdup
        }
        UpdateCache();
        InvalidateRect(hwnd, nullptr, TRUE);
        //UpdateWindow(g_hView);
    }


}

// WM_EPUB_NAVIGATE
void HtmlWindow::OnNavigate() 
{
    // 更新阅读记录
    if (!g_tickTimer)
    {
        g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
    }

    wchar_t* url = reinterpret_cast<wchar_t*>(wp);
    g_vd->OnTreeSelChanged(w2a(url));  // 现在安全地在主线程执行
    free(url);


}

void HtmlWindow::OnMouseLeave() 
{
    if (m_container && m_doc)
    {
        litehtml::position::vector redraw_boxes;
        m_doc->on_mouse_leave(redraw_boxes);
        float offset = g_offsetY.load(std::memory_order_relaxed);
        if (!redraw_boxes.empty()) {
            for (auto& box : redraw_boxes)
            {

                RECT rc{ g_center_offset + box.left(), box.top() - offset, g_center_offset + box.right(), box.bottom() - offset };
                InvalidateRect(m_hwnd, &rc, false);
            }
        }
    }

}

void HtmlWindow::OnMButtonDown() 
{
    // 检测Ctrl键是否按下
    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
        // Ctrl+中键被按下
        m_container->m_zoom_factor = 1.0f;
        UpdateCache();
        // 3. 重绘
        InvalidateRect(m_hwnd, NULL, TRUE);
        return ;  // 已处理该消息
    }
}

void HtmlWindow::OnMouseWheel(int delta) 
{
    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
      
        float factor = (delta > 0) ? 1.1f : 0.9f;     // 放大 / 缩小系数

        // 2. 更新全局缩放
        m_container->clear_selection();
        m_container->m_zoom_factor = std::clamp(g_cMain->m_zoom_factor * factor, 0.25f, 5.0f);

        UpdateCache();
        // 3. 重绘
        InvalidateRect(m_hwnd, NULL, TRUE);

        return ;   // 已处理，不再传递
    }


    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float h = float(rc.bottom - rc.top);

    // 每格 3 行 → 每行像素 * 3
    float pxPerLine = g_cfg.font_size * g_cfg.line_height;
    float pxDelta = -delta / 120.0f * pxPerLine * 3.0f;   // 负号：上滚为负
    if (m_container) { m_container->on_mouse_wheel(-pxDelta); }
    if (g_cfg.enableScrollAnimation)
    {
        // 累加速度，而不是直接改目标
        g_velocity.fetch_add(pxDelta * 12.0f, std::memory_order_relaxed);

        // 启动 1 kHz 高精度定时器
        if (g_scrollTimer == 0) {
            g_scrollTimer = timeSetEvent(1, 0, OnScrollTimer, 0, TIME_PERIODIC);
        }
    }
    else
    {
        float cur = g_offsetY.load(std::memory_order_relaxed);
        float newOffset = std::clamp(cur + pxDelta, -h / 2.0f, std::max(g_vd->m_height - h / 2.0f, 0.0f));
        g_offsetY.store(newOffset, std::memory_order_relaxed);
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }

    // 更新阅读记录
    if (!g_tickTimer)
    {
        g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
    }
    litehtml::position::vector redraw_boxes;
    if (m_container && m_doc)
    {
        m_doc->on_mouse_leave(redraw_boxes);
        float offset = g_offsetY.load(std::memory_order_relaxed);
        if (!redraw_boxes.empty()) {
            for (auto& box : redraw_boxes)
            {

                RECT rc{ g_center_offset + box.left(), box.top() - offset, g_center_offset + box.right(), box.bottom() - offset };
                InvalidateRect(m_hwnd, &rc, false);
            }
        }

    }
    UpdateCache();
}
void HtmlWindow::OnPaint() 
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);


    if (m_container && m_doc)
    {
        g_frame_count += 1;
        //OutputDebugStringA("[View] WM_PAINT\n");
        //RECT rc;
        //GetClientRect(g_hView, &rc);
        int x = g_center_offset;
        int y = -g_offsetY.load(std::memory_order_relaxed);
        //float w = g_cfg.document_width;
        //float h = rc.bottom - rc.top;
        //litehtml::position clip(x, 0, w, h / g_cMain->m_zoom_factor);

        RECT r = ps.rcPaint;
        litehtml::position clip{ (float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top) };
        m_container->present(x, y, &clip);




        TimerOutput::Instance().end();
        std::string txt = "==================================\n";
        OutputDebugStringA(txt.c_str());

    }

    // 可视化重绘区域
    //HBRUSH hBkBrush = CreateSolidBrush(RGB(255, 255, 255));
    //HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBkBrush);
    //SetBkMode(hdc, TRANSPARENT);
    //RECT updateRect = ps.rcPaint;
    //HPEN hRedPen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    //HPEN hOldPen = (HPEN)SelectObject(hdc, hRedPen);
    //Rectangle(hdc, updateRect.left, updateRect.top, updateRect.right, updateRect.bottom);
    //SelectObject(hdc, hOldPen);
    //SelectObject(hdc, hOldBrush);
    //DeleteObject(hBkBrush);
    //DeleteObject(hRedPen);
    EndPaint(m_hwnd, &ps);
}

void HtmlWindow::OnEraseBackground() 
{
    if (m_container) { m_container->clear_background(); }
}

