#include "HomePage.h"

#include "SimpleContainer.h"
#include "AppStates.h"

LRESULT CALLBACK HomepageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        if (g_cHome && g_cHome->m_doc)
        {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int width = rcClient.right - rcClient.left;
            int height = rcClient.bottom - rcClient.top;
            g_cHome->m_doc->render(width);
            g_cHome->resize(width, height);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
    {
        //OpenEpubWithDialog(hwnd);
        return 0;
    }
    case WM_PAINT:
    {
        if (!IsWindowVisible(g_hHomepage)) { return 0; }
        if (!g_cHome)
        {
            OutputDebugStringA("[TooltipProc] self or doc null\n");
            break;
        }
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        if (!g_states.isLoaded && g_cHome->m_doc)
        {
            //OutputDebugStringA("[Homepage] WM_PAINT\n");
            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;
            litehtml::position clip(0, 0, width, height);


            g_cHome->present(0, 0, &clip);

        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}


void register_homepage_class()
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;   // 关键
    wc.lpfnWndProc = HomepageProc;          // 你的新 WndProc
    wc.hInstance = g_hInst;
    wc.lpszClassName = HOMEPAGE_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;             // 自绘
    wc.cbWndExtra = sizeof(LONG_PTR);   // ← 必须有
    RegisterClassExW(&wc);
}
