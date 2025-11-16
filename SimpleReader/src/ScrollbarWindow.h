#pragma once
class ScrollBarEx
{
public:
    ScrollBarEx();
    ~ScrollBarEx();
    void GetWindow(HWND hwnd);
    // API
    void SetSpineCount(int n);

    void SetPosition(int spineId, float totalHeightPx, float offsetPx);
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
private:


    void OnPaint();

    void OnLButtonDown(int x, int y);
    void OnMouseLeave(int x, int y);
    void OnMouseMove(int x, int y);
    void OnLButtonUp();

    void OnRButtonUp();

    int m_count = 0;
    ScrollPosition m_pos;

    bool m_dragging = false;
    int  m_dragAnchor = 0;
    int dot_r;      // 普通圆点半径
    int ACTIVE_R = 6;      // 当前圆点半径
    int thumbH = 24;     // 滑块高度
    int LINE_W = 2;      // 竖线宽
    int GUTTER_W = 14;     // 整个滚动条宽

    bool m_mouseIn = false;
    struct ThumbState
    {
        bool hot = false;
        bool drag = false;
        int  dragY = 0;     // 鼠标按下时相对滑块顶部的偏移
    };
    ThumbState m_thumb;
    HWND m_hwnd = nullptr;
    HICON m_hIcon = nullptr;
};

