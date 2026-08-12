#include "Tools.h"

#include <algorithm>

#include "AppSettings.h"
#include "SimpleContainer.h"
#include "VirtualDoc.h"
#include "MML2SVG.h"

std::wstring seconds2string(int64_t sec)
{
    int64_t days = sec / 86400;
    int64_t hours = (sec % 86400) / 3600;
    int64_t minutes = (sec % 3600) / 60;
    int64_t seconds = sec % 60;

    std::wstring timeStr;
    if (days)    timeStr += std::to_wstring(days) + L"天 ";
    if (hours || days)   timeStr += std::to_wstring(hours) + L"时";
    if (minutes || hours || days) timeStr += std::to_wstring(minutes) + L"分";
    timeStr += std::to_wstring(seconds) + L"秒";
    return timeStr;
}

// 1. 仅内嵌 global.css
 std::string get_global_css()
{
    fs::path file = exe_dir() / "res" / "global.css";
    if (!fs::exists(file)) { return ""; }
    std::string css = read_file(file);
    return css;

}

// ---------- 工具 ----------
 std::string w2a(const std::wstring& s)
{
    //Timer timer("    w2a");
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len - 1, 0);                 // 去掉末尾 '\0'
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

 std::wstring a2w(const std::string& s)
{
    //Timer timer("    a2w");
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len - 1, 0);                // 去掉末尾 '\0'
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    return out;
}

// 读取 ./config/global.css

 fs::path exe_dir()
{
    wchar_t buf[1024]{};
    GetModuleFileNameW(nullptr, buf, 1024);
    return fs::path(buf).parent_path();

}


fs::path documents_dir()
{
    PWSTR pszPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pszPath)))
    {
        fs::path path(pszPath);
        CoTaskMemFree(pszPath);
        return path;
    }
    return L""; // 或抛出异常
}

// 工具：把文件内容读成字符串
 std::string read_file(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}


void CheckAllMenuItem()
{
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CSS,
        MF_BYCOMMAND | (g_cfg.enableCSS ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_GLOBAL_CSS,
        MF_BYCOMMAND | (g_cfg.enableGlobalCSS ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_EPUB_FONTS,
        MF_BYCOMMAND | (g_cfg.enableEPUBFonts ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_HOVER_PREVIEW,
        MF_BYCOMMAND | (g_cfg.enableHoverPreview ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CLICK_PREVIEW,
        MF_BYCOMMAND | (g_cfg.enableClickPreview ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_SCROLL_ANIMATION,
        MF_BYCOMMAND | (g_cfg.enableScrollAnimation ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CUSTOM_FONT,
        MF_BYCOMMAND | (g_cfg.enableCustomFont ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_FRAME_RATE,
        MF_BYCOMMAND | (g_cfg.displayFrameRate ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_STATUS_WINDOW,
        MF_BYCOMMAND | (g_cfg.displayStatusBar ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_TOC_WINDOW,
        MF_BYCOMMAND | (g_cfg.displayTOC ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_SCROLLBAR_WINDOW,
        MF_BYCOMMAND | (g_cfg.displayScrollBar ? MF_CHECKED : MF_UNCHECKED));

    EnableMenuItem(GetMenu(g_hWnd), ID_CHOOSE_FONT, g_cfg.enableCustomFont ? MF_ENABLED : MF_DISABLED);
}


void UpdateCache()
{
    if (!g_cMain || !g_vd || !g_book) return;

    if (g_updateTimer)
    {

        return;
    }
    g_updateTimer = timeSetEvent(g_cfg.update_interval_ms, 0, OnUpdateTimer, 0, TIME_ONESHOT);
    RECT rc;
    GetClientRect(g_hView, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    w /= g_cMain->m_zoom_factor;
    h /= g_cMain->m_zoom_factor;

    g_vd->update_doc(h);


    g_center_offset = (w - g_cfg.document_width) * 0.5f;
}

void convert_coordinate(POINT& pt)
{
    if (g_cMain)
    {
        pt.x = pt.x / g_cMain->m_zoom_factor - g_center_offset;
        pt.y = pt.y / g_cMain->m_zoom_factor + g_offsetY.load(std::memory_order_relaxed); ;
    }

}

bool SaveHDCAsBmp(HDC hdc, int width, int height, const wchar_t* name)
{
    // 1. 创建兼容的内存 DC 和位图
    HDC     memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = height;   // 正数 = 底-上 DIB
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;       // 24-bit RGB
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp) { DeleteDC(memDC); return false; }

    // 2. 把 hdc 内容拷到 DIB
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);
    BitBlt(memDC, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

    // 3. 构造 BITMAPFILEHEADER + DIB 数据
    BITMAPFILEHEADER bfh = { 0 };
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    DWORD dibSize = ((width * 3 + 3) & ~3) * height; // 每行 4 字节对齐
    bfh.bfSize = bfh.bfOffBits + dibSize;

    // 4. 写文件
    HANDLE hFile = CreateFileW(name,
        GENERIC_WRITE,
        0, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);
        return false;
    }

    DWORD written = 0;
    WriteFile(hFile, &bfh, sizeof(bfh), &written, nullptr);
    WriteFile(hFile, &bi.bmiHeader, sizeof(bi.bmiHeader), &written, nullptr);
    WriteFile(hFile, bits, dibSize, &written, nullptr);
    CloseHandle(hFile);

    // 5. 清理
    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    return true;
}


void LogToFile(const std::string& message)
{
    fs::path debug_path = documents_dir() / g_cfg.appName / "debug_log.txt";

    std::ofstream log(debug_path, std::ios::app);
    if (log.is_open())
    {
        log << std::to_string(nowUs()) << " " << message << std::endl;
    }
}

// 生成临时目录，返回路径（带反斜杠）
 std::string make_temp_dir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + g_cfg.temp_dir + L"\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    return fs::path(dir).generic_string();
}

// 工具：把路径拷到堆，返回指针
inline wchar_t* DupPath(const wchar_t* src)
{
    size_t len = wcslen(src) + 1;
    wchar_t* buf = (wchar_t*)CoTaskMemAlloc(len * sizeof(wchar_t));
    wcscpy_s(buf, len, src);
    return buf;
}

/* ---------- 工具 ---------- */
 int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}


void CALLBACK OnFrameRateTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    if (!g_cfg.displayFrameRate) { timeKillEvent(g_framerateTimer); g_framerateTimer = 0; }
    std::wstring txt = L"帧率：" + std::to_wstring(g_frame_count);
    SetStatus(STATUSBAR_FRAME_RATE, txt.c_str());
    g_frame_count = 0;

}

void CALLBACK OnScrollTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{

    float dt = 0.001f;


    // 物理惯性：v = v * e^(-k*dt)
    const float friction = 8.0f;   // 越大越快停
    float v = g_velocity.load(std::memory_order_relaxed) * std::exp(-friction * dt);

    // 位移
    float cur = g_offsetY.load(std::memory_order_relaxed);
    float newY = cur + v * dt;

    // 边界
    RECT rc;
    GetClientRect(g_hView, &rc);
    float h = float(rc.bottom - rc.top);
    newY = std::clamp(newY, -h / 2.0f, std::max(0.0f, g_vd->m_height - h));
    //OutputDebugStringA(std::to_string(newY).c_str());
    //OutputDebugStringA("\n");
    g_offsetY.store(newY, std::memory_order_relaxed);
    g_velocity.store(v, std::memory_order_relaxed);

    //ScrollWindowEx(g_hView, 0, v * dt, NULL, NULL, NULL, NULL, SW_INVALIDATE);

    InvalidateRect(g_hView, nullptr, FALSE);

    // 速度接近 0 时停止定时器
    if (std::fabs(v) < 0.1f) {
        g_velocity.store(0.0f);
        timeKillEvent(g_scrollTimer);
        g_scrollTimer = 0;
    }
}







void CALLBACK OnFlush(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    // 直接在工作线程/回调里刷新
    OutputDebugStringA("OnFlush\n");
    if (g_recorder) { g_recorder->flush(); }

    g_flushTimer = 0;
}

void CALLBACK Tick(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    // 直接在工作线程/回调里刷新
    if (g_recorder && g_recorder->m_book_record.id >= 0 && !g_vd->m_blocks.empty())
    {

        if (g_book)
        {

            g_recorder->m_book_record.enableCSS = g_cfg.enableCSS;
            g_recorder->m_book_record.enableGlobalCSS = g_cfg.enableGlobalCSS;
            g_recorder->m_book_record.enableCustomFont = g_cfg.enableCustomFont;
            g_recorder->m_book_record.fontName = g_cfg.font_name;


            g_recorder->m_book_record.fontSize = g_cfg.font_size;
            g_recorder->m_book_record.lineHeightMul = g_cfg.line_height;
            g_recorder->m_book_record.docWidth = g_cfg.document_width;
            g_recorder->m_book_record.totalTime += 1;



            g_recorder->m_book_record.title = g_book->get_title();


            g_recorder->m_book_record.author = g_book->get_author();


            timeFragment tf;
            tf.path = g_recorder->m_book_record.path;
            tf.title = g_recorder->m_book_record.title;
            tf.author = g_recorder->m_book_record.author;
            tf.timestamp = nowUs();

            if (g_vd) {
                ScrollPosition p = g_vd->get_scroll_position();
                g_recorder->m_book_record.lastSpineId = p.spine_id;
                g_recorder->m_book_record.lastOffset = p.offset;

                tf.spine_id = p.spine_id;
                tf.chapter = g_book->get_chapter_name_by_id(p.spine_id);
            }
            g_recorder->m_time_frag.push_back(std::move(tf));
            if (!g_flushTimer)
            {
                g_flushTimer = timeSetEvent(g_cfg.record_flush_interval_ms, 0, OnFlush, 0, TIME_ONESHOT);
            }


            g_recorder->m_setting_record.displayFrameRate = g_cfg.displayFrameRate;
            g_recorder->m_setting_record.displayScrollBar = g_cfg.displayScrollBar;
            g_recorder->m_setting_record.displayStatusBar = g_cfg.displayStatusBar;
            g_recorder->m_setting_record.displayTOC = g_cfg.displayTOC;
            g_recorder->m_setting_record.enableClickPreview = g_cfg.enableClickPreview;
            g_recorder->m_setting_record.enableFontRealtimePreview = g_cfg.enableFontRealtimePreview;
            g_recorder->m_setting_record.enableHoverPreview = g_cfg.enableHoverPreview;
            g_recorder->m_setting_record.enableLoadEPUBFonts = g_cfg.enableEPUBFonts;
            g_recorder->m_setting_record.enableScrollAnimation = g_cfg.enableScrollAnimation;
        }
        // OutputDebugStringA("定时器触发\n");

    }
    g_tickTimer = 0;
}


void CALLBACK OnUpdateTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    //OutputDebugStringA("OnUpdateTimer\n");
    g_updateTimer = 0;
}

 void DumpBookRecord()
{
    if (!g_recorder || g_recorder->m_book_record.id < 0) { return; }
    auto& r = g_recorder->m_book_record;
    std::wostringstream oss;            // ← 宽字符流
    oss << std::boolalpha;
    oss << L"===== BookRecord Dump =====\n";
    oss << L"id                 = " << r.id << L'\n';
    oss << L"path               = " << a2w(r.path) << L'\n';   // path 已经是 std::wstring 就 OK
    oss << L"title              = " << a2w(r.title) << L'\n';
    oss << L"author             = " << a2w(r.author) << L'\n';
    oss << L"openCount          = " << r.openCount << L'\n';
    oss << L"totalWords         = " << r.totalWords << L'\n';
    oss << L"lastSpineId        = " << r.lastSpineId << L'\n';
    oss << L"lastOffset         = " << r.lastOffset << L'\n';
    oss << L"fontSize           = " << r.fontSize << L'\n';
    oss << L"lineHeightMul      = " << std::fixed << std::setprecision(2) << r.lineHeightMul << L'\n';
    oss << L"docWidth           = " << r.docWidth << L'\n';
    oss << L"totalTime          = " << r.totalTime << L" (s)\n";
    oss << L"lastOpenTimestamp  = " << r.lastOpenTimestamp << L" (us)\n";
    oss << L"enableCSS          = " << r.enableCSS << L'\n';
    oss << L"enableGlobalCSS    = " << r.enableGlobalCSS << L'\n';
    oss << L"enableCustomFont      = " << r.enableCustomFont << L'\n';
    oss << L"customFontName        = " << a2w(r.fontName) << L'\n';
    oss << L"\nTotal Read Time (s): " << std::to_wstring(g_recorder->getTotalTime()) << L"\n";
    oss << L"============================\n";

    OutputDebugStringW(oss.str().c_str());   // ← 宽字符版本


    std::string txt = "--------------------------------------\n";
    txt += "Title: " + g_book->get_title() + "\n";
    txt += "Author: " + g_book->get_author() + "\n";
    txt += "EPUB Version: " + g_book->get_version() + "\n";
    txt += "Script: " + std::string(g_book->has_script() ? "Yes" : "No") + "\n";
    txt += "CSS: " + std::string(g_book->has_css() ? "Yes" : "No") + "\n";
    txt += "Font: " + std::string(g_book->has_font() ? "Yes" : "No") + "\n";
    txt += "--------------------------------------\n";
    OutputDebugStringW(a2w(txt).c_str());
}

bool IsMouseOverWindow(HWND hWnd) {
    POINT ptCursor;
    GetCursorPos(&ptCursor); // 获取鼠标的屏幕坐标

    RECT rectWindow;
    GetWindowRect(hWnd, &rectWindow); // 获取窗口的屏幕坐标矩形

    return PtInRect(&rectWindow, ptCursor);
}

//std::wstring seconds2string(int64_t sec)
//{
//    int64_t days = sec / 86400;
//    int64_t hours = (sec % 86400) / 3600;
//    int64_t minutes = (sec % 3600) / 60;
//    int64_t seconds = sec % 60;
//
//    std::wstring timeStr;
//    if (days)    timeStr += std::to_wstring(days) + L"天 ";
//    if (hours || days)   timeStr += std::to_wstring(hours) + L"时";
//    if (minutes || hours || days) timeStr += std::to_wstring(minutes) + L"分";
//    timeStr += std::to_wstring(seconds) + L"秒";
//    return timeStr;
//}

inline void SetStatus(int pane, const wchar_t* msg)
{
    if (!g_hStatus || !msg) return;

    // 1. 更新/插入片段
    if (msg && *msg)
        g_statusBuf[pane] = msg;
    else
        g_statusBuf.erase(pane);   // 空串时移除

    // 按 key 升序
    std::vector<std::pair<int, std::wstring>> vec(g_statusBuf.begin(),
        g_statusBuf.end());
    std::sort(vec.begin(), vec.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });


    // 2. 拼成一条字符串
    std::wstring text;
    for (const auto& kv : vec)
    {
        if (kv.first == 0) { continue; }
        text += kv.second + L"    ";
    }
    // 3. 一次性写到状态栏第 0 栏
    SendMessageW(g_hStatus, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
}


std::wstring OpenEpubWithDialog(HWND hwnd)
{
    wchar_t szFile[MAX_PATH] = { 0 };

    OPENFILENAME ofn = { 0 };               // 全部清零
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;                // 最好给 owner
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;

    // 过滤器：注意双 null 结尾
    const wchar_t* filter = L"EPUB 电子书\0*.epub\0所有文件\0*.*\0";
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;

    ofn.lpstrTitle = L"打开电子书";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        PostMessage(g_hWnd, WM_EPUB_OPEN, 0, (LPARAM)DupPath(szFile));
        //OutputDebugStringW(szFile);
    }
    return L"";
}


inline std::string blade16(std::string_view data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());

    std::array<uint8_t, 16> out;
    blake3_hasher_finalize(&hasher, out.data(), out.size());

    char hex[33];
    for (size_t i = 0; i < out.size(); ++i)
        std::sprintf(hex + i * 2, "%02x", out[i]);
    return std::string(hex, 32);   // 32 个十六进制字符
}


 void save_image(const ImageFrame& img, const std::filesystem::path& bmpPath)
{
    const int w = img.width;
    const int h = img.height;
    const int rowBytes = w * 4;
    const int imgSize = rowBytes * h;
    const int fileSize = 54 + imgSize;   // 54 = 14 + 40

    std::ofstream ofs(bmpPath, std::ios::binary);
    if (ofs)
    {
        // BITMAPFILEHEADER (14 bytes)
        uint16_t bfType = 0x4D42;        // 'BM'
        uint32_t bfSize = fileSize;
        uint32_t bfOffBits = 54;
        ofs.write(reinterpret_cast<const char*>(&bfType), 2);
        ofs.write(reinterpret_cast<const char*>(&bfSize), 4);
        ofs.seekp(4, std::ios::cur);     // skip reserved
        ofs.write(reinterpret_cast<const char*>(&bfOffBits), 4);

        // BITMAPINFOHEADER (40 bytes)
        uint32_t biSize = 40;
        int32_t  biWidth = w;
        int32_t  biHeight = -h;          // top-down
        uint16_t biPlanes = 1;
        uint16_t biBitCount = 32;
        uint32_t biCompression = 0;
        uint32_t biSizeImage = imgSize;
        ofs.write(reinterpret_cast<const char*>(&biSize), 4);
        ofs.write(reinterpret_cast<const char*>(&biWidth), 4);
        ofs.write(reinterpret_cast<const char*>(&biHeight), 4);
        ofs.write(reinterpret_cast<const char*>(&biPlanes), 2);
        ofs.write(reinterpret_cast<const char*>(&biBitCount), 2);
        ofs.write(reinterpret_cast<const char*>(&biCompression), 4);
        ofs.seekp(20, std::ios::cur);    // skip rest (zeros)

        // pixel data
        ofs.write(reinterpret_cast<const char*>(img.rgba.data()), imgSize);
    }
}


// ------------------------------------------------
// 主流程：SVG 内 <image> 零拷贝替换
// ------------------------------------------------

void replace_svg_with_img(std::string& html,
    const fs::path& tempDir)
{
    fs::create_directories(tempDir);

    /* ---------- 1. 解析 ---------- */
    GumboOutput* output = gumbo_parse(html.c_str());

    /* ---------- 2. 收集 <svg> 节点 ---------- */
    struct SvgNode {
        size_t start;   // <svg ...> 的起始偏移
        size_t end;     // </svg> 的结束偏移
    };
    std::vector<SvgNode> svgs;

    std::function<void(GumboNode*)> walk = [&](GumboNode* node)
    {
        if (node->type != GUMBO_NODE_ELEMENT) return;
        GumboElement& el = node->v.element;
        if (el.tag == GUMBO_TAG_SVG)
        {
            size_t start = el.start_pos.offset;
            size_t end = el.end_pos.offset + el.original_end_tag.length;
            svgs.push_back({ start, end });
        }
        for (unsigned i = 0; i < el.children.length; ++i)
            walk(static_cast<GumboNode*>(el.children.data[i]));
    };
    walk(output->root);

    /* ---------- 3. 从后往前替换 ---------- */

    for (auto it = svgs.rbegin(); it != svgs.rend(); ++it)
    {
        std::string svgBlock = html.substr(it->start, it->end - it->start);
        std::string hash = blade16(svgBlock);
        if (!g_cMain) continue;
        if (!g_cMain->isImageCached(hash))
        {

            // ------------------------------------------------
            std::regex imgRe("<(image|img)\\b[^>]*\\b(href|xlink:href)\\s*=\\s*\"([^\"]+)\"",
                std::regex::icase);
            std::string patchedSvg = svgBlock;
            std::smatch m;
            std::string::const_iterator search(patchedSvg.cbegin());

            while (std::regex_search(search, patchedSvg.cend(), m, imgRe))
            {
                std::string imgRel = m[3].str();          // zip 内路径


                auto mf = g_book->get_binary(g_book->get_current_dir(), imgRel);
                if (!mf.empty())
                {
                    // 1. 根据扩展名决定 MIME
                    fs::path p(imgRel);
                    std::string mime = "image/png";
                    if (p.extension() == ".jpg" || p.extension() == ".jpeg")
                        mime = "image/jpeg";

                    // 2. 编码 base64
                    std::string b64 = base64_encode(mf);

                    // 3. 生成 data URI
                    std::string dataUri = "data:" + mime + ";base64," + b64;

                    // 4. 替换 href
                    patchedSvg.replace(m.position(3), m.length(3), dataUri);
                    search = patchedSvg.cbegin() + m.position() + dataUri.size();
                }
                else
                {
                    // 读不到就保持原路径
                    search = m[0].second;
                }
            }

            g_cMain->addImageCache(hash, patchedSvg);
            g_cImage->m_img_cache[hash] = g_cMain->m_img_cache[hash];
            g_cTooltip->m_img_cache[hash] = g_cMain->m_img_cache[hash];

        }
        std::ostringstream imgTag;
        imgTag << R"(<img src=")" << hash << R"(")";
        imgTag << " display=\"block\" ";
        imgTag << " width=\"100%\" ";
        imgTag << " width=\"auto\" ";
        imgTag << " />";

        html.replace(it->start, it->end - it->start, imgTag.str());
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
}
// 替代 “duk_push_string_file”

//static std::string read_file(const char* path) {
//    FILE* fp = fopen(path, "rb");
//    if (!fp) return {};
//    fseek(fp, 0, SEEK_END);
//    size_t sz = ftell(fp);
//    fseek(fp, 0, SEEK_SET);
//    std::string buf(sz, '\0');
//    fread(buf.data(), 1, sz, fp);
//    fclose(fp);
//    return buf;
//}
//
//std::string tex_to_html(const std::string& tex, bool displayMode = false)
//{
//    JSRuntime* rt = JS_NewRuntime();
//    if (!rt) return {};
//    JSContext* ctx = JS_NewContext(rt);
//    if (!ctx) { JS_FreeRuntime(rt); return {}; }
//
//
//    /* 1. 注入 KaTeX 单文件 */
//    fs::path katex_path = exe_dir() / "config" / "katex" / "katex.min.js";
//    std::string katex_js = read_file(katex_path);
//    if (katex_js.empty()) { JS_FreeContext(ctx); JS_FreeRuntime(rt); return {}; }
//    JS_Eval(ctx, katex_js.c_str(), katex_js.size(), "<katex>", JS_EVAL_TYPE_GLOBAL);
//
//    /* 2. stub：把 TeX 转 HTML */
//
//    char stub[512];
//    snprintf(stub, sizeof(stub),
//        "katex.renderToString(tex, {"
//        "  displayMode: %s,"
//        "  throwOnError: false,"
//        "  output: 'html'"
//        "})",
//        displayMode ? "true" : "false");
//    /* 3. 把公式字符串放进全局变量 `tex` */
//    JSValue global = JS_GetGlobalObject(ctx);
//    JS_SetPropertyStr(ctx, global, "tex", JS_NewString(ctx, tex.c_str()));
//    JS_FreeValue(ctx, global);
//
//    /* 4. 执行 stub 并取结果 */
//    JSValue ret = JS_Eval(ctx, stub, strlen(stub), "<stub>", JS_EVAL_TYPE_GLOBAL);
//    std::string html;
//    if (JS_IsException(ret)) {
//        JSValue ex = JS_GetException(ctx);
//        const char* err = JS_ToCString(ctx, ex);
//        fprintf(stderr, "KaTeX error: %s\n", err ? err : "unknown");
//        JS_FreeCString(ctx, err);
//        JS_FreeValue(ctx, ex);
//    }
//    else {
//        const char* str = JS_ToCString(ctx, ret);
//        if (str) { html = str; JS_FreeCString(ctx, str); }
//    }
//
//    JS_FreeValue(ctx, ret);
//    JS_FreeContext(ctx);
//    JS_FreeRuntime(rt);
//    return html;
//}

/* ---------- 3. 主函数 ---------- */
//std::string replace_math_with_katex(const std::string& html)
//{
//    GumboOutput* output = gumbo_parse(html.c_str());
//
//    /* 收集所有 <math> 节点 */
//    struct MathNode {
//        GumboElement* el;
//        size_t start;
//        size_t end;
//    };
//    std::vector<MathNode> mathNodes;
//    std::function<void(GumboNode*)> walk = [&](GumboNode* node) {
//        if (node->type == GUMBO_NODE_ELEMENT) {
//            GumboElement& el = node->v.element;
//            if (el.tag == GUMBO_TAG_MATH) {
//                mathNodes.push_back({ &el,
//                                      el.start_pos.offset,
//                                      el.end_pos.offset + el.original_end_tag.length});
//            }
//            for (unsigned i = 0; i < el.children.length; ++i)
//                walk(static_cast<GumboNode*>(el.children.data[i]));
//        }
//        };
//    walk(output->root);
//
//    /* 从后往前替换，避免字节偏移失效 */
//    std::string patched = html;
//    for (auto it = mathNodes.rbegin(); it != mathNodes.rend(); ++it) {
//        /* 1. 取 MathML 原文 */
//        std::string mathml = patched.substr(it->start, it->end - it->start);
//
//        std::string tex = mathml2tex::convert(mathml);
//        
//        /* 3. LaTeX → KaTeX HTML */
//        std::string katexHtml = tex_to_html(tex, /*display=*/false);
//        OutputDebugStringA(katexHtml.c_str());
//        OutputDebugStringA("\n");
//        if (katexHtml.empty()) continue;
//
//        /* 4. 直接替换原 <math> 标签 */
//        patched.replace(it->start, it->end - it->start, katexHtml);
//    }
//
//    gumbo_destroy_output(&kGumboDefaultOptions, output);
//    return patched;
//}
//


void replace_math_with_svg(std::string& html) {
    GumboOutput* output = gumbo_parse(html.c_str());

    /* 收集所有 <math> 或 <m:math> 节点 */
    struct MathNode {
        GumboElement* el;
        size_t start;
        size_t end;
    };
    std::vector<MathNode> mathNodes;
    std::function<void(GumboNode*)> walk = [&](GumboNode* node) {
        if (node->type == GUMBO_NODE_ELEMENT) {
            GumboElement& el = node->v.element;

            // 新的检测逻辑
            bool isMathElement = false;

            // 方法1：检查原始标签名
            if (el.original_tag.data) {
                // 获取完整的原始标签名（如"math"或"m:math"）
                std::string originalTag(el.original_tag.data, el.original_tag.length);

                // 转换为小写统一比较
                std::transform(originalTag.begin(), originalTag.end(), originalTag.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                // 检查是否是math标签（支持带命名空间）
                size_t mathPos = originalTag.find("math");
                if (mathPos != std::string::npos) {
                    // 确保"math"是标签名的最后部分
                    if (mathPos + 4 == originalTag.length()) {
                        isMathElement = true;
                    }
                    // 或者前面是命名空间分隔符
                    else if (mathPos > 0 && originalTag[mathPos - 1] == ':') {
                        isMathElement = true;
                    }
                }
            }

            // 方法2：补充检查标准math标签
            if (!isMathElement && el.tag == GUMBO_TAG_MATH) {
                isMathElement = true;
            }

            if (isMathElement) {
                mathNodes.push_back({
                    &el,
                    el.start_pos.offset,
                    el.end_pos.offset + el.original_end_tag.length
                    });
            }

            // 递归处理子节点
            for (unsigned i = 0; i < el.children.length; ++i) {
                walk(static_cast<GumboNode*>(el.children.data[i]));
            }
        }
    };
    walk(output->root);

    /* 从后往前替换，避免字节偏移失效 */
    for (auto it = mathNodes.rbegin(); it != mathNodes.rend(); ++it) {
        /* 1. 取 MathML 原文 */
        std::string mathml = html.substr(it->start, it->end - it->start);

        // 处理带命名空间的标签（如 <m:math> → <math>）
        if (mathml.find("m:math") != std::string::npos) {
            boost::replace_all(mathml, "m:math", "math");
            boost::replace_all(mathml, "m:", ""); // 移除其他命名空间前缀（如 m:mrow）
        }

        size_t altimgPos = mathml.find("altimg=\"");
        if (altimgPos != std::string::npos) {
            // 提取 altimg 属性值
            size_t valueStart = altimgPos + 8; // 跳过 "altimg=\""
            size_t valueEnd = mathml.find('"', valueStart);
            if (valueEnd != std::string::npos) {
                std::string altimgSrc = mathml.substr(valueStart, valueEnd - valueStart);

                // 直接构建 img 标签
                std::string imgTag = R"(<img class="math-png" src=")" + altimgSrc + R"(" alt="math" />)";
                html.replace(it->start, it->end - it->start, imgTag);
                continue; // 跳过后续转换流程
            }
        }

        std::string hash = blade16(mathml);
        if (!g_cMain) continue;

        if (!g_cMain->isImageCached(hash)) {
            /* 2. LaTeX → KaTeX → SVG（你原来的逻辑） */
            MathML2SVG& m2s = MathML2SVG::instance();
            std::string svg = m2s.convert(mathml);
            if (svg.empty()) continue;

            g_cMain->addImageCache(hash, svg);
            g_cImage->m_img_cache[hash] = g_cMain->m_img_cache[hash];
            g_cTooltip->m_img_cache[hash] = g_cMain->m_img_cache[hash];
        }

        std::string imgTag = R"(<img class="math-png" src=")" + hash + R"(" alt="math" />)";

        /* 7. 替换原 <math> 标签 */
        html.replace(it->start, it->end - it->start, imgTag);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);
}





inline HtmlFeatureFlags detect_html_features(const std::string& html) noexcept {
    HtmlFeatureFlags f;
    if (html.empty()) return f;

    // 统一转换为小写（仅需一次）
    std::string lower_html;
    lower_html.reserve(html.size());
    for (char c : html) {
        lower_html.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // 直接搜索子字符串（不关心标签完整性）
    f.has_svg = (lower_html.find("<svg") != std::string::npos);
    f.has_math = (lower_html.find("<math") != std::string::npos) ||
        (lower_html.find("<m:math") != std::string::npos);
    f.has_script = (lower_html.find("<script") != std::string::npos);

    return f;
}



void PreprocessHTML(std::string& html)
{

    auto flags = detect_html_features(html);
    if (flags.has_math) replace_math_with_svg(html);

    html = std::regex_replace(
        html,
        std::regex(R"(<([a-zA-Z][a-zA-Z0-9]*)\b([^>]*?)/\s*>)", std::regex::icase),
        "<$1$2></$1>");

    if (flags.has_script)preprocess_js(html);


    if (flags.has_svg)
    {
        std::string dir = make_temp_dir();
        replace_svg_with_img(html, dir);
    }

}

void preprocess_js(std::string& html)
{
    if (!g_cfg.enableJS) {
        // 1) 删除 <script ...>...</script>
        static const std::regex reScriptPair(
            R"(<\s*script\b[^>]*>.*?</script>)",
            std::regex::icase | std::regex::optimize | std::regex::nosubs);

        // 2) 删除自闭合 <script ... />
        static const std::regex reScriptSelf(
            R"(<\s*script\b[^>]*\/>)",
            std::regex::icase | std::regex::optimize | std::regex::nosubs);

        html = std::regex_replace(html, reScriptPair, "");
        html = std::regex_replace(html, reScriptSelf, "");
    }
    else
    {
        std::regex  scRe(R"(<script\b([^>]*)\bsrc\s*=\s*["']([^"']*)["']([^>]*)/\s*>)",
            std::regex::icase);
        std::string out;
        out.reserve(html.size());

        std::sregex_iterator it(html.begin(), html.end(), scRe);
        std::sregex_iterator end;
        size_t last = 0;

        for (; it != end; ++it)
        {
            const std::smatch& m = *it;

            // 2.1 读文件
            std::string src = m[2].str();

            auto mf = g_book->get_binary(g_book->get_current_dir(), src);
            std::string code;
            if (!mf.empty())
                code.assign(reinterpret_cast<const char*>(mf.data()),
                    mf.size());

            // 2.2 去掉 src 属性
            std::string attrs = m[1].str() + m[3].str();
            attrs = std::regex_replace(attrs,
                std::regex(R"(\s*\bsrc\s*=\s*["'][^"']*["'])", std::regex::icase), "");

            // 2.3 拼成对标签
            out.append(html, last, m.position() - last);
            out += "<script" + attrs + ">" + code + "</script>";
            last = m.position() + m.length();
        }
        out.append(html, last, std::string::npos);
        html.swap(out);
    }
}



inline std::string base64_encode(const std::vector<uint8_t>& in) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(B64[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(B64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

 std::vector<unsigned char> base64_decode(const std::string& in)
{
    static const int tbl[256] = {
        /* 略：把 base64 字符映射到 0-63，非法字符为 -1 */
    };
    std::vector<unsigned char> out;
    /* 标准 Base64 解码实现，略 */
    return out;
}


void LogPtrint(std::string txt)
{
    std::cout << txt;
}





void DumpHex(const wchar_t* tag, const std::wstring& s)
{
    std::wostringstream oss;
    oss << tag << L"(" << s.size() << L"): ";
    for (wchar_t ch : s)
        oss << std::hex << std::setw(4) << std::setfill(L'0') << static_cast<unsigned>(ch) << L" ";
    oss << L"\n";
    OutputDebugStringW(oss.str().c_str());
}

// HTML 转义辅助函数
std::string _escape_html(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
        case '&':  oss << "&amp;";  break;
        case '<':  oss << "&lt;";   break;
        case '>':  oss << "&gt;";   break;
        case '"':  oss << "&quot;"; break;
        case '\'': oss << "&apos;"; break;
        default:   oss << c;       break;
        }
    }
    return oss.str();
}



// 转换为小写函数
std::string to_lower(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return lower_str;
}

std::string generate_html(litehtml::element::ptr elem) {
    if (!elem) return "";

    std::ostringstream oss;

    // 处理文本节点
    if (elem->is_text()) {
        std::string text;
        elem->get_text(text);
        if (!text.empty()) { // 只有当有文本内容时才输出
            oss << _escape_html(text);
        }
    }
    // 处理普通元素
    else {
        const char* tag_name = elem->get_tagName();
        if (!tag_name) return "";

        // 输出开始标签
        oss << "<" << tag_name;

        //// 使用 dump_get_attrs 获取所有属性
        //auto attrs = elem->dump_get_attrs();
        //for (const auto& attr : attrs) {
        //    oss << " " << std::get<0>(attr) << "=\""
        //        << _escape_html(std::get<1>(attr)) << "\"";
        //}
        // 修改后的属性处理
        std::vector<const char*> standardAttrs = {
            "id", "class", "name", "value", "type", "src", "href"
        };

        for (const char* name : standardAttrs) {
            const char* value = elem->get_attr(name);
            if (value && value[0] != '\0') {
                oss << " " << name << "=\"" << _escape_html(value) << "\"";
            }
        }

        // 处理自闭合标签
        std::string tag_str = to_lower(tag_name);
        bool is_void = (void_tags.find(tag_str) != void_tags.end());

        if (is_void) {
            oss << "/>";
        }
        else {
            oss << ">";

            // 递归处理子元素
            const auto& children = elem->children();
            for (const auto& child : children) {
                oss << generate_html(child);
            }

            // 输出闭合标签
            oss << "</" << tag_name << ">";
        }
    }

    return oss.str();
}
 void gumbo_serialize(const GumboNode* node, std::string& out)
{
    if (node->type == GUMBO_NODE_TEXT)
    {
        out.append(node->v.text.text);
        return;
    }
    if (node->type != GUMBO_NODE_ELEMENT) return;

    const GumboElement& elem = node->v.element;
    out.push_back('<');
    out.append(gumbo_normalized_tagname(elem.tag));

    // 属性
    for (unsigned int i = 0; i < elem.attributes.length; ++i)
    {
        auto* attr = static_cast<GumboAttribute*>(elem.attributes.data[i]);
        out.append(" ").append(attr->name).append("=\"")
            .append(attr->value).append("\"");
    }

    if (elem.tag == GUMBO_TAG_IMG || elem.tag == GUMBO_TAG_BR)
    {
        // 自闭合
        out.append(" />");
    }
    else
    {
        out.push_back('>');
        for (unsigned int i = 0; i < elem.children.length; ++i)
            gumbo_serialize(static_cast<GumboNode*>(elem.children.data[i]), out);
        out.append("</").append(gumbo_normalized_tagname(elem.tag)).push_back('>');
    }
}



// 完整的文档导出函数
std::string get_document_html(litehtml::document::ptr doc) {
    if (!doc) return "";

    // 添加文档类型声明
    std::ostringstream oss;
    oss << "<!DOCTYPE html>";

    // 从根元素开始生成
    if (doc->root()) {
        oss << generate_html(doc->root());
    }

    return oss.str();
}

void save_document_html(litehtml::document::ptr doc) {
    std::string modified_html = get_document_html(doc);

    // 输出到文件
    std::ofstream out("output.html");
    if (out.is_open()) {
        out << modified_html;
        out.close();
        std::cout << "HTML 导出成功" << std::endl;
    }
    else {
        std::cerr << "无法创建输出文件" << std::endl;
    }
}
 bool ends_with(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

 bool is_image_url(const char* url)
{
    if (!url) return false;
    std::string u = url;
    return ends_with(u, ".jpg") ||
        ends_with(u, ".png") ||
        ends_with(u, ".jpeg") ||
        ends_with(u, ".gif");
}