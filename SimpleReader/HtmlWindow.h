#pragma once

#include <string>

#include <Windows.h>
#include <Windowsx.h>

#include "d2dContainer.h"

class HtmlWindow
{
public:
    HtmlWindow();
    ~HtmlWindow();
    void GetWindow(HWND hwnd);
    // API
    void OpenHtml(const std::string& path);
    void SetHtml(const std::string& html);
    void convert_coordinate(POINT& pt);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

private:

    void OnSize();
    void OnMouseWheel(int delta);

    void OnPaint();

    void OnEraseBackground();

    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnLButtonDoubleClick(int x, int y);
 
    void OnMouseMove(int x, int y);
    void OnAnchor();
    void OnNavigate();
    void OnMouseLeave();

    void OnMButtonDown();

private:
    HWND m_hwnd = nullptr;
    std::unique_ptr<d2dContainer>  m_container = nullptr;
    litehtml::document::ptr m_doc = nullptr;

}