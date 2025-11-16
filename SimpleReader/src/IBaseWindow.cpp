#include "IBaseWindow.h"


LRESULT CALLBACK IBaseWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    IBaseWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<IBaseWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else {
        self = reinterpret_cast<IBaseWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT IBaseWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        std::string txt = "hello world";
        TextOutA(hdc, 0, 0, txt.c_str(), txt.size());
        EndPaint(m_hwnd, &ps);
    }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}


IBaseWindow::IBaseWindow(HWND parent, HINSTANCE hInst, LPCWSTR className)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!GetClassInfoEx(hInst, className, &wc)) {
        if (!RegisterClassEx(&wc)) {
            MessageBox(nullptr, L"RegisterClassEx failed", L"Error", MB_OK);
        }
    }

    m_hwnd = CreateWindowEx(
        0,
        className,
        className,
        WS_CHILD | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        parent,
        nullptr,
        hInst,
        this
    );

    if (!m_hwnd) {
        MessageBox(nullptr, L"CreateWindowEx failed", L"Error", MB_OK);
    }
}

//HWND IBaseWindow::Create(HWND parent, HINSTANCE hInst, LPCWSTR className) {
//
//
//    WNDCLASSEX wc = { 0 };
//    wc.cbSize = sizeof(WNDCLASSEX);
//    wc.lpfnWndProc = WndProc;
//    wc.hInstance = hInst;
//    wc.lpszClassName = className;
//    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
//    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
//
//    if (!GetClassInfoEx(hInst, className, &wc)) {
//        if (!RegisterClassEx(&wc)) {
//            MessageBox(nullptr, L"RegisterClassEx failed", L"Error", MB_OK);
//            return nullptr;
//        }
//    }
//
//    m_hwnd = CreateWindowEx(
//        0,
//        className,
//        className,
//        WS_CHILD | WS_CLIPCHILDREN,
//        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
//        parent,
//        nullptr,
//        hInst,
//        this
//    );
//
//    if (!m_hwnd) {
//        MessageBox(nullptr, L"CreateWindowEx failed", L"Error", MB_OK);
//    }
//
//    ShowWindow(m_hwnd, SW_SHOW);
//    UpdateWindow(m_hwnd);
//
//    return m_hwnd;
//}
IBaseWindow::~IBaseWindow()
{
    DestroyWindow(m_hwnd);
}

HWND IBaseWindow::GetHandle()
{
    return m_hwnd;
}
