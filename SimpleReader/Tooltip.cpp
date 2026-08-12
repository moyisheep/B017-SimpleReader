#include "Tooltip.h"

#include "SimpleContainer.h"

void register_tooltip_class()
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = TooltipProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = TOOLTIP_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
}


LRESULT CALLBACK TooltipProc(HWND hwnd, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {

    case WM_PAINT:
    {
        if (!IsWindowVisible(g_hTooltip)) { return 0; }
        if (!g_cTooltip)
        {
            OutputDebugStringA("[TooltipProc] self or doc null\n");
            break;
        }

        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        /* D2D 渲染 */

        {
            //OutputDebugStringA("[Tooltip] WM_PAINT\n");
            RECT rc;
            GetClientRect(g_hTooltip, &rc);
            litehtml::position clip(0, 0, rc.right - rc.left, rc.bottom - rc.top);
            g_cTooltip->present(0, 0, &clip);


        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    case WM_ERASEBKGND:
        if (g_cTooltip) { g_cTooltip->clear_background(); }
        return 1;
    }
    return DefWindowProc(hwnd, m, w, l);
}
