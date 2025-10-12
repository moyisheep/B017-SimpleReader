#include "TocWindow.h"

const wchar_t TOC_CLASS[] = L"TocPanelClass";
void register_toc_class()
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = TocPanel::WndProc;          // 你的新 WndProc
    wc.hInstance = g_hInst;
    wc.lpszClassName = TOC_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;             // 自绘
    wc.cbWndExtra = sizeof(LONG_PTR);   // ← 必须有
    RegisterClassExW(&wc);
}





void TocPanel::clear()
{
    m_nodes.clear();
    m_roots.clear();
    m_visible.clear();      // 可见行索引
    m_lineH = 20;
    m_scrollY = 0;
    m_totalH = 0;
    m_selLine = -1;

}

void TocPanel::GetWindow(HWND hwnd)
{
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    m_hwnd = hwnd;
}


void TocPanel::Load(const OCFPackage& pkg)
{
    // 复用你原来的 BuildTree 算法
    m_nodes.clear();
    m_roots.clear();
    m_nodes.reserve(pkg.toc.size());
    std::vector<size_t> st;
    st.push_back(SIZE_MAX);
    for (const auto& np : pkg.toc)
    {
        while (st.size() > static_cast<size_t>(np.order + 1)) st.pop_back();
        size_t idx = m_nodes.size();
        m_nodes.push_back(Node{ &np });
        if (st.back() != SIZE_MAX)
            m_nodes[st.back()].childIdx.push_back(idx);
        else
            m_roots.push_back(idx);
        st.push_back(idx);
    }
    for (auto& n : m_nodes)
    {
        // 1. 分离锚点
        std::wstring href = n.nav->href;
        size_t pos = href.find(L'#');
        std::wstring pure = pos == std::wstring::npos ? href : href.substr(0, pos);
        for (int i = 0; i < pkg.spine.size(); i++)
        {
            if (pkg.spine[i].href == pure)
            {
                n.spineId = i;
                break;
            }
        }
        n.expanded = false;
    }
    RebuildVisible();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

/* ---------- 内部实现 ---------- */
LRESULT CALLBACK TocPanel::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{

    TocPanel* self = (TocPanel*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    return self ? self->HandleMsg(msg, wp, lp) : DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT TocPanel::HandleMsg(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {

    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; OnPaint(BeginPaint(m_hwnd, &ps)); EndPaint(m_hwnd, &ps); } return 0;
    case WM_LBUTTONDOWN: OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSEMOVE: OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSELEAVE: OnMouseLeave(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSEWHEEL:  OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp)); return 0;
    case WM_VSCROLL:     OnVScroll(LOWORD(wp), HIWORD(wp)); return 0;
    case WM_KEYDOWN:
        if (wp == VK_UP && m_selLine > 0) { m_selLine--; EnsureVisible(m_selLine); InvalidateRect(m_hwnd, nullptr, FALSE); }
        if (wp == VK_DOWN && m_selLine + 1 < (int)m_visible.size()) { m_selLine++; EnsureVisible(m_selLine); InvalidateRect(m_hwnd, nullptr, FALSE); }
        return 0;

    }


    return DefWindowProc(m_hwnd, msg, wp, lp);
}

void TocPanel::OnMouseMove(int x, int y)
{
    // 1. 先把鼠标“抓”住
    if (GetCapture() != m_hwnd)
        SetCapture(m_hwnd);

    // 2. 判断坐标
    RECT rc; GetClientRect(m_hwnd, &rc);
    if (!PtInRect(&rc, { x, y }))
    {
        // 真正离开了客户区
        if (GetCapture() == m_hwnd)
            ReleaseCapture();          // 释放捕获
        m_curHover = -1;
        if (m_hTip) { ShowWindow(m_hTip, SW_HIDE); }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    int line = HitTest(y);
    if (line < 0)
    {
        m_curHover = -1;
        if (m_hTip) { ShowWindow(m_hTip, SW_HIDE); }
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return;
    }
    if (m_curHover == line) { return; }
    if (line == m_selLine) { m_curHover = -1; if (m_hTip) { ShowWindow(m_hTip, SW_HIDE); }return; }

    m_curHover = line;
    SetCursor(LoadCursor(nullptr, IDC_HAND));
    std::wstring wtxt = m_nodes[m_visible[line]].nav->label;
    // 2. 判断文字是否被截断
    HDC hdc = GetDC(m_hwnd);
    HFONT old = (HFONT)SelectObject(hdc, m_hFont);   // 你的字体
    SIZE sz{};
    GetTextExtentPoint32W(hdc, wtxt.c_str(), (int)wtxt.size(), &sz);
    int indent = m_nodes[m_visible[line]].nav->order * 16 + m_marginLeft;
    int fullW = sz.cx + indent;

    SelectObject(hdc, old);
    ReleaseDC(m_hwnd, hdc);

    // 3. 可用宽度 = 客户区宽 - 左边缩进
    int clientW = rc.right - rc.left;   // 根据你实际缩进计算

    bool truncated = (fullW > clientW);



    //if (truncated)
    {
        RECT rc; GetWindowRect(m_hwnd, &rc);
        int x = (m_nodes[m_visible[line]].nav->order * 16 + 12) + m_marginLeft + rc.left;
        int y = (m_marginTop + line * m_lineH - m_scrollY) + rc.top;

        SetWindowText(m_hTip, wtxt.c_str());


        //SetWindowLong(m_hTip, GWL_STYLE,
        //    GetWindowLong(m_hTip, GWL_STYLE) | WS_BORDER);

        ShowWindow(m_hTip, SW_SHOWNOACTIVATE);
        MoveWindow(m_hTip, x, y, fullW, m_lineH, true);
        InvalidateRect(m_hTip, NULL, TRUE);
        UpdateWindow(m_hTip);
    }
    //else
    //{
    //    // 文字完整，不需要 tooltip
    //    if (m_hTip) { ShowWindow(m_hTip, SW_HIDE); }
    //}

    InvalidateRect(m_hwnd, nullptr, FALSE);
}
void TocPanel::OnMouseLeave(int x, int y)
{

}
TocPanel::TocPanel()
{
    // 16 px 高，默认宽度，正常粗细，不斜体，不 underline，不 strikeout
    m_hFont = CreateFontW(18, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        g_cfg.default_font_name.c_str());   // 字体名

    // 1. 在 WM_CREATE / 初始化时创建一次

    m_hightlightBrush = CreateSolidBrush(g_cfg.highlight_color_cr);
    m_hoverBrush = CreateSolidBrush(g_cfg.hover_color_cr);
    m_hTip = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"STATIC", L"",
        WS_POPUP | SS_LEFT | SS_NOPREFIX | WS_BORDER,
        0, 0, 0, 0,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowFont(m_hTip, m_hFont, TRUE);
    SetClassLongPtr(m_hTip, GCLP_HBRBACKGROUND, (LONG_PTR)m_hoverBrush);
    ShowWindow(m_hTip, SW_HIDE);

}
TocPanel::~TocPanel()
{
    if (m_hFont) DeleteObject(m_hFont);
}
void TocPanel::RebuildVisible()
{
    m_visible.clear();
    std::function<void(size_t)> walk = [&](size_t idx) {
        m_visible.push_back(idx);
        const Node& n = m_nodes[idx];
        if (n.expanded)
            for (size_t c : n.childIdx) walk(c);
        };
    for (size_t r : m_roots) walk(r);

    // 1. 总高度（像素）
    m_totalH = (int)m_visible.size() * m_lineH + m_marginTop + m_marginBottom;


}

int TocPanel::HitTest(int y) const
{
    int line = (y + m_scrollY) / m_lineH;
    return (line >= 0 && line < (int)m_visible.size()) ? line : -1;
}

void TocPanel::Toggle(int line)
{
    size_t idx = m_visible[line];
    m_nodes[idx].expanded = !m_nodes[idx].expanded;
    RebuildVisible();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TocPanel::EnsureVisible(int line)
{
    RECT rc; GetClientRect(m_hwnd, &rc);
    int y = line * m_lineH;
    if (y < m_scrollY) m_scrollY = y;
    else if (y + m_lineH > m_scrollY + rc.bottom) m_scrollY = y + m_lineH - rc.bottom;
    //SetScrollPos(m_hwnd, SB_VERT, m_scrollY, TRUE);
}

void TocPanel::OnPaint(HDC hdc)
{
    OutputDebugStringA("[TocPanel] WM_PAINT\n");
    RECT rc; GetClientRect(m_hwnd, &rc);
    /* 1. 先把整块客户区刷成背景色，解决残影 */
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
    int first = m_scrollY / m_lineH;
    int last = std::min(first + rc.bottom / m_lineH + 1, (long)m_visible.size());
    HFONT hOld = (HFONT)SelectObject(hdc, m_hFont);


    for (int i = first; i < last; ++i)
    {
        const Node& n = m_nodes[m_visible[i]];

        // 行矩形：整体向下、向右各偏移 marginTop / marginLeft
        RECT r{ m_marginLeft,
                m_marginTop + i * m_lineH - m_scrollY,
                rc.right,
                m_marginTop + (i + 1) * m_lineH - m_scrollY };

        // 高亮当前选择

        HBRUSH   br = (i == m_selLine)
            ? m_hightlightBrush
            : GetSysColorBrush(COLOR_WINDOW);
        FillRect(hdc, &r, br);




        int indent = n.nav->order * 16;
        WCHAR sign[2] = L"";
        if (!n.childIdx.empty())
            sign[0] = n.expanded ? L'−' : L'+';

        SetBkMode(hdc, TRANSPARENT);


        SetTextColor(hdc, GetSysColor(i == m_selLine
            ? COLOR_HIGHLIGHTTEXT
            : COLOR_WINDOWTEXT));
        // 文字再缩进：左侧留白 + 层级缩进
        int textLeft = m_marginLeft + indent;
        TextOutW(hdc, textLeft, r.top + 2, sign, lstrlenW(sign));
        textLeft += 12;
        TextOutW(hdc, textLeft, r.top + 2,
            n.nav->label.c_str(),
            static_cast<int>(n.nav->label.size()));
    }
    SelectObject(hdc, hOld);   // 恢复
}

void TocPanel::OnLButtonDown(int x, int y)
{
    int line = HitTest(y);
    if (line < 0) return;
    ShowWindow(m_hTip, SW_HIDE);
    const Node& n = m_nodes[m_visible[line]];
    m_curTarget = m_visible[line];
    if (n.childIdx.empty())
    {
        m_selLine = line;
        InvalidateRect(m_hwnd, nullptr, false);
        UpdateWindow(m_hwnd);

        if (m_onNavigate) m_onNavigate(n.nav->href);
    }
    else
    {
        m_selLine = line;

        Toggle(line);
    }
}
float TocPanel::getAnchorOffsetY(const std::wstring& href)
{
    if (!g_cMain || !g_cMain->m_doc) { return 0; }
    size_t pos = href.find(L'#');
    std::wstring pure = pos == std::wstring::npos ? href : href.substr(0, pos);
    std::wstring anchor = pos == std::wstring::npos ? L"" : href.substr(pos + 1);

    if (!anchor.empty()) {
        std::string cssSel = "[id=\"" + w2a(anchor) + "\"]";
        if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
            return el->get_placement().y;
        }
    }
    return 0;
}
size_t TocPanel::getTargetNode(const ScrollPosition& sp)
{
    size_t target = m_nodes.size();
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].nav && m_nodes[i].spineId == sp.spine_id)
        {
            target = i; break;
        }
    }
    for (size_t i = target + 1; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].nav && m_nodes[i].spineId == m_nodes[target].spineId)
        {

            std::wstring href = m_nodes[i].nav->href;
            int offsetY = getAnchorOffsetY(href);
            //OutputDebugStringA("[offsetY] ");
            //OutputDebugStringA(std::to_string(offsetY).c_str());
            //OutputDebugStringA("\n");
            if (sp.offset < offsetY) { break; }
            target = i;
        }
        else { break; }
    }
    return target;
}
void TocPanel::SetHighlight(ScrollPosition sp)
{
    // ---------- 1. 找目标节点 ----------
    size_t target = getTargetNode(sp);
    if (target == m_nodes.size() || target == m_curTarget) return;

    m_curTarget = target;
    //for (auto& n : m_nodes) n.expanded = false;
    // ---------- 2. 记录路径并展开 ----------
    // path 只需存需要展开的节点，最多树高
    std::vector<size_t> path;
    std::function<bool(size_t)> dfs = [&](size_t idx) -> bool
        {
            if (idx == target) return true;          // 命中目标

            Node& n = m_nodes[idx];
            //if (!n.expanded)                       // 折叠就展开
            //    n.expanded = true;

            for (size_t c : n.childIdx)
                if (dfs(c))
                {
                    path.push_back(idx);           // 回溯时记录父节点
                    return true;
                }
            return false;
        };

    for (size_t r : m_roots)                    // 支持多根
        if (dfs(r)) break;
    for (size_t idx : path) m_nodes[idx].expanded = true;
    // 3. 重建可见表（O(N) 一次遍历）
    RebuildVisible();

    // 4. 直接取行号
    m_selLine = -1;
    for (size_t i = 0; i < m_visible.size(); ++i)
        if (m_visible[i] == target) { m_selLine = static_cast<int>(i); break; }

    if (m_selLine != -1)
        EnsureVisible(m_selLine);
    // 5. 重绘
    InvalidateRect(m_hwnd, nullptr, FALSE);
}
void TocPanel::OnVScroll(int code, int pos)
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int clientH = rc.bottom - rc.top;
    int maxY = std::max(0, m_totalH - clientH);

    switch (code)
    {
    case SB_LINEUP:      m_scrollY -= m_lineH; break;
    case SB_LINEDOWN:    m_scrollY += m_lineH; break;
    case SB_PAGEUP:      m_scrollY -= clientH; break;   // 按页滚 = 客户区高度
    case SB_PAGEDOWN:    m_scrollY += clientH; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
    {
        SCROLLINFO si{ sizeof(si), SIF_TRACKPOS };
        if (GetScrollInfo(m_hwnd, SB_VERT, &si))
            m_scrollY = si.nTrackPos;      // 拿到 32 位真实位置
        break;
    }
    }

    m_scrollY = std::max(0, std::min(m_scrollY, maxY));

    //SetScrollPos(m_hwnd, SB_VERT, m_scrollY, TRUE);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}
void TocPanel::OnMouseWheel(int delta)
{
    ShowWindow(m_hTip, SW_HIDE);
    // 每 120 单位滚一行；可根据需要改成多行或整页
    int lines = delta / WHEEL_DELTA;          // WHEEL_DELTA = 120
    for (int i = 0; i < abs(lines); ++i)
    {
        OnVScroll(lines > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
    }
}

