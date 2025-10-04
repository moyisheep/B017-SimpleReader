#include "main.h"

HWND  g_hToc = nullptr;    // 侧边栏 TreeView
HIMAGELIST g_hImg = nullptr;   // 图标(可选)
HWND      g_hWnd = nullptr;
HWND g_hStatus = nullptr;   // 状态栏句柄
HWND g_hView = nullptr;
HWND g_hTooltip = nullptr;
HWND g_hImageview = nullptr;
HWND g_hViewScroll = nullptr;
HWND g_hHomepage = nullptr;
// ---------- 全局 ----------
HINSTANCE g_hInst;
std::shared_ptr<SimpleContainer> g_cMain;
std::shared_ptr<SimpleContainer> g_cTooltip;
std::shared_ptr<SimpleContainer> g_cImage;
std::shared_ptr<SimpleContainer> g_cHome;

std::shared_ptr<EPUBBook>  g_book;

std::future<void> g_parse_task;

std::unique_ptr<VirtualDoc> g_vd;
//static float g_scrollY = 0.0f;   // 当前像素偏移
static std::atomic<float> g_offsetY{ 0.0f };
std::vector<FontItem> g_fontList;


constexpr UINT WM_EPUB_PARSED = WM_APP + 1;
constexpr UINT WM_EPUB_UPDATE_SCROLLBAR = WM_APP + 2;

constexpr UINT WM_EPUB_CACHE_UPDATED = WM_APP + 4;
constexpr UINT WM_EPUB_ANCHOR = WM_APP + 5;
constexpr UINT WM_EPUB_TOOLTIP = WM_APP + 6;
constexpr UINT WM_EPUB_NAVIGATE = WM_APP + 7;
constexpr UINT TB_SETBUTTONTEXT(WM_USER + 8);
constexpr UINT WM_LOAD_ERROR(WM_USER + 9);
constexpr UINT WM_USER_SCROLL(WM_USER + 10);
constexpr UINT SBM_SETSPINECOUNT(WM_USER + 11);
constexpr UINT SBM_SETPOSITION(WM_USER + 12);
constexpr UINT WM_EPUB_OPEN(WM_USER + 13);


// 设置为0时不显示
constexpr UINT STATUSBAR_INFO = 1;
constexpr UINT STATUSBAR_SPINE_INFO = 2;
constexpr UINT STATUSBAR_OFFSET_INFO = 3;
constexpr UINT STATUSBAR_TOTAL_TIME = 4;
constexpr UINT STATUSBAR_FONT_NAME = 5;
constexpr UINT STATUSBAR_FONT_SIZE = 6;
constexpr UINT STATUSBAR_LINE_HEIGHT = 7;
constexpr UINT STATUSBAR_DOC_WIDTH = 8;
constexpr UINT STATUSBAR_DOC_ZOOM = 9;
constexpr UINT STATUSBAR_FRAME_RATE = 10;

// 可随时改
static UINT g_frame_count = 0;



AppStates g_states;
AppSettings g_cfg;



void PreprocessHTML(std::string& html);
void UpdateCache(void);


std::unique_ptr<AppBootstrap> g_bootstrap;
std::unique_ptr<ReadingRecorder> g_recorder;

static MMRESULT g_tickTimer = 0;   // 0 表示当前没有定时器
static MMRESULT g_flushTimer = 0;

static MMRESULT g_updateTimer = 0;

static MMRESULT g_scrollTimer = 0;
static MMRESULT g_framerateTimer = 0;
std::atomic<float> g_velocity{ 0 };     // 像素/秒


int g_center_offset = 0;

std::string g_globalCSS = "";

static int   g_splitX = 200;       // 当前 TOC 宽度（初始值）
static bool  g_dragging = false;     // 是否正在拖动
static bool  g_imageview_dragging = false;
static POINT g_imageview_drag_pos{ 0,0 };
static bool g_mouse_tracked = false;


static int g_imageviewRenderW = 0;
// 全局
std::unique_ptr<TocPanel> g_toc;
std::unique_ptr<ScrollBarEx> g_scrollbar;

//std::vector<std::wstring> g_fontNames;       // 保存字体名
// 1. 在全局或合适位置声明
    // 整篇文档的所有行
static int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string documents_dir()
{
    PWSTR pszPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pszPath)))
    {
        fs::path path(pszPath);
        CoTaskMemFree(pszPath);
        return path.generic_string();
    }
    return ""; // 或抛出异常
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

// 工具：把路径拷到堆，返回指针
inline wchar_t* DupPath(const wchar_t* src)
{
    size_t len = wcslen(src) + 1;
    wchar_t* buf = (wchar_t*)CoTaskMemAlloc(len * sizeof(wchar_t));
    wcscpy_s(buf, len, src);
    return buf;
}

// HTML 转义辅助函数
inline bool save_rgba_as_bmp(const std::wstring& path,
    const uint8_t* rgba,
    int width,
    int height)
{
    if (!rgba || width <= 0 || height <= 0) return false;

    const int rowBytes = width * 4;
    const int imageSize = rowBytes * height;
    const int fileSize = sizeof(BmpHeader) + sizeof(BmpInfo) + imageSize;

    BmpHeader hdr;
    hdr.bfSize = fileSize;

    BmpInfo info;
    info.biWidth = width;
    info.biHeight = -height;  // 负值 ⇒ 顶-下像素顺序（与 RGBA 顺序一致）

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    ofs.write(reinterpret_cast<const char*>(&info), sizeof(info));
    ofs.write(reinterpret_cast<const char*>(rgba), imageSize);
    return !!ofs;
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

// 自闭合标签集合
static const std::set<std::string> void_tags = {
    "area", "base", "br", "col", "embed", "hr", "img",
    "input", "keygen", "link", "meta", "param", "source",
    "track", "wbr"
};

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
static void gumbo_serialize(const GumboNode* node, std::string& out)
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
// 生成临时目录，返回路径（带反斜杠）
static std::wstring make_temp_dir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + g_cfg.temp_dir + L"\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
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
static bool ends_with(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool is_image_url(const char* url)
{
    if (!url) return false;
    std::string u = url;
    return ends_with(u, ".jpg") ||
        ends_with(u, ".png") ||
        ends_with(u, ".jpeg") ||
        ends_with(u, ".gif");
}








void DumpAllFontNames()
{
    HDC hdc = GetDC(nullptr);
    LOGFONTW lf{ 0 };
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf,
        [](const LOGFONTW* lpelfe, const TEXTMETRICW*, DWORD, LPARAM) -> int {
            OutputDebugStringW((L"[Enum] " + std::wstring(lpelfe->lfFaceName) + L"\n").c_str());
            return 1;
        }, 0, 0);
    ReleaseDC(nullptr, hdc);
}

// 真正读文件






static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

static std::vector<unsigned char> base64_decode(const std::string& in)
{
    static const int tbl[256] = {
        /* 略：把 base64 字符映射到 0-63，非法字符为 -1 */
    };
    std::vector<unsigned char> out;
    /* 标准 Base64 解码实现，略 */
    return out;
}




// 读取 ./config/global.css

static fs::path exe_dir()
{
    wchar_t buf[1024]{};
    GetModuleFileNameW(nullptr, buf, 1024);
    return fs::path(buf).parent_path();

}


// 工具：把文件内容读成字符串
static std::string read_file(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}


// 1. 仅内嵌 global.css
static std::string get_global_css()
{
    fs::path file = exe_dir() / "res" / "global.css";
    if (!fs::exists(file)) { return ""; }
    std::string css = read_file(file);
    return css;

}

static void inject_global_css(std::string& html)
{
    fs::path file = exe_dir() / "res" / "global.css";

    std::string css = read_file(file);
    if (css.empty()) return;

    std::ostringstream style;
    style << "<style>\n" << css << "</style>\n";

    const std::string& block = style.str();
    size_t pos = html.find("</head>");
    if (pos != std::string::npos)
        html.insert(pos, block);
    else
        html.insert(0, "<head>" + block + "</head>");

}

static void inject_css(std::string& html)
{
    std::ostringstream style;
    style << "<style>\n"
        << ":root,body,p,li,div,h1,h2,h3,h4,h5,h6,span, ul{line-height:" << g_cfg.line_height << ";}\n"
        << "</style>\n";

    const std::string& block = style.str();
    size_t pos = html.find("</head>");
    if (pos != std::string::npos)
        html.insert(pos, block);
    else
        html.insert(0, "<head>" + block + "</head>");

}

void EnableClearType()
{
    BOOL ct = FALSE;
    SystemParametersInfoW(SPI_GETCLEARTYPE, 0, &ct, 0);
    if (!ct)
        SystemParametersInfoW(SPI_SETCLEARTYPE, TRUE, 0, SPIF_UPDATEINIFILE);
}

// int -> wstring，保存不同片段
static std::unordered_map<int, std::wstring> g_statusBuf;

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


INT_PTR CALLBACK FontDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        HWND hList = GetDlgItem(hDlg, IDC_LIST_FONT);
        for (size_t i = 0; i < g_fontList.size(); ++i)
        {
            const FontItem& fi = g_fontList[i];
            int pos = (int)SendMessage(hList, LB_ADDSTRING, 0,
                (LPARAM)fi.displayName.c_str());
            // 把索引 i 存进去
            SendMessage(hList, LB_SETITEMDATA, pos, (LPARAM)i);
        }
        SendMessage(hList, LB_SETCURSEL, 0, 0);

        // 设置"启用实时预览"复选框的初始状态
        HWND hRealtimePreviewCheck = GetDlgItem(hDlg, IDM_TOGGLE_FONT_REALTIME_PREVIEW);
        SendMessage(hRealtimePreviewCheck, BM_SETCHECK,
            g_cfg.enableFontRealtimePreview ? BST_CHECKED : BST_UNCHECKED, 0);
        return TRUE;
    }
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hDlg, &rc);

        // 用白色填充整个客户区
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        return TRUE; // 表示我们已经处理了背景擦除
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wp;
        HWND hwndStatic = (HWND)lp;
        UINT ctrlId = GetDlgCtrlID(hwndStatic);

        // 处理复选框背景
        if (ctrlId == IDM_TOGGLE_CUSTOM_FONT ||
            ctrlId == IDM_TOGGLE_FONT_REALTIME_PREVIEW)
        {
            SetBkMode(hdcStatic, TRANSPARENT);
            SetTextColor(hdcStatic, RGB(0, 0, 0)); // 黑色文本

            // 使用淡蓝色背景 (RGB: 240, 248, 255 - AliceBlue)
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
            return (INT_PTR)hBrush; // 注意: Windows会自动删除这个画刷
        }
        return FALSE;
    }


    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_LIST_FONT:   // 来自列表框的消息
            switch (HIWORD(wp))
            {
            case LBN_SELCHANGE:   // 单击改变选择
            case LBN_DBLCLK:      // 双击
            {
                if (!g_cfg.enableFontRealtimePreview) { break; }
                HWND hList = (HWND)lp;
                int pos = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (pos != LB_ERR)
                {
                    size_t idx = (size_t)SendMessage(hList, LB_GETITEMDATA, pos, 0);
                    g_cfg.font_name = g_fontList[idx].familyName;   // 立即保存
                    if (g_vd) { g_vd->reload(); }
                }
                return TRUE;
            }
            }
            break;

        case IDM_TOGGLE_FONT_REALTIME_PREVIEW:
            g_cfg.enableFontRealtimePreview = !g_cfg.enableFontRealtimePreview;          // 切换状态
            break;
        case IDOK:
        {
            HWND hList = GetDlgItem(hDlg, IDC_LIST_FONT);
            int pos = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (pos != LB_ERR)
            {
                size_t idx = (size_t)SendMessage(hList, LB_GETITEMDATA, pos, 0);

                if (idx < g_fontList.size())
                {
                    g_cfg.font_name = g_fontList[idx].familyName;   // 立即保存
                    if (g_vd) { g_vd->reload(); }
                    EndDialog(hDlg, static_cast<INT_PTR>(idx + 1)); // 任意非 0
                    return TRUE;
                }


            }

            EndDialog(hDlg, 0);

    
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}




void CALLBACK OnFrameRateTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    if (!g_cfg.displayFrameRate) { timeKillEvent(g_framerateTimer); g_framerateTimer = 0; }
    std::wstring txt = L"帧率：" + std::to_wstring(g_frame_count);
    SetStatus(STATUSBAR_FRAME_RATE, txt.c_str());
    g_frame_count = 0;

}

//void CALLBACK OnScrollTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
//{
//
//    float dt = 0.001f;
//
//
//    // 物理惯性：v = v * e^(-k*dt)
//    const float friction = 8.0f;   // 越大越快停
//    float v = g_velocity.load(std::memory_order_relaxed) * std::exp(-friction * dt);
//
//    // 位移
//    float cur = g_offsetY.load(std::memory_order_relaxed);
//    float newY = cur + v * dt;
//
//    // 边界
//    RECT rc;
//    GetClientRect(g_hView, &rc);
//    float h = float(rc.bottom - rc.top);
//    newY = std::clamp(newY, -h/2.0f, std::max(0.0f, g_vd->m_height - h));
//    //OutputDebugStringA(std::to_string(newY).c_str());
//    //OutputDebugStringA("\n");
//    g_offsetY.store(newY, std::memory_order_relaxed);
//    g_velocity.store(v, std::memory_order_relaxed);
//
//    InvalidateRect(g_hView, nullptr, FALSE);
//
//    // 速度接近 0 时停止定时器
//    if (std::fabs(v) < 0.1f) {
//        g_velocity.store(0.0f);
//        timeKillEvent(g_scrollTimer);
//        g_scrollTimer = 0;
//    }
//}




void convert_coordinate(POINT& pt)
{
    if(g_cMain)
    {
        pt.x = pt.x/ g_cMain->m_zoom_factor - g_center_offset;
        pt.y = pt.y/g_cMain->m_zoom_factor + g_offsetY.load(std::memory_order_relaxed); ;
    }

}

void CALLBACK OnFlush(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    //// 直接在工作线程/回调里刷新
    OutputDebugStringA("OnFlush\n");
    if (g_recorder) { g_recorder->flush(); }
    g_flushTimer = 0;
}

void CALLBACK Tick(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    // 直接在工作线程/回调里刷新
    if (g_recorder) 
    { 
        auto& bookRecord = g_recorder->getBookRecord();
        auto& settingRecord = g_recorder->getSettingRecord();
        if (bookRecord.id < 0) { return; }

        if (g_book)
        {

            bookRecord.enableCSS = g_cfg.enableCSS;
            bookRecord.enableGlobalCSS = g_cfg.enableGlobalCSS;
            bookRecord.enableCustomFont = g_cfg.enableCustomFont;
            bookRecord.fontName = w2a(g_cfg.font_name);


            bookRecord.fontSize = g_cfg.font_size;
            bookRecord.lineHeightMul = g_cfg.line_height;
            bookRecord.docWidth = g_cfg.document_width;
            bookRecord.totalTime += 1;


            if (bookRecord.title.empty() && g_book && !g_book->ocf_pkg_.meta.empty())
            {
                auto titIt = g_book->ocf_pkg_.meta.find(L"dc:title");
                bookRecord.title = titIt != g_book->ocf_pkg_.meta.end() ? w2a(titIt->second) : "";
            }
            if (bookRecord.author.empty() && g_book && !g_book->ocf_pkg_.meta.empty())
            {
                auto authIt = g_book->ocf_pkg_.meta.find(L"dc:creator");
                bookRecord.author = authIt != g_book->ocf_pkg_.meta.end() ? w2a(authIt->second) : "";
            }

            TimeFragment tf;
            tf.path = bookRecord.path;
            tf.title = bookRecord.title;
            tf.author = bookRecord.author;
            tf.timestamp = nowUs();

            if (g_vd) {
                ScrollPosition p = g_vd->get_scroll_position();
                bookRecord.lastSpineId = p.spine_id;
                bookRecord.lastOffset = p.offset;

                tf.spine_id = p.spine_id;
                tf.chapter = w2a(g_book->get_chapter_name_by_id(p.spine_id));
            }
            g_recorder->pushTimeFrag(std::move(tf));


            settingRecord.displayFrameRate = g_cfg.displayFrameRate;
            settingRecord.displayScrollBar = g_cfg.displayScrollBar;
            settingRecord.displayStatusBar = g_cfg.displayStatusBar;
            settingRecord.displayTOC = g_cfg.displayTOC;
            settingRecord.enableClickPreview = g_cfg.enableClickPreview;
            settingRecord.enableFontRealtimePreview = g_cfg.enableFontRealtimePreview;
            settingRecord.enableHoverPreview = g_cfg.enableHoverPreview;
            settingRecord.enableLoadEPUBFonts = g_cfg.enableEPUBFonts;
            settingRecord.enableScrollAnimation = g_cfg.enableScrollAnimation;
        }
    }
   // OutputDebugStringA("定时器触发\n");
    if (g_recorder && !g_flushTimer)
    {
        g_flushTimer = timeSetEvent(g_cfg.record_flush_interval_ms, 0, OnFlush, 0, TIME_ONESHOT);
    }
    g_tickTimer = 0;
}
LRESULT CALLBACK ViewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        if ( g_cMain)
        {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);   // ← 这才是客户区
            g_cMain->resize(rcClient.right, rcClient.bottom);
            //UpdateCache();
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //if (!g_cMain || !g_cMain->m_doc) { return 0; }

        //POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //g_cMain->on_lbutton_down(pt.x/g_cMain->m_zoom_factor, pt.y/g_cMain->m_zoom_factor);
        //convert_coordinate(pt);
        //litehtml::position::vector redraw_boxes;
        //g_cMain->m_doc->on_lbutton_down(pt.x, pt.y, 0, 0, redraw_boxes);
        //if (!redraw_boxes.empty()) {
        //    InvalidateRect(hwnd, nullptr, false);
        //}
        if (g_vd) { g_vd->on_lbutton_down(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }

        //if (g_cMain && g_cMain->m_doc)
        //{
        //    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //    g_cMain->on_lbutton_up();
        //    convert_coordinate(pt);

        //    litehtml::position::vector redraw_boxes;
        //    g_cMain->m_doc->on_lbutton_up(pt.x, pt.y, 0, 0, redraw_boxes);
        //    if (!redraw_boxes.empty()) {
        //        InvalidateRect(hwnd, nullptr, false);
        //    }
   
        //}
        if (g_vd) { g_vd->on_lbutton_up(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
    {
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //if (g_cMain)
        //{
        //    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //    g_cMain->on_lbutton_dblclk(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        //    InvalidateRect(hwnd, nullptr, false);
        //}
        if (g_vd) { g_vd->on_lbutton_dblclk(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //if (g_cMain && g_cMain->m_doc)
        //{
        //    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        //    g_cMain->on_mouse_move(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
        //    convert_coordinate(pt);



        //    litehtml::position::vector redraw_boxes;
        //    g_cMain->m_doc->on_mouse_over(pt.x, pt.y, 0, 0, redraw_boxes);
        //    if (!redraw_boxes.empty()) {
        //        InvalidateRect(hwnd, nullptr, false);
        //    }
      
        //}
        if (g_vd) { g_vd->on_mouse_move(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;
    }


    case WM_EPUB_ANCHOR:
    {
        // 更新阅读记录
        if (!g_tickTimer) 
        { 
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);  
        }
 
        if (!g_cMain->m_doc) { return 0; }
        wchar_t* sel = reinterpret_cast<wchar_t*>(wp);
        if (sel) {
            std::string cssSel = "[id=\"" + w2a(sel) + "\"]";
            if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
                g_offsetY.store(el->get_placement().y, std::memory_order_relaxed);
            }
            free(sel);          // 对应 _wcsdup
        }
        //UpdateCache();
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(g_hView);

        return 0;
    }
    case WM_EPUB_NAVIGATE:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }

        wchar_t* url = reinterpret_cast<wchar_t*>(wp);
        g_vd->OnTreeSelChanged(url);  // 现在安全地在主线程执行
        free(url);


        return 0;
    }

    case WM_MOUSELEAVE:
    {
        //if(g_cMain && g_cMain->m_doc)
        //{
        //    litehtml::position::vector redraw_boxes;
        //    g_cMain->m_doc->on_mouse_leave(redraw_boxes);
        //    if (!redraw_boxes.empty()) {
        //        InvalidateRect(hwnd, nullptr, false);
        //    }
        //}
        if (g_vd) { g_vd->on_mouse_leave(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }

        return 0;
    }
    case WM_MBUTTONDOWN:  // 鼠标中键按下
    {
        // 检测Ctrl键是否按下
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            // Ctrl+中键被按下
            g_cMain->m_zoom_factor = 1.0f;
           // UpdateCache();
            // 3. 重绘
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;  // 已处理该消息
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        //if (g_cMain) { g_cMain->clear_selection(); }
        //if (GetKeyState(VK_CONTROL) & 0x8000)
        //{
        //    int delta = GET_WHEEL_DELTA_WPARAM(wp);   // ±120
        //    float factor = (delta > 0) ? 1.1f : 0.9f;     // 放大 / 缩小系数

        //    // 2. 更新全局缩放
        //    g_cMain->m_zoom_factor = std::clamp(g_cMain->m_zoom_factor * factor, 0.25f, 5.0f);
        //    UpdateCache();
        //    // 3. 重绘
        //    InvalidateRect(hwnd, NULL, FALSE);
        //
        //    return 0;   // 已处理，不再传递
        //}
 
        if (g_vd) { g_vd->on_mouse_wheel(GET_WHEEL_DELTA_WPARAM(wp)); }
        //RECT rc;
        //GetClientRect(hwnd, &rc);
        //float h = float(rc.bottom - rc.top);

        //int zDelta = GET_WHEEL_DELTA_WPARAM(wp);
        //// 每格 3 行 → 每行像素 * 3
        //float pxPerLine = g_cfg.font_size * g_cfg.line_height ;
        //float pxDelta = -zDelta / 120.0f * pxPerLine * 3.0f;   // 负号：上滚为负

        //if(g_cfg.enableScrollAnimation)
        //{
        //    // 累加速度，而不是直接改目标
        //    g_velocity.fetch_add(pxDelta * 12.0f, std::memory_order_relaxed);

        //    // 启动 1 kHz 高精度定时器
        //    if (g_scrollTimer == 0) {
        //        g_scrollTimer = timeSetEvent(1, 0, OnScrollTimer, 0, TIME_PERIODIC);
        //    }
        //}
        //else
        //{
        //    float cur = g_offsetY.load(std::memory_order_relaxed);
        //    cur = std::clamp(cur + pxDelta, -h/2.0f, std::max(g_vd->m_height - h / 2.0f, 0.0f));
        //    g_offsetY.store(cur, std::memory_order_relaxed);
        //}

        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        //litehtml::position::vector redraw_box;
        //if (g_cMain && g_cMain->m_doc)
        //{
        //    g_cMain->m_doc->on_mouse_leave(redraw_box);

        //}
        //UpdateCache();
        //InvalidateRect(hwnd, nullptr, FALSE);

        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);

        //if (g_cMain  && g_cMain->m_doc )
        //{
        //    g_frame_count += 1;
        //    OutputDebugStringA("[View] WM_PAINT\n");
        //    RECT rc;
        //    GetClientRect(g_hView, &rc);
        //    int x = g_center_offset;
        //    int y = -g_offsetY.load(std::memory_order_relaxed);
        //    float w = g_cfg.document_width;
        //    float h = rc.bottom - rc.top;
        //    litehtml::position clip(x, 0, w, h/g_cMain->m_zoom_factor);
        //    g_cMain->present(x, y, &clip);

        //}
        RECT rc;
        GetClientRect(g_hView, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        if (g_vd && width > 0 && height > 0)
        {

            g_vd->present(width, height);

        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}



void CALLBACK OnUpdateTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    //OutputDebugStringA("OnUpdateTimer\n");
    g_updateTimer = 0;
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

   // g_vd->update_doc(h, g_offsetY.load(std::memory_order_relaxed));
 

    g_center_offset = (w - g_cfg.document_width) * 0.5f;
}

inline void DumpBookRecord()
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
    oss << L"============================\n";

    OutputDebugStringW(oss.str().c_str());   // ← 宽字符版本
    OutputDebugStringW((L"\nTotal Read Time (s): " + std::to_wstring(g_recorder->getTotalTime()) + L"\n").c_str());
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
            OutputDebugStringA("[ImageView] WM_PAINT\n");
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
   
        InvalidateRect(hwnd, nullptr, false);

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
        g_book->hide_imageview();
        return 0;
    }
    case WM_ERASEBKGND: {
        return 1;
    }
    }
    return DefWindowProc(hwnd, m, w, l);
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
            OutputDebugStringA("[Tooltip] WM_PAINT\n");
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
        return 1;
    }
    return DefWindowProc(hwnd, m, w, l);
}
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
            OutputDebugStringA("[Homepage] WM_PAINT\n");
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
const wchar_t TOOLTIP_CLASS[] = L"TooltipClass";

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

const wchar_t VIEW_CLASS[] = L"ViewClass";
void register_view_class()
{
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;   // 关键
    wc.lpfnWndProc = ViewWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = VIEW_CLASS;
    RegisterClassW(&wc);
}


const wchar_t IMAGEVIEW_CLASS[] = L"Imageview";

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

const wchar_t SCROLLBAR_CLASS[] = L"ScrollBarEx";

void register_scrollbar_class()
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = ScrollBarEx::WndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = SCROLLBAR_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
}


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

const wchar_t HOMEPAGE_CLASS[] = L"HomepageClass";
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

#include <Windows.h>
#include <vector>
#include <string>

std::vector<std::wstring> GetSystemFonts() {
    std::vector<std::wstring> fonts;
    HDC hdc = GetDC(NULL);
    LOGFONT lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET;

    EnumFontFamiliesExW(
        hdc, &lf,
        [](const LOGFONT* lpelfe, const TEXTMETRIC*, DWORD, LPARAM lParam) {
            auto& fonts = *reinterpret_cast<std::vector<std::wstring>*>(lParam);
            fonts.push_back(lpelfe->lfFaceName);
            return 1;
        },
        reinterpret_cast<LPARAM>(&fonts), 0
    );

    ReleaseDC(NULL, hdc);
    return fonts;
}

// 显示自定义字体选择对话框
std::wstring ShowSimpleFontDialog(HWND hParent) {
    std::vector<std::wstring> fonts = GetSystemFonts();

    // 创建 ComboBox 窗口
    HWND hCombo = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        10, 10, 300, 200, hParent, NULL, NULL, NULL
    );

    // 填充字体列表
    for (const auto& font : fonts) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font.c_str()));
    }

    // 显示模态对话框
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 获取用户选择的字体
    WCHAR selectedFont[LF_FACESIZE] = { 0 };
    SendMessageW(hCombo, CB_GETLBTEXT, SendMessageW(hCombo, CB_GETCURSEL, 0, 0), reinterpret_cast<LPARAM>(selectedFont));

    DestroyWindow(hCombo);
    return selectedFont;
}
void ChooseFontWithDialog(HWND hwnd)
{
    // 1. 准备一个 LOGFONTW 结构体
    LOGFONTW lf = { 0 };
    wcscpy_s(lf.lfFaceName, g_cfg.font_name.c_str());   // 默认字体
    lf.lfHeight = -g_cfg.font_size;                      // 16 像素高

    // 2. 填充 CHOOSEFONT
    CHOOSEFONTW cf = { 0 };
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hwnd;               // 没有父窗口
    cf.lpLogFont = &lf;                   // 输入/输出字体
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    // 3. 弹出对话框
    if (ChooseFontW(&cf))
    {

        HFONT hFont = CreateFontIndirectW(&lf);
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);  // 选入设备上下文
        TCHAR fontName[LF_FACESIZE];
        GetTextFace(hdc, LF_FACESIZE, fontName);
        SelectObject(hdc, oldFont);  // 恢复旧字体
        ReleaseDC(NULL, hdc);
        g_cfg.font_name = fontName;

    }
    else
    {
        DWORD err = CommDlgExtendedError();
        if (err)
            std::wcout << L"ChooseFont 失败，错误码: " << err << L"\n";
        else
            std::wcout << L"用户取消\n";
    }
    return;
}
// ---------- 窗口 ----------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {

        register_view_class();

        register_tooltip_class();
        register_imageview_class();
        register_scrollbar_class();
        register_toc_class();
        register_homepage_class();
        // 放在主窗口 CreateWindow 之后
        g_hStatus = CreateWindowEx(
            0, STATUSCLASSNAME, L"就绪",
            WS_CHILD | SBARS_SIZEGRIP,
            0, 0, 0, 0,           // 位置和大小由 WM_SIZE 调整
            hwnd, nullptr, g_hInst, nullptr);

        // 2. 创建
        g_hToc = CreateWindowExW(
            WS_EX_COMPOSITED ,          // 双缓冲
            TOC_CLASS,                 // 用注册的类名
            nullptr,
            WS_CHILD   | WS_BORDER,
            0, 0, 200, 600,
            hwnd, (HMENU)100, g_hInst, nullptr);



        g_hView = CreateWindowExW(
            0, VIEW_CLASS, nullptr,
            WS_CHILD | WS_CLIPSIBLINGS,
            0, 0, 1, 1,
            hwnd, (HMENU)101, g_hInst, nullptr);

        g_hTooltip = CreateWindowExW(
            WS_EX_COMPOSITED,
            TOOLTIP_CLASS, nullptr,
            WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_BORDER,
            0, 0, 300, 200,
            hwnd, nullptr, g_hInst, nullptr);


        g_hImageview = CreateWindowExW(
            WS_EX_COMPOSITED,
            IMAGEVIEW_CLASS, nullptr,
            WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN | WS_BORDER,
            0, 0, 300, 200,
            hwnd, nullptr, g_hInst, nullptr);


        g_hViewScroll = CreateWindowExW(0, SCROLLBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0, hwnd, nullptr,
            g_hInst, nullptr);

        g_hHomepage = CreateWindowExW(0, HOMEPAGE_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0, hwnd, nullptr,
            g_hInst, nullptr);

        g_bootstrap = std::make_unique<AppBootstrap>();
  
        // =====初始化隐藏=====


        DragAcceptFiles(hwnd, TRUE);

        SendMessage(g_hWnd, WM_SIZE, 0, 0);
     
        SetForegroundWindow(hwnd);          // 关键：把输入焦点抢过来
        return 0;
    }
    case WM_DROPFILES:
    {
        wchar_t file[MAX_PATH]{};
        DragQueryFileW((HDROP)wp, 0, file, MAX_PATH);
        DragFinish((HDROP)wp);
        PostMessage(hwnd, WM_EPUB_OPEN, 0, (LPARAM)DupPath(file));
        return 0;
    }
 
    case WM_COPYDATA:
    {
        PCOPYDATASTRUCT p = (PCOPYDATASTRUCT)lp;
        if (p->dwData == WM_EPUB_OPEN && p->lpData)
        {
            PostMessage(hwnd, WM_EPUB_OPEN, 0,
                (LPARAM)DupPath((const wchar_t*)p->lpData));
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        //RECT rc; GetClientRect(hwnd, &rc);
        //float page = rc.bottom - rc.top;
        //float line = g_cMain->m_line_height;

        //float delta = 0.f;
        //switch (wp)
        //{
        //case VK_UP:     delta = -line;  break;
        //case VK_DOWN:   delta = line;  break;
        //case VK_PRIOR:  delta = -page;  break;
        //case VK_NEXT:   delta = page;  break;
        //default:        return DefWindowProc(hwnd, msg, wp, lp);
        //}

        //// 原子读-改-写
        //float old = g_offsetY.load(std::memory_order_relaxed);
        //float desired;
        //do {
        //    desired = std::clamp(old + delta,
        //        -1.0f,
        //        std::max(0.0f, g_vd->m_height - page));
        //} while (!g_offsetY.compare_exchange_weak(old, desired,
        //    std::memory_order_relaxed,
        //    std::memory_order_relaxed));

        //UpdateCache();
        //InvalidateRect(g_hView, nullptr, FALSE);
        return 0;
    }

    case WM_CONTEXTMENU:
    {
        HWND hwndFrom = (HWND)wp;
        if (hwndFrom == g_hView || hwndFrom == g_hToc)
        {
            // 1. 取出屏幕坐标
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);

            // 2. 如果是由键盘触发（x == -1 && y == -1），
            //    把菜单放到窗口左上角
            if (x == -1 && y == -1)
            {
                RECT rc;
                GetWindowRect(hwndFrom, &rc);
                x = rc.left + 10;
                y = rc.top + 10;
            }

            // 3. 弹出菜单
            HMENU hPopup = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_POPUP));
            HMENU hSub = GetSubMenu(hPopup, 0);
            CheckMenuItem(hSub, IDM_TOGGLE_TOC_WINDOW,
                g_cfg.displayTOC ? MF_CHECKED : MF_UNCHECKED);
            EnableMenuItem(hSub, ID_CHOOSE_FONT, g_cfg.enableCustomFont?MF_ENABLED:MF_DISABLED);

            TrackPopupMenuEx(
                hSub,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON ,
                x, y,
                g_hWnd,
                nullptr
            );

            DestroyMenu(hPopup);
        }
        break;
    }
    // ---------- WM_EPUB_OPEN ----------
    case WM_EPUB_OPEN:
    {
        const wchar_t* file = (const wchar_t*)lp;
        const wchar_t* ext = wcsrchr(file, L'.');
        if (!ext || _wcsicmp(ext, L".epub") != 0)
        {
            SetStatus(STATUSBAR_INFO, L"不是有效的 epub 文件");
            OutputDebugStringW(L"不是有效的 epub 文件\n");
            CoTaskMemFree((void*)file);   // 释放堆拷贝
            return 0;
        }

        // 如果上一次任务还在跑，直接忽略（或加入队列）
        if (g_parse_task.valid() &&
            g_parse_task.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            SetStatus(STATUSBAR_INFO, L"正在加载其他文件，请稍候…");
            OutputDebugStringW(L"正在加载其他文件，请稍候…\n");
            CoTaskMemFree((void*)file);
            return 0;
        }

        // 清理旧数据

        g_cMain->clear();
        g_cImage->clear();
        g_cTooltip->clear();
        g_book->clear();
        g_toc->clear();
        g_vd->clear();
        InvalidateRect(g_hView, nullptr, TRUE);
        InvalidateRect(g_hToc, nullptr, TRUE);

        // 启动新任务
        SetStatus(STATUSBAR_INFO, L"正在加载...");
        g_parse_task = std::async(std::launch::async, [file] {
            try
            {
                g_book->load(file);
                PostMessage(g_hWnd, WM_EPUB_PARSED, 0, 0);
            }
            catch (const std::exception& e)
            {
                auto* buf = DupPath(a2w(e.what()).c_str());
                PostMessage(g_hWnd, WM_LOAD_ERROR, 0, (LPARAM)buf);
            }
            catch (...)
            {
                PostMessage(g_hWnd, WM_LOAD_ERROR, 0,
                    (LPARAM)DupPath(L"未知错误"));
            }
            CoTaskMemFree((void*)file);   // 任务结束释放路径
            });
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_EPUB_PARSED: {

        if (!g_states.isLoaded) {
            g_states.isLoaded = true;
        }
        if(!g_framerateTimer && g_cfg.displayFrameRate)
        {
            g_framerateTimer = timeSetEvent(1000, 0, OnFrameRateTimer, 0, TIME_PERIODIC);
        }

        std::string book_path = w2a(g_book->get_book_path());
        g_vd->clear();
        g_recorder->flush();
        g_recorder->openBook(book_path);
   
        DumpBookRecord();

        // 更新设置
        auto& record = g_recorder->m_book_record;
        auto spine_id = record.lastSpineId;


        
       //g_offsetY.store(record.lastOffset, std::memory_order_relaxed) ;
        g_cfg.font_size = record.fontSize > 0 ? record.fontSize:g_cfg.default_font_size;
        g_cfg.line_height = record.lineHeightMul > 0 ? record.lineHeightMul : g_cfg.default_line_height;
        g_vd->set_document_width(record.docWidth > 0 ? record.docWidth : g_cfg.default_document_width);
    
        int spine_size = g_book->ocf_pkg_.spine.size();
        SendMessage(g_hViewScroll, SBM_SETSPINECOUNT, spine_size, 0);

        g_cfg.enableCSS = record.enableCSS;
        CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CSS,
            MF_BYCOMMAND | (g_cfg.enableCSS ? MF_CHECKED : MF_UNCHECKED));
        
        g_cfg.enableGlobalCSS = record.enableGlobalCSS;
        CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_GLOBAL_CSS,
            MF_BYCOMMAND | (g_cfg.enableGlobalCSS ? MF_CHECKED : MF_UNCHECKED));

        g_cfg.enableCustomFont = record.enableCustomFont;
        CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CUSTOM_FONT,
            MF_BYCOMMAND | (g_cfg.enableCustomFont ? MF_CHECKED : MF_UNCHECKED));

        g_cfg.font_name = a2w(record.fontName);
        g_vd->load_book(g_book, g_cMain, g_hView);
        g_vd->load_html(g_book->ocf_pkg_.spine[spine_id].href);
        ScrollPosition sp{};
        sp.spine_id = spine_id;
        sp.offset = record.lastOffset;
        g_vd->set_scroll_position(sp);

  
        std::string title;
        auto t = g_book->get_title();
        if (t.empty()) { t = fs::path(book_path).filename().generic_string(); }
        if (!t.empty()) { title += t + " - "; }
        auto a = g_book->get_author();
        if (!a.empty()) { title += a + " - "; }
        title += g_cfg.appName;
        SetWindowTextW(g_hWnd, a2w(title).c_str());
 
  
        //UpdateCache();          // 复用前面给出的 UpdateCache()

        
        PostMessage(hwnd, WM_SIZE, 0, 0);
        InvalidateRect(g_hView, nullptr, true);
        SetStatus(STATUSBAR_INFO, L"加载完成");
        SetForegroundWindow(hwnd);          // 关键：把输入焦点抢过来
        return 0;
    }


    case WM_SIZE:
    {
      
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        const int cx = rcClient.right;
        int cyClient = rcClient.bottom;
        if (g_states.isLoaded)
        {

            ShowWindow(g_hHomepage, SW_HIDE);
            ShowWindow(g_hImageview, SW_HIDE);
            ShowWindow(g_hTooltip, SW_HIDE);
 
            /* 1. 工具栏高度 */
            int cyTB = 0;

            /* 2. 状态栏高度 */
            int cySB = 0;
            if (g_cfg.displayStatusBar && g_hStatus)
            {
                ShowWindow(g_hStatus, SW_SHOW);
                SendMessage(g_hStatus, WM_SIZE, 0, 0);
                RECT rcSB{};
                GetWindowRect(g_hStatus, &rcSB);
                cySB = rcSB.bottom - rcSB.top;
            }
            else if (g_hStatus)
            {
                ShowWindow(g_hStatus, SW_HIDE);
            }

            /* 3. 剩余可用高度 */
            const int cy = cyClient - cyTB - cySB;

            /* 4. 目录宽度 & 竖线 */


            // 最终宽度：显示 TOC 时取 g_splitX 与 idealTocW 的较大值
            const int tocW = g_cfg.displayTOC ? g_splitX : 0;
            ShowWindow(g_hToc, g_cfg.displayTOC ? SW_SHOW : SW_HIDE);


            /* 5. 滚动条宽度 */
            const int sbW = g_cfg.displayScrollBar ? 20 : 0;
            ShowWindow(g_hViewScroll, g_cfg.displayScrollBar ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hView,  SW_SHOW );
            /* 6. 摆放子窗口（Y 起点统一为 cyTB） */

        /* 5. 用 SetWindowPos 摆位置，禁止立即重绘、禁止改 Z 序 */
            SetWindowPos(g_hToc, NULL, 0, cyTB, tocW, cy,
                SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
  
            SetWindowPos(g_hView, NULL, tocW , cyTB, cx - tocW - sbW, cy,
                SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(g_hViewScroll, NULL, cx - sbW, cyTB, sbW, cy,
                SWP_NOREDRAW | SWP_NOZORDER | SWP_NOACTIVATE);

            /* 6. 统一重绘整个父窗口及其子窗口 */
            RedrawWindow(hwnd, NULL, NULL,
                RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        else
        {

            ShowWindow(g_hStatus, SW_HIDE);

            ShowWindow(g_hToc, SW_HIDE);
            ShowWindow(g_hViewScroll, SW_HIDE);
            ShowWindow(g_hView, SW_HIDE);
            ShowWindow(g_hImageview, SW_HIDE);
            ShowWindow(g_hTooltip, SW_HIDE);



            ShowWindow(g_hHomepage, SW_SHOW);
            MoveWindow(g_hHomepage, 0, 0, cx, cyClient, TRUE);

        }

        return 0;
    }
    case WM_LOAD_ERROR:
    {
        wchar_t* msg = (wchar_t*)lp;
        SetStatus(STATUSBAR_INFO, msg);
        OutputDebugStringW(msg);
        OutputDebugStringW(L"\n");
        CoTaskMemFree(msg);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (LOWORD(lp) >= g_splitX && LOWORD(lp) <= g_splitX + 2)
        {
            SetCapture(hwnd); g_dragging = true;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (g_dragging)
        {
            int x = LOWORD(lp);
            if (x < 50) x = 50; if (x > 400) x = 400;
            g_splitX = x;
            PostMessage(hwnd, WM_SIZE, 0, 0);
        }
        break;
    }
    case WM_LBUTTONUP:
    {
        if (g_dragging) { ReleaseCapture(); g_dragging = false; }
        break;
    }
    case WM_SETCURSOR:
    {
        if (LOWORD(lp) == HTCLIENT)
        {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            if (pt.x >= g_splitX && pt.x <= g_splitX + 2)
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE)); return TRUE;
            }
        }
        if (HWND(wp) == g_hView)   // 确保是客户区
        {
            if (g_cMain)
            {
                SetCursor(LoadCursor(nullptr, g_cMain->m_currentCursor));
                return TRUE;                  // 已设置光标
            }
        }
        break;
    }
    case WM_EPUB_CACHE_UPDATED:
    {
        //if (g_vd->m_isReloading.exchange(false)) {
        //    g_offsetY.store(g_vd->m_percent * g_vd->m_height,
        //        std::memory_order_relaxed);
        //}

        //float delta = static_cast<float>(lp);
        //// 原子 += delta
        //float old = g_offsetY.load(std::memory_order_relaxed);
        //float desired;
        //do {
        //    desired = old + delta;
        //} while (!g_offsetY.compare_exchange_weak(old, desired,
        //    std::memory_order_relaxed,
        //    std::memory_order_relaxed));

        //g_cMain->m_doc = std::move(g_vd->m_doc);

        //if (g_vd->m_isAnchor.exchange(false)) {
        //    std::string cssSel = "[id=\"" + g_vd->m_anchor_id + "\"]";
        //    if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
        //        g_offsetY.store(el->get_placement().y,
        //            std::memory_order_relaxed);
        //    }
        //}

        //UpdateCache();
        //InvalidateRect(g_hView, nullptr, FALSE);
        //return 0;
    }
    case WM_DESTROY: {
        timeKillEvent(g_framerateTimer);
        timeKillEvent(g_scrollTimer);
        g_recorder->flush();

        PostQuitMessage(0);
        return 0;
    }


    case WM_COMMAND: {
        switch (LOWORD(wp))
        {
        case IDM_TOGGLE_CSS:
            g_cfg.enableCSS = !g_cfg.enableCSS;
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CSS,
                MF_BYCOMMAND | (g_cfg.enableCSS ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_JS:
            g_cfg.enableJS = !g_cfg.enableJS;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_JS,
                MF_BYCOMMAND | (g_cfg.enableJS ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_GLOBAL_CSS:
            g_cfg.enableGlobalCSS = !g_cfg.enableGlobalCSS;          // 切换状态
            g_cfg.enableGlobalCSS ? g_globalCSS = get_global_css() : g_globalCSS = "";
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_GLOBAL_CSS,
                MF_BYCOMMAND | (g_cfg.enableGlobalCSS ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_EPUB_FONTS:
            g_cfg.enableEPUBFonts = !g_cfg.enableEPUBFonts;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_EPUB_FONTS,
                MF_BYCOMMAND | (g_cfg.enableEPUBFonts ? MF_CHECKED : MF_UNCHECKED));
            if (g_cMain) { g_cMain->clear_font_cache(); }
            //if (g_book && g_book->m_fontBin.empty()) {  g_book->build_epub_font_index(); }
            if (g_vd) { g_vd->reload(); }
            break;

        case IDM_TOGGLE_HOVER_PREVIEW:
            g_cfg.enableHoverPreview = !g_cfg.enableHoverPreview;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_HOVER_PREVIEW,
                MF_BYCOMMAND | (g_cfg.enableHoverPreview ? MF_CHECKED : MF_UNCHECKED));
            break;
        case IDM_TOGGLE_CLICK_PREVIEW:
            g_cfg.enableClickPreview = !g_cfg.enableClickPreview;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CLICK_PREVIEW,
                MF_BYCOMMAND | (g_cfg.enableClickPreview ? MF_CHECKED : MF_UNCHECKED));
            break;
        case IDM_TOGGLE_CUSTOM_FONT:
            g_cfg.enableCustomFont = !g_cfg.enableCustomFont;          // 切换状态
            EnableMenuItem(GetMenu(g_hWnd), ID_CHOOSE_FONT, g_cfg.enableCustomFont?MF_ENABLED:MF_DISABLED);
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_CUSTOM_FONT,
                MF_BYCOMMAND | (g_cfg.enableCustomFont ? MF_CHECKED : MF_UNCHECKED));
            if (g_vd) { g_vd->reload(); }
            break;
        case IDM_TOGGLE_SCROLL_ANIMATION:
            g_cfg.enableScrollAnimation = !g_cfg.enableScrollAnimation;          // 切换状态
            timeKillEvent(g_scrollTimer);
            g_scrollTimer = 0;
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_SCROLL_ANIMATION,
                MF_BYCOMMAND | (g_cfg.enableScrollAnimation ? MF_CHECKED : MF_UNCHECKED));
            break;

        case IDM_TOGGLE_FRAME_RATE:
            g_cfg.displayFrameRate = !g_cfg.displayFrameRate;          // 切换状态
            if (g_cfg.displayFrameRate)
            {
                timeKillEvent(g_framerateTimer);
                g_framerateTimer = timeSetEvent(1000, 0, OnFrameRateTimer, 0, TIME_PERIODIC);
            }
            else
            {
                g_statusBuf.clear();
                timeKillEvent(g_framerateTimer);
                g_framerateTimer = 0;
            }

            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_FRAME_RATE,
                MF_BYCOMMAND | (g_cfg.displayFrameRate ? MF_CHECKED : MF_UNCHECKED));
            break;
        case IDM_TOGGLE_TOC_WINDOW:

            g_cfg.displayTOC = !g_cfg.displayTOC;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_TOC_WINDOW,
                MF_BYCOMMAND | (g_cfg.displayTOC ? MF_CHECKED : MF_UNCHECKED));

            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            break;
        case IDM_TOGGLE_SCROLLBAR_WINDOW:
        {
            g_cfg.displayScrollBar = !g_cfg.displayScrollBar;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_SCROLLBAR_WINDOW,
                MF_BYCOMMAND | (g_cfg.displayScrollBar ? MF_CHECKED : MF_UNCHECKED));

            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            break;
        }
        case IDM_TOGGLE_STATUS_WINDOW:

            g_cfg.displayStatusBar = !g_cfg.displayStatusBar;          // 切换状态
            CheckMenuItem(GetMenu(g_hWnd), IDM_TOGGLE_STATUS_WINDOW,
                MF_BYCOMMAND | (g_cfg.displayStatusBar ? MF_CHECKED : MF_UNCHECKED));


            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            break;

        case ID_EPUB_RELOAD:

            if (g_vd && g_states.isLoaded) { g_vd->reload(); }
            break;
        case ID_CHOOSE_FONT:
        {
            auto original = g_cfg.font_name;
            int idx = (int)DialogBoxParam(g_hInst,
                MAKEINTRESOURCE(IDD_FONTDLG),
                hwnd,
                FontDlgProc,
                0);

            if (idx <= 0)
            {
                g_cfg.font_name = original;
                if (g_vd) { g_vd->reload(); }
            }
        
            break;
        }
        case ID_FONT_BIGGER:        // Ctrl + '+'
            g_cfg.font_size = std::min(g_cfg.font_size + 1.0f, 72.0f);   // 上限 72
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_vd) { g_vd->reload(); }   // 重新加载并排版
            break;

        case ID_FONT_SMALLER:       // Ctrl + '-'
            g_cfg.font_size = std::max(g_cfg.font_size - 1.0f, 8.0f);    // 下限 8
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_FONT_RESET:         // Ctrl + '0'
            g_cfg.font_size = g_cfg.default_font_size;   // 默认字号
            if (g_cMain) { g_cMain->clear_font_cache(); }
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_LINE_HEIGHT_UP:     // Ctrl + Shift + '+'
            g_cfg.line_height = std::min(g_cfg.line_height + 0.1f, 3.0f);
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_LINE_HEIGHT_DOWN:   // Ctrl + Shift + '-'
            g_cfg.line_height = std::max(g_cfg.line_height - 0.1f, 1.0f);
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_LINE_HEIGHT_RESET:  // Ctrl + Shift + '0'
            g_cfg.line_height = g_cfg.default_line_height;  // 默认行高
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_WIDTH_BIGGER:       // Alt + '→'
            g_cfg.document_width = std::min(g_cfg.document_width + 50.0f, 2000.0f);
            if (g_vd) { g_vd->reload(); }   // 仅重新排版即可
            break;

        case ID_WIDTH_SMALLER:      // Alt + '←'
            g_cfg.document_width = std::max(g_cfg.document_width - 50.0f, 300.0f);
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_WIDTH_RESET:        // Alt + '0'
            g_cfg.document_width = g_cfg.default_document_width;   // 默认宽度
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_EDIT_COPY:
        {
             g_cMain->copy_to_clipboard(); 
            break;
        }
        case ID_FILE_OPEN:
        {
            OpenEpubWithDialog(hwnd);   // 就是之前那段 IFileOpenDialog 代码
            break;
        }
        case ID_FILE_EXIT:
            PostQuitMessage(0);
            break;
        case ID_ZOOM_BIGGER:
            g_cMain->m_zoom_factor = std::min(g_cMain->m_zoom_factor + 0.1f,  5.0f);
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_ZOOM_SMALLER:
            g_cMain->m_zoom_factor = std::max(g_cMain->m_zoom_factor - 0.1f, 0.25f);
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_ZOOM_RESET:
            g_cMain->m_zoom_factor = 1.0f;
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_RESET_ALL:
            g_cMain->m_zoom_factor = 1.0f;
            g_cfg.font_name = g_cfg.default_font_name;
            g_cfg.font_size = g_cfg.default_font_size;
            g_cfg.document_width = g_cfg.default_document_width;
            g_cfg.line_height = g_cfg.default_line_height;
            g_cfg.displayFrameRate = true;
            g_cfg.displayScrollBar = true;
            g_cfg.displayStatusBar = true;
            g_cfg.displayTOC = true;
            g_cfg.enableClickPreview = true;
            g_cfg.enableCSS = true;
            g_cfg.enableCustomFont = false;
            g_cfg.enableEPUBFonts = true;
            g_cfg.enableFontRealtimePreview = true;
            g_cfg.enableGlobalCSS = true;
            g_cfg.enableHoverPreview = true;
            g_cfg.enableScrollAnimation = false;
            CheckAllMenuItem();
            PostMessage(g_hWnd, WM_SIZE, 0, 0);
            if (g_vd) { g_vd->reload(); }
            break;
        }
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);

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
const wchar_t MAIN_CLASS[] = L"SimpleEPUBReader";
void register_main_class()
{
    WNDCLASSEX w{ sizeof(WNDCLASSEX) };
    w.style = CS_HREDRAW | CS_VREDRAW ;   // 关键
    w.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MY_APPLICATION));
    w.hIconSm = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MY_APPLICATION)); // 小图标
    w.lpfnWndProc = WndProc;
    w.hInstance = g_hInst;
    w.hCursor = LoadCursor(nullptr, IDC_ARROW);
    w.lpszClassName = MAIN_CLASS;
    w.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    w.lpszMenuName = nullptr;   // ← 必须为空
    RegisterClassEx(&w);
}

//void LogToFile(const std::string& message)
//{
//    fs::path debug_path = documents_dir() / g_cfg.appName / "debug_log.txt";
//   
//    std::ofstream log(debug_path, std::ios::app);
//    if (log.is_open())
//    {
//        log << message << std::endl;
//    }
//}



// ---------- 入口 ----------
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int n)
{
 

    // ---------- 1. 解析命令行 ----------
    int argc = 0;
    std::wstring cmd = GetCommandLineW();
    //LogToFile(w2a(L"[cmdline] " + cmd + L"\n"));
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
   
    //调试
    std::wstring txt = L"[argc] " + std::to_wstring(argc) + L"\n";
    txt += L"[argv]\n";
    for (int i = 0; i < argc; i++) { txt = txt  + argv[i] + L"\n"; }
    txt += L"\n";
    //LogToFile(w2a(txt));

    wchar_t* firstFile = nullptr;
    if (argc > 1)
        firstFile = DupPath(argv[1]);   // 堆拷贝
    LocalFree(argv);

    // ---------- 2. 单例检测 ----------
    CreateMutex(nullptr, TRUE, L"SimpleEPUBReader_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        //LogToFile("[firstFile]\n");
        if (firstFile)
        {
            HWND hPrev = FindWindow(MAIN_CLASS, nullptr);
            if (hPrev)
            {
                COPYDATASTRUCT cds{};
                cds.dwData = WM_EPUB_OPEN;
                cds.cbData = (DWORD)(wcslen(firstFile) + 1) * sizeof(wchar_t);
                cds.lpData = firstFile;          // 指向堆
                SendMessage(hPrev, WM_COPYDATA, 0, (LPARAM)&cds);
                // SendMessage 同步返回后即可释放
            }
        }
        CoTaskMemFree(firstFile);   // 无论发没发成功都要释放
        return 0;
    }
 
    ULONG_PTR gdiplusToken{};
    GdiplusStartupInput gdiplusStartupInput{};
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    g_hInst = h;
    InitCommonControls();
    register_main_class();

    // 在 CreateWindow 之前
    HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU_MAIN));

    if (!hMenu) {
        OutputDebugStringW(L"LoadMenu 失败\n");
        MessageBox(nullptr, L"LoadMenu 失败", L"Error", MB_ICONERROR);
        return 0;
    }

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        SetProcessDPIAware();


    g_hWnd = CreateWindowW(MAIN_CLASS, a2w(g_cfg.appName).c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, 0, 800, 600,
        nullptr, nullptr, h, nullptr);
 



    // 1. 在全局或合适位置声明
    AccelManager gAccel(g_hWnd);

    // 2. 在 WinMain 里，窗口创建完成后立刻注册
    // 书签栏
    gAccel.set(IDM_TOGGLE_TOC_WINDOW, FVIRTKEY, VK_TAB);   // 无修饰符的 Tab
    
    // 刷新
    gAccel.set(ID_EPUB_RELOAD, FVIRTKEY, VK_F5);    // 新增 F5

    // 字体
    gAccel.add(ID_FONT_BIGGER, FCONTROL | FVIRTKEY, VK_OEM_PLUS);
    gAccel.add(ID_FONT_BIGGER, FCONTROL | FVIRTKEY, VK_ADD);
    gAccel.add(ID_FONT_SMALLER, FCONTROL | FVIRTKEY, VK_OEM_MINUS);
    gAccel.add(ID_FONT_SMALLER, FCONTROL | FVIRTKEY, VK_SUBTRACT);
    gAccel.add(ID_FONT_RESET, FCONTROL | FVIRTKEY, VK_BACK);


    // 行高
    gAccel.add(ID_LINE_HEIGHT_UP, FCONTROL | FSHIFT | FVIRTKEY, VK_OEM_PLUS);
    gAccel.add(ID_LINE_HEIGHT_UP, FCONTROL | FSHIFT | FVIRTKEY, VK_ADD);
    gAccel.add(ID_LINE_HEIGHT_DOWN, FCONTROL | FSHIFT | FVIRTKEY, VK_OEM_MINUS);
    gAccel.add(ID_LINE_HEIGHT_DOWN, FCONTROL | FSHIFT | FVIRTKEY, VK_SUBTRACT);
    gAccel.add(ID_LINE_HEIGHT_RESET, FCONTROL | FSHIFT | FVIRTKEY, VK_BACK);


    // 文档宽度
    gAccel.add(ID_WIDTH_BIGGER, FALT | FVIRTKEY, VK_OEM_PLUS);
    gAccel.add(ID_WIDTH_BIGGER, FALT | FVIRTKEY, VK_ADD);
    gAccel.add(ID_WIDTH_SMALLER, FALT | FVIRTKEY, VK_SUBTRACT);
    gAccel.add(ID_WIDTH_SMALLER, FALT | FVIRTKEY, VK_OEM_MINUS);
    gAccel.add(ID_WIDTH_RESET, FALT | FVIRTKEY, VK_BACK);
  
    // 新增：复制文本（Ctrl + C）
    gAccel.add(ID_EDIT_COPY, FCONTROL | FVIRTKEY, 'C');
    gAccel.add(ID_FILE_OPEN, FCONTROL | FVIRTKEY, 'O');


    //// 加载图标（可缩放，支持 32/48/256 像素）
    //fs::path icoPath = exe_dir() / "res" / "app.ico";
    //HICON hIcon = (HICON)LoadImageW(nullptr, icoPath.c_str(),
    //    IMAGE_ICON,
    //    0, 0,               // 0,0 = 使用图标内最佳尺寸
    //    LR_LOADFROMFILE | LR_DEFAULTSIZE);

    //if (hIcon)
    //{
    //    // 设置窗口图标
    //    SendMessageW(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    //    SendMessageW(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    //}




    SetMenu(g_hWnd, hMenu);            // ← 放在 CreateWindow 之后

    CheckAllMenuItem();

    EnableMenuItem(hMenu, IDM_TOGGLE_MENUBAR_WINDOW, MF_BYCOMMAND | MF_GRAYED);

    EnableClearType();
  

    // ====================
    ShowWindow(g_hWnd, n);
    UpdateWindow(g_hWnd);

    // ---------- 4. 首次启动时如有文件立即加载 ----------
    if (firstFile && fs::exists(firstFile))
        PostMessage(g_hWnd, WM_EPUB_OPEN, 0, (LPARAM)firstFile);
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!gAccel.translate(&msg)) {   // ← 先给 AccelManager
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    //::CoUninitialize();
    GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}

// ---------- 目录解析 ----------



// SimpleContainer.cpp



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
  
            MemFile mf = g_book->get_binary(g_book->get_current_dir(), a2w(src));
            std::string code;
            if (!mf.data.empty())
                code.assign(reinterpret_cast<const char*>(mf.data.data()),
                    mf.data.size());

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

namespace fs = std::filesystem;

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

static std::wstring blake3_hex(const std::vector<uint8_t>& data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());

    std::array<uint8_t, BLAKE3_OUT_LEN> hash;          // 32 字节
    blake3_hasher_finalize(&hasher, hash.data(), hash.size());

    std::wostringstream oss;
    for (uint8_t b : hash)
        oss << std::hex << std::setw(2) << std::setfill(L'0') << (b & 0xFF);
    return oss.str();                                  // 64 个十六进制字符
}
static void save_image(const ImageFrame& img, const std::filesystem::path& bmpPath)
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
                std::wstring wRel = a2w(imgRel);

                MemFile mf = g_book->get_binary(g_book->get_current_dir(), wRel);
                if (!mf.data.empty())
                {
                    // 1. 根据扩展名决定 MIME
                    fs::path p(imgRel);
                    std::string mime = "image/png";
                    if (p.extension() == ".jpg" || p.extension() == ".jpeg")
                        mime = "image/jpeg";

                    // 2. 编码 base64
                    std::string b64 = base64_encode(mf.data);

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
//
//// --------------------------------------------------
//// 通用 HTML 预处理
//// --------------------------------------------------
//void replace_math_with_svg(std::string& html)
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
//    //std::string patched = html;
//    for (auto it = mathNodes.rbegin(); it != mathNodes.rend(); ++it) {
//        /* 1. 取 MathML 原文 */
//        std::string mathml = html.substr(it->start, it->end - it->start);
//        size_t altimgPos = mathml.find("altimg=\"");
//        if (altimgPos != std::string::npos) {
//            // 提取 altimg 属性值
//            size_t valueStart = altimgPos + 8; // 跳过 "altimg=\""
//            size_t valueEnd = mathml.find('"', valueStart);
//            if (valueEnd != std::string::npos) {
//                std::string altimgSrc = mathml.substr(valueStart, valueEnd - valueStart);
//
//                // 直接构建 img 标签
//                std::string imgTag = R"(<img class="math-png" src=")" + altimgSrc + R"(" alt="math"></img>)";
//                html.replace(it->start, it->end - it->start, imgTag);
//                continue; // 跳过后续转换流程
//            }
//        }
//        std::string hash = blade16(mathml);
//        if (!g_cMain)continue;
//        if(!g_cMain->isImageCached(hash))
//        {
//            /* 2. LaTeX → KaTeX → SVG（你原来的逻辑） */
//            MathML2SVG& m2s = MathML2SVG::instance();
//            std::string svg = m2s.convert(mathml);
//            if (svg.empty()) continue;
//            g_cMain->addImageCache(hash, svg);
//            g_cImage->m_img_cache[hash] = g_cMain->m_img_cache[hash];
//            g_cTooltip->m_img_cache[hash] = g_cMain->m_img_cache[hash];
//        }
//        std::string imgTag;
//        imgTag =  R"(<img class="math-png" src=")" + hash
//            + R"(" alt="math" />)";
//
//        /* 7. 替换原 <math> 标签 */
//        html.replace(it->start, it->end - it->start, imgTag);
//    }
//
//    gumbo_destroy_output(&kGumboDefaultOptions, output);

//}
//


struct HtmlFeatureFlags {
    bool has_svg = false;
    bool has_math = false;
    bool has_script = false;
    bool all() const { return has_svg && has_math && has_script; }
};

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
//struct HtmlFeatureFlags {
//    bool has_svg = false;
//    bool has_math = false;
//    bool has_script = false;
//    bool all() const { return has_svg && has_math && has_script; }
//};
//
//inline HtmlFeatureFlags detect_html_features(const std::string& html) noexcept
//{
//    HtmlFeatureFlags f;
//    const char* s = html.data();
//    const char* end = s + html.size();
//
//    while (s < end - 6)   // 最短 "<svg" 4 字节，留余量
//    {
//        if (*s == '<')
//        {
//            ++s;
//            // 跳过空白： <  svg  或 <  script
//            while (s < end && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
//                ++s;
//
//            if (s + 3 <= end) {
//                char c0 = static_cast<char>(std::tolower(*s));
//                char c1 = static_cast<char>(std::tolower(*(s + 1)));
//                char c2 = static_cast<char>(std::tolower(*(s + 2)));
//
//                if (c0 == 's' && c1 == 'v' && c2 == 'g') { f.has_svg = true; }
//                else if (c0 == 'm' && c1 == 'a' && c2 == 't') { f.has_math = true; }
//                else if (c0 == 's' && c1 == 'c' && c2 == 'r') { f.has_script = true; }
//
//                if (f.all()) break;   // 提前终止
//            }
//        }
//        ++s;
//    }
//    return f;
//}
//



void PreprocessHTML(std::string& html)
{

    auto flags = detect_html_features(html);
    if(flags.has_math) replace_math_with_svg(html); 

    html = std::regex_replace(
        html,
        std::regex(R"(<([a-zA-Z][a-zA-Z0-9]*)\b([^>]*?)/\s*>)", std::regex::icase),
        "<$1$2></$1>");

    if (flags.has_script)preprocess_js(html);

  
     if (flags.has_svg) 
     {
         std::wstring dir = make_temp_dir();
         replace_svg_with_img(html, dir);
     } 

}




















//------------------------------------------
// 公共辅助：从 EPUB 提取字体 blob
//------------------------------------------

static std::wstring make_safe_filename(std::wstring_view src)
{
    // 1. 去掉路径，只保留纯文件名
    size_t last = src.find_last_of(L"/\\");
    std::wstring name = (last == std::wstring::npos)
        ? std::wstring{ src }
    : std::wstring{ src.substr(last + 1) };

    // 2. 去掉前后空格/句点
    const std::wregex trim_re(L"^[ \\.]+|[ \\.]+$");
    name = std::regex_replace(name, trim_re, L"");

    // 3. 替换非法字符为单个下划线
    const std::wregex illegal_re(L"[<>:\"/\\\\|?*\\x00-\\x1F]+");
    name = std::regex_replace(name, illegal_re, L"_");

    // 4. 处理 Windows 保留设备名
    const std::wregex reserved_re(
        L"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\\..*)?$",
        std::regex_constants::icase);
    if (std::regex_match(name, reserved_re))
        name = L"_" + name;

    // 5. 空文件名兜底
    if (name.empty())
        name = L"file";

    // 6. 拼上序号
    return  name;
}


namespace fs = std::filesystem;

// 大小写不敏感的 set / map 比较器
struct CaseInsensitiveLess
{
    bool operator()(const std::wstring& a, const std::wstring& b) const
    {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    }
};

inline int hex_to_int(wchar_t c)
{
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    return 0;   // 非法字符按 0 处理
}
// 简单 URL decode（仅处理 %20 等）
std::wstring url_decode(const std::wstring& in)
{
    std::wstring out;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == L'%' && i + 2 < in.size())
        {
            int hi = hex_to_int(in[i + 1]);
            int lo = hex_to_int(in[i + 2]);
            out.push_back(static_cast<wchar_t>((hi << 4) | lo));
            i += 2;
        }
        else
            out.push_back(in[i]);
    }
    return out;
}






// 注册 Cambria Math（常规字重，非粗非斜）
bool lunasvgRegisterCambriaMath()
{

    wchar_t fontDir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, fontDir)))
        return false;

    wchar_t fullPath[MAX_PATH]{};
    PathCombineW(fullPath, fontDir, L"cambria.ttc");   // Cambria Math 在 .ttc 里

    char utf8Path[MAX_PATH * 3]{};
    WideCharToMultiByte(CP_UTF8, 0, fullPath, -1, utf8Path, sizeof(utf8Path), nullptr, nullptr);

    // 把 Cambria Math 的 Regular face 注册为 "Cambria Math"
    return lunasvg_add_font_face_from_file("Cambria Math", false, false, utf8Path);

}


AppBootstrap::AppBootstrap() {
    //make_tooltip_backend();
    if (lunasvgRegisterCambriaMath()) { OutputDebugStringW(L"[lunasvg] 注册字体成功： Cambria Math\n"); }
    if (g_cfg.enableJS) { enableJS(); }

    if (!g_book){ g_book = std::make_unique<EPUBBook>(); }

    if (!g_vd){g_vd = std::make_unique<VirtualDoc>();}

    if(!g_recorder)
    { 
       
        g_recorder = std::make_unique<ReadingRecorder>(documents_dir()); 
        if(g_recorder)
        {
            auto& settings = g_recorder->m_setting_record;
            g_cfg.enableClickPreview = settings.enableClickPreview;
            g_cfg.enableEPUBFonts = settings.enableLoadEPUBFonts;
            g_cfg.enableFontRealtimePreview = settings.enableFontRealtimePreview;
            g_cfg.enableHoverPreview = settings.enableHoverPreview;
            g_cfg.enableScrollAnimation = settings.enableScrollAnimation;
            g_cfg.displayFrameRate = settings.displayFrameRate;
            g_cfg.displayScrollBar = settings.displayScrollBar;
            g_cfg.displayStatusBar = settings.displayStatusBar;
            g_cfg.displayTOC = settings.displayTOC;
            CheckAllMenuItem();
            g_globalCSS = g_cfg.enableGlobalCSS ? get_global_css() : "";
        }
    }

    if (!g_toc) 
    { 
        g_toc = std::make_unique<TocPanel>(); 
        g_toc->GetWindow(g_hToc);
        // 绑定目录点击 -> 章节跳转
        g_toc->SetOnNavigate([](const std::wstring& href) {
            g_vd->OnTreeSelChanged(href.c_str());
            });
    }
    if (!g_cMain) { g_cMain = std::make_unique<SimpleContainer>(10, 10, g_hView); }

    if(!g_cTooltip){ g_cTooltip = std::make_unique<SimpleContainer>(10, 10, g_hTooltip); }

    if(!g_cImage){ g_cImage = std::make_unique<SimpleContainer>(10, 10, g_hImageview); }

    if(!g_scrollbar) 
    {
        g_scrollbar = std::make_unique<ScrollBarEx>();
        g_scrollbar->GetWindow(g_hViewScroll);
    }

    if(!g_cHome)
    {
        g_cHome = std::make_unique<SimpleContainer>(10, 10, g_hHomepage);
        fs::path html_path = exe_dir() / "res" / "homepage.html";
        auto html = read_file(html_path);
        if (html.empty()) { OutputDebugStringA("[AppBootstrap] html is null!"); return; }
        std::string time_txt = "";
        if(g_recorder)
        {
            int64_t seconds = g_recorder->getTotalTime();
            time_txt =   w2a(seconds2string(seconds));
        }
        boost::algorithm::replace_first(html, "[ID_READING_TIME]", time_txt);
        g_cHome->m_doc = litehtml::document::createFromString({ html.c_str(), litehtml::encoding::utf_8 }, g_cHome.get());
        if (!g_cHome->m_doc) { OutputDebugStringA("[AppBootstrap] g_cHome->m_doc is null!"); return; }
    }

    //BuildSplashWithText();
}

AppBootstrap::~AppBootstrap() {

}







void AppBootstrap::enableJS()
{
    //if (!m_jsrt) m_jsrt = std::make_unique<js_runtime>(g_doc.get());
    //if (!m_jsrt->switch_engine("duktape"))
    //    OutputDebugStringA("[Duktape] Duktape init failed\n");
    //else {
    //    OutputDebugStringA("[Duktape] Duktape init OK\n");
    //    m_jsrt->set_logger(OutputDebugStringA);
    //    m_jsrt->eval("console.log('hello from duktape\n');");
    //}
}

void AppBootstrap::disableJS()
{
    //m_jsrt.reset();   // 直接销毁即可，js_runtime 会负责 shutdown
}

void AppBootstrap::run_pending_scripts()
{
    //    if (!m_jsrt) return;          // 没有 JS 引擎就跳过
    //    for (const auto& script : m_pending_scripts)
    //    {
    //        litehtml::string code;
    //        script.el->get_text(code);  // 取出 <script> 里的纯文本
    //        if (!code.empty())
    //            m_jsrt->eval(code, "<script>");  // 交给 QuickJS / Duktape / V8
    //    }
    //    m_pending_scripts.clear();    // 执行完清空
}

void AppBootstrap::bind_host_objects()
{
    //if (!m_jsrt) return;
   // m_jsrt->bind_document(g_doc.get());   // js_runtime 内部会转发到当前引擎
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
    if (line < 0 )
    {
        m_curHover = -1;
        if (m_hTip) { ShowWindow(m_hTip, SW_HIDE); }
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return;
    }
    if (m_curHover == line) {  return; }
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
    int fullW =  sz.cx + indent;

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
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST ,
        L"STATIC", L"",
        WS_POPUP | SS_LEFT | SS_NOPREFIX|WS_BORDER,
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
    std::wstring anchor = pos == std::wstring::npos ? L"" : href.substr(pos+1);
 
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
    for (size_t i = target+1; i < m_nodes.size(); ++i)
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




// ---------- 静态 ----------



void ScrollBarEx::GetWindow(HWND hwnd)
{
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    m_hwnd = hwnd;
}
// ---------- API ----------
void ScrollBarEx::SetSpineCount(int n)
{
    m_count = n;
    m_pos = {};
    InvalidateRect(m_hwnd, nullptr, false);
    UpdateWindow(m_hwnd);
}



void ScrollBarEx::SetPosition(int spineId, float totalHeightPx, float offsetPx)
{
    if (spineId >= 0 && spineId < m_count)
    {

        m_pos.spine_id = spineId;
        m_pos.height = totalHeightPx;
        m_pos.offset = offsetPx;

        InvalidateRect(m_hwnd, nullptr, false);
        UpdateWindow(m_hwnd);
    }
}

ScrollBarEx::ScrollBarEx()
{
    m_hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_SCROLLBAR_DOT));
}
ScrollBarEx::~ScrollBarEx()
{
    // 销毁图标句柄
    DestroyIcon(m_hIcon);
}
// ---------- 内部 ----------
LRESULT CALLBACK ScrollBarEx::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ScrollBarEx* self = (ScrollBarEx*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg)
    {
    case WM_PAINT:          self->OnPaint(); return 0;
    case WM_LBUTTONDOWN:  self->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSEMOVE:      self->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_MOUSELEAVE: self->OnMouseLeave(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_LBUTTONUP:    self->OnLButtonUp(); return 0;
    case WM_RBUTTONUP:    self->OnRButtonUp() ; return 0;
    case SBM_SETSPINECOUNT:
        if (self) self->SetSpineCount((int)wp);

        return 0;

    case SBM_SETPOSITION:
        if (self) self->SetPosition((int)wp, (float)LOWORD(lp), (float)HIWORD(lp));
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ScrollBarEx::OnPaint()
{
    OutputDebugStringA("[ScrollBarEx] WM_PAINT\n");
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int W = rc.right - rc.left;
    const int H = rc.bottom - rc.top;
    const int CX = W / 2;

    /* ---- 双缓冲 ---- */
    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bi{ 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits;
    HBITMAP bmp = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, bmp);
    FillRect(memDC, &rc, (HBRUSH)(COLOR_BTNFACE + 1));

    if (H > 0 && m_count > 0)
    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        const bool crowded = (m_count > 500);
        if (m_count >= 1 && m_count < 30) dot_r = 2;
        else if (m_count >= 30 && m_count < 100) dot_r = 2;
        else if (m_count >= 100) dot_r = 1;
        const double step = static_cast<double>(H) / m_count;

        /* 串起所有点的浅色细线 */
        if (m_count > 1)
        {
            Gdiplus::Pen linkPen(Gdiplus::Color(220, 220, 220), 1);
            g.DrawLine(&linkPen, CX, 0, CX, H );
        }



        for (int i = 0; i < m_count; ++i)
        {
            /* 拥挤时只画当前点 */
            if (i == m_pos.spine_id) continue;
     

            const int y = static_cast<int>((i + 0.5) * step);
    

            const int r = dot_r;
            Gdiplus::Color c =  Gdiplus::Color(200, 200, 200);

            Gdiplus::SolidBrush br(c);
            g.FillEllipse(&br, CX - r, y - r, 2 * r, 2 * r);
        }
        /* 画当前章节的点 */

        if (m_hIcon)
        {
            // 将 HICON 转换为 GDI+ Image
            Gdiplus::Bitmap bitmap(m_hIcon);

            // 计算绘制位置（图标中心在 y 位置）
             // 计算绘制位置（图标中心在 y 位置）
            int y = (m_pos.spine_id + 0.5) * step;
            int iconSize = 15; // 15x15 像素

            // 绘制图标（缩放至 15x15）
            g.DrawImage(
                &bitmap,
                CX - iconSize / 2,  // X 居中
                y - iconSize / 2,   // Y 居中
                iconSize,           // 目标宽度
                iconSize            // 目标高度
            );


        }
        else
        {
            // 如果加载图标失败，回退到原来的圆点绘制
            Gdiplus::Color c = g_cfg.scrollbar_dot_color_highlight;
            int r = ACTIVE_R;
            int y = (m_pos.spine_id + 0.5) * step;
            Gdiplus::SolidBrush br(c);
            g.FillEllipse(&br, CX - r, y - r, 2 * r, 2 * r);
        }
    

        /* 1. 计算竖线中心 Y 坐标（比例 0~1） */
        double ratio = 0.0;
        if (m_pos.height > 0)          // 防 0
            ratio = std::clamp(static_cast<double>(m_pos.offset) / m_pos.height, 0.0, 1.0);

        /* 2. 竖线几何：宽 2，高 8，贴在最右边缘（假设可用宽度为 W） */
  
        int line_w = 2;
        int barY = static_cast<int>((ratio * H) - (line_w / 2.0));   // 居中在比例位置

        /* 3. 画竖线 */
      
        Gdiplus::Pen linePen(g_cfg.scrollbar_dot_color_highlight, line_w);
        g.DrawLine(&linePen,
            0,
            barY,
            W,
            barY);

    }
    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    EndPaint(m_hwnd, &ps);
}


void ScrollBarEx::OnLButtonDown(int x, int y)
{

}
void ScrollBarEx::OnMouseLeave(int x, int y)
{
}
void ScrollBarEx::OnMouseMove(int x, int y)
{

 
}

void ScrollBarEx::OnLButtonUp()
{


}

void ScrollBarEx::OnRButtonUp()
{

}




//namespace mathml2tex {
//
//
//    using namespace tinyxml2;
//
//    // ---------- 工具 ----------
//    static inline void write(std::string& out, const std::string& s) { out += s; }
//
//    static inline std::string trim(const std::string& s)
//    {
//        const char* ws = " \t\n\r\f\v";
//        size_t first = s.find_first_not_of(ws);
//        if (first == std::string::npos) return "";
//        size_t last = s.find_last_not_of(ws);
//        return s.substr(first, last - first + 1);
//    }
//    static inline std::string get_attr(const XMLElement* e,
//        const char* name,
//        const char* def = "")
//    {
//        const char* v = e->Attribute(name);
//        return v ? v : def;
//    }
//
//    static inline std::string escape_text(const std::string& s)
//    {
//        std::string r;
//        r.reserve(s.size());
//        for (char c : s)
//        {
//            switch (c)
//            {
//            case '\\': r += "\\textbackslash{}"; break;
//            case '{':  r += "\\{"; break;
//            case '}':  r += "\\}"; break;
//            case '$':  r += "\\$"; break;
//            case '&':  r += "\\&"; break;
//            case '%':  r += "\\%"; break;
//            case '#':  r += "\\#"; break;
//            case '^':  r += "\\^{}"; break;
//            case '_':  r += "\\_"; break;
//            case '~':  r += "\\textasciitilde{}"; break;
//            default:   r += c;
//            }
//        }
//        return r;
//    }
//
//    // ---------- 主转换 ----------
//    void convert_node(const XMLNode* node, std::string& out, bool display = false)
//    {
//        if (!node) return;
//        if (const XMLText* txt = node->ToText())
//        {
//            write(out, escape_text(trim(txt->Value())));
//            return;
//        }
//
//        const XMLElement* e = node->ToElement();
//        if (!e) return;
//
//        // 使用 gperf 或编译期哈希可再提速，这里用 switch-case 展开
//        switch (e->Name()[0])
//        {
//        case 'm':
//        {
//            switch (e->Name()[1])
//            {
//            case 'a': // math
//                if (std::strcmp(e->Name(), "math") == 0)
//                {
//                    bool d = display || (get_attr(e, "display") == "block");
//                    for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                        convert_node(c, out, d);
//                    return;
//                }
//                break;
//
//            case 'f': // mfrac
//                if (std::strcmp(e->Name(), "mfrac") == 0)
//                {
//                    std::string lt = get_attr(e, "linethickness");
//                    if (!lt.empty() && lt != "1")
//                    {
//                        write(out, "\\genfrac{}{}{" + lt + "}{");
//                    }
//                    else
//                    {
//                        write(out, "\\frac{");
//                    }
//                    convert_node(e->FirstChild(), out, display);
//                    write(out, "}{");
//                    convert_node(e->FirstChild()->NextSibling(), out, display);
//                    write(out, "}");
//                    return;
//                }
//                break;
//
//            case 'r':
//                if (std::strcmp(e->Name(), "mroot") == 0)
//                {
//                    write(out, "\\sqrt[");
//                    convert_node(e->FirstChild()->NextSibling(), out, display);
//                    write(out, "]{");
//                    convert_node(e->FirstChild(), out, display);
//                    write(out, "}");
//                    return;
//                }
//                else if (std::strcmp(e->Name(), "mrow") == 0)
//                {
//                    for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                        convert_node(c, out, display);
//                    return;
//                }
//                break;
//
//            case 's':
//                switch (e->Name()[2])
//                {
//                case 'q': // msqrt
//                    if (std::strcmp(e->Name(), "msqrt") == 0)
//                    {
//                        write(out, "\\sqrt{");
//                        for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                            convert_node(c, out, display);
//                        write(out, "}");
//                        return;
//                    }
//                    break;
//
//                case 'u': // msub, msup, msubsup
//                    if (std::strcmp(e->Name(), "msub") == 0)
//                    {
//                        convert_node(e->FirstChild(), out, display);
//                        write(out, "_{");
//                        convert_node(e->FirstChild()->NextSibling(), out, display);
//                        write(out, "}");
//                        return;
//                    }
//                    else if (std::strcmp(e->Name(), "msup") == 0)
//                    {
//                        convert_node(e->FirstChild(), out, display);
//                        write(out, "^{");
//                        convert_node(e->FirstChild()->NextSibling(), out, display);
//                        write(out, "}");
//                        return;
//                    }
//                    else if (std::strcmp(e->Name(), "msubsup") == 0)
//                    {
//                        convert_node(e->FirstChild(), out, display);
//                        write(out, "_{");
//                        convert_node(e->FirstChild()->NextSibling(), out, display);
//                        write(out, "}^{");
//                        convert_node(e->FirstChild()->NextSibling()->NextSibling(), out, display);
//                        write(out, "}");
//                        return;
//                    }
//                    break;
//
//                case 't': // mtable, mtr, mtd
//                    if (std::strcmp(e->Name(), "mtable") == 0)
//                    {
//                        write(out, "\\begin{array}");
//                        std::string colalign = get_attr(e, "columnalign");
//                        if (!colalign.empty())
//                        {
//                            write(out, "{");
//                            for (char c : colalign)
//                            {
//                                switch (c)
//                                {
//                                case 'l': write(out, "l"); break;
//                                case 'c': write(out, "c"); break;
//                                case 'r': write(out, "r"); break;
//                                default:  write(out, "c");
//                                }
//                            }
//                            write(out, "}");
//                        }
//                        else
//                        {
//                            // 默认列数：第一行 <mtr> 的 <mtd> 数量
//                            int cols = 0;
//                            if (const XMLNode* firstRow = e->FirstChild())
//                                for (const XMLNode* cell = firstRow->FirstChild(); cell; cell = cell->NextSibling())
//                                    ++cols;
//                            write(out, std::string(std::max(cols, 1), 'c'));
//                        }
//
//                        for (const XMLNode* row = e->FirstChild(); row; row = row->NextSibling())
//                        {
//                            write(out, "\n");
//                            for (const XMLNode* cell = row->FirstChild(); cell; cell = cell->NextSibling())
//                            {
//                                if (cell != row->FirstChild()) write(out, " & ");
//                                convert_node(cell, out, display);
//                            }
//                            write(out, " \\\\");
//                        }
//                        write(out, "\n\\end{array}");
//                        return;
//                    }
//                    else if (std::strcmp(e->Name(), "mtr") == 0 || std::strcmp(e->Name(), "mtd") == 0)
//                    {
//                        for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                            convert_node(c, out, display);
//                        return;
//                    }
//                    break;
//                }
//                break;
//
//            case 'o': // mo, mi, mn
//                if (std::strcmp(e->Name(), "mo") == 0 ||
//                    std::strcmp(e->Name(), "mi") == 0 ||
//                    std::strcmp(e->Name(), "mn") == 0)
//                {
//                    if (const char* txt = e->GetText())
//                    {
//                        std::string s = txt;
//                        if (std::strcmp(e->Name(), "mo") == 0)
//                        {
//                            // 简单映射常用符号
//                            if (s == "−") s = "-";
//                            else if (s == "×") s = "\\times";
//                            else if (s == "·") s = "\\cdot";
//                            else if (s == "→") s = "\\to";
//                            else if (s == "∞") s = "\\infty";
//                            else if (s == "≤") s = "\\leq";
//                            else if (s == "≥") s = "\\geq";
//                            else if (s == "≠") s = "\\neq";
//                            else if (s == "±") s = "\\pm";
//                        }
//                        write(out, s);
//                    }
//                    return;
//                }
//                break;
//
//            case 'e': // merror, menclose
//                if (std::strcmp(e->Name(), "menclose") == 0)
//                {
//                    std::string notation = get_attr(e, "notation");
//                    if (notation == "longdiv")
//                    {
//                        write(out, "\\longdiv{");
//                    }
//                    else
//                    {
//                        write(out, "\\boxed{");
//                    }
//                    for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                        convert_node(c, out, display);
//                    write(out, "}");
//                    return;
//                }
//                break;
//
//            case 'i': // mspace, mstyle, mphantom, mpadded
//                if (std::strcmp(e->Name(), "mstyle") == 0)
//                {
//                    std::string scriptlevel = get_attr(e, "scriptlevel");
//                    if (!scriptlevel.empty())
//                    {
//                        int lvl = std::stoi(scriptlevel);
//                        if (lvl > 0) write(out, "\\scriptstyle ");
//                        else         write(out, "\\displaystyle ");
//                    }
//                    for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                        convert_node(c, out, display);
//                    return;
//                }
//                else if (std::strcmp(e->Name(), "mspace") == 0)
//                {
//                    std::string w = get_attr(e, "width");
//                    if (!w.empty())
//                    {
//                        write(out, "\\hspace{" + w + "}");
//                    }
//                    return;
//                }
//                else if (std::strcmp(e->Name(), "mphantom") == 0)
//                {
//                    write(out, "\\phantom{");
//                    for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//                        convert_node(c, out, display);
//                    write(out, "}");
//                    return;
//                }
//                break;
//            }
//            break;
//        }
//
//        default:
//            break;
//        }
//
//        // 兜底：未知节点直接递归子节点
//        for (const XMLNode* c = e->FirstChild(); c; c = c->NextSibling())
//            convert_node(c, out, display);
//    }
//    /* ---------- 递归转换 ---------- */
//    std::string mathml2tex(const std::string& mathml)
//    {
//        XMLDocument doc;
//        doc.Parse(mathml.c_str());
//        const XMLElement* math = doc.RootElement();
//        if (!math || std::strcmp(math->Name(), "math") != 0)
//            return "";
//
//        std::string tex;
//        convert_node(math, tex, true);
//        return tex;
//    }
//
//    /* ---------- 对外接口 ---------- */
//    std::string convert(const std::string& mathml) {
//        return mathml2tex(mathml);
//    }
//
//} // namespace mathml2tex



// 方便打印任意字符串
inline void DbgPrint(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}



