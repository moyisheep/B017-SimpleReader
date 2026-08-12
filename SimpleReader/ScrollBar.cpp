#include "ScrollBar.h"

#include <Windows.h>
#include <gdiplus.h>

#include "AppSettings.h"
// ---------- 静态 ----------



void ScrollBarEx::GetWindow(HWND hwnd)
{
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    m_hwnd = hwnd;
}
// ---------- API ----------
void ScrollBarEx::SetSpineCount(int n)
{
    m_count = n;
    m_pos = {};
    InvalidateRect(m_hwnd, nullptr, false);
    UpdateWindow(m_hwnd);
}



void ScrollBarEx::SetPosition(int spineId, float totalHeightPx, float offsetPx)
{
    if (spineId >= 0 && spineId < m_count)
    {

        m_pos.spine_id = spineId;
        m_pos.height = totalHeightPx;
        m_pos.offset = offsetPx;

        InvalidateRect(m_hwnd, nullptr, false);
        UpdateWindow(m_hwnd);
    }
}

ScrollBarEx::ScrollBarEx()
{
    m_hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_SCROLLBAR_DOT));
}
ScrollBarEx::~ScrollBarEx()
{
    // 销毁图标句柄
    DestroyIcon(m_hIcon);
}
// ---------- 内部 ----------
LRESULT CALLBACK ScrollBarEx::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ScrollBarEx* self = (ScrollBarEx*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_PAINT:          self->OnPaint(); return 0;
    case WM_LBUTTONDOWN:  self->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSEMOVE:      self->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSELEAVE: self->OnMouseLeave(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_LBUTTONUP:    self->OnLButtonUp(); return 0;
    case WM_RBUTTONUP:    self->OnRButtonUp(); return 0;
    case SBM_SETSPINECOUNT:
        if (self) self->SetSpineCount((int)wp);

        return 0;

    case SBM_SETPOSITION:
        if (self) self->SetPosition((int)wp, (float)LOWORD(lp), (float)HIWORD(lp));
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ScrollBarEx::OnPaint()
{
    //OutputDebugStringA("[ScrollBarEx] WM_PAINT\n");
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int W = rc.right - rc.left;
    const int H = rc.bottom - rc.top;
    const int CX = W / 2;

    /* ---- 双缓冲 ---- */
    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bi{ 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits;
    HBITMAP bmp = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, bmp);
    FillRect(memDC, &rc, (HBRUSH)(COLOR_BTNFACE + 1));

    if (H > 0 && m_count > 0)
    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        const bool crowded = (m_count > 500);
        if (m_count >= 1 && m_count < 30) dot_r = 2;
        else if (m_count >= 30 && m_count < 100) dot_r = 2;
        else if (m_count >= 100) dot_r = 1;
        const double step = static_cast<double>(H) / m_count;

        /* 串起所有点的浅色细线 */
        if (m_count > 1)
        {
            Gdiplus::Pen linkPen(Gdiplus::Color(220, 220, 220), 1);
            g.DrawLine(&linkPen, CX, 0, CX, H);
        }



        for (int i = 0; i < m_count; ++i)
        {
            /* 拥挤时只画当前点 */
            if (i == m_pos.spine_id) continue;


            const int y = static_cast<int>((i + 0.5) * step);


            const int r = dot_r;
            Gdiplus::Color c = Gdiplus::Color(200, 200, 200);

            Gdiplus::SolidBrush br(c);
            g.FillEllipse(&br, CX - r, y - r, 2 * r, 2 * r);
        }
        /* 画当前章节的点 */

        if (m_hIcon)
        {
            // 将 HICON 转换为 GDI+ Image
            Gdiplus::Bitmap bitmap(m_hIcon);

            // 计算绘制位置（图标中心在 y 位置）
             // 计算绘制位置（图标中心在 y 位置）
            int y = (m_pos.spine_id + 0.5) * step;
            int iconSize = 15; // 15x15 像素

            // 绘制图标（缩放至 15x15）
            g.DrawImage(
                &bitmap,
                CX - iconSize / 2,  // X 居中
                y - iconSize / 2,   // Y 居中
                iconSize,           // 目标宽度
                iconSize            // 目标高度
            );


        }
        else
        {
            // 如果加载图标失败，回退到原来的圆点绘制
            Gdiplus::Color c = g_cfg.scrollbar_dot_color_highlight;
            int r = ACTIVE_R;
            int y = (m_pos.spine_id + 0.5) * step;
            Gdiplus::SolidBrush br(c);
            g.FillEllipse(&br, CX - r, y - r, 2 * r, 2 * r);
        }


        /* 1. 计算竖线中心 Y 坐标（比例 0~1） */
        double ratio = 0.0;
        if (m_pos.height > 0)          // 防 0
            ratio = std::clamp(static_cast<double>(m_pos.offset) / m_pos.height, 0.0, 1.0);

        /* 2. 竖线几何：宽 2，高 8，贴在最右边缘（假设可用宽度为 W） */

        int line_w = 2;
        int barY = static_cast<int>((ratio * H) - (line_w / 2.0));   // 居中在比例位置

        /* 3. 画竖线 */

        Gdiplus::Pen linePen(g_cfg.scrollbar_dot_color_highlight, line_w);
        g.DrawLine(&linePen,
            0,
            barY,
            W,
            barY);

    }
    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    EndPaint(m_hwnd, &ps);
}


void ScrollBarEx::OnLButtonDown(int x, int y)
{

}
void ScrollBarEx::OnMouseLeave(int x, int y)
{
}
void ScrollBarEx::OnMouseMove(int x, int y)
{


}

void ScrollBarEx::OnLButtonUp()
{


}

void ScrollBarEx::OnRButtonUp()
{

}



void register_scrollbar_class()
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = ScrollBarEx::WndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = SCROLLBAR_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
}
