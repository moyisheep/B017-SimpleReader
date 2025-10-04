#include "SimpleContainer.h"



static inline std::string trim_any(const std::string& s,
    const char* ws = " \t\"'")
{
    if (s.empty()) return s;
    size_t first = s.find_first_not_of(ws);
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(ws);
    return s.substr(first, last - first + 1);
}

static ImgFmt detect_fmt(const uint8_t* d, size_t n, const wchar_t* ext)
{

    if (n >= 4 && memcmp(d, "\x89PNG", 4) == 0) return ImgFmt::PNG;
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xD8)   return ImgFmt::JPEG;
    if (n >= 2 && d[0] == 'B' && d[1] == 'M')      return ImgFmt::BMP;
    if (n >= 6 && memcmp(d, "GIF87a", 6) == 0)    return ImgFmt::GIF;
    if (n >= 6 && memcmp(d, "GIF89a", 6) == 0)    return ImgFmt::GIF;
    if (n >= 4 && memcmp(d, "MM\x00*", 4) == 0)   return ImgFmt::TIFF;
    if (n >= 4 && memcmp(d, "II*\x00", 4) == 0)   return ImgFmt::TIFF;
    if (ext && _wcsicmp(ext, L"svg") == 0)       return ImgFmt::SVG;

    return ImgFmt::UNKNOWN;
}


static ImageFrame decode_img(const MemFile& mf, const wchar_t* ext)
{
    ImageFrame frame;
    auto fmt = detect_fmt(mf.data.data(), mf.data.size(), ext);

    switch (fmt)
    {
    case ImgFmt::SVG:
    {
        auto doc = lunasvg::Document::loadFromData(
            reinterpret_cast<const char*>(mf.data.data()), mf.data.size());
        if (!doc) return {};

        lunasvg::Bitmap svgBmp = doc->renderToBitmap();
        if (svgBmp.isNull()) return {};

        frame.width = svgBmp.width();
        frame.height = svgBmp.height();
        frame.stride = frame.width * 4;
        frame.rgba.assign(
            reinterpret_cast<const uint8_t*>(svgBmp.data()),
            reinterpret_cast<const uint8_t*>(svgBmp.data()) + frame.stride * frame.height);
        break;
    }

    default:
    {
        /* ---------- PNG/JPEG/BMP/... ---------- */
        int w, h, comp;
        stbi_uc* pixels = stbi_load_from_memory(
            mf.data.data(), static_cast<int>(mf.data.size()),
            &w, &h, &comp, 4);                 // 强制 4 通道 RGBA
        if (!pixels) return {};

        frame.width = w;
        frame.height = h;
        frame.stride = w * 4;
        frame.rgba.assign(pixels, pixels + frame.stride * h);
        for (size_t i = 0; i < frame.rgba.size(); i += 4)
            std::swap(frame.rgba[i], frame.rgba[i + 2]);   // BGRA → RGBA
        stbi_image_free(pixels);
    }
    }
    return frame;
}


int64_t SimpleContainer::hit_test(float x, float y)
{

    for (const auto& line : m_lines)
        for (const auto& cb : line)
            if (x >= cb.rect.left && x <= cb.rect.right &&
                y >= cb.rect.top && y <= cb.rect.bottom)
                return cb.offset;

    return -1;
}

void SimpleContainer::on_lbutton_down(int x, int y)
{
    m_selecting = true;

    m_selStart = m_selEnd = -1;

}


void SimpleContainer::on_mouse_move(int x, int y)
{
    if (m_selecting)
    {
        m_currentCursor = IDC_IBEAM;
        SetCursor(LoadCursor(nullptr, m_currentCursor));

        auto result = hit_test((float)x, (float)y);
        if (result >= 0)
        {
            if (m_selStart < 0) { m_selStart = result; }
            m_selEnd = result;
            InvalidateRect(m_hwnd, nullptr, false);
        }

    }
}

void SimpleContainer::on_lbutton_up()
{

    m_selecting = false;

    m_currentCursor = IDC_ARROW;
    SetCursor(LoadCursor(nullptr, m_currentCursor));
    InvalidateRect(m_hwnd, nullptr, false);
}
void SimpleContainer::copy_to_clipboard()
{
    //if (m_selStart == m_selEnd) return;

    //// 确保选区不越界
    //size_t start = std::min(m_selStart, m_selEnd);
    //size_t end = std::max(m_selStart, m_selEnd);
    //end = std::min(end, m_plainText.size());
    //if (start >= end) return;

    //size_t len = end - start;

    //HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
    //if (!hMem) return;                       // 内存不足
    //wchar_t* dst = (wchar_t*)GlobalLock(hMem);
    //if (!dst) { GlobalFree(hMem); return; }  // 锁失败

    //memcpy(dst, m_plainText.c_str() + start, len * sizeof(wchar_t));
    //dst[len] = L'\0';

    //GlobalUnlock(hMem);

    //if (OpenClipboard(g_hWnd))
    //{
    //    EmptyClipboard();
    //    SetClipboardData(CF_UNICODETEXT, hMem);
    //    CloseClipboard();
    //}
    //else
    //{
    //    GlobalFree(hMem);
    //}
}

std::vector<RECT> SimpleContainer::get_selection_rows() const
{
    std::vector<RECT> rows;
    if (m_selStart == m_selEnd) return rows;

    const auto start = std::min(m_selStart, m_selEnd);
    const auto end = std::max(m_selStart, m_selEnd);

    /* 1. 先按原逻辑收集每一“词”的矩形 */
    for (const auto& line : m_lines)
    {
        if (line.empty()) continue;

        const size_t lineFirst = line.front().offset;
        const size_t lineLast = line.back().offset;
        if (lineLast < start || lineFirst >= end) continue;

        size_t idx0 = 0;
        while (idx0 < line.size() && line[idx0].offset < start) ++idx0;

        size_t idx1 = line.size() - 1;
        while (idx1 != static_cast<size_t>(-1) && line[idx1].offset >= end) --idx1;

        if (idx0 > idx1) continue;

        const D2D1_RECT_F& r0 = line[idx0].rect;
        const D2D1_RECT_F& r1 = line[idx1].rect;

        RECT row;
        row.left = static_cast<LONG>(r0.left);
        row.top = static_cast<LONG>(r0.top);
        row.right = static_cast<LONG>(r1.right);
        row.bottom = static_cast<LONG>(std::max(r0.bottom, r1.bottom));
        rows.push_back(row);
    }

    /* 2. 把同一水平行的矩形横向合并（最小改动） */
    if (rows.empty()) return rows;

    std::vector<RECT> merged;
    RECT cur = rows.front();

    for (size_t i = 1; i < rows.size(); ++i)
    {
        const RECT& r = rows[i];
        // 同一行：top 差值 ≤ 1 像素
        if (std::abs(r.top - cur.top) <= 1)
        {
            cur.left = std::min(cur.left, r.left);
            cur.right = std::max(cur.right, r.right);
            cur.bottom = std::max(cur.bottom, r.bottom);
        }
        else
        {
            merged.push_back(cur);
            cur = r;
        }
    }
    merged.push_back(cur);
    return merged;
}

void SimpleContainer::BeginDraw()
{
    m_dc->BeginDraw();
    m_dc->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 保存原始矩阵
    m_dc->GetTransform(&m_oldMatrix);

    // 缩放
    m_dc->SetTransform(
        D2D1::Matrix3x2F::Scale(
            m_zoom_factor,
            m_zoom_factor,
            D2D1::Point2F(0.0f, 0.0f)));
}
void SimpleContainer::EndDraw()
{
    m_dc->SetTransform(m_oldMatrix);

    HRESULT hr = m_dc->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        // 设备丢失，重建
        return;
    }

    // 呈现
    m_swapChain->Present(1, 0);
}
void SimpleContainer::present(float x, float y, litehtml::position* clip)
{
    m_lines.clear();
    m_plainText.clear();

    m_dc->BeginDraw();
    m_dc->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // 保存原始矩阵
    m_dc->GetTransform(&m_oldMatrix);

    // 缩放
    m_dc->SetTransform(
        D2D1::Matrix3x2F::Scale(
            m_zoom_factor,
            m_zoom_factor,
            D2D1::Point2F(0.0f, 0.0f)));


    // 绘制 html
    m_doc->draw(getContext(), x, y, clip);

    // 高亮选中行
    //if (!m_selBrush)
    //{
    //    m_dc->CreateSolidColorBrush(
    //        g_cfg.highlight_color_d2d,
    //        &m_selBrush);
    //}
    //if (m_selStart != m_selEnd && m_selBrush && m_selStart >= 0 && m_selEnd >= 0)
    //{
    //    for (const auto& row : get_selection_rows())
    //    {
    //        D2D1_RECT_F r = D2D1::RectF(
    //            row.left, row.top, row.right, row.bottom);
    //        m_dc->FillRectangle(r, m_selBrush.Get());
    //    }
    //}

    // 恢复矩阵
    m_dc->SetTransform(m_oldMatrix);

    HRESULT hr = m_dc->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        // 设备丢失，重建
        return;
    }

    // 呈现
    m_swapChain->Present(1, 0);
}


//void SimpleContainer::present(float x, float y, litehtml::position* clip)
//{
//
//    m_lines.clear();
//    m_plainText.clear();
//
//    m_rt->BeginDraw();
//    m_rt->Clear(D2D1::ColorF(D2D1::ColorF::White));
//
//    // 1. 保存原始矩阵
//    m_rt->GetTransform(&m_oldMatrix);
//
//    // 3. 以鼠标位置为中心整体缩放
//    m_rt->SetTransform(
//        D2D1::Matrix3x2F::Scale(
//            m_zoom_factor,
//            m_zoom_factor,
//            D2D1::Point2F(static_cast<float>(0),
//                static_cast<float>(0))));
//    m_doc->draw(getContext(),   // 强制转换
//        x, y, clip);
//
//
//    // 高亮选中行
//    if (!m_selBrush)
//    {
//        m_rt->CreateSolidColorBrush(
//            g_cfg.highlight_color_d2d,   // 半透明蓝
//            &m_selBrush);
//    }
//    if (m_selStart != m_selEnd && m_selBrush && m_selStart >= 0 && m_selEnd >= 0)
//    {
//        m_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
//        for (const auto& row : get_selection_rows())
//        {
//            D2D1_RECT_F r = D2D1::RectF(
//                row.left ,
//                row.top ,
//                row.right ,
//                row.bottom );
//            m_rt->FillRectangle(r, m_selBrush.Get());
//        }
//    }
//    // 恢复原始矩阵
//    m_rt->SetTransform(m_oldMatrix);
//    m_rt->EndDraw();
//
//}


// 判断是否为单词边界（空格、标点、换行）
static bool is_word_boundary(wchar_t ch)
{
    return iswspace(ch) || iswpunct(ch) || ch == L'\r' || ch == L'\n';
}


void SimpleContainer::clear_selection()
{
    m_selStart = m_selEnd = -1;
    m_selecting = false;
}


void SimpleContainer::on_lbutton_dblclk(int x, int y)
{
    if (m_plainText.empty() || m_lines.empty()) return;

    /* 1. 字符偏移 */
    size_t clickPos = hit_test(x, y);
    if (clickPos == size_t(-1) || clickPos >= m_plainText.size())
        return;

    /* 2. 在 m_lines 里找到当前行 */
    size_t lineStart = 0, lineEnd = 0;
    for (const auto& line : m_lines)
    {
        if (line.empty()) continue;
        lineStart = line.front().offset;
        lineEnd = line.back().offset + 1;   // [start , end)
        if (clickPos >= lineStart && clickPos < lineEnd)
            break;
    }
    if (lineEnd <= lineStart) return;   // 没找到行

    /* 3. 自定义“选词”：空格/制表/换行 + ASCII 标点视为分隔符 */
    auto isDelimiter = [](unsigned char c) -> bool
        {
            return std::isspace(c) || std::ispunct(c);
        };

    /* 3.1 找词起始 */
    size_t wordStart = clickPos;
    while (wordStart > lineStart && !isDelimiter(m_plainText[wordStart - 1]))
        --wordStart;

    /* 3.2 找词结束 */
    size_t wordEnd = clickPos;
    while (wordEnd < lineEnd && !isDelimiter(m_plainText[wordEnd]))
        ++wordEnd;

    if (wordStart >= wordEnd) return;

    /* 4. 裁剪首尾空格/标点（可选，与 ICU 版本保持一致） */
    while (wordStart < wordEnd && isDelimiter(m_plainText[wordStart]))
        ++wordStart;
    while (wordEnd > wordStart && isDelimiter(m_plainText[wordEnd - 1]))
        --wordEnd;

    if (wordStart >= wordEnd) return;

    /* 5. 更新选区 */
    m_selStart = wordStart;
    m_selEnd = wordEnd;
    //UpdateCache();
}
//void SimpleContainer::on_lbutton_dblclk(int x, int y)
//{
//    if (m_plainText.empty() || m_lines.empty()) return;
//    /* 1. 字符偏移 */
//    size_t clickPos = hit_test(x, y);
//    if (clickPos == size_t(-1) || clickPos >= m_plainText.size())
//        return;
//
//    /* 2. 在 m_lines 里找到当前行 */
//    size_t lineStart = 0, lineEnd = 0;
//    for (const auto& line : m_lines)
//    {
//        if (line.empty()) continue;
//        lineStart = line.front().offset;
//        lineEnd = line.back().offset + 1;   // [start , end)
//        if (clickPos >= lineStart && clickPos < lineEnd)
//            break;
//    }
//    if (lineEnd <= lineStart) return;   // 没找到行
//
//    /* 3. 在这一行里用 ICU 选词 */
//    icu::UnicodeString us(m_plainText.data(), m_plainText.size());
//    const UChar* buf = us.getBuffer();
//
//    UErrorCode err = U_ZERO_ERROR;
//    UBreakIterator* wordBI = ubrk_open(
//        UBRK_WORD, nullptr,
//        buf + lineStart,
//        static_cast<int32_t>(lineEnd - lineStart),
//        &err);
//    if (U_FAILURE(err)) return;
//
//    int32_t relPos = static_cast<int32_t>(clickPos - lineStart);
//
//    int32_t wordStartRel = ubrk_preceding(wordBI, relPos);
//    if (wordStartRel == UBRK_DONE) wordStartRel = 0;
//
//    int32_t wordEndRel = ubrk_following(wordBI, relPos);
//    if (wordEndRel == UBRK_DONE) wordEndRel = lineEnd - lineStart;
//
//    ubrk_close(wordBI);
//
//    int32_t wordStart = lineStart + wordStartRel;
//    int32_t wordEnd = lineStart + wordEndRel;
//
//    /* 4. 裁剪首尾空格/标点 */
//    auto isVisible = [](UChar32 c) {
//        return !u_isspace(c) && (u_isalnum(c) || c == 0x2019);
//        };
//
//    while (wordStart < wordEnd) {
//        UChar32 c; int32_t idx = wordStart;
//        U16_NEXT(buf, idx, wordEnd, c);
//        if (isVisible(c)) break;
//        wordStart = idx;
//    }
//    while (wordEnd > wordStart) {
//        UChar32 c; int32_t idx = wordEnd;
//        U16_PREV(buf, wordStart, idx, c);
//        if (isVisible(c)) { wordEnd = idx + U16_LENGTH(c); break; }
//        wordEnd = idx;
//    }
//
//    if (wordStart >= wordEnd) return;
//
//    /* 5. 更新选区 */
//    m_selStart = static_cast<size_t>(wordStart);
//    m_selEnd = static_cast<size_t>(wordEnd);
//    UpdateCache();
//}
bool SimpleContainer::isImageCached(std::string src)
{
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    if (m_img_cache.contains(src)) return true;
    return false;
}

void SimpleContainer::addImageCache(std::string hash, std::string svg)
{
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    auto doc = lunasvg::Document::loadFromData(svg);
    if (!doc) return;

    lunasvg::Bitmap svgBmp = doc->renderToBitmap();
    if (svgBmp.isNull()) return;

    //svgBmp.convertToRGBA();   // 1. 原地转格式
    ImageFrame frame;
    frame.width = svgBmp.width();
    frame.height = svgBmp.height();
    frame.stride = frame.width * 4;
    frame.rgba.assign(
        reinterpret_cast<const uint8_t*>(svgBmp.data()),
        reinterpret_cast<const uint8_t*>(svgBmp.data()) + frame.stride * frame.height);

    m_img_cache.emplace(hash, std::move(frame));
}


void SimpleContainer::load_image(const char* src, const char* baseurl, bool redraw_on_ready)
{
    OutputDebugStringA("load_image\n");
    //if (!src) { return; }
    //std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    //if (m_img_cache.contains(src)) return;

    //if (g_book)
    //{
    //    auto wpath = a2w(src);
    //    MemFile mf = g_book->get_binary(g_book->get_current_dir(), wpath);
    //    if (mf.data.empty())
    //    {
    //        OutputDebugStringW((L"EPUB not found: " + wpath + L"\n").c_str());
    //        return;
    //    }


    //    if (fs::path(wpath).extension().generic_string() == ".svg")
    //    {
    //        auto frame = decode_img(mf, L"svg");
    //        if (!frame.rgba.empty())
    //        {
    //            frame.raw_data = std::move(mf.data);
    //            m_img_cache.emplace(src, std::move(frame));
    //        }
    //        return;
    //    }
    //    ImageFrame frame{};

    //    int w, h, comp;
    //    if (stbi_info_from_memory(mf.data.data(), static_cast<int>(mf.data.size()),
    //        &w, &h, &comp)) {
    //        frame.width = w;
    //        frame.height = h;
    //        frame.raw_data = std::move(mf.data);
    //    }
    //    if (!frame.raw_data.empty())
    //    {
    //        m_img_cache.emplace(src, std::move(frame));
    //    }
    //    else
    //    {
    //        OutputDebugStringA(("EPUB decode failed: " + std::string(src) + "\n").c_str());
    //    }
    //    return;
    //}

}





void SimpleContainer::get_image_size(const char* src, const char* baseurl, litehtml::size& sz) {
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    if (!m_img_cache.contains(src)) { sz.width = sz.height = 0; return; }
    auto img = m_img_cache[src];
    sz.width = img.width;
    sz.height = img.height;

}

// get_client_rect -> get_viewport
void SimpleContainer::get_viewport(litehtml::position& client) const
{
    OutputDebugStringA("get_viewport\n");
    // 1. 取客户区物理像素
    //RECT rc{};
    //GetClientRect(m_hwnd, &rc);


    //// 4. 逻辑像素
    //int width = static_cast<int>(g_cfg.document_width * m_zoom_factor);
    //int height = static_cast<int>((rc.bottom - rc.top) / m_zoom_factor);

    //client = litehtml::position(0, 0, width, height);
}



litehtml::element::ptr
SimpleContainer::create_element(const char* tag,
    const litehtml::string_map& attrs,
    const std::shared_ptr<litehtml::document>& doc)
{

    return nullptr;   // 让 litehtml 自己建别的节点

}



void SimpleContainer::import_css(litehtml::string& text,
    const litehtml::string& url,
    litehtml::string& baseurl)
{
    OutputDebugStringA("import_css\n");
    //if (!g_cfg.enableCSS) {
    //    //text.clear();           // 禁用所有外部/内部 CSS
    //    return;
    //}
    //if (auto it = m_css_cache.find(url); it != m_css_cache.end())
    //{
    //    text = it->second;
    //    return;
    //}
    //if (baseurl.empty())
    //{
    //    std::wstring wdir = g_book->get_current_dir();
    //    baseurl = wdir.empty() ? "" : w2a(wdir);
    //}
    //if (g_book)
    //{
    //    auto mf = g_book->get_binary(a2w(baseurl), a2w(url));
    //    if (!mf.data.empty())
    //    {
    //        // 直接填到 text（litehtml 期望 UTF-8）
    //        auto css = std::string(mf.data.begin(), mf.data.end());
    //        m_css_cache.emplace(url, css);
    //        text = css;
    //    }
    //    else
    //    {
    //        OutputDebugStringW((L"CSS not found: " + a2w(url) + L"\n").c_str());
    //    }
    //}


    //std::wstring wdir = g_book->get_current_dir();
    //std::wstring wpath = wdir.empty() ? L"" : g_book->resolve_path(wdir, a2w(url));

    //baseurl = fs::path(wpath).parent_path().generic_string();

    // baseurl 保持原样即可
}





// ---------- 2. 标题 ----------------------------------------------------
void SimpleContainer::set_caption(const char* cap)
{
    OutputDebugStringA("set_caption\n");
    return;
}

// ---------- 3. base url -------------------------------------------------
void SimpleContainer::set_base_url(const char* base)
{
    OutputDebugStringA("set_base_url\n");
    return;
}

// ---------- 4. 链接注册 --------------------------------------------------
void SimpleContainer::link(const std::shared_ptr<litehtml::document>& doc,
    const litehtml::element::ptr& el)
{
    OutputDebugStringA(el->get_tagName());
    OutputDebugStringA("\n");
    // 简单做法：把锚点 id -> 元素 存起来，点击时滚动
    const char* id = el->get_attr("id");
    if (id && *id)
        m_anchor_map[id] = el;
}

// ---------- 5. 点击锚点 -------------------------------------------------
void SimpleContainer::on_anchor_click(const char* url,
    const litehtml::element::ptr& el)
{
    //if (!url || !*url) return;

    //std::string_view sv{ url };
    //if (sv.starts_with('#'))
    //{
    //    /* 锚点 */
    //    std::wstring cssSel = a2w(url + 1);   // 去掉开头的 '#'  

    //    PostMessageW(g_hView, WM_EPUB_ANCHOR,
    //        reinterpret_cast<WPARAM>(_wcsdup(cssSel.c_str())), 0);
    //}
    //else if (sv.starts_with("http") || sv.starts_with("mailto:")) { /* 外部 */ }
    //else
    //{
    //    /* 章节跳转 */
    //    std::wstring href = g_book->resolve_path(g_book->get_current_dir(), a2w(url));
    //    wchar_t* url_copy = _wcsdup(href.c_str());
    //    PostMessageW(g_hView, WM_EPUB_NAVIGATE,
    //        reinterpret_cast<WPARAM>(url_copy), 0);
    //}

}

bool SimpleContainer::on_element_click(const litehtml::element::ptr& el)
{
    OutputDebugStringA(el->get_tagName());
    OutputDebugStringA("\n");
    //el->set_pseudo_class(litehtml::_hover_, true);
    //if (std::strcmp(el->get_tagName(), "img") == 0 && g_cfg.enableClickPreview && !g_book->find_link_in_chain(el))
    //{

    //    g_book->show_imageview(el);
    //}
    return true;
}
void SimpleContainer::on_mouse_event(const litehtml::element::ptr& el,
    litehtml::mouse_event event)
{
    if (event == litehtml::mouse_event::mouse_event_enter)
    {
        OutputDebugStringA("On Mouse Move\n");
    }
    else { OutputDebugStringA("On Mouse Leave\n"); }
    //if (!g_cfg.enableHoverPreview) return;

    //if (event == litehtml::mouse_event::mouse_event_enter)
    //{

    //    if (!el) return;
    //    auto link = g_book->find_link_in_chain(el);

    //    std::string html;
    //    if (!link) { return; }
    //    const char* href_raw = link->get_attr("href");
    //    if (!href_raw) { return; }
    //    std::string id = g_book->extract_anchor(href_raw);
    //    if (id.empty()) { return; }
    //    html = g_book->html_of_anchor_paragraph(g_cMain->m_doc.get(), id);

    //    if (g_book) { g_book->delayed_show_tooltip(std::move(html), g_cfg.tooltip_width, g_cfg.tooltip_delay_ms); }
    //}
    //else
    //{
    //    if (g_book) { g_book->cancel_delayed_tooltip(); }   // 先杀旧计时器
    //}
}




//// 内部工具：UTF-8 → UTF-16
//static std::vector<UChar> utf8_to_utf16(const char* src)
//{
//    std::vector<UChar> buf;
//    if (!src || !*src) return buf;
//    int32_t len8 = static_cast<int32_t>(std::strlen(src));
//    UErrorCode status = U_ZERO_ERROR;
//    int32_t len16 = 0;
//    u_strFromUTF8(nullptr, 0, &len16, src, len8, &status);
//    buf.resize(len16 + 1);
//    status = U_ZERO_ERROR;
//    u_strFromUTF8(buf.data(), len16 + 1, nullptr, src, len8, &status);
//    if (U_FAILURE(status)) buf.clear();
//    else buf.resize(len16);
//    return buf;
//}
//
//// 内部工具：UTF-16 → 堆上 UTF-8（以 '\0' 结尾，可直接传给 on_word/on_space）
//static char* utf16_to_heap_utf8(const UChar* src, int32_t len)
//{
//    if (!src || len <= 0) return nullptr;
//    UErrorCode status = U_ZERO_ERROR;
//    int32_t len8 = 0;
//    u_strToUTF8(nullptr, 0, &len8, src, len, &status);
//    char* out = new char[len8 + 1];
//    status = U_ZERO_ERROR;
//    u_strToUTF8(out, len8 + 1, nullptr, src, len, &status);
//    if (U_FAILURE(status)) { delete[] out; return nullptr; }
//    return out;
//}


//void SimpleContainer::split_text(
//    const char* text,
//    const std::function<void(const char*)>& on_word,
//    const std::function<void(const char*)>& on_space)
//{
//    if (!text || !*text) return;
//
//    /* 1. UTF-8 → UTF-16 */
//    auto u16 = utf8_to_utf16(text);
//    if (u16.empty()) return;
//
//    /* 2. 创建 ICU 单词边界迭代器（系统默认 locale，支持多语言） */
//    UErrorCode status = U_ZERO_ERROR;
//    UBreakIterator* brk = ubrk_open(
//        UBRK_LINE, nullptr,
//        u16.data(), static_cast<int32_t>(u16.size()),
//        &status);
//    if (U_FAILURE(status)) return;
//
//    /* 3. 遍历所有边界区间 */
//    std::vector<std::u16string> tokens;
//    int32_t prev = ubrk_first(brk);
//    for (int32_t curr = ubrk_next(brk);
//        curr != UBRK_DONE;
//        prev = curr, curr = ubrk_next(brk))
//    {
//        /* 3.1 判断区间是否全为空格 */
//        bool all_space = true;
//        UChar32 cp;
//        int32_t idx = prev;
//        while (idx < curr) {
//            U16_NEXT(u16.data(), idx, curr, cp);
//            if (!u_isspace(cp)) { all_space = false; break; }
//        }
//
//        /* 3.2 转回 UTF-8 并回调 */
//        char* out = utf16_to_heap_utf8(u16.data() + prev, curr - prev);
//        if (!out) continue;
//
//        if (all_space && on_space)      on_space(out);
//        else if (!all_space && on_word) on_word(out);
//
//        delete[] out;   // 回调后立即释放
//    }
//
//    ubrk_close(brk);
//}

//void SimpleContainer::split_text(const char* text,
//    const std::function<void(const char*)>& on_word,
//    const std::function<void(const char*)>& on_space)
//{
//    if (!text || !*text) return;
//
//    // UTF-8 → ICU UnicodeString
//    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text);
//
//    // 创建行断行迭代器（UAX #14）
//    UErrorCode status = U_ZERO_ERROR;
//    std::unique_ptr<icu::BreakIterator> brk(
//        icu::BreakIterator::createLineInstance(icu::Locale::getDefault(), status));
//    if (U_FAILURE(status)) return;
//
//    brk->setText(ustr);
//
//    int32_t prev = brk->first();
//    for (int32_t curr = brk->next(); curr != icu::BreakIterator::DONE;
//        prev = curr, curr = brk->next())
//    {
//        icu::UnicodeString seg(ustr, prev, curr - prev);
//
//        // 判断这一段是不是纯空格
//        bool all_space = true;
//        for (int32_t i = 0; i < seg.length(); ++i) {
//            if (!u_isspace(seg.char32At(i))) { all_space = false; break; }
//        }
//
//        std::string out;
//        seg.toUTF8String(out);
//
//        if (all_space) {
//            if (on_space) on_space(out.c_str());
//        }
//        else {
//            if (on_word) on_word(out.c_str());
//        }
//    }
//}
//✅ 使用
// ---------- 6. 鼠标形状 -------------------------------------------------
// 预置常用系统光标
static const std::unordered_map<std::string, LPCWSTR> kSysCursors = {
    {"default",  IDC_ARROW},
    {"pointer",  IDC_HAND},
    {"text",     IDC_IBEAM},
    {"wait",     IDC_WAIT},
    {"crosshair",IDC_CROSS},
    {"move",     IDC_SIZEALL},
    {"e-resize", IDC_SIZEWE},
    {"n-resize", IDC_SIZENS},
    {"w-resize", IDC_SIZEWE},
    {"s-resize", IDC_SIZENS},
    {"ne-resize",IDC_SIZENESW},
    {"nw-resize",IDC_SIZENWSE},
    {"se-resize",IDC_SIZENWSE},
    {"sw-resize",IDC_SIZENESW},
};
void SimpleContainer::set_cursor(const char* cursor)
{
    //OutputDebugStringA("set_cursor\n");
    //m_currentCursor = IDC_ARROW;           // 默认箭头

    if (!cursor) return;

    // 1. 系统内置光标
    auto it = kSysCursors.find(cursor);
    if (it != kSysCursors.end())
    {
        m_currentCursor = it->second;
        return;
    }



    // 3. 兜底：箭头
    m_currentCursor = IDC_ARROW;
}

// ---------- 7. 文本转换 ----------------------------------------------
void SimpleContainer::transform_text(litehtml::string& text,
    litehtml::text_transform tt)
{
    OutputDebugStringA("transform_text\n");
    if (text.empty()) return;
    std::wstring w = a2w(text.c_str());
    switch (tt)
    {
    case litehtml::text_transform_capitalize:
        if (!w.empty()) w[0] = towupper(w[0]);
        for (size_t i = 1; i < w.size(); ++i)
            if (iswspace(w[i - 1])) w[i] = towupper(w[i]);
        break;
    case litehtml::text_transform_uppercase:
        CharUpperBuffW(w.data(), (DWORD)w.size());
        break;
    case litehtml::text_transform_lowercase:
        CharLowerBuffW(w.data(), (DWORD)w.size());
        break;
    default: break;
    }
    text = w2a(w);
}

// ---------- 8. 裁剪 ----------------------------------------------------



// ---------- 9. 媒体查询 -----------------------------------------------
void SimpleContainer::get_media_features(litehtml::media_features& mf) const
{
    // 1. 窗口客户区（物理像素）
    RECT rc;
    GetClientRect(m_hwnd, &rc);


    // 4. 逻辑像素
    int width = static_cast<int>((rc.right - rc.left) * m_zoom_factor);
    int height = static_cast<int>((rc.bottom - rc.top) / m_zoom_factor);
    mf.width = MulDiv(width, GetDpiForWindow(m_hwnd), 96);
    mf.height = MulDiv(height, GetDpiForWindow(m_hwnd), 96);

    // 2. 屏幕物理分辨率
    const UINT dpiX = GetDpiForWindow(m_hwnd);   // 也可用 GetDpiForSystem
    mf.resolution = dpiX;
    mf.device_width = MulDiv(GetSystemMetricsForDpi(SM_CXSCREEN, dpiX), dpiX, 96);
    mf.device_height = MulDiv(GetSystemMetricsForDpi(SM_CYSCREEN, dpiX), dpiX, 96);

    // 3. 颜色深度（24 位）
    HDC hdc = GetDC(nullptr);
    mf.color = GetDeviceCaps(hdc, BITSPIXEL);   // 通常 24 或 32
    mf.monochrome = 0;

    ReleaseDC(nullptr, hdc);
    mf.type = litehtml::media_type_screen;
}

// ---------- 10. 语言 ---------------------------------------------------
void SimpleContainer::get_language(litehtml::string& language,
    litehtml::string& culture) const
{
    OutputDebugStringA("get_language\n");
    language = "en";
    culture = "US";
    // 真正 EPUB 可从 OPF <dc:language> 读
}


void SimpleContainer::init_dpi() {
    float dpiX, dpiY;
    m_d2dFactory->GetDesktopDpi(&dpiX, &dpiY);  // 使用D2D的DPI
    m_px_per_pt = dpiY / 72.0f;  // 使用Y轴DPI（通常与LOGPIXELSY一致）
}

// 保持 pt_to_px 不变
litehtml::pixel_t SimpleContainer::pt_to_px(float pt) const {
    // 乘法 + 位移，比 MulDiv 更快
    OutputDebugStringA("pt_to_px\n");
    return pt * m_px_per_pt;
}



std::wstring SimpleContainer::normalize_quotes(const std::wstring& src)
{
    std::wstring out;
    out.reserve(src.size());
    for (wchar_t ch : src)
    {
        switch (ch)
        {
        case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0xFF07:
            out.push_back(L'\''); break;
        case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0xFF02:
            out.push_back(L'\"'); break;
        default:
            out.push_back(ch);
        }
    }
    return out;
}

// ---------- 实现 ----------
ComPtr<ID2D1SolidColorBrush> SimpleContainer::getBrush(litehtml::uint_ptr hdc, const litehtml::web_color& c)
{
    uint32_t key = (c.alpha << 24) | (c.red << 16) | (c.green << 8) | c.blue;
    auto it = m_brushPool.find(key);
    if (it != m_brushPool.end()) return it->second;
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(
        D2D1::ColorF(c.red / 255.f, c.green / 255.f, c.blue / 255.f, c.alpha / 255.f),
        &brush);
    m_brushPool[key] = brush;
    return brush;
}

ComPtr<IDWriteTextLayout> SimpleContainer::getLayout(const std::wstring& txt,
    litehtml::uint_ptr hFont,
    float maxW)
{
    // 1. 先替换花引号
    auto* fp = reinterpret_cast<FontPair*>(hFont);
    if (!fp->format) { return nullptr; }
    std::wstring clean = normalize_quotes(txt);
    LayoutKey k{ clean, a2w(fp->descr.hash()) + fp->familyName, maxW };
    auto layout = m_layoutCache.get(k);    // 原来是 m_layoutCache.find(k)->second
    if (layout) return layout;

    //std::vector<std::wstring> faces;
    //if (!fp->descr.family.empty() && !g_cfg.enableCustomFont)
    //{
    //    faces = split_font_list(fp->descr.family);
    //}
    //else
    //{
    //    faces.push_back(g_cfg.font_name);
    //}

    //// 默认字体兜底

    //faces.push_back(g_cfg.default_font_name);
    //FontCachePair* fcp;
    //for (auto& name : faces)
    //{
    //    fcp = m_fontCache.get(name, fp->descr, m_sysFontColl.Get());
    //    if (!fcp->font || !fcp->fmt) { continue; }
    //    BOOL exists = false;
    //    for (auto& w : clean)
    //    {
    //        fcp->font->HasCharacter(w, &exists);
    //        if (!exists) { continue; }
    //    }
    //    if (exists) { break; }
    //}
    //if (!fcp->fmt) { return nullptr; }
    //m_dwrite->CreateTextLayout(clean.c_str(), (UINT32)clean.size(),
    //    fcp->fmt.Get(), maxW, 512.f, &layout);

    m_dwrite->CreateTextLayout(clean.c_str(), (UINT32)clean.size(),
        fp->format.Get(), maxW, 512.f, &layout);

    if (!layout) return nullptr;

    // 换行/截断
    bool nowrap = false;
    //layout->SetWordWrapping(nowrap ? DWRITE_WORD_WRAPPING_NO_WRAP
    //    : DWRITE_WORD_WRAPPING_WRAP);
    if (nowrap) {
        DWRITE_TRIMMING trim{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        layout->SetTrimming(&trim, nullptr);
    }

    m_layoutCache.set(k, layout);          // 原来是 m_layoutCache[k] = layout;
    return layout;
}

void SimpleContainer::record_char_boxes(ID2D1DeviceContext* rt,
    IDWriteTextLayout* layout,
    const std::wstring& wtxt,
    const litehtml::position& pos)
{

    LineBoxes line;
    float originX = static_cast<float>(pos.x);
    float originY = static_cast<float>(pos.y);

    for (size_t i = 0; i < wtxt.size(); ++i)
    {
        DWRITE_HIT_TEST_METRICS htm;
        float left, top;
        layout->HitTestTextPosition(i, FALSE, &left, &top, &htm);

        CharBox cb;
        cb.ch = wtxt[i];
        cb.rect = D2D1::RectF(
            originX + left,
            originY + top,
            originX + left + htm.width,
            originY + top + htm.height);
        cb.offset = m_plainText.size() + i;
        line.push_back(cb);
    }
    m_lines.emplace_back(std::move(line));

    // 同时累积纯文本
    m_plainText += wtxt;
}


void SimpleContainer::draw_text(litehtml::uint_ptr hdc,
    const char* text,
    litehtml::uint_ptr hFont,
    litehtml::web_color color,
    const litehtml::position& pos)
{
    if (!text || !*text || !hFont) return;
    auto* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    FontPair* fp = reinterpret_cast<FontPair*>(hFont);
    if (!fp) return;

    // 1. 画刷
    auto brush = getBrush(hdc, color);
    if (!brush) return;

    // 2. 文本
    std::wstring wtxt = a2w(text);
    if (wtxt.empty()) return;

    float maxW = 8192.0f;
    auto layout = getLayout(wtxt, hFont, maxW);
    if (!layout) return;
    record_char_boxes(rt, layout.Get(), wtxt, pos);
    // 3. 绘制文本
    rt->DrawTextLayout(D2D1::Point2F(static_cast<float>(pos.x),
        static_cast<float>(pos.y)),
        layout.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

    // 4. 绘制装饰线（下划线 / 删除线 / 上划线）
    draw_decoration(hdc, fp, color, pos, layout.Get());
}
void SimpleContainer::draw_decoration(litehtml::uint_ptr hdc, const FontPair* fp,
    litehtml::web_color color,
    const litehtml::position& pos,
    IDWriteTextLayout* layout)
{
    if (fp->descr.decoration_line == litehtml::text_decoration_line_none)
        return;
    auto* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    /* 1. 文本整体尺寸 */
    DWRITE_TEXT_METRICS tm{};
    layout->GetMetrics(&tm);
    if (tm.width <= 0) return;

    /* 2. 取第一行的 baseline */
    std::vector<DWRITE_LINE_METRICS> lineMetrics;
    UINT32 lineCount = 0;
    layout->GetLineMetrics(nullptr, 0, &lineCount);
    if (lineCount == 0) return;
    lineMetrics.resize(lineCount);
    layout->GetLineMetrics(lineMetrics.data(), lineCount, &lineCount);

    const float baseline = lineMetrics[0].baseline;
    const float yBase = static_cast<float>(pos.y) + baseline;

    /* 3. 画刷 */
    auto brush = getBrush(hdc, fp->descr.decoration_color.is_current_color
        ? color
        : fp->descr.decoration_color);
    if (!brush) return;

    /* 4. 线粗：先用 1 px，后续可按 decoration_thickness 计算 */
    const float thick = fp->descr.decoration_thickness.val();

    /* 5. 绘制三种装饰线 */
    const float x0 = static_cast<float>(pos.x);
    const float x1 = x0 + tm.width;

    /* 下划线 */
    if (fp->descr.decoration_line & litehtml::text_decoration_line_underline)
    {
        const float y = yBase + 1.0f;   // 可根据字体度量再微调
        rt->DrawLine({ x0, y }, { x1, y }, brush.Get(), thick);
    }

    /* 删除线 */
    if (fp->descr.decoration_line & litehtml::text_decoration_line_line_through)
    {
        const float y = yBase - lineMetrics[0].height * 0.35f;
        rt->DrawLine({ x0, y }, { x1, y }, brush.Get(), thick);
    }

    /* 上划线 */
    if (fp->descr.decoration_line & litehtml::text_decoration_line_overline)
    {
        const float y = yBase - lineMetrics[0].height;
        rt->DrawLine({ x0, y }, { x1, y }, brush.Get(), thick);
    }
}
// ----------------------------------------------------------
// 工具：根据 box 类型返回实际矩形
// ----------------------------------------------------------
static litehtml::position clip_box(const litehtml::background_layer& layer,
    litehtml::background_box box_type)
{
    switch (box_type)
    {
    case litehtml::background_box_content:
    case litehtml::background_box_padding:
        // 你的版本没有 content_box / padding_box，统一回退到 border_box
        return layer.border_box;
    default:
        return layer.border_box;
    }
}

// ----------------------------------------------------------
// 工具：加载位图（WIC -> D2D）
// ----------------------------------------------------------

ComPtr<ID2D1Bitmap> SimpleContainer::getBitmap(litehtml::uint_ptr hdc, std::string url)
{
    /* ---------- 1. 取缓存位图 ---------- */
    //std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    //auto it = m_img_cache.find(url);

    //if (it == m_img_cache.end()) { return nullptr; }

    //ImageFrame& frame = it->second;

    //if (frame.rgba.empty())
    //{
    //    if (!g_book) { return nullptr; }

    //    auto wpath = a2w(url);
    //    auto dot = wpath.find_last_of(L'.');
    //    std::wstring ext;
    //    if (dot != std::wstring::npos && dot + 1 < wpath.size())
    //    {
    //        ext = wpath.substr(dot + 1);
    //        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    //    }

    //    frame = decode_img(MemFile{ frame.raw_data }, ext.empty() ? nullptr : ext.c_str());
    //    if (!frame.rgba.empty())
    //    {
    //        m_img_cache.emplace(url, frame);
    //    }
    //    else
    //    {
    //        OutputDebugStringA(("EPUB decode failed: " + std::string(url) + "\n").c_str());
    //        return nullptr;
    //    }


    //}

    //if (frame.rgba.empty()) return nullptr;


    ///* ---------- 2. 取/建 D2D 位图 ---------- */
    //ComPtr<ID2D1Bitmap> bmp = m_d2dBmpCache[url];   // 引用现有或新建
    //if (!bmp) {
    //    auto* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    //    D2D1_BITMAP_PROPERTIES bp =
    //        D2D1::BitmapProperties(
    //            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
    //                D2D1_ALPHA_MODE_PREMULTIPLIED));
    //    rt->CreateBitmap(
    //        D2D1::SizeU(frame.width, frame.height),
    //        frame.rgba.data(),
    //        frame.stride,
    //        bp,
    //        &bmp);
    //    if (!bmp) return nullptr;
    //    m_d2dBmpCache.emplace(url, bmp);

    //}
    //return bmp;
    return nullptr;
}
// ----------------------------------------------------------
// 主函数：draw_image
// ----------------------------------------------------------
void SimpleContainer::draw_image(litehtml::uint_ptr hdc,
    const litehtml::background_layer& layer,
    const std::string& url,
    const std::string& base_url)
{
    if (url.empty()) return;
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);

    auto bmp = getBitmap(hdc, url);
    if (!bmp) { return; }


    /* ---------- 2. 计算目标矩形 ---------- */
    D2D1_RECT_F dst = D2D1::RectF(
        float(layer.border_box.left()),
        float(layer.border_box.top()),
        float(layer.border_box.right()),
        float(layer.border_box.bottom()));

    /* ---------- 3. 计算绘制区域（cover / contain / stretch） ---------- */
    float imgW = float(bmp->GetPixelSize().width);
    float imgH = float(bmp->GetPixelSize().height);
    if (imgW == 0 || imgH == 0) return;

    float dstW = dst.right - dst.left;
    float dstH = dst.bottom - dst.top;

    // 这里只演示 cover（填满 + 居中）
    float scale = std::max(dstW / imgW, dstH / imgH);
    float bgW = imgW * scale;
    float bgH = imgH * scale;
    float bgX = dst.left + (dstW - bgW) * 0.5f;
    float bgY = dst.top + (dstH - bgH) * 0.5f;

    D2D1_RECT_F drawRect = { bgX, bgY, bgX + bgW, bgY + bgH };

    //std::string border_txt = "[border_box] " + \
    //    std::to_string(layer.border_box.left()) + ", " + \
    //    std::to_string(layer.border_box.top()) + ", " + \
    //    std::to_string(layer.border_box.right()) + ", " + \
    //    std::to_string(layer.border_box.bottom()) + "\n";
    //std::string clip_txt = "[clip_box]   " + \
    //    std::to_string(layer.clip_box.left()) + ", " + \
    //    std::to_string(layer.clip_box.top()) + ", " + \
    //    std::to_string(layer.clip_box.right()) + ", " + \
    //    std::to_string(layer.clip_box.bottom()) + "\n";
    //std::string origin_txt = "[origin_box] " + \
    //    std::to_string(layer.origin_box.left()) + ", " + \
    //    std::to_string(layer.origin_box.top()) + ", " + \
    //    std::to_string(layer.origin_box.right()) + ", " + \
    //    std::to_string(layer.origin_box.bottom()) + "\n";
    //std::string draw_txt = "[draw_box]   " + \
    //    std::to_string(drawRect.left) + ", " + \
    //    std::to_string(drawRect.top) + ", " + \
    //    std::to_string(drawRect.right) + ", " + \
    //    std::to_string(drawRect.bottom) + "\n";
    //OutputDebugStringA(origin_txt.c_str());
    //OutputDebugStringA(border_txt.c_str());
    //OutputDebugStringA(clip_txt.c_str());
    //OutputDebugStringA(draw_txt.c_str());
    //OutputDebugStringA("\n");
    /* ---------- 4. 绘制 ---------- */

    rt->DrawBitmap(bmp.Get(), drawRect, 1.0f,
        D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC,
        D2D1::RectF(0, 0, imgW, imgH));

}

inline bool SimpleContainer::is_all_zero(const litehtml::border_radiuses& r)
{
    return r.top_left_x == 0 && r.top_left_y == 0 &&
        r.top_right_x == 0 && r.top_right_y == 0 &&
        r.bottom_right_x == 0 && r.bottom_right_y == 0 &&
        r.bottom_left_x == 0 && r.bottom_left_y == 0;
}
void SimpleContainer::draw_solid_fill(litehtml::uint_ptr hdc,
    const litehtml::background_layer& layer,
    const litehtml::web_color& color)
{
    // 1. 取出 D2D 渲染目标
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    if (!rt) return;

    // 2. 创建/复用纯色画刷
    auto brush = getBrush(hdc, color);

    if (!brush) return;

    // 3. 计算要填充的矩形（border_box）
    D2D1_RECT_F rc = D2D1::RectF(
        static_cast<float>(layer.border_box.left()),
        static_cast<float>(layer.border_box.top()),
        static_cast<float>(layer.border_box.right()),
        static_cast<float>(layer.border_box.bottom()));


    // 4. 若存在圆角，用圆角矩形；否则直接矩形
    if (is_all_zero(layer.border_radius))
    {
        rt->FillRectangle(rc, brush.Get());
    }
    else
    {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            rc,
            static_cast<float>(layer.border_radius.top_left_x),
            static_cast<float>(layer.border_radius.top_left_y));
        rt->FillRoundedRectangle(rr, brush.Get());
    }
}


void SimpleContainer::draw_linear_gradient(
    litehtml::uint_ptr hdc,
    const litehtml::background_layer& layer,
    const litehtml::background_layer::linear_gradient& g)
{
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    if (!rt) return;

    // 1. 把 color_points 转成 D2D 色标
    std::vector<D2D1_GRADIENT_STOP> stops;
    stops.reserve(g.color_points.size());
    for (const auto& cp : g.color_points)
    {
        stops.push_back(D2D1::GradientStop(
            static_cast<float>(cp.offset),
            D2D1::ColorF(
                cp.color.red / 255.0f,
                cp.color.green / 255.0f,
                cp.color.blue / 255.0f,
                cp.color.alpha / 255.0f)));
    }

    // 2. 创建 stop collection
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopColl;
    if (FAILED(rt->CreateGradientStopCollection(
        stops.data(),
        static_cast<UINT>(stops.size()),
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &stopColl)))
        return;

    // 3. 创建线性渐变画刷
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> brush;
    if (FAILED(rt->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(static_cast<float>(g.start.x),
                static_cast<float>(g.start.y)),
            D2D1::Point2F(static_cast<float>(g.end.x),
                static_cast<float>(g.end.y))),
        stopColl.Get(),
        &brush)))
        return;

    // 4. 计算要填充的矩形
    const D2D1_RECT_F rc = D2D1::RectF(
        static_cast<float>(layer.border_box.left()),
        static_cast<float>(layer.border_box.top()),
        static_cast<float>(layer.border_box.right()),
        static_cast<float>(layer.border_box.bottom()));

    // 5. 圆角判断
    auto& r = layer.border_radius;
    bool no_radius = r.top_left_x == 0 && r.top_left_y == 0 &&
        r.top_right_x == 0 && r.top_right_y == 0 &&
        r.bottom_right_x == 0 && r.bottom_right_y == 0 &&
        r.bottom_left_x == 0 && r.bottom_left_y == 0;

    if (no_radius)
        rt->FillRectangle(rc, brush.Get());
    else
    {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            rc,
            static_cast<float>(r.top_left_x),
            static_cast<float>(r.top_left_y));
        rt->FillRoundedRectangle(rr, brush.Get());
    }
}


void SimpleContainer::draw_radial_gradient(
    litehtml::uint_ptr hdc,
    const litehtml::background_layer& layer,
    const litehtml::background_layer::radial_gradient& g)
{
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    if (!rt) return;

    // 1. 构造 D2D 色标
    std::vector<D2D1_GRADIENT_STOP> stops;
    stops.reserve(g.color_points.size());
    for (const auto& cp : g.color_points)
    {
        stops.push_back(D2D1::GradientStop(
            static_cast<float>(cp.offset),
            D2D1::ColorF(
                cp.color.red / 255.0f,
                cp.color.green / 255.0f,
                cp.color.blue / 255.0f,
                cp.color.alpha / 255.0f)));
    }

    // 2. 创建 stop collection
    ComPtr<ID2D1GradientStopCollection> stopColl;
    if (FAILED(rt->CreateGradientStopCollection(
        stops.data(),
        static_cast<UINT>(stops.size()),
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &stopColl)))
        return;

    // 3. 创建径向渐变画刷
    ComPtr<ID2D1RadialGradientBrush> brush;
    if (FAILED(rt->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(static_cast<float>(g.position.x),
                static_cast<float>(g.position.y)), // 圆心
            D2D1::Point2F(0.0f, 0.0f),                  // 偏移（0,0）即可
            static_cast<float>(g.radius.x),                 // rx
            static_cast<float>(g.radius.y)),                // ry（保持圆形）
        stopColl.Get(),
        &brush)))
        return;

    // 4. 计算填充区域
    const D2D1_RECT_F rc = D2D1::RectF(
        static_cast<float>(layer.border_box.left()),
        static_cast<float>(layer.border_box.top()),
        static_cast<float>(layer.border_box.right()),
        static_cast<float>(layer.border_box.bottom()));

    // 5. 圆角判断
    auto& r = layer.border_radius;
    bool no_radius = r.top_left_x == 0 && r.top_left_y == 0 &&
        r.top_right_x == 0 && r.top_right_y == 0 &&
        r.bottom_right_x == 0 && r.bottom_right_y == 0 &&
        r.bottom_left_x == 0 && r.bottom_left_y == 0;

    if (no_radius)
        rt->FillRectangle(rc, brush.Get());
    else
    {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            rc,
            static_cast<float>(r.top_left_x),
            static_cast<float>(r.top_left_y));
        rt->FillRoundedRectangle(rr, brush.Get());
    }
}


// 角度归一化到
static inline float normalize_angle(float a)
{
    a = fmodf(a, 2.0f * float(3.14));
    return a < 0 ? a + 2.0f * float(3.14) : a;
}

// 在色标数组中按角度(0..1) 线性插值颜色
static litehtml::web_color sample_color(float t,
    const std::vector<litehtml::background_layer::color_point>& stops)
{
    if (stops.empty()) return {};
    if (stops.size() == 1) return stops.front().color;

    // 保证色标有序
    auto cmp = [](const litehtml::background_layer::color_point& a,
        const litehtml::background_layer::color_point& b)
        { return a.offset < b.offset; };
    if (!std::is_sorted(stops.begin(), stops.end(), cmp))
    {
        std::vector<litehtml::background_layer::color_point> tmp = stops;
        std::sort(tmp.begin(), tmp.end(), cmp);
        return sample_color(t, tmp);
    }

    // 找到区间
    auto it = std::lower_bound(stops.begin(), stops.end(), t,
        [](const litehtml::background_layer::color_point& s, float v)
        { return s.offset < v; });

    if (it == stops.end()) return stops.back().color;
    if (it == stops.begin()) return stops.front().color;

    const auto& prev = *(it - 1);
    const auto& next = *it;
    float factor = (t - prev.offset) / (next.offset - prev.offset);
    factor = std::clamp(factor, 0.0f, 1.0f);

    litehtml::web_color c;
    c.red = static_cast<BYTE>(prev.color.red + (next.color.red - prev.color.red) * factor);
    c.green = static_cast<BYTE>(prev.color.green + (next.color.green - prev.color.green) * factor);
    c.blue = static_cast<BYTE>(prev.color.blue + (next.color.blue - prev.color.blue) * factor);
    c.alpha = static_cast<BYTE>(prev.color.alpha + (next.color.alpha - prev.color.alpha) * factor);
    return c;
}

void SimpleContainer::draw_conic_gradient(
    litehtml::uint_ptr hdc,
    const litehtml::background_layer& layer,
    const litehtml::background_layer::conic_gradient& g)
{
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    if (!rt) return;

    // 1. 计算填充矩形
    const D2D1_RECT_F rc = D2D1::RectF(
        static_cast<float>(layer.border_box.left()),
        static_cast<float>(layer.border_box.top()),
        static_cast<float>(layer.border_box.right()),
        static_cast<float>(layer.border_box.bottom()));

    const float w = rc.right - rc.left;
    const float h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    // 2. 生成位图大小（固定 512×512，可改）
    const UINT bmpSize = 512;
    const UINT stride = bmpSize * 4;
    std::vector<BYTE> pixels(bmpSize * bmpSize * 4, 0); // BGRA

    // 3. 逐像素填色
    for (UINT y = 0; y < bmpSize; ++y)
    {
        for (UINT x = 0; x < bmpSize; ++x)
        {
            // 归一化到 [-1,1]
            float nx = (x / float(bmpSize - 1)) * 2.0f - 1.0f;
            float ny = (y / float(bmpSize - 1)) * 2.0f - 1.0f;

            float angle = atan2f(ny, nx);          // -π..π
            angle += float(3.14);                  // 0..2π
            angle = normalize_angle(angle + g.angle); // 支持全局旋转
            float t = angle / (2.0f * float(3.14));   // 0..1

            litehtml::web_color c = sample_color(t, g.color_points);

            UINT idx = (y * bmpSize + x) * 4;
            pixels[idx + 0] = c.blue;
            pixels[idx + 1] = c.green;
            pixels[idx + 2] = c.red;
            pixels[idx + 3] = c.alpha;
        }
    }

    // 4. 创建 D2D 位图
    ComPtr<ID2D1Bitmap> bmp;
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(rt->CreateBitmap(
        D2D1::SizeU(bmpSize, bmpSize),
        pixels.data(),
        stride,
        props,
        &bmp)))
        return;

    // 5. 圆角判断
    auto& r = layer.border_radius;
    bool no_radius = r.top_left_x == 0 && r.top_left_y == 0 &&
        r.top_right_x == 0 && r.top_right_y == 0 &&
        r.bottom_right_x == 0 && r.bottom_right_y == 0 &&
        r.bottom_left_x == 0 && r.bottom_left_y == 0;

    // 6. 绘制
    rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    if (no_radius)
    {
        rt->DrawBitmap(bmp.Get(), rc);
    }
    else
    {
        // 用圆角矩形裁剪
        ComPtr<ID2D1Layer> layerPtr;
        rt->CreateLayer(nullptr, &layerPtr);
        rt->PushLayer(
            D2D1::LayerParameters(
                rc,
                nullptr,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::IdentityMatrix(),
                1.0f,
                nullptr,
                D2D1_LAYER_OPTIONS_NONE),
            layerPtr.Get());

        rt->DrawBitmap(bmp.Get(), rc);

        rt->PopLayer();
    }
}

void SimpleContainer::draw_list_marker(
    litehtml::uint_ptr hdc,
    const litehtml::list_marker& marker)
{
    ID2D1DeviceContext* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    if (!rt) return;

    // 1. 基础信息
    const float x = static_cast<float>(marker.pos.x);
    const float y = static_cast<float>(marker.pos.y);
    const float sz = static_cast<float>(marker.pos.width);
    const D2D1_COLOR_F color = D2D1::ColorF(
        marker.color.red / 255.0f,
        marker.color.green / 255.0f,
        marker.color.blue / 255.0f,
        marker.color.alpha / 255.0f);

    ComPtr<ID2D1SolidColorBrush> brush = getBrush(hdc, marker.color);
    if (!brush) { return; }

    rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    switch (marker.marker_type)
    {
    case litehtml::list_style_type_disc:
    {
        rt->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(x + sz * 0.5f, y + sz * 0.5f), sz * 0.5f, sz * 0.5f),
            brush.Get());
    }
    break;

    case litehtml::list_style_type_circle:
    {
        rt->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(x + sz * 0.5f, y + sz * 0.5f), sz * 0.5f, sz * 0.5f),
            brush.Get(),
            sz * 0.1f); // 线宽
    }
    break;

    case litehtml::list_style_type_square:
    {
        const D2D1_RECT_F rc = D2D1::RectF(x, y, x + sz, y + sz);
        rt->FillRectangle(rc, brush.Get());
    }
    break;

    default:
        // 其他类型（decimal、lower-alpha 等）由文本层绘制，这里忽略
        break;
    }
}


namespace {

    struct SideInfo {
        float          width = 0;
        litehtml::web_color color{};
        litehtml::border_style style = litehtml::border_style_solid;
    };

    // 根据 style 计算明暗色
    D2D1_COLOR_F AdjustColor(const litehtml::web_color& c, float factor)
    {
        return D2D1::ColorF(
            std::clamp(c.red * factor / 255.0f, 0.0f, 1.0f),
            std::clamp(c.green * factor / 255.0f, 0.0f, 1.0f),
            std::clamp(c.blue * factor / 255.0f, 0.0f, 1.0f),
            c.alpha / 255.0f);
    }

} // namespace

void SimpleContainer::draw_borders(litehtml::uint_ptr hdc,
    const litehtml::borders& borders,
    const litehtml::position& draw_pos,
    bool root)
{
    if (!hdc) return;
    auto* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    // 1. 收集四边
    std::array<SideInfo, 4> sides = {
        SideInfo{ (float)borders.top.width,    borders.top.color,    borders.top.style },
        SideInfo{ (float)borders.right.width,  borders.right.color,  borders.right.style },
        SideInfo{ (float)borders.bottom.width, borders.bottom.color, borders.bottom.style },
        SideInfo{ (float)borders.left.width,   borders.left.color,   borders.left.style }
    };

    if (std::all_of(sides.begin(), sides.end(),
        [](const SideInfo& s) { return s.width <= 0; }))
        return;

    // 2. 建立工厂
    ComPtr<ID2D1Factory> factory;
    rt->GetFactory(&factory);

    // 3. 构造外轮廓
    auto build_rounded_rect = [&](float l, float t, float r, float b,
        const litehtml::border_radiuses& rad,
        ComPtr<ID2D1PathGeometry>& out) -> bool
        {
            ComPtr<ID2D1PathGeometry> geo;
            if (FAILED(factory->CreatePathGeometry(&geo))) return false;
            ComPtr<ID2D1GeometrySink> sink;
            if (FAILED(geo->Open(&sink))) return false;

            float rtl = (float)rad.top_left_x, rtr = (float)rad.top_right_x;
            float rbr = (float)rad.bottom_right_x, rbl = (float)rad.bottom_left_x;

            sink->BeginFigure(D2D1::Point2F(l + rtl, t), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(r - rtr, t));
            if (rbr > 0) sink->AddArc(D2D1::ArcSegment(
                D2D1::Point2F(r - rbr, b),
                D2D1::SizeF(rbr, rbr),
                0.0f,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                D2D1_ARC_SIZE_SMALL));

            if (rbl > 0) sink->AddArc(D2D1::ArcSegment(
                D2D1::Point2F(l, b - rbl),
                D2D1::SizeF(rbl, rbl),
                0.0f,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                D2D1_ARC_SIZE_SMALL));

            if (rtl > 0) sink->AddArc(D2D1::ArcSegment(
                D2D1::Point2F(l + rtl, t),
                D2D1::SizeF(rtl, rtl),
                0.0f,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                D2D1_ARC_SIZE_SMALL));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            out = std::move(geo);
            return true;
        };

    float l = (float)draw_pos.left();
    float t = (float)draw_pos.top();
    float r = (float)draw_pos.right();
    float b = (float)draw_pos.bottom();

    ComPtr<ID2D1PathGeometry> outer, inner;
    if (!build_rounded_rect(l, t, r, b, borders.radius, outer)) return;

    // 4. 内轮廓（统一用最小边宽）
    float minW = std::min({ sides[0].width, sides[1].width, sides[2].width, sides[3].width });
    litehtml::border_radiuses innerRad = borders.radius;
    innerRad.top_left_x = std::max(innerRad.top_left_x - minW, 0.0f);
    innerRad.top_right_x = std::max(innerRad.top_right_x - minW, 0.0f);
    innerRad.bottom_right_x = std::max(innerRad.bottom_right_x - minW, 0.0f);
    innerRad.bottom_left_x = std::max(innerRad.bottom_left_x - minW, 0.0f);

    if (!build_rounded_rect(l + sides[3].width, t + sides[0].width,
        r - sides[1].width, b - sides[2].width,
        innerRad, inner)) return;

    // 5. 创建边框几何 = outer - inner
    ComPtr<ID2D1PathGeometry> borderGeo;
    factory->CreatePathGeometry(&borderGeo);
    ComPtr<ID2D1GeometrySink> sink;
    borderGeo->Open(&sink);
    outer->CombineWithGeometry(inner.Get(), D2D1_COMBINE_MODE_EXCLUDE,
        nullptr, sink.Get());
    sink->Close();

    // 6. 画四边（按顺序 top/right/bottom/left）
    const std::array<const char*, 4> sideNames = { "top","right","bottom","left" };
    const std::array<float, 4> offsets = { 0, 0, 0, 0 }; // 预留
    (void)offsets;

    // 6-a 纯色简单实现：先整体填充背景色，再描边
    // 这里为了演示，只画四条独立路径，实际可优化
    for (int idx = 0; idx < 4; ++idx)
    {
        const SideInfo& side = sides[idx];
        if (side.width <= 0) continue;

        ComPtr<ID2D1SolidColorBrush> brush;
        D2D1_COLOR_F clr;
        switch (side.style)
        {
        case litehtml::border_style_groove:
            clr = AdjustColor(side.color, 0.75f); break;
        case litehtml::border_style_ridge:
            clr = AdjustColor(side.color, 1.25f); break;
        case litehtml::border_style_inset:
            clr = AdjustColor(side.color, 0.60f); break;
        case litehtml::border_style_outset:
            clr = AdjustColor(side.color, 1.40f); break;
        default:
            clr = D2D1::ColorF(side.color.red / 255.0f,
                side.color.green / 255.0f,
                side.color.blue / 255.0f,
                side.color.alpha / 255.0f);
        }
        rt->CreateSolidColorBrush(clr, &brush);

        // 为每条边单独构造路径（略繁琐，但保证独立颜色）
        ComPtr<ID2D1PathGeometry> sidePath;
        factory->CreatePathGeometry(&sidePath);
        ComPtr<ID2D1GeometrySink> sideSink;
        sidePath->Open(&sideSink);

        switch (idx)
        {
        case 0: // top
            sideSink->BeginFigure(D2D1::Point2F(l + borders.radius.top_left_x, t), D2D1_FIGURE_BEGIN_HOLLOW);
            sideSink->AddLine(D2D1::Point2F(r - borders.radius.top_right_x, t));
            break;
        case 1: // right
            sideSink->BeginFigure(D2D1::Point2F(r, t + borders.radius.top_right_x), D2D1_FIGURE_BEGIN_HOLLOW);
            sideSink->AddLine(D2D1::Point2F(r, b - borders.radius.bottom_right_x));
            break;
        case 2: // bottom
            sideSink->BeginFigure(D2D1::Point2F(r - borders.radius.bottom_right_x, b), D2D1_FIGURE_BEGIN_HOLLOW);
            sideSink->AddLine(D2D1::Point2F(l + borders.radius.bottom_left_x, b));
            break;
        case 3: // left
            sideSink->BeginFigure(D2D1::Point2F(l, b - borders.radius.bottom_left_x), D2D1_FIGURE_BEGIN_HOLLOW);
            sideSink->AddLine(D2D1::Point2F(l, t + borders.radius.top_left_x));
            break;
        }
        sideSink->EndFigure(D2D1_FIGURE_END_OPEN);
        sideSink->Close();

        // 描边
        rt->DrawGeometry(sidePath.Get(), brush.Get(), side.width);
    }
}
// 工具：转小写
std::wstring  SimpleContainer::toLower(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), towlower);
    return s;
}



std::vector<std::wstring>
SimpleContainer::split_font_list(const std::string& src) {
    std::vector<std::wstring> out;
    std::string token;
    for (size_t i = 0, n = src.size(); i < n; ++i)
    {
        if (src[i] == ',')
        {
            token = trim_any(token);
            if (!token.empty()) {
                std::wstring face = a2w(token);
                out.emplace_back(face);
                token.clear();
            }
        }
        else
        {
            token += src[i];
        }
    }
    token = trim_any(token);
    if (!token.empty())
    {
        std::wstring face = a2w(token);
        out.emplace_back(face);
    }
    return out;
};


litehtml::uint_ptr SimpleContainer::create_font(const litehtml::font_description& descr,
    const litehtml::document* doc,
    litehtml::font_metrics* fm)
{

    if (!m_dwrite || !fm) return 0;

    /*----------------------------------------------------------
      1. 把 font-family 字符串拆成单个字体名
    ----------------------------------------------------------*/
    std::vector<std::wstring> faces;
    if (!descr.family.empty() 
        //&& !g_cfg.enableCustomFont
        )
    {
        faces = split_font_list(descr.family);
    }
    else
    {
        //faces.push_back(g_cfg.font_name);
    }

    // 默认字体兜底

    //faces.push_back(g_cfg.default_font_name);
    std::wstring family_name = L"";
    FontCachePair* fcp;
    for (auto f : faces)
    {
        family_name = f;
        fcp = m_fontCache.get(f, descr, m_sysFontColl.Get());
        if (fcp->fmt)break;
    }
    if (!fcp->fmt) {
        OutputDebugStringW(L"[DWrite] 加载默认字体失败\n");
        return 0;
    }

    DWRITE_FONT_METRICS m{};
    fcp->font->GetMetrics(&m);
    const float dip = descr.size / static_cast<float>(m.designUnitsPerEm);

    fm->font_size = descr.size;
    fm->ascent = m.ascent * dip;
    fm->descent = m.descent * dip;
    fm->height = (m.ascent + m.descent + m.lineGap) * dip;
    fm->x_height = m.xHeight * dip;
    fm->draw_spaces = descr.style == litehtml::font_style_italic || descr.decoration_line != litehtml::text_decoration_line_none;
    fm->ch_width = fm->font_size * 3 / 5;
    fm->sub_shift = descr.size / 5;
    fm->super_shift = descr.size / 3;




    return reinterpret_cast<litehtml::uint_ptr>(new FontPair(fcp->fmt, descr, fcp->familyName));
}

void SimpleContainer::delete_font(litehtml::uint_ptr h)
{
    if (!h) return;
    //auto* fp = reinterpret_cast<FontPair*>(h);
    //delete fp;              // 4. 真正释放
}

litehtml::pixel_t SimpleContainer::text_width(const char* text,
    litehtml::uint_ptr hFont)
{
    if (!text || !*text || !hFont) return 0;

    std::wstring wtxt = a2w(text);
    if (wtxt.empty()) return 0;

    // 1. 创建 TextLayout

    float maxW = 8192.0f;
    auto layout = getLayout(wtxt, hFont, maxW);
    if (!layout) { return 0; }


    // 3. 取逻辑宽度（已含空白、连字、kerning）
    DWRITE_TEXT_METRICS tm{};
    HRESULT hr = layout->GetMetrics(&tm);
    if (FAILED(hr)) { return 0; }

    // 4. DPI → 物理像素（Win7 也支持）
    float dpiX, dpiY; dpiX = dpiY = 96.0f;
    m_d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
    float physical = tm.widthIncludingTrailingWhitespace * dpiX / 96.0f;

    return physical;
}


void SimpleContainer::build_rounded_rect_path(
    ComPtr<ID2D1GeometrySink>& sink,
    const litehtml::position& pos,
    const litehtml::border_radiuses& bdr)
{
    float l = float(pos.left()), t = float(pos.top());
    float r = float(pos.right()), b = float(pos.bottom());

    float rtl = float(bdr.top_left_x);
    float rtr = float(bdr.top_right_x);
    float rbr = float(bdr.bottom_right_x);
    float rbl = float(bdr.bottom_left_x);

    sink->BeginFigure(D2D1::Point2F(l + rtl, t), D2D1_FIGURE_BEGIN_FILLED);

    // top edge
    sink->AddLine(D2D1::Point2F(r - rtr, t));
    if (rtr > 0) sink->AddArc(D2D1::ArcSegment(
        D2D1::Point2F(r, t + rtr), D2D1::SizeF(rtr, rtr),
        0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

    // right edge
    sink->AddLine(D2D1::Point2F(r, b - rbr));
    if (rbr > 0) sink->AddArc(D2D1::ArcSegment(
        D2D1::Point2F(r - rbr, b), D2D1::SizeF(rbr, rbr),
        0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

    // bottom edge
    sink->AddLine(D2D1::Point2F(l + rbl, b));
    if (rbl > 0) sink->AddArc(D2D1::ArcSegment(
        D2D1::Point2F(l, b - rbl), D2D1::SizeF(rbl, rbl),
        0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

    // left edge
    sink->AddLine(D2D1::Point2F(l, t + rtl));
    if (rtl > 0) sink->AddArc(D2D1::ArcSegment(
        D2D1::Point2F(l + rtl, t), D2D1::SizeF(rtl, rtl),
        0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

void SimpleContainer::set_clip(const litehtml::position& pos,
    const litehtml::border_radiuses& bdr)
{
    if (!m_dc) return;

    // 无圆角 → 矩形裁剪
    if (is_all_zero(bdr))
    {
        m_dc->PushAxisAlignedClip(
            D2D1::RectF(float(pos.left()), float(pos.top()),
                float(pos.right()), float(pos.bottom())),
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        m_clipStack.emplace_back(nullptr);          // 标记为矩形
        return;
    }

    // 有圆角 → 用 PathGeometry + Layer
    ComPtr<ID2D1Factory> factory;
    m_dc->GetFactory(&factory);

    ComPtr<ID2D1PathGeometry> path;
    factory->CreatePathGeometry(&path);
    ComPtr<ID2D1GeometrySink> sink;
    path->Open(&sink);
    build_rounded_rect_path(sink, pos, bdr);        // 见下
    sink->Close();

    ComPtr<ID2D1Layer> layer;
    if (SUCCEEDED(m_dc->CreateLayer(nullptr, &layer)))
    {
        m_dc->PushLayer(
            D2D1::LayerParameters(D2D1::InfiniteRect(), path.Get()),
            layer.Get());
        m_clipStack.emplace_back(std::move(layer));
    }
}

void SimpleContainer::del_clip()
{
    if (m_clipStack.empty()) return;
    if (m_clipStack.back())
        m_dc->PopLayer();           // 圆角
    else
        m_dc->PopAxisAlignedClip(); // 矩形
    m_clipStack.pop_back();
}


litehtml::uint_ptr SimpleContainer::getContext() { return reinterpret_cast<litehtml::uint_ptr>(m_dc.Get()); }





//void SimpleContainer::resize(int w, int h)
//{
//    if (w <= 0 || h <= 0) return;
//
//    m_w = w;
//    m_h = h;
//    m_d2dBmpCache.clear();   // 释放所有旧位图
//    if (!m_rt) { return; }
//
//        D2D1_SIZE_U size{ static_cast<UINT32>(w), static_cast<UINT32>(h) };
//        if (SUCCEEDED(m_rt->Resize(size))) return;   // DPI 不变时直接 Resize
//
//}


void SimpleContainer::resize(int w, int h)
{
    if (w <= 0 || h <= 0) return;

    m_w = w;
    m_h = h;

    // 1) 释放所有依赖后台缓冲的 D2D 资源
    m_dc->SetTarget(nullptr);          // 解绑
    m_targetBmp.Reset();               // 你之前叫 targetBmp，这里起名叫 m_targetBmp
    m_d2dBmpCache.clear();             // 你自己的缓存

    // 2) 调整交换链缓冲大小
    HRESULT hr = m_swapChain->ResizeBuffers(
        0,                             // 保持 BufferCount
        static_cast<UINT>(w),
        static_cast<UINT>(h),
        DXGI_FORMAT_B8G8R8A8_UNORM,
        0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        // 设备丢失，需要重新创建设备链（略）
        return;
    }

    // 3) 重新绑定新的后台缓冲
    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    m_dc->CreateBitmapFromDxgiSurface(backBuffer.Get(), bmpProps, &m_targetBmp);
    m_dc->SetTarget(m_targetBmp.Get());

    // 4) 更新 DPI（可选）
    float dpiX = 96.0f, dpiY = 96.0f;
    m_d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
    m_dc->SetDpi(dpiX, dpiY);
}



SimpleContainer::SimpleContainer(int w, int h, HWND hwnd) :
    m_w(w), m_h(h), m_hwnd(hwnd)
{

    /* 1) D2D 工厂（1.1 ） */
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        nullptr,
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf()));
    if (FAILED(hr)) {
        OutputDebugStringA("D2D1CreateFactory failed\n");
        return;
    }

    /* 2) 计算窗口 DPI 缩放 */
    float dpiX = 96.0f, dpiY = 96.0f;
    m_d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
    const float scale = dpiX / 96.0f;


    // 2) 创建 D3D11 设备（flag 选 D3D11_CREATE_DEVICE_BGRA_SUPPORT）
    Microsoft::WRL::ComPtr<ID3D11Device>        d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dCtx;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &d3dDevice, nullptr, &d3dCtx);
    if (DXGI_ERROR_UNSUPPORTED == hr)
    {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION,
            &d3dDevice, nullptr, &d3dCtx);
    }


    // 3) 拿到 DXGI 设备
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    d3dDevice.As(&dxgiDevice);

    // 4) 用 DXGI 设备创建 D2D 设备
    m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);

    // 5) 创建 D2D 设备上下文
    m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_dc);

    m_dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    // 6) 创建交换链
    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    //scDesc.Scaling = DXGI_SCALING_STRETCH;
    //scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    dxgiFactory->CreateSwapChainForHwnd(
        d3dDevice.Get(), m_hwnd, &scDesc, nullptr, nullptr, &m_swapChain);

    // 7) 把交换链的后台缓冲绑定到 D2D 目标位图
    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    
    m_dc->CreateBitmapFromDxgiSurface(backBuffer.Get(), bmpProps, &m_targetBmp);
    m_dc->SetTarget(m_targetBmp.Get());
    m_dc->SetDpi(dpiX, dpiY);
   

    /* 4) DirectWrite 工厂 */
    IDWriteFactory* pRaw = nullptr;
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&pRaw));   // OK
    m_dwrite.Attach(pRaw);   // 把裸指针交给 ComPtr 管理
    if (FAILED(hr)) {
        OutputDebugStringA("DWriteCreateFactory failed\n");
        return;
    }
    m_dwrite->CreateTextAnalyzer(&m_analyzer);
    /* 5) 系统字体集合 */
    hr = m_dwrite->GetSystemFontCollection(&m_sysFontColl, FALSE);
    if (FAILED(hr)) {
        OutputDebugStringA("GetSystemFontCollection failed\n");
    }
    BuildFontList();
    init_dpi();
}

void SimpleContainer::BuildFontList()
{
    //g_fontList.clear();

    //auto sysColl = m_sysFontColl;
    //if (!sysColl)
    //    return;

    //UINT32 count = sysColl->GetFontFamilyCount();
    //for (UINT32 i = 0; i < count; ++i)
    //{
    //    ComPtr<IDWriteFontFamily> family;
    //    if (FAILED(sysColl->GetFontFamily(i, &family)))
    //        continue;

    //    ComPtr<IDWriteLocalizedStrings> names;
    //    if (FAILED(family->GetFamilyNames(&names)))
    //        continue;

    //    // 1. 取英文字体原名
    //    UINT32 idx = 0;
    //    BOOL exists = FALSE;
    //    names->FindLocaleName(L"en-us", &idx, &exists);
    //    if (!exists) idx = 0; // fallback

    //    UINT32 len = 0;
    //    names->GetStringLength(idx, &len);
    //    std::wstring familyName(len + 1, 0);
    //    names->GetString(idx, familyName.data(), len + 1);

    //    // 2. 尝试取中文名
    //    std::wstring displayName = familyName; // 默认
    //    names->FindLocaleName(L"zh-cn", &idx, &exists);
    //    if (exists)
    //    {
    //        names->GetStringLength(idx, &len);
    //        displayName.resize(len);
    //        names->GetString(idx, displayName.data(), len + 1);
    //    }

    //    g_fontList.push_back({ familyName, displayName });
    //}

    //// 可选：按 displayName 排序
    //std::sort(g_fontList.begin(), g_fontList.end(),
    //    [](const FontItem& a, const FontItem& b)
    //    { return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0; });
}
SimpleContainer::~SimpleContainer()
{
    clear();
}


void SimpleContainer::clear()
{
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    clear_selection();
    m_img_cache.clear();
    m_d2dBmpCache.clear();
    m_anchor_map.clear();
    m_doc.reset();
    m_privateFonts.Reset();

    m_clipStack.clear();
    m_fontCache.clear();
    m_layoutCache.clear();
    m_brushPool.clear();

}

litehtml::pixel_t SimpleContainer::get_default_font_size() const
{
    return 16;
}
const char* SimpleContainer::get_default_font_name() const
{
    return "Microsoft YaHei";
}


FontCache::FontCache() {
    DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dw.GetAddressOf()));   // 
    m_loader = new FileCollectionLoader();
    m_dw->RegisterFontCollectionLoader(m_loader);
}

/* ---------------------------------------------------------- */
FontCachePair*
FontCache::get(std::wstring& familyName, const litehtml::font_description& descr,
    IDWriteFontCollection* sysColl) {
    // 1. 构造键

    std::wstring search_key = std::wstring(familyName) + a2w(descr.hash());
    // 2. 读缓存
    {
        std::shared_lock sl(m_mtx);
        if (auto it = m_map.find(search_key); it != m_map.end() && it->second->font && it->second->fmt)
            return it->second;
    }

    // 3. 未命中，创建并写入
    auto fcp = create(familyName, descr, sysColl);
    {
        std::unique_lock ul(m_mtx);
        m_map[search_key] = fcp;          // 若并发重复，后写覆盖，无妨
    }
    return fcp;
}
ComPtr<IDWriteFontCollection>
FontCache::CreatePrivateCollectionFromFile(IDWriteFactory* dw, const wchar_t* path)
{
    ComPtr<IDWriteFontFile> file;
    if (FAILED(dw->CreateFontFileReference(path, nullptr, &file)))
        return nullptr;
    BOOL isSupported = FALSE;
    DWRITE_FONT_FILE_TYPE fileType = DWRITE_FONT_FILE_TYPE_UNKNOWN;
    DWRITE_FONT_FACE_TYPE faceType = DWRITE_FONT_FACE_TYPE_UNKNOWN;
    UINT32  faceCount = 0;
    if (FAILED(file->Analyze(
        &isSupported,
        &fileType,
        &faceType,
        &faceCount)) || !isSupported || faceCount < 1)
    {
        return nullptr;
    }
    // 用系统自带的“文件集合加载器”
    ComPtr<IDWriteFontCollection> collection;
    IDWriteFontFile* files[] = { file.Get() };
    IDWriteFontFile* key[] = { file.Get() };
    if (FAILED(m_dw->CreateCustomFontCollection(
        m_loader,
        key,
        sizeof(key),
        &collection)))
        return nullptr;

    return collection;
}

FontCachePair*
FontCache::create(std::wstring& familyName, const litehtml::font_description& descr, IDWriteFontCollection* sysColl)
{



    ///* ---------- 1. 候选列表（路径优先） ---------- */
    std::vector<std::wstring> tryNames{ familyName };
    //if (g_book && g_cfg.enableEPUBFonts)
    //{
    //    FontKey exact{ familyName, descr.weight, descr.style, 0 };
    //    if (auto it = g_book->m_fontBin.find(exact); it != g_book->m_fontBin.end())
    //        tryNames.insert(tryNames.end(), it->second.begin(), it->second.end());
    //    else
    //    {
    //        for (const auto& kv : g_book->m_fontBin)
    //            if (kv.first.family == familyName)
    //                tryNames.insert(tryNames.end(), kv.second.begin(), kv.second.end());
    //    }

    //}

    ///* ---------- 2. 工具：一次性生成 metrics ---------- */


    ///* ---------- 3. 路径字体（私有集合） ---------- */
    //if (g_cfg.enableEPUBFonts)
    //{
    //    for (std::wstring& name : tryNames)
    //    {
    //        if (name.find(L':') == std::wstring_view::npos) continue;
    //        auto& coll = collCache[name];
    //        if (!coll)
    //        {
    //            coll = CreatePrivateCollectionFromFile(m_dw.Get(), std::wstring(name).c_str());
    //            if (coll) { collCache.emplace(name, coll); }
    //        }
    //        if (!coll) continue;

    //        UINT32 familyCount = coll->GetFontFamilyCount();
    //        if (familyCount == 0) continue;

    //        ComPtr<IDWriteFontFamily> family;
    //        if (FAILED(coll->GetFontFamily(0, &family))) continue;

    //        ComPtr<IDWriteFont> font;
    //        if (FAILED(family->GetFirstMatchingFont(
    //            static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
    //            DWRITE_FONT_STRETCH_NORMAL,
    //            descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
    //            &font))) continue;

    //        wchar_t familyName[LF_FACESIZE]{};
    //        {
    //            ComPtr<IDWriteLocalizedStrings> names;
    //            if (SUCCEEDED(family->GetFamilyNames(&names)))
    //            {
    //                UINT32 idx = 0, len = 0;
    //                BOOL exists = FALSE;
    //                names->FindLocaleName(L"en-us", &idx, &exists);
    //                if (!exists) idx = 0;
    //                names->GetStringLength(idx, &len);
    //                if (len < LF_FACESIZE)
    //                    names->GetString(idx, familyName, len + 1);
    //            }
    //        }
    //        if (!familyName[0]) { continue; }
    //        ComPtr<IDWriteTextFormat> fmt;
    //        if (SUCCEEDED(m_dw->CreateTextFormat(
    //            familyName,
    //            coll.Get(),
    //            static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
    //            descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
    //            DWRITE_FONT_STRETCH_NORMAL,
    //            static_cast<float>(descr.size), L"en-us", &fmt)))
    //            return new FontCachePair{ familyName,  fmt, font };

    //    }
    //}


    /* ---------- 4. 系统字体 ---------- */
    if (sysColl)
    {
        for (std::wstring& name : tryNames)
        {
            UINT32 index = 0;
            BOOL exists = FALSE;
            sysColl->FindFamilyName(std::wstring(name).c_str(), &index, &exists);
            if (!exists) continue;

            ComPtr<IDWriteFontFamily> family;
            if (FAILED(sysColl->GetFontFamily(index, &family))) continue;

            ComPtr<IDWriteFont> font;
            if (FAILED(family->GetFirstMatchingFont(
                static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
                DWRITE_FONT_STRETCH_NORMAL,
                descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                &font))) continue;
            ComPtr<IDWriteTextFormat> fmt;
            if (FAILED(m_dw->CreateTextFormat(
                std::wstring(name).c_str(), sysColl,
                static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
                descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                static_cast<float>(descr.size), L"en-us", &fmt))) {
                continue;
            }

            return new FontCachePair{ std::wstring(name), fmt , font };

        }
    }

    /* ---------- 5. 失败 ---------- */
    return new FontCachePair{ familyName, nullptr, nullptr };
}
/* ---------------------------------------------------------- */
//FontCachePair
//FontCache::create(const FontKey& key, IDWriteFontCollection* privateColl, IDWriteFontCollection* sysColl) {
//    // 候选家族列表：精确 → 仅 family → 默认
//    std::wstring tryName;
//    tryName = key.family;
//
//    // 若 g_book->m_fontBin 有映射，追加真实文件名
//    if (g_book) {
//        FontKey exact{ key.family, key.weight, key.italic, 0 }; // size 忽略
//        if (auto it = g_book->m_fontBin.find(exact); it != g_book->m_fontBin.end())
//            tryName = it->second;
//
//        // 退而求其次：仅 family
//        for (const auto& kv : g_book->m_fontBin)
//            if (kv.first.family == key.family) { tryName = kv.second; break; }
//    }
//
//
//    // 逐个尝试
//
//    for (auto coll : { privateColl, sysColl }) {   // 先私有，再系统
//        if (!coll) continue;
//        UINT32 index = 0;
//        Microsoft::WRL::ComPtr<IDWriteFontFamily> dwFamily;
//        if (!findFamily(coll, tryName, dwFamily, index)) continue;
//
//        Microsoft::WRL::ComPtr<IDWriteTextFormat> fmt;
//        if (SUCCEEDED(m_dw->CreateTextFormat(
//            tryName.c_str(), coll,
//            static_cast<DWRITE_FONT_WEIGHT>(key.weight),
//            key.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
//            DWRITE_FONT_STRETCH_NORMAL,
//            static_cast<float>(key.size),
//            L"en-us",
//            &fmt))) 
//        {
//            ComPtr<IDWriteFont>  dwFont;
//            dwFamily->GetFirstMatchingFont(
//                static_cast<DWRITE_FONT_WEIGHT>(key.weight),
//                DWRITE_FONT_STRETCH_NORMAL,
//                key.italic == litehtml::font_style_italic ? DWRITE_FONT_STYLE_ITALIC
//                : DWRITE_FONT_STYLE_NORMAL,
//                &dwFont);
//            FontCachePair fcp;
//            fcp.fmt = fmt;
//            fcp.dwFont = dwFont;
//            return std::move(fcp);   // 成功
//        }
//    }
//
//    // 理论上不会走到这里，除非默认字体也失败
//    
//    return { nullptr, nullptr };
//    
//}

/* ---------------------------------------------------------- */
bool FontCache::findFamily(IDWriteFontCollection* coll,
    const std::wstring& target,
    Microsoft::WRL::ComPtr<IDWriteFontFamily>& family,
    UINT32& index)
{
    // 1) 快路径：DWrite 自带
    //BOOL exists = FALSE;
    //if (SUCCEEDED(coll->FindFamilyName(target.c_str(), &index, &exists)) && exists)
    //    return true;

    // 2) 慢路径：逐 family 遍历
    UINT32 count = coll->GetFontFamilyCount();
    for (UINT32 i = 0; i < count; ++i)
    {

        if (FAILED(coll->GetFontFamily(i, &family)))
            continue;

        Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
        if (FAILED(family->GetFamilyNames(&names)))
            continue;

        UINT32 len = 0;
        if (FAILED(names->GetStringLength(0, &len)))
            continue;

        std::wstring buf(len + 1, L'\0');
        if (FAILED(names->GetString(0, buf.data(), len + 1)))
            continue;
        buf.resize(len);

        if (_wcsicmp(buf.c_str(), target.c_str()) == 0)
        {
            index = i;
            return true;
        }
    }
    return false;   // 真没找到
}

void FontCache::clear() {
    std::unique_lock ul(m_mtx);
    m_map.clear();          // ComPtr 归零，DWrite 对象随之释放
}


