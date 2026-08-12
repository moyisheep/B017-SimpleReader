#include "ImageView.h"

#include "SimpleContainer.h"
#include "AppBootstrap.h"

void register_imageview_class()
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = ImageviewProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = IMAGEVIEW_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
}


LRESULT CALLBACK ImageviewProc(HWND hwnd, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_DESTROY:
        return 0;
    case WM_PAINT:
    {

        if (!IsWindowVisible(g_hImageview)) { return 0; }
        if (!g_cImage)
        {
            OutputDebugStringA("[ImageviewProc] self or doc null\n");
            break;
        }

        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);


        {
            //OutputDebugStringA("[ImageView] WM_PAINT\n");
            RECT rc;
            GetClientRect(g_hImageview, &rc);
            litehtml::position clip(0, 0, rc.right - rc.left, rc.bottom - rc.top);
            g_cImage->present(0, 0, &clip);


        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEWHEEL: {



        int delta = GET_WHEEL_DELTA_WPARAM(w);
        float factor = (delta > 0) ? 1.1f : 0.9f; /* 1. 鼠标指针在屏幕上的位置 */
        POINT pt;
        GetCursorPos(&pt); /* 2. 当前窗口矩形（屏幕坐标） */
        RECT wr; GetWindowRect(hwnd, &wr); /* 3. 获取屏幕工作区大小（不含任务栏） */

        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfo(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
        int scrW = mi.rcWork.right - mi.rcWork.left;
        int scrH = mi.rcWork.bottom - mi.rcWork.top;
        /* 3.5 提前判断：若窗口外框已顶满屏幕，放大就忽略 */
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        UINT dpi = GetDpiForWindow(hwnd);
        RECT rNow{ 0, 0, g_imageviewRenderW, 1 }; // 高度先随便填，只要算宽度 
        AdjustWindowRectExForDpi(&rNow, style, FALSE, exStyle, dpi);
        int winW_now = rNow.right - rNow.left;
        g_cImage->m_doc->render(g_imageviewRenderW);
        int docH_now = g_cImage->m_doc->height(); RECT rH{ 0, 0, 1, docH_now };
        AdjustWindowRectExForDpi(&rH, style, FALSE, exStyle, dpi);
        int winH_now = rH.bottom - rH.top; // 放大且已顶满 → 直接 return 
        if (factor > 1.0f && (winW_now >= scrW || winH_now >= scrH)) { return 0; } /* 4. 计算新的渲染尺寸，并立即限制在屏幕内 */
        int renderW = std::max(32, static_cast<int>(g_imageviewRenderW * factor + 0.5f));
        int renderH = 0; // 先限制宽度 
        renderW = std::min(renderW, scrW); // 重新渲染得到高度 
        g_cImage->m_doc->render(renderW);
        renderH = g_cImage->m_doc->height(); // 再限制高度
        renderH = std::min(renderH, scrH);
        renderW = std::max(renderW, 32); // 防止极端情况宽度过小 
        /* 5. 计算窗口外框尺寸（含标题栏/边框） */
        RECT r{ 0, 0, renderW, renderH };
        AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
        int winW = r.right - r.left; int winH = r.bottom - r.top;
        /* 6. 以鼠标位置为缩放原点，计算新左上角 */
        int newX = pt.x - (pt.x - wr.left) * winW / (wr.right - wr.left);
        int newY = pt.y - (pt.y - wr.top) * winH / (wr.bottom - wr.top);
        /* 7. 最终再保证左上角也在屏幕内（简单 clamp） */
        newX = std::max((long)newX, mi.rcWork.left);
        newY = std::max((long)newY, mi.rcWork.top);
        newX = std::min((long)newX, mi.rcWork.right - winW);
        newY = std::min((long)newY, mi.rcWork.bottom - winH);
        /* 8. 更新窗口 & 画布 */
        g_cImage->resize(renderW, renderH);
        SetWindowPos(hwnd, HWND_TOPMOST, newX, newY, winW, winH, SWP_NOACTIVATE | SWP_NOZORDER);

        g_imageviewRenderW = renderW;

        InvalidateRect(hwnd, nullptr, TRUE);

        return 0;
    }
                      /* 2. 左键拖动 */
    case WM_LBUTTONDOWN:
        g_imageview_dragging = true;
        SetCapture(hwnd);                     // 锁定鼠标
        g_imageview_drag_pos = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        return 0;

    case WM_LBUTTONUP:
        if (g_imageview_dragging)
        {
            g_imageview_dragging = false;
            ReleaseCapture();
        }

        return 0;

    case WM_MOUSEMOVE:
        if (g_imageview_dragging)
        {
            int dx = GET_X_LPARAM(l) - g_imageview_drag_pos.x;
            int dy = GET_Y_LPARAM(l) - g_imageview_drag_pos.y;

            RECT wr;
            GetWindowRect(hwnd, &wr);
            SetWindowPos(hwnd, nullptr,
                wr.left + dx, wr.top + dy,
                0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

        }
        return 0;
    case WM_RBUTTONUP:
    {
        g_bootstrap->hide_imageview();
        return 0;
    }

    case WM_ERASEBKGND:
        if (g_cImage) { g_cImage->clear_background(); }
        return 1;
    }
    return DefWindowProc(hwnd, m, w, l);
}