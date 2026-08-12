#pragma once

#include <vector>
#include <functional>



#include "WindowBase.h"
#include "Tools.h"
#include "Book.h"

void register_toc_class();



class TocPanel
{
public:
    using OnNavigate = std::function<void(const std::string& href)>;
    struct TreeNode {
        const OCFNavPoint* nav = nullptr;
        std::vector<size_t> childIdx;
        // 仅用于自绘面板
        bool expanded = false;   // 当前是否展开
        int  spineId = -1;      //
    };



    TocPanel();
    ~TocPanel();
    void clear();
    void GetWindow(HWND hwnd);

    void Load(const OCFPackage& pkg);
    void SetOnNavigate(OnNavigate cb) { m_onNavigate = std::move(cb); }
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    size_t getTargetNode(const ScrollPosition& sp);
    void SetHighlight(ScrollPosition sp);
    void copy_to_clipboard();
    std::wstring m_sel_text = L"";
private:
    struct Node : TreeNode {};

    // 消息泵

    LRESULT HandleMsg(UINT, WPARAM, LPARAM);

    // 内部工具
    void RebuildVisible();
    int  HitTest(int y) const;
    void Toggle(int line);
    void EnsureVisible(int line);

    // 绘制
    void OnPaint(HDC);
    void OnVScroll(int code, int pos);
    void OnMouseWheel(int delta);
    void OnLButtonDown(int x, int y);

    float getAnchorOffsetY(const std::string& href);
    void OnMouseMove(int x, int y);
    void OnMouseLeave(int x, int y);
    // 数据
    std::vector<Node>          m_nodes;
    std::vector<size_t>        m_roots;
    std::vector<size_t>        m_visible;      // 可见行索引
    int                        m_lineH = 20;
    int                        m_scrollY = 0;
    int                        m_totalH = 0;
    int                        m_selLine = -1;
    OnNavigate                 m_onNavigate;
    HWND m_hTip = nullptr;

    HFONT m_hFont = nullptr;
    HWND m_hwnd = nullptr;
    int m_marginTop = 4;   // 顶部留白
    int m_marginLeft = 10;  // 左侧留白
    int m_marginBottom = 10;
    HBRUSH   m_hightlightBrush;
    HBRUSH   m_hoverBrush;
    int m_curTarget = 0;
    int m_curHover = -1;

};

