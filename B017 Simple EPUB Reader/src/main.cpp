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




// ---------- 工具 ----------
static std::string w2a(const std::wstring& s)
{
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len - 1, 0);                 // 去掉末尾 '\0'
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

static std::wstring a2w(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len - 1, 0);                // 去掉末尾 '\0'
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    return out;
}

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
    newY = std::clamp(newY, -h/2.0f, std::max(0.0f, g_vd->m_height - h));
    //OutputDebugStringA(std::to_string(newY).c_str());
    //OutputDebugStringA("\n");
    g_offsetY.store(newY, std::memory_order_relaxed);
    g_velocity.store(v, std::memory_order_relaxed);

    InvalidateRect(g_hView, nullptr, FALSE);

    // 速度接近 0 时停止定时器
    if (std::fabs(v) < 0.1f) {
        g_velocity.store(0.0f);
        timeKillEvent(g_scrollTimer);
        g_scrollTimer = 0;
    }
}




void convert_coordinate(POINT& pt)
{
    if(g_cMain)
    {
        pt.x = pt.x/ g_cMain->m_zoom_factor - g_center_offset;
        pt.y = pt.y/g_cMain->m_zoom_factor + g_offsetY.load(std::memory_order_relaxed); ;
    }

}



void CALLBACK Tick(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    // 直接在工作线程/回调里刷新
    if (g_recorder && !g_vd->m_blocks.empty()) { g_recorder->updateRecord(); }
   // OutputDebugStringA("定时器触发\n");
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
            UpdateCache();
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        if (!g_cMain || !g_cMain->m_doc) { return 0; }

        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        g_cMain->on_lbutton_down(pt.x/g_cMain->m_zoom_factor, pt.y/g_cMain->m_zoom_factor);
        convert_coordinate(pt);
        litehtml::position::vector redraw_boxes;
        g_cMain->m_doc->on_lbutton_down(pt.x, pt.y, 0, 0, redraw_boxes);
        if (!redraw_boxes.empty()) {
            InvalidateRect(hwnd, nullptr, false);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }

        if (g_cMain && g_cMain->m_doc)
        {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            g_cMain->on_lbutton_up();
            convert_coordinate(pt);

            litehtml::position::vector redraw_boxes;
            g_cMain->m_doc->on_lbutton_up(pt.x, pt.y, 0, 0, redraw_boxes);
            if (!redraw_boxes.empty()) {
                InvalidateRect(hwnd, nullptr, false);
            }
   
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
    {
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        if (g_cMain)
        {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            g_cMain->on_lbutton_dblclk(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
            InvalidateRect(hwnd, nullptr, false);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        if (g_cMain && g_cMain->m_doc)
        {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            g_cMain->on_mouse_move(pt.x / g_cMain->m_zoom_factor, pt.y / g_cMain->m_zoom_factor);
            convert_coordinate(pt);



            litehtml::position::vector redraw_boxes;
            g_cMain->m_doc->on_mouse_over(pt.x, pt.y, 0, 0, redraw_boxes);
            if (!redraw_boxes.empty()) {
                InvalidateRect(hwnd, nullptr, false);
            }
      
        }
  
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
        UpdateCache();
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
        if(g_cMain && g_cMain->m_doc)
        {
            litehtml::position::vector redraw_boxes;
            g_cMain->m_doc->on_mouse_leave(redraw_boxes);
            if (!redraw_boxes.empty()) {
                InvalidateRect(hwnd, nullptr, false);
            }
        }


        return 0;
    }
    case WM_MBUTTONDOWN:  // 鼠标中键按下
    {
        // 检测Ctrl键是否按下
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            // Ctrl+中键被按下
            g_cMain->m_zoom_factor = 1.0f;
            UpdateCache();
            // 3. 重绘
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;  // 已处理该消息
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        if (g_cMain) { g_cMain->clear_selection(); }
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);   // ±120
            float factor = (delta > 0) ? 1.1f : 0.9f;     // 放大 / 缩小系数

            // 2. 更新全局缩放
            g_cMain->m_zoom_factor = std::clamp(g_cMain->m_zoom_factor * factor, 0.25f, 5.0f);
            UpdateCache();
            // 3. 重绘
            InvalidateRect(hwnd, NULL, FALSE);
        
            return 0;   // 已处理，不再传递
        }
 

        RECT rc;
        GetClientRect(hwnd, &rc);
        float h = float(rc.bottom - rc.top);

        int zDelta = GET_WHEEL_DELTA_WPARAM(wp);
        // 每格 3 行 → 每行像素 * 3
        float pxPerLine = g_cfg.font_size * g_cfg.line_height ;
        float pxDelta = -zDelta / 120.0f * pxPerLine * 3.0f;   // 负号：上滚为负

        if(g_cfg.enableScrollAnimation)
        {
            // 累加速度，而不是直接改目标
            g_velocity.fetch_add(pxDelta * 12.0f, std::memory_order_relaxed);

            // 启动 1 kHz 高精度定时器
            if (g_scrollTimer == 0) {
                g_scrollTimer = timeSetEvent(1, 0, OnScrollTimer, 0, TIME_PERIODIC);
            }
        }
        else
        {
            float cur = g_offsetY.load(std::memory_order_relaxed);
            cur = std::clamp(cur + pxDelta, -h/2.0f, std::max(g_vd->m_height - h / 2.0f, 0.0f));
            g_offsetY.store(cur, std::memory_order_relaxed);
        }

        // 更新阅读记录
        if (!g_tickTimer)
        {
            g_tickTimer = timeSetEvent(g_cfg.record_update_interval_ms, 0, Tick, 0, TIME_ONESHOT);
        }
        litehtml::position::vector redraw_box;
        if (g_cMain && g_cMain->m_doc)
        {
            g_cMain->m_doc->on_mouse_leave(redraw_box);

        }
        UpdateCache();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT:
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
    
        if (g_cMain  && g_cMain->m_doc )
        {
            g_frame_count += 1;
            OutputDebugStringA("[View] WM_PAINT\n");
            RECT rc;
            GetClientRect(g_hView, &rc);
            int x = g_center_offset;
            int y = -g_offsetY.load(std::memory_order_relaxed);
            float w = g_cfg.document_width;
            float h = rc.bottom - rc.top;
            litehtml::position clip(x, 0, w, h/g_cMain->m_zoom_factor);
            g_cMain->present(x, y, &clip);

        }
        EndPaint(hwnd, &ps);
        return 0;

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

    g_vd->update_doc(h);
 

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
        RECT rc; GetClientRect(hwnd, &rc);
        float page = rc.bottom - rc.top;
        float line = g_cMain->m_line_height;

        float delta = 0.f;
        switch (wp)
        {
        case VK_UP:     delta = -line;  break;
        case VK_DOWN:   delta = line;  break;
        case VK_PRIOR:  delta = -page;  break;
        case VK_NEXT:   delta = page;  break;
        default:        return DefWindowProc(hwnd, msg, wp, lp);
        }

        // 原子读-改-写
        float old = g_offsetY.load(std::memory_order_relaxed);
        float desired;
        do {
            desired = std::clamp(old + delta,
                -1.0f,
                std::max(0.0f, g_vd->m_height - page));
        } while (!g_offsetY.compare_exchange_weak(old, desired,
            std::memory_order_relaxed,
            std::memory_order_relaxed));

        UpdateCache();
        InvalidateRect(g_hView, nullptr, FALSE);
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
        g_offsetY.store(record.lastOffset, std::memory_order_relaxed) ;
        g_cfg.font_size = record.fontSize > 0 ? record.fontSize:g_cfg.default_font_size;
        g_cfg.line_height = record.lineHeightMul > 0 ? record.lineHeightMul : g_cfg.default_line_height;
        g_cfg.document_width = record.docWidth > 0 ? record.docWidth : g_cfg.default_document_width;
    
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
        g_vd->load_book();
        g_vd->load_html(g_book->ocf_pkg_.spine[spine_id].href);


  
        std::string title;
        auto t = g_book->get_title();
        if (t.empty()) { t = fs::path(book_path).filename().generic_string(); }
        if (!t.empty()) { title += t + " - "; }
        auto a = g_book->get_author();
        if (!a.empty()) { title += a + " - "; }
        title += g_cfg.appName;
        SetWindowTextW(g_hWnd, a2w(title).c_str());
 
  
        UpdateCache();          // 复用前面给出的 UpdateCache()

        
        PostMessage(hwnd, WM_SIZE, 0, 0);

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
        if (g_vd->m_isReloading.exchange(false)) {
            g_offsetY.store(g_vd->m_percent * g_vd->m_height,
                std::memory_order_relaxed);
        }

        float delta = static_cast<float>(lp);
        // 原子 += delta
        float old = g_offsetY.load(std::memory_order_relaxed);
        float desired;
        do {
            desired = old + delta;
        } while (!g_offsetY.compare_exchange_weak(old, desired,
            std::memory_order_relaxed,
            std::memory_order_relaxed));

        g_cMain->m_doc = std::move(g_vd->m_doc);

        if (g_vd->m_isAnchor.exchange(false)) {
            std::string cssSel = "[id=\"" + g_vd->m_anchor_id + "\"]";
            if (auto el = g_cMain->m_doc->root()->select_one(cssSel.c_str())) {
                g_offsetY.store(el->get_placement().y,
                    std::memory_order_relaxed);
            }
        }

        UpdateCache();
        InvalidateRect(g_hView, nullptr, FALSE);
        return 0;
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

void LogToFile(const std::string& message)
{
    fs::path debug_path = documents_dir() / g_cfg.appName / "debug_log.txt";
   
    std::ofstream log(debug_path, std::ios::app);
    if (log.is_open())
    {
        log << message << std::endl;
    }
}



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



// ---------- 点击目录跳转 ----------
void VirtualDoc::OnTreeSelChanged(std::wstring href)
{
    if (href.empty()) return;


    /* 1. 分离文件路径与锚点 */

    size_t pos = href.find(L'#');
    std::wstring file_path = (pos == std::wstring::npos) ? href : href.substr(0, pos);
    int spine_id = get_id_by_href(file_path);
    m_anchor_id = (pos == std::wstring::npos) ? "" :
        w2a(href.substr(pos + 1));

    if (spine_id != get_scroll_position().spine_id)
    {
        clear();
        insert_chapter(spine_id);

        m_isAnchor.store(m_anchor_id.empty()? false: true);

    }
    else
    {
        if (!m_anchor_id.empty())
        {
            std::wstring cssSel = a2w(m_anchor_id);   // 转成宽字符
            // WM_APP + 3 约定为“跳转到锚点选择器”
            PostMessageW(g_hView, WM_EPUB_ANCHOR,
                reinterpret_cast<WPARAM>(_wcsdup(cssSel.c_str())), 0);
        }
    }



/* 3. 跳转到锚点 */
  
    //InvalidateRect(g_hView, nullptr, true);
    //UpdateWindow(g_hWnd);
}

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
        g_recorder = std::make_unique<ReadingRecorder>(); 
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








VirtualDoc::VirtualDoc()
{
    m_worker = std::thread(&VirtualDoc::workerLoop, this);
}

VirtualDoc::~VirtualDoc()
{

    m_worker.detach();   // 或 join，取决于生命周期
    clear();

}

void VirtualDoc::load_book()
{
    m_book = g_book;
    m_container = g_cMain;

    m_spine = m_book->ocf_pkg_.spine;
}


// ---------- 分页 ----------
std::wstring VirtualDoc::get_href_by_id(int id)
{

    if (id < m_spine.size() && id >= 0)
    {
        return m_spine[id].href;
    }
    return L"";
}

int VirtualDoc::get_id_by_href(std::wstring& href)
{
    for (int i = 0; i < m_spine.size(); i++)
    {
        if (m_spine[i].href == href) {
            return i;
        }
    }
    return -1;
}

void VirtualDoc::merge_block(HtmlBlock& dst, HtmlBlock& src, bool isAddToBottom)
{

    dst.head = src.head;
    // 2. 把新的 body_blocks 追加到尾部
    if (isAddToBottom)
    {

        dst.body_blocks.insert(
            dst.body_blocks.end(),
            src.body_blocks.begin(),
            src.body_blocks.end());

    }
    // 追加到顶部
    else
    {
        dst.body_blocks.insert(
            dst.body_blocks.begin(),
            src.body_blocks.begin(),
            src.body_blocks.end());
    }
}


HtmlBlock VirtualDoc::get_html_block(std::string html, int spine_id)
{
    HtmlBlock block;
    block.spine_id = spine_id;
    block.head = get_head(html);
    block.body_blocks = get_body_blocks(html, spine_id);
    


        BodyBlock bi;
        bi.spine_id = spine_id;
        bi.height = 0;
        bi.html = "<div style = \"height:" + std::to_string(g_cfg.split_space_height) + "px; \"></div>";
        bi.block_id = block.body_blocks.back().block_id + 1;
        block.body_blocks.push_back(std::move(bi));



    return block;
}


// ---------- 工具：节点序列化 ----------
bool VirtualDoc::gumbo_tag_is_void(GumboTag tag)
{
    switch (tag)
    {
    case GUMBO_TAG_AREA:
    case GUMBO_TAG_BASE:
    case GUMBO_TAG_BR:
    case GUMBO_TAG_COL:
    case GUMBO_TAG_EMBED:
    case GUMBO_TAG_HR:
    case GUMBO_TAG_IMG:
    case GUMBO_TAG_INPUT:
    case GUMBO_TAG_LINK:
    case GUMBO_TAG_META:
    case GUMBO_TAG_PARAM:
    case GUMBO_TAG_SOURCE:
    case GUMBO_TAG_TRACK:
    case GUMBO_TAG_WBR:
        return true;
    default:
        return false;
    }
}

void VirtualDoc::serialize_element(const GumboElement& el, std::ostream& out) {
    out << '<' << gumbo_normalized_tagname(el.tag);
    for (unsigned int i = 0; i < el.attributes.length; ++i) {
        auto* attr = static_cast<GumboAttribute*>(el.attributes.data[i]);
        out << ' ' << attr->name << "=\"" << attr->value << '"';
    }
    if (el.children.length == 0 && gumbo_tag_is_void(el.tag)) {
        out << " />";
        return;
    }
    out << '>';
    for (unsigned int i = 0; i < el.children.length; ++i)
        serialize_node(static_cast<GumboNode*>(el.children.data[i]), out);
    out << "</" << gumbo_normalized_tagname(el.tag) << '>';
}

void VirtualDoc::serialize_node(const GumboNode* node, std::ostream& out) {
    if (!node) return;
    switch (node->type) {
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_CDATA:
        out << node->v.text.text;
        break;
    case GUMBO_NODE_WHITESPACE:
        out << node->v.text.text;
        break;
    case GUMBO_NODE_ELEMENT:
        serialize_element(node->v.element, out);
        break;
    default: break;
    }
}

// ---------- 1. 提取 <head> ----------
std::string VirtualDoc::get_head(std::string& html) {
    GumboOutput* out = gumbo_parse(html.c_str());
    std::string result;
    if (out->root->type == GUMBO_NODE_ELEMENT) {
        for (unsigned int i = 0; i < out->root->v.element.children.length; ++i) {
            auto* node = static_cast<GumboNode*>(out->root->v.element.children.data[i]);
            if (node->type == GUMBO_NODE_ELEMENT &&
                node->v.element.tag == GUMBO_TAG_HEAD) {
                std::ostringstream oss;
                serialize_element(node->v.element, oss);
                result = oss.str();
                break;
            }
        }
    }
    gumbo_destroy_output(&kGumboDefaultOptions, out);
    return result;
}

std::vector<BodyBlock>
VirtualDoc::get_body_blocks(std::string& html,
     int spine_id,
     size_t max_chunk_bytes)
{
    // 1. 用 string_view 避免拷贝
    std::string_view sv(html);

    // 2. 找到 <body ...> 和 </body>
    static const std::string_view body_tag = "<body";
    static const std::string_view body_end = "</body>";

    size_t body_open = sv.find(body_tag);
    if (body_open == std::string_view::npos) return {};

    body_open = sv.find('>', body_open);          // 跳过属性
    if (body_open == std::string_view::npos) return {};
    ++body_open;                                  // 指向 '>' 之后

    size_t body_close = sv.find(body_end, body_open);
    if (body_close == std::string_view::npos) return {};

    // 3. 直接取子串（零拷贝）
    std::string_view body_content = sv.substr(body_open, body_close - body_open);

    // 4. 构造唯一块
    return { BodyBlock{0, 0, std::string(body_content)} };
}
// ---------- 2. 切 <body> ----------
//std::vector<BodyBlock> VirtualDoc::get_body_blocks(std::string& html,
//    int spine_id,
//    size_t max_chunk_bytes) {
//    std::vector<BodyBlock> blocks;
//    GumboOutput* out = gumbo_parse(html.c_str());
//    GumboNode* body = nullptr;
//
//    // 找到 body
//    if (out->root->type == GUMBO_NODE_ELEMENT) {
//        for (unsigned int i = 0; i < out->root->v.element.children.length; ++i) {
//            auto* node = static_cast<GumboNode*>(out->root->v.element.children.data[i]);
//            if (node->type == GUMBO_NODE_ELEMENT &&
//                node->v.element.tag == GUMBO_TAG_BODY) {
//                body = node;
//                break;
//            }
//        }
//    }
//    if (!body) { gumbo_destroy_output(&kGumboDefaultOptions, out); return blocks; }
//
//    // 收集 body 的直接子节点
//    std::vector<const GumboNode*> nodes;
//    auto& children = body->v.element.children;
//    for (unsigned int i = 0; i < children.length; ++i)
//        nodes.emplace_back(static_cast<GumboNode*>(children.data[i]));
//
//    // 分块
//    std::ostringstream current;
//    size_t current_bytes = 0;
//    int block_id = 0;
//
//    auto flush = [&]() {
//        if (current.str().empty()) return;
//        BodyBlock bb;
//        bb.spine_id = spine_id;
//        bb.block_id = block_id++;
//        bb.html = current.str();
//        blocks.emplace_back(std::move(bb));
//        current.str("");
//        current.clear();
//        current_bytes = 0;
//        };
//
//    for (const GumboNode* n : nodes) {
//        std::ostringstream tmp;
//        serialize_node(n, tmp);
//        std::string frag = tmp.str();
//        if (current_bytes + frag.size() > max_chunk_bytes && !current.str().empty())
//            flush();
//        current << frag;
//        current_bytes += frag.size();
//    }
//    flush(); // 最后一块
//    gumbo_destroy_output(&kGumboDefaultOptions, out);
//    return blocks;
//}

void VirtualDoc::load_html(std::wstring& href)
{

    auto id = get_id_by_href(href);
    if(id < 0)
    {
        OutputDebugStringW(href.c_str());
        OutputDebugStringW(L" 未找到\n");
        return ;
    }
 
    insert_chapter(id);
  

}

float VirtualDoc::get_height_by_id(int spine_id)
{
    for (const auto& b: m_blocks)
    {
        if(b.spine_id == spine_id)
        {
            return b.height;
        }
    }
    return 0;
}
void VirtualDoc::reload()
{
    if ( m_workerBusy) return;
    if (g_cMain) { g_cMain->clear_selection(); }
    if(m_blocks.empty())
    {
        insert_chapter(0);
    }
    else
    {
        // 1. 记录当前滚动百分比
        ScrollPosition old = get_scroll_position();
        double percent = 0.0;
        if (old.height > 0.0f)          // 旧文档高度
            m_percent = double(old.offset) / old.height;


        clear();
        insert_chapter(old.spine_id);

        m_isReloading.store(true);
    }
    

    // 3. 把百分比换算成新的像素值

}
bool VirtualDoc::load_by_id(int spine_id, bool isPushBack)
{
    try
    {
 
        std::wstring href = get_href_by_id(spine_id);
        if (href.empty()) return false;

  
        std::string html = m_book->load_html(href);
        if (html.empty()) return false;

   
        PreprocessHTML(html);          // 可能抛异常

  
        auto block = get_html_block(html, spine_id);

        if (isPushBack)
            m_blocks.push_back(std::move(block));
        else
            m_blocks.emplace(m_blocks.begin(), std::move(block));

        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA(("load_by_id exception: " +
            std::string(e.what()) + "\n").c_str());
    }
    catch (...)
    {
        OutputDebugStringA("load_by_id unknown exception\n");
    }
    return false;
}
ScrollPosition VirtualDoc::get_scroll_position()
{

    ScrollPosition pos{};
    if (m_blocks.empty()) { return pos; }
  
    pos.offset = g_offsetY.load(std::memory_order_relaxed);
    for (const auto& hb: m_blocks)
    {
        pos.spine_id = hb.spine_id;
        pos.height = hb.height;
  
        if ((pos.offset - hb.height) < 0.0f) {break; }
        pos.offset -= hb.height;
    }

    return pos;

}
void VirtualDoc::set_scroll_position( ScrollPosition sp)
{
    float offset = 0.0f;
    for (const auto& bk: m_blocks)
    {
        if (bk.spine_id == bk.spine_id)break;
        offset += bk.height;
    }
    offset += sp.offset;
    g_offsetY.store(offset, std::memory_order_relaxed);
}


void VirtualDoc::update_doc(int client_h)
{
    if (!m_book || !m_container ) { return ; }

    float offsetY = g_offsetY.load(std::memory_order_relaxed);

    OutputDebugStringA("[before] ");
    OutputDebugStringA(std::to_string(offsetY).c_str());
    OutputDebugStringA("\n");


    if (offsetY < 0)
    {
        insert_prev_chapter();

    }

    if (offsetY > m_height - static_cast<float>(client_h)*3.0f)
    {
        insert_next_chapter();

    }


    ScrollPosition p = get_scroll_position();

    g_toc->SetHighlight(p);

    std::wstring spine_info = L"总进度：" + std::to_wstring(p.spine_id + 1) + L" / " + std::to_wstring(m_spine.size());
    std::wstring offset_info = L"当前进度：" + std::to_wstring((int)p.offset) + L" / " + std::to_wstring((int)p.height);
    SetStatus(STATUSBAR_SPINE_INFO, spine_info.c_str());
    SetStatus(STATUSBAR_OFFSET_INFO, offset_info.c_str());
 
    auto time_string = seconds2string(g_recorder->getBookTotalTime());
    SetStatus(STATUSBAR_TOTAL_TIME, (L"阅读时长：" + time_string).c_str());
    SetStatus(STATUSBAR_FONT_NAME, (L"自定义字体：" + g_cfg.font_name).c_str());
    SetStatus(STATUSBAR_FONT_SIZE, (L"字体大小：" + std::to_wstring(g_cfg.font_size)).c_str());
    SetStatus(STATUSBAR_LINE_HEIGHT, (L"行间距：" + std::to_wstring(g_cfg.line_height)).c_str());
    SetStatus(STATUSBAR_DOC_WIDTH, (L"文档宽度：" + std::to_wstring(g_cfg.document_width)).c_str());
    SetStatus(STATUSBAR_DOC_ZOOM, (L"文档缩放倍数：" + std::to_wstring(g_cMain->m_zoom_factor)).c_str());

    g_scrollbar->SetPosition(p.spine_id, p.height, p.offset);

}


float VirtualDoc::get_height()
{
    float height = 0.0f;
    if (m_blocks.empty()) { return height; }
    for (const auto& b: m_blocks)
    {
        height += b.height;
    }
    return height;
}
bool VirtualDoc::insert_chapter(int spine_id)
{
    if (m_workerBusy.load(std::memory_order_relaxed)) return false;
    int id = spine_id;
    if (id < 0 || id >= static_cast<int>(m_spine.size()) || exists(id)) return false;

    {
        std::lock_guard<std::mutex> lk(m_taskMtx);
        if (!m_taskQueue.empty()) return false;
        m_taskQueue.push({ id, false });
    }
    m_taskCv.notify_one();
    return false;
}

bool VirtualDoc::insert_prev_chapter()
{
    if (m_workerBusy.load(std::memory_order_relaxed)) return false;
    int id = m_blocks.empty()? 0:m_blocks.front().spine_id - 1;
    if (id < 0 || exists(id)) return false;

    {
        std::lock_guard<std::mutex> lk(m_taskMtx);
        if (!m_taskQueue.empty()) return false;
        m_taskQueue.push({ id, true });
    }
    m_taskCv.notify_one();
    return false;
}

bool VirtualDoc::insert_next_chapter()
{

    if (m_workerBusy.load(std::memory_order_relaxed)) return false;
    int id = m_blocks.empty() ? 0 : m_blocks.back().spine_id + 1;
    if (id >= static_cast<int>(m_spine.size()) || exists(id)) return false;


    {
        std::lock_guard<std::mutex> lk(m_taskMtx);
        if (!m_taskQueue.empty()) return false;
        m_taskQueue.push({ id, false });
    }
    m_taskCv.notify_one();
    return false;
}
void VirtualDoc::workerLoop()
{
    while (true)
    {

        Task task;
        {
            std::unique_lock<std::mutex> lk(m_taskMtx);
            m_taskCv.wait(lk, [this] {
                /* 等待时也要能响应取消 */
                return !m_taskQueue.empty();
                });

            if (m_taskQueue.empty())
                continue;                // 虚假唤醒
            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
        }
        BusyGuard bg(m_workerBusy);   // 从这里开始置忙，析构时自动清 0

        OutputDebugStringA("[VirtualDod thread] 开始更新\n");
        // 1. 耗时 IO
                /* ---------- 2. 耗时 IO ---------- */



        if (!load_by_id(task.chapterId, !task.insertAtFront))
        {
            continue;
        }



        // 2. 组装 HTML
        float height = 0.0f;
        HtmlBlock& target = task.insertAtFront ? m_blocks.front() : m_blocks.back();
   

        std::string html = "";
        for (auto&hb : m_blocks)
        {
            html += "<html>" + hb.head + "<body>";
            for (auto& b : hb.body_blocks) html += b.html;
            html += "</body></html>";
        }
     
        /* ---------- 4. render ---------- */

        std::string css =  g_globalCSS;
        css += ":root,body,p,li,div,h1,h2,h3,h4,h5,h6,span, ul{line-height:" + std::to_string(g_cfg.line_height) + ";}\n";

        m_doc = litehtml::document::createFromString(
            { html.c_str(), litehtml::encoding::utf_8 }, m_container.get(), litehtml::master_css, css);
        m_doc->render(g_cfg.document_width);

        /* ---------- 5. 计算高度 ---------- */


        height = m_doc->height() - m_height;
 
        target.height = height;
        m_height = m_doc->height();
        float delta = task.insertAtFront ? height : 0.0f;
 

        PostMessage(g_hWnd, WM_EPUB_CACHE_UPDATED, 0, static_cast<LPARAM>(delta));
        OutputDebugStringA("[VirtualDod thread] 更新结束\n");
    }
}

bool VirtualDoc::exists(int spine_id)
{
    if (m_blocks.empty()) return false;
    for (const auto& b: m_blocks)
    {
        if (b.spine_id == spine_id) { return true; }
    }
    return false;
}

void VirtualDoc::clear()
{


    m_blocks.clear();
    g_offsetY.store(0.0f, std::memory_order_relaxed);
    float v = g_offsetY.load(std::memory_order_relaxed);

    m_height = 0.0f;


}


/* ---------- 工具 ---------- */
static int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/* ---------- 构造/析构 ---------- */
ReadingRecorder::ReadingRecorder() 
{ 
    m_book_record = {};
    m_time_frag = {};
    m_setting_record = {};
    initDB(); 

}
ReadingRecorder::~ReadingRecorder() 
{ 
    if (m_dbBook) sqlite3_close(m_dbBook); 
    if (m_dbTime) sqlite3_close(m_dbTime);
    if (m_dbSetting) sqlite3_close(m_dbSetting);
}

/* ---------- 初始化数据库 ---------- */
void ReadingRecorder::initDB() {
    namespace fs = std::filesystem;
    fs::path db_path = documents_dir() / g_cfg.appName / "data";
    fs::create_directories(db_path);
    fs::path db_book_path = db_path / "Books.db";
    fs::path db_time_path = db_path / "Time.db";
    fs::path db_setting_path = db_path / "Settings.db";

    /* ---------- Books.db ---------- */
    if (sqlite3_open(db_book_path.generic_string().c_str(), &m_dbBook) != SQLITE_OK)
        OutputDebugStringA("Books.db sqlite open failed\n");

    sqlite3_exec(m_dbBook, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS books(
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            path             TEXT UNIQUE,
            title            TEXT,
            author           TEXT,
            open_count       INTEGER DEFAULT 0,
            total_words      INTEGER DEFAULT 0,
            last_spine_id    INTEGER DEFAULT 0,
            last_offset      INTEGER DEFAULT 0,
            font_size        INTEGER DEFAULT 0,
            line_height_mul  REAL    DEFAULT 0.0,
            doc_width        INTEGER DEFAULT 0,
            total_time_s     INTEGER DEFAULT 0,
            first_open_us    INTEGER DEFAULT 0,
            last_open_us     INTEGER DEFAULT 0,
            enable_epub_css        INTEGER DEFAULT 1,
            enable_global_css      INTEGER DEFAULT 0,
            enable_custom_font     INTEGER DEFAULT 0,
            custom_font_name       TEXT
        );
    )";
    sqlite3_exec(m_dbBook, sql, nullptr, nullptr, nullptr);

    /* ---------- Time.db ---------- */
    if (sqlite3_open(db_time_path.generic_string().c_str(), &m_dbTime) != SQLITE_OK)
        OutputDebugStringA("Time.db sqlite open failed\n");
    sqlite3_exec(m_dbTime, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    const char* sqlTime = R"(
        CREATE TABLE IF NOT EXISTS reading_time(
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            path     TEXT,
            title         TEXT,
            authors       TEXT,
            spine_id      INTEGER,
            current_chapter TEXT,
            start_time    REAL,
            end_time      REAL,
            duration      INTEGER
        );
    )";
    sqlite3_exec(m_dbTime, sqlTime, nullptr, nullptr, nullptr);

    /* ---------- Settingss.db ---------- */
    if (sqlite3_open(db_setting_path.generic_string().c_str(), &m_dbSetting) != SQLITE_OK)
        OutputDebugStringA("Settings.db sqlite open failed\n");

    sqlite3_exec(m_dbSetting, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    const char* sqlSetting = R"(
        CREATE TABLE IF NOT EXISTS settings(
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            enable_load_epub_fonts        INTEGER DEFAULT 1,
            enable_scroll_animation       INTEGER DEFAULT 0,
            enable_hover_preview          INTEGER DEFAULT 1,
            enable_click_preview          INTEGER DEFAULT 1,
            enable_font_realtime_preview INTEGER DEFAULT 1,
            diaplay_toc                  INTEGER DEFAULT 1,
            display_status_bar          INTEGER DEFAULT 1,
            display_scroll_bar            INTEGER DEFAULT 1,
            display_frame_rate            INTEGER DEFAULT 1
        );
    )";
    sqlite3_exec(m_dbSetting, sqlSetting, nullptr, nullptr, nullptr);
    loadSettings();
}

bool ReadingRecorder::loadSettings() {
    if (!m_dbSetting) {
        OutputDebugStringA("Settings database not initialized\n");
        return false;
    }

    const char* sql = "SELECT * FROM settings LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_dbSetting, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        OutputDebugStringA("Failed to prepare SQL statement for settings\n");
        return false;
    }

    // Initialize with default values in case the query returns no rows
    m_setting_record = SettingRecord();

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Column indices (adjust if your table structure changes)
        m_setting_record.enableLoadEPUBFonts = sqlite3_column_int(stmt, 1) != 0;
        m_setting_record.enableScrollAnimation = sqlite3_column_int(stmt, 2) != 0;
        m_setting_record.enableHoverPreview = sqlite3_column_int(stmt, 3) != 0;
        m_setting_record.enableClickPreview = sqlite3_column_int(stmt, 4) != 0;
        m_setting_record.enableFontRealtimePreview = sqlite3_column_int(stmt, 5) != 0;
        m_setting_record.displayTOC = sqlite3_column_int(stmt, 6) != 0;
        m_setting_record.displayStatusBar = sqlite3_column_int(stmt, 7) != 0;
        m_setting_record.displayScrollBar = sqlite3_column_int(stmt, 8) != 0;
        m_setting_record.displayFrameRate = sqlite3_column_int(stmt, 9) != 0;
    }

    sqlite3_finalize(stmt);
    return true;
}
/* ---------- 打开书 ---------- */
void ReadingRecorder::openBook(const std::string absolutePath) {
    m_book_record = {};
    m_time_frag = {};
    BookRecord rec;
    rec.path = absolutePath;

    const char* select = "SELECT * FROM books WHERE path=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_dbBook, select, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, absolutePath.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // 已存在
        rec.id = sqlite3_column_int64(stmt, 0);
        auto colText = [](sqlite3_stmt* s, int idx) -> std::string {
            const char* p = reinterpret_cast<const char*>(sqlite3_column_text(s, idx));
            return p ? std::string(p, sqlite3_column_bytes(s, idx)) : "";
            };
        rec.title = colText(stmt, 2);
        rec.author = colText(stmt, 3);
        rec.openCount = sqlite3_column_int(stmt, 4);
        rec.totalWords = sqlite3_column_int(stmt, 5);
        rec.lastSpineId = sqlite3_column_int(stmt, 6);
        rec.lastOffset = sqlite3_column_int(stmt, 7);
        rec.fontSize = sqlite3_column_int(stmt, 8);
        rec.lineHeightMul = static_cast<float>(sqlite3_column_double(stmt, 9));
        rec.docWidth = sqlite3_column_int(stmt, 10);
        rec.totalTime = sqlite3_column_int(stmt, 11);
        rec.lastOpenTimestamp = sqlite3_column_int64(stmt, 13);
        rec.enableCSS = sqlite3_column_int(stmt, 14);
        rec.enableGlobalCSS = sqlite3_column_int(stmt, 15);
        rec.enableCustomFont = sqlite3_column_int(stmt, 16);
        rec.fontName = colText(stmt, 17);
    }
    else {
        // 新书：用当前 g_book 状态插入
        const char* insert = R"(
            INSERT INTO books(path, first_open_us, last_open_us)
            VALUES(?, ?, ?);
        )";
        sqlite3_prepare_v2(m_dbBook, insert, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, absolutePath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, nowUs());
        sqlite3_bind_int64(stmt, 3, nowUs());
        sqlite3_step(stmt);

        rec.id = sqlite3_last_insert_rowid(m_dbBook);

    }
    sqlite3_finalize(stmt);

    // 更新打开次数 & 最后打开时间
    const char* update = "UPDATE books SET open_count=open_count+1, last_open_us=? WHERE id=?;";
    sqlite3_prepare_v2(m_dbBook, update, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, nowUs());
    sqlite3_bind_int64(stmt, 2, rec.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    m_book_record =  std::move(rec);
}
int64_t ReadingRecorder::getTotalTime()
{
    const char* sql = "SELECT COALESCE(SUM(duration),0) FROM reading_time;";
    sqlite3_stmt* stmt = nullptr;
    int64_t totalUs = 0;

    if (sqlite3_prepare_v2(m_dbTime, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            totalUs = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return totalUs / 1'000'000;   // 返回秒
}
int64_t ReadingRecorder::getBookTotalTime() const
{

    return m_book_record.totalTime;
}
/* ---------- 写入 ---------- */
void ReadingRecorder::flush() {
    if (m_book_record.id < 0) return;   // 无效记录

    flushBookRecord();
    flushTimeRecord();
    flushSettingRecord();
}

void ReadingRecorder::flushSettingRecord() {
    if (!m_dbSetting) {
        OutputDebugStringA("Settings database not initialized\n");
        return;
    }

    // First, check if there's any existing record
    const char* checkSql = "SELECT COUNT(*) FROM settings;";
    sqlite3_stmt* checkStmt = nullptr;
    int count = 0;

    if (sqlite3_prepare_v2(m_dbSetting, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            count = sqlite3_column_int(checkStmt, 0);
        }
        sqlite3_finalize(checkStmt);
    }

    // Prepare the appropriate SQL statement (INSERT or UPDATE)
    const char* sql;
    if (count == 0) {
        sql = R"(
            INSERT INTO settings (
                enable_load_epub_fonts,
                enable_scroll_animation,
                enable_hover_preview,
                enable_click_preview,
                enable_font_realtime_preview,
                diaplay_toc,
                display_status_bar,
                display_scroll_bar,
                display_frame_rate
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    }
    else {
        sql = R"(
            UPDATE settings SET
                enable_load_epub_fonts = ?,
                enable_scroll_animation = ?,
                enable_hover_preview = ?,
                enable_click_preview = ?,
                enable_font_realtime_preview = ?,
                diaplay_toc = ?,
                display_status_bar = ?,
                display_scroll_bar = ?,
                display_frame_rate = ?
            WHERE id = 1;
        )";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_dbSetting, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        OutputDebugStringA("Failed to prepare SQL statement for settings update\n");
        return;
    }

    // Bind parameters
    sqlite3_bind_int(stmt, 1, m_setting_record.enableLoadEPUBFonts ? 1 : 0);
    sqlite3_bind_int(stmt, 2, m_setting_record.enableScrollAnimation ? 1 : 0);
    sqlite3_bind_int(stmt, 3, m_setting_record.enableHoverPreview ? 1 : 0);
    sqlite3_bind_int(stmt, 4, m_setting_record.enableClickPreview ? 1 : 0);
    sqlite3_bind_int(stmt, 5, m_setting_record.enableFontRealtimePreview ? 1 : 0);
    sqlite3_bind_int(stmt, 6, m_setting_record.displayTOC ? 1 : 0);
    sqlite3_bind_int(stmt, 7, m_setting_record.displayStatusBar ? 1 : 0);
    sqlite3_bind_int(stmt, 8, m_setting_record.displayScrollBar ? 1 : 0);
    sqlite3_bind_int(stmt, 9, m_setting_record.displayFrameRate ? 1 : 0);

    // Execute the statement
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        OutputDebugStringA("Failed to execute settings update\n");
    }

    sqlite3_finalize(stmt);
}
void ReadingRecorder::flushBookRecord()
{
    auto& rec = m_book_record;
    const char* sql = R"(
        UPDATE books SET
            title           = ?,
            author          = ?,
            total_words     = ?,
            last_spine_id   = ?,
            last_offset     = ?,
            font_size       = ?,
            line_height_mul = ?,
            doc_width       = ?, 
            total_time_s    = ?,
            enable_epub_css       = ?,
            enable_global_css      = ?,
            enable_custom_font   = ?,
            custom_font_name      = ?
        WHERE id = ?;
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_dbBook, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, rec.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, rec.author.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, rec.totalWords);
    sqlite3_bind_int(stmt, 4, rec.lastSpineId);
    sqlite3_bind_int(stmt, 5, rec.lastOffset);
    sqlite3_bind_int(stmt, 6, rec.fontSize);
    sqlite3_bind_double(stmt, 7, rec.lineHeightMul);
    sqlite3_bind_double(stmt, 8, rec.docWidth);
    sqlite3_bind_int(stmt, 9, rec.totalTime);
    sqlite3_bind_int(stmt, 10, rec.enableCSS);
    sqlite3_bind_int(stmt, 11, rec.enableGlobalCSS);
    sqlite3_bind_int(stmt, 12, rec.enableCustomFont);
    sqlite3_bind_text(stmt, 13, rec.fontName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 14, rec.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ReadingRecorder::flushTimeRecord()
{
    if (m_time_frag.empty()) return;

    /* 0. 先把缓存拿出来，防止 flush 期间又被写入 */
    std::vector<timeFragment> batch = std::move(m_time_frag);
    m_time_frag.clear();                 // 立即清空原缓存

    /* 1. 按时间升序 */
    std::sort(batch.begin(), batch.end(),
        [](const timeFragment& a, const timeFragment& b)
        { return a.timestamp < b.timestamp; });

    /* 2. 事务开始 */
    char* err = nullptr;
    if (sqlite3_exec(m_dbTime, "BEGIN;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        OutputDebugStringA(("BEGIN failed: " + std::string(err) + "\n").c_str());
        sqlite3_free(err);
        return;
    }

    constexpr int64_t MERGE_THRESHOLD_US = 2'000'000;

    for (const timeFragment& frag : batch)
    {
        /* 3. 查询最近一条 */
        const char* sqlSel = R"(
            SELECT id, end_time, duration
            FROM reading_time
            WHERE path = ? AND current_chapter = ? AND spine_id = ?
            ORDER BY end_time DESC
            LIMIT 1;
        )";
        sqlite3_stmt* sel = nullptr;
        if (sqlite3_prepare_v2(m_dbTime, sqlSel, -1, &sel, nullptr) != SQLITE_OK)
        {
            OutputDebugStringA(("prepare SELECT failed\n"));
            continue;
        }
        sqlite3_bind_text(sel, 1, frag.path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(sel, 2, frag.chapter.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(sel, 3, frag.spine_id);

        bool merged = false;
        if (sqlite3_step(sel) == SQLITE_ROW)
        {
            int     oldId = sqlite3_column_int(sel, 0);
            int64_t oldEnd = sqlite3_column_int64(sel, 1);
            if (frag.timestamp - oldEnd <= MERGE_THRESHOLD_US)
            {
                const char* sqlUpd = R"(
                    UPDATE reading_time
                    SET end_time = ?,
                        duration = duration + (? - end_time)
                    WHERE id = ?;
                )";
                sqlite3_stmt* upd = nullptr;
                if (sqlite3_prepare_v2(m_dbTime, sqlUpd, -1, &upd, nullptr) == SQLITE_OK)
                {
                    sqlite3_bind_int64(upd, 1, frag.timestamp);
                    sqlite3_bind_int64(upd, 2, frag.timestamp);
                    sqlite3_bind_int(upd, 3, oldId);
                    if (sqlite3_step(upd) != SQLITE_DONE)
                        OutputDebugStringA(("UPDATE step failed\n"));
                    sqlite3_finalize(upd);
                }
                else
                {
                    OutputDebugStringA(("prepare UPDATE failed\n"));
                }
                merged = true;
            }
        }
        sqlite3_finalize(sel);

        if (!merged)
        {
            const char* sqlIns = R"(
                INSERT INTO reading_time
                (path, title, authors, spine_id, current_chapter,
                 start_time, end_time, duration)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?);
            )";
            sqlite3_stmt* ins = nullptr;
            if (sqlite3_prepare_v2(m_dbTime, sqlIns, -1, &ins, nullptr) == SQLITE_OK)
            {
                sqlite3_bind_text(ins, 1, frag.path.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(ins, 2, frag.title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(ins, 3, frag.author.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(ins, 4, frag.spine_id);
                sqlite3_bind_text(ins, 5, frag.chapter.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int64(ins, 6, frag.timestamp);
                sqlite3_bind_int64(ins, 7, frag.timestamp);
                sqlite3_bind_int64(ins, 8, 0);

                if (sqlite3_step(ins) != SQLITE_DONE)
                    OutputDebugStringA(("INSERT step failed\n"));
                sqlite3_finalize(ins);
            }
            else
            {
                OutputDebugStringA(("prepare INSERT failed\n"));
            }
        }
    }

    /* 4. 提交事务 */
    if (sqlite3_exec(m_dbTime, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        OutputDebugStringA(("COMMIT failed: " + std::string(err) + "\n").c_str());
        sqlite3_free(err);
    }
}
void CALLBACK OnFlush(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    // 直接在工作线程/回调里刷新
    OutputDebugStringA("OnFlush\n");
    if (g_recorder) { g_recorder->flush(); }
    g_flushTimer = 0;
}
void ReadingRecorder::updateRecord()
{
    if (m_book_record.id < 0) { return; }
 
    if (g_book)
    {

        m_book_record.enableCSS = g_cfg.enableCSS;
        m_book_record.enableGlobalCSS = g_cfg.enableGlobalCSS;
        m_book_record.enableCustomFont = g_cfg.enableCustomFont;
        m_book_record.fontName = w2a(g_cfg.font_name);
 

        m_book_record.fontSize = g_cfg.font_size;
        m_book_record.lineHeightMul = g_cfg.line_height;
        m_book_record.docWidth = g_cfg.document_width;
        m_book_record.totalTime += 1;


        if (m_book_record.title.empty() && g_book && !g_book->ocf_pkg_.meta.empty())
        {
            auto titIt = g_book->ocf_pkg_.meta.find(L"dc:title");
            m_book_record.title = titIt != g_book->ocf_pkg_.meta.end() ? w2a(titIt->second) : "";
        }
        if(m_book_record.author.empty()&& g_book && !g_book->ocf_pkg_.meta.empty())
        {
            auto authIt = g_book->ocf_pkg_.meta.find(L"dc:creator");
            m_book_record.author = authIt != g_book->ocf_pkg_.meta.end() ? w2a(authIt->second) : "";
        }

        timeFragment tf;
        tf.path = m_book_record.path;
        tf.title = m_book_record.title;
        tf.author = m_book_record.author;
        tf.timestamp = nowUs();
        
        if (g_vd) {
            ScrollPosition p = g_vd->get_scroll_position();
            m_book_record.lastSpineId = p.spine_id;
            m_book_record.lastOffset = p.offset;
            
            tf.spine_id = p.spine_id;
            tf.chapter = w2a(g_book->get_chapter_name_by_id(p.spine_id));
        }
        m_time_frag.push_back(std::move(tf));
        if (!g_flushTimer)
        {
            g_flushTimer = timeSetEvent(g_cfg.record_flush_interval_ms, 0, OnFlush, 0, TIME_ONESHOT);
        }


        m_setting_record.displayFrameRate = g_cfg.displayFrameRate;
        m_setting_record.displayScrollBar = g_cfg.displayScrollBar;
        m_setting_record.displayStatusBar = g_cfg.displayStatusBar;
        m_setting_record.displayTOC = g_cfg.displayTOC;
        m_setting_record.enableClickPreview = g_cfg.enableClickPreview;
        m_setting_record.enableFontRealtimePreview = g_cfg.enableFontRealtimePreview;
        m_setting_record.enableHoverPreview = g_cfg.enableHoverPreview;
        m_setting_record.enableLoadEPUBFonts = g_cfg.enableEPUBFonts;
        m_setting_record.enableScrollAnimation = g_cfg.enableScrollAnimation;
    }
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




using namespace tinyxml2;


/* ---------- 内部实现 ---------- */
class MathML2SVG::Impl {
public:
    /* 样式结构体 —— 只在 Impl 内部可见 */


    /* 策略表 */
    using RenderFn = std::function<std::string(const tinyxml2::XMLElement*, const Style&)>;
    using AttrFn = void(*)(const class tinyxml2::XMLAttribute*, class Style&);

    std::unordered_map<std::string, RenderFn> tagRender;
    std::unordered_map<std::string, AttrFn>   attrApply;
    std::mutex                                mtx;
    bool m_usePath = true;   // 外部可改
    Impl() { registerAll(); }

    /* 线程安全注册 */
    void registerTag(const std::string& tag, RenderFn fn) {
        std::lock_guard<std::mutex> lock(mtx);
        tagRender[tag] = std::move(fn);
    }
    void registerAttr(const std::string& attr, AttrFn fn) {
        std::lock_guard<std::mutex> lock(mtx);
        attrApply[attr] = fn;
    }

    std::string cleanup(std::string svg) {
        std::regex re(R"(\s*data-w="[^"]*")");
        return std::regex_replace(svg, re, "");
    }

    /* 主转换 */
    std::string convert(const std::string& mathml) {
        tinyxml2::XMLDocument doc;
        if (doc.Parse(mathml.c_str()) != XML_SUCCESS)
            return "<!-- parse error -->";

        XMLElement* root = doc.RootElement();
        if (!root) return "<!-- empty -->";

        Style st;
        std::string inner = renderElement(root, st);
        double ascent = extractAscent(inner);
        double descent = extractDescent(inner);
        double height = ascent - descent;
        double width = extractWidth(inner);
        double em = std::stod(st.fontSize);
        double margin = 0.25* em;
        double x = margin;
        double y = margin + ascent;

        std::ostringstream svg;
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<< width + margin
            <<"\" height=\"" << height + margin << "\">"
            << "<g transform = \"translate(" << x << "," << y << ")\">" << inner << "</g>"
            << "</svg>";
        return (svg.str());
    }

private:

    static std::string xmlEscape(std::string_view raw)
    {
        std::string out;
        out.reserve(raw.size() + 16);          // 小优化
        for (char c : raw)
        {
            switch (c)
            {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
            }
        }
        return out;
    }
    static std::wstring extractPlainText(const tinyxml2::XMLElement* e) {
        std::wstring out;
        if (!e) return out;

        // 深度优先收集所有文本节点
        if (const char* txt = e->GetText())
            out += a2w(txt);

        for (const tinyxml2::XMLElement* c = e->FirstChildElement();
            c; c = c->NextSiblingElement()) {
            out += extractPlainText(c);
        }
        return out;
    }

    /* ---------- 工具函数 ---------- */
    static std::string textRender(const tinyxml2::XMLElement* e, const Style& st)
    {
        std::string txt = e->GetText() ? e->GetText() : "";
        for (size_t pos = 0;
            (pos = txt.find("&nbsp;", pos)) != std::string::npos; )
        {
            txt.replace(pos, 6, " ");   // 普通空格 U+0020
            ++pos;                      // 继续向后找
        }

        std::wstring wtxt = a2w(txt);

        // 1. 精确测量
        auto si = FreeTypeTextMeasurer::instance().measure(
            wtxt, st.fontFamily, std::stof(st.fontSize));

        // 2. 生成裸 <text>（相对于基线原点）
        std::ostringstream os;
        os << "<text x=\"0\" y=\"" << 0 << "\""
            << " font-size=\"" << st.fontSize
            << "\" font-family=\"" << w2a(st.fontFamily)
            << "\" fill=\"" << st.fill << "\">"
            << xmlEscape(txt)
            << "</text>";

        // 3. 用 <g> 包一层，把尺寸放在 g 的 data-* 上
        std::ostringstream finalOSS;
        finalOSS << "<g data-w=\"" << si.width
            << "\" data-asc=\"" << si.ascent
            << "\" data-desc=\"" << si.descent << "\">"
            << os.str()
            << "</g>";
        return finalOSS.str();
    }
    static double getDimAttr(const tinyxml2::XMLElement* e,
        const char* name,
        double defVal /*em*/)
    {
        const char* v = e->Attribute(name);
        if (!v) return defVal;
        std::string s = v;
        // 去掉单位，只保留数字
        if (s.back() == 'e' || s.back() == 'm') s.pop_back();
        if (s.empty()) return defVal;
        return std::stod(s);
    }
    static double extractWidth(const std::string& svg) {
        const char* tag = "data-w=\"";
        size_t pos = svg.find(tag);      
        if (pos == std::string::npos) return 0;
        pos += strlen(tag);
        size_t end = svg.find('"', pos);
        return std::stod(svg.substr(pos, end - pos));
    }

    static double extractAscent(const std::string& svg) {
        const char* tag = "data-asc=\"";
        size_t pos = svg.find(tag);
        if (pos == std::string::npos) return 0;
        pos += strlen(tag);
        size_t end = svg.find('"', pos);
        return std::stod(svg.substr(pos, end - pos));
    }
    static double extractDescent(const std::string& svg) {
        const char* tag = "data-desc=\"";
        size_t pos = svg.find(tag);
        if (pos == std::string::npos) return 0;
        pos += strlen(tag);
        size_t end = svg.find('"', pos);
        return std::stod(svg.substr(pos, end - pos));
    }

    static std::string hbox(const std::vector<std::string>& parts,
        double dx = 2.0, std::string tag_name = "hbox")
    {
        if (parts.empty())
            return R"(<g data-w="0"  data-asc="0" data-desc="0" />)";

        double totalW = 0.0;
        double asc = 0.0, des = 0.0;

     
        for (const auto& p : parts)
        {
            totalW += extractWidth(p);
            asc = std::max(asc, extractAscent(p));
            des = std::min(des, extractDescent(p));
        }
        totalW += dx * (parts.size() - 1); 

        /* 拼 SVG：所有子元素 y=0 对齐 */
        std::ostringstream os;
        os << "<g class=\"" << tag_name << "\" data-w=\"" << totalW
            << "\" data-asc=\"" << asc
            << "\" data-desc=\"" << des << "\" >";

        double x = 0.0;
        for (const auto& p : parts)
        {
            os << "<g transform=\"translate(" << x << ",0)\">" << p << "</g>";
            x += extractWidth(p) + dx;
        }
        os << "</g>";
        return os.str();
    }
    static std::string vbox(const std::vector<std::string>& parts,
        double dy=2.0, std::string tag_name = "vbox")
    {
        if (parts.empty())
            return R"(<g data-w="0"  data-asc="0" data-desc="0" />)";

        double width = 0.0;
        double asc = 0.0, des = 0.0;
        double totalH = 0.0;

        for (const auto& p : parts)
        {
            width = std::max(width, extractWidth(p));
            totalH += (extractAscent(p) - extractDescent(p));
        }
        totalH += dy * (parts.size() - 1);
        asc = totalH * 0.5;
        des = -(totalH - asc);
        /* 拼 SVG：所有子元素 y=0 对齐 */
        std::ostringstream os;
        os << "<g class=\"" << tag_name << "\" data-w=\"" << width
            << "\" data-asc=\"" << asc
            << "\" data-desc=\"" << des << "\" >";

        double y = -asc;
        for (const auto& p : parts)
        {
            os << "<g transform=\"translate(0, " << y+ extractAscent(p) << ")\">" << p << "</g>";
            y += (extractAscent(p) - extractDescent(p)) + dy;

        }
        os << "</g>";
        return os.str();
    }


    /* ---------- 递归渲染 ---------- */
    std::string renderElement(const XMLElement* e, Style st) {
        //DbgPrint("[renderElement] <%s>\n", e->Name());
        /* 1. 处理属性 */
        for (const XMLAttribute* a = e->FirstAttribute(); a; a = a->Next()) {
            auto it = attrApply.find(a->Name());
            if (it != attrApply.end()) it->second(a, st);
        }

        /* 2. 如果是文本类节点，直接渲染，不再递归子元素 */
        const char* tag = e->Name();
        if (!strcmp(tag, "mtext") || !strcmp(tag, "mo") || !strcmp(tag, "mi") || !strcmp(tag, "mn")) {
            return textRender(e, st);
        }

        /* 3. 容器节点：递归子元素 */
        std::vector<std::string> children;
        for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
            children.push_back(renderElement(c, st));
        }

        /* 4. 查找是否有注册的渲染器（如 mfrac、msup 等） */
        auto it = tagRender.find(tag);
        if (it != tagRender.end()) {
            return it->second(e, st);
        }

        /* 5. 默认水平排列子节点 */
        return hbox(children, 2.0, "math");
    }
    /* ---------- 注册表 ---------- */
    void registerAll() {
        /* 记号类 */
        registerTag("mi", textRender);
        registerTag("mn", textRender);
        registerTag("mo", textRender);
        registerTag("ms", textRender);
        registerTag("mtext", textRender);

        /* 布局类 */
        registerTag("mrow",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> boxes;
                /* 1. 渲染所有子元素并收集尺寸 */
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                {
                    boxes.push_back(renderElement(c, st));
                }
                return hbox(boxes, 2.0, "mrow");
            });
        registerTag("mfrac",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 子元素 ---------- */
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- mfrac needs 2 children -->";

                /* ---------- 2. 解析属性 ---------- */
                double thickness = 1.0;
                const char* lt = e->Attribute("linethickness");
                if (lt) {
                    std::string val = lt;
                    if (val == "thin")        thickness = 0.5;
                    else if (val == "medium") thickness = 1.0;
                    else if (val == "thick")  thickness = 2.0;
                    else if (val.back() == 'x' || val.back() == 'X')
                        thickness = std::stod(val.substr(0, val.size() - 2));
                    else if (val.back() == 't' || val.back() == 'T')
                        thickness = std::stod(val.substr(0, val.size() - 2)) * 1.33;
                    else if (val.back() == 'm' || val.back() == 'M')
                        thickness = std::stod(val.substr(0, val.size() - 2)) * std::stof(st.fontSize);
                    else
                        thickness = std::stod(val);
                }
                std::string numAlign = e->Attribute("numalign") ? e->Attribute("numalign") : "center";
                std::string denAlign = e->Attribute("denomalign") ? e->Attribute("denomalign") : "center";

                /* ---------- 3. 精确盒尺寸 ---------- */
                double numW = extractWidth(kids[0]);
                double numAsc = extractAscent(kids[0]);
                double numDesc = extractDescent(kids[0]);

                double denW = extractWidth(kids[1]);
                double denAsc = extractAscent(kids[1]);
                double denDesc = extractDescent(kids[1]);

                /* ---------- 4. TeX 参数 ---------- */
                double em = std::stof(st.fontSize);
                double y_shift = 0.3 * em;
                double rule = thickness;
                double gap = 0.3 * em;

                /* ---------- 5. 垂直距离（分数线 y = 0） ---------- */
     

                double ascent = (numAsc-numDesc) + gap + rule/2.0 + y_shift;   // 分子最上沿到分数线
                double descent = -((denAsc - denDesc) + gap + rule/2.0) + y_shift;   // 分数线到分母最下沿
   

                /* ---------- 6. 水平对齐 ---------- */
                double ruleW = std::max(numW, denW);
                auto offset = [](double w, double ruleW, const std::string& align)
                    {
                        if (align == "left")  return 0.0;
                        if (align == "right") return ruleW - w;
                        return (ruleW - w) * 0.5;
                    };
                double numX = offset(numW, ruleW, numAlign);
                double denX = offset(denW, ruleW, denAlign);

                /* ---------- 7. 分子、分母相对于分数线 y = 0 的 y ---------- */
                double lineY = -y_shift;
                double numY = -(-numDesc + rule * 0.5 + gap + y_shift);   // 分子基线
                double denY = (rule * 0.5 + denAsc + gap - y_shift);  // 分母基线
                
                /* ---------- 8. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"mfrac\" data-w=\"" << ruleW
                    << "\" data-asc=\"" << ascent
                    << "\" data-desc=\"" << descent << "\">";
                oss << "<g transform=\"translate(" << numX << "," << numY << ")\">" << kids[0] << "</g>";
                oss << "<line x1=\"0\" y1=\""<< lineY << "\" x2=\"" << ruleW << "\" y2=\"" << lineY << "\" "
                    << "stroke=\"black\" stroke-width=\"" << rule << "\"/>";
                oss << "<g transform=\"translate(" << denX << "," << denY << ")\">" << kids[1] << "</g>";
                oss << "</g>";
                return oss.str();
            });
        registerTag("msup",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- msup needs 2 children -->";

                /* ---------- 1. 主体尺寸 ---------- */
                double baseW = extractWidth(kids[0]);
                double baseAsc = extractAscent(kids[0]);
                double baseDes = extractDescent(kids[0]);


                /* ---------- 2. 上标原始尺寸 ---------- */
                double supW0 = extractWidth(kids[1]);
                double supAsc0 = extractAscent(kids[1]);
                double supDes0 = extractDescent(kids[1]);
                
     
                double em = std::stod(st.fontSize);
                double y_shift = 0.5 * em;
                /* ---------- 3. 缩放后尺寸 ---------- */
                const double scale = 0.7;
                double supW = supW0 * scale;
                double supAsc = supAsc0 * scale;
                double supDes = supDes0 * scale;

                /* ---------- 4. 上标位置 ---------- */
   
                double supX = baseW;                               // 右上角
                /* 补偿缩放导致的基线偏移：先平移 -supBase0*scale，再整体放到 gap 上方 */
                double supY = -(baseAsc - supDes) ;

                /* ---------- 5. 整体盒 ---------- */
                double totalW = baseW + supW;
                double totalAsc = baseAsc + (supAsc-supDes);   // 最上沿
                double totalDes = baseDes;                           // 最下沿

                /* ---------- 6. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"msup\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes<< "\">"   
                    << kids[0]                                         // 主体：已位于 baseBaseline
                    << "<g transform=\"translate(" << supX << "," << supY << ") scale(" << scale << ")\">"
                    << kids[1]                                         // 上标：相对位移
                    << "</g>"
                    << "</g>";
                return oss.str();
            });
        registerTag("msub",
            [this](const XMLElement* e, const Style& st) -> std::string {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() < 2) return kids.empty() ? "" : kids[0];

                /* ---------- 1. 主体尺寸 ---------- */
                double baseW = extractWidth(kids[0]);
                double baseAsc = extractAscent(kids[0]);
                double baseDes = extractDescent(kids[0]);
   

                /* ---------- 2. 下标原始尺寸 ---------- */
                double subW0 = extractWidth(kids[1]);
                double subAsc0 = extractAscent(kids[1]);
                double subDes0 = extractDescent(kids[1]);


                /* ---------- 3. 缩放后尺寸 ---------- */
                const double scale = 0.7;
            
                //const double drop = 0.25 * baseDes;         // 下标基线相对主体基线的下移量

                double subW = subW0 * scale;
                double subAsc = subAsc0 * scale ;
                double subDes = subDes0 * scale ;

                /* ---------- 4. 下标位置 ---------- */
                double subX = baseW ;                     // 右下角
                /* 补偿缩放导致的基线偏移：先平移 -subBaseline0*scale，再整体下移 drop */
                double subY =  subAsc - baseDes;

                /* ---------- 5. 整体盒尺寸 ---------- */
                double totalW = subX + subW;
                double totalAsc = baseAsc;                     // 最上沿
                double totalDes = baseDes - (subAsc - subDes); // 最下沿

                /* ---------- 6. 组装 ---------- */
                std::ostringstream os;
                os << "<g class=\"msub\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\">"
                    << kids[0]                                  // 主体：基线 y = 0
                    << "<g transform=\"translate(" << subX << "," << subY << ") scale(" << scale << ")\">"
                    << kids[1]                                  // 下标：已补偿基线
                    << "</g>"
                    << "</g>";
                return os.str();
            });
        registerTag("msubsup",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() < 3) return kids.empty() ? "" : kids[0];

                /* ---------- 1. 主体尺寸 ---------- */
                double baseW = extractWidth(kids[0]);
                double baseAsc = extractAscent(kids[0]);
                double baseDes = extractDescent(kids[0]);
  

                /* ---------- 2. 上标原始尺寸 ---------- */
                double supW0 = extractWidth(kids[1]);
                double supAsc0 = extractAscent(kids[1]);
                double supDes0 = extractDescent(kids[1]);
       
                /* ---------- 3. 下标原始尺寸 ---------- */
                double subW0 = extractWidth(kids[2]);
                double subAsc0 = extractAscent(kids[2]);
                double subDes0 = extractDescent(kids[2]);


                /* ---------- 4. 缩放后尺寸 ---------- */
                const double scale = 0.7;


                double supW = supW0 * scale;
                double supAsc = supAsc0 * scale ;
                double supDes = supDes0 * scale ;

                double subW = subW0 * scale;
                double subAsc = subAsc0 * scale ;
                double subDes = subDes0 * scale ;

                /* ---------- 5. 上标位置 ---------- */
                double supX = baseW ;
                /* 补偿缩放导致的基线偏移：先平移 -supBase0*scale，再整体放到 shiftUp 上方 */
                double supY = baseAsc - supDes;

                /* ---------- 6. 下标位置 ---------- */
                double subX = baseW;
                /* 补偿缩放导致的基线偏移：先平移 -subBase0*scale，再整体放到 shiftDown 下方 */
                double subY = -(supAsc-baseDes);

                /* ---------- 7. 整体盒尺寸 ---------- */
                double totalW = std::max(baseW, supX + std::max(supW, subW));
                double totalAsc = baseAsc + (supAsc - supDes);   // 最上沿
                double totalDes = baseDes - (supAsc - supDes); // 最下沿

                /* ---------- 8. 组装 ---------- */
                std::ostringstream os;
                os << "<g class=\"msubsup\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\">"
                    << kids[0]                                   // 主体：基线 y = 0
                    << "<g transform=\"translate(" << supX << "," << supY << ") scale(" << scale << ")\">"
                    << kids[1]                                   // 上标：已补偿基线
                    << "</g>"
                    << "<g transform=\"translate(" << subX << "," << subY << ") scale(" << scale << ")\">"
                    << kids[2]                                   // 下标：已补偿基线
                    << "</g>"
                    << "</g>";
                return os.str();
            });
        registerTag("msqrt",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 收集子元素 ---------- */
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.empty()) return "<!-- msqrt needs at least 1 child -->";

                /* ---------- 2. 水平拼接子元素 ---------- */
                std::string inner = hbox(kids);

                /* ---------- 3. 内容尺寸 ---------- */
                double em = std::stof(st.fontSize);
                double rule = 1.0;                       // 线厚
                double gap = 0.2 * em;                // 根号与内容间隙
                double hook = 0.4 * em;                // 左侧小勾宽度

                double contW = extractWidth(inner);
                double contAsc = extractAscent(inner);
                double contDes = extractDescent(inner);

                /* ---------- 4. 根号盒高（以内容基线为 0） ---------- */
                double boxAsc = contAsc + gap + rule;           // 内容顶部到基线
                double boxDes = contDes ;           // 基线到内容底部


                /* ---------- 5. 根号路径（相对于内容基线） ---------- */
                double left = hook;                     // 根号左侧勾起点
                double right = left + contW + gap;       // 根号横线终点
                double barY = -(contAsc + gap);                  // 横线 y 坐标（负值，在基线上方）

                std::ostringstream path;
                path << "M" << 0 << "," << -contAsc*0.5
                    << "L" << left << "," << 0
                    << "L" << (left) << "," << barY
                    << "L" << right << "," << barY ;

                /* ---------- 6. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"msqrt\" data-w=\"" << (right + rule)
                    << "\" data-asc=\"" << boxAsc
                    << "\" data-desc=\"" << boxDes << "\">";
                oss << "<path d=\"" << path.str()
                    << "\" stroke=\"black\" fill=\"none\" stroke-width=\"" << rule << "\"/>";
                oss << "<g transform=\"translate(" << left + gap << ",0)\">"   // 内容基线 y = 0
                    << inner << "</g>";
                oss << "</g>";
                return oss.str();
            });
        registerTag("mroot",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 收集子元素 ---------- */
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- mroot needs exactly 2 children -->";

                /* ---------- 2. 被开方内容尺寸（基线 = 0） ---------- */
                double innerW = extractWidth(kids[0]);
                double innerAsc = extractAscent(kids[0]);
                double innerDes = extractDescent(kids[0]);

                /* ---------- 3. 指数原始尺寸 ---------- */
                double idxW0 = extractWidth(kids[1]);
                double idxAsc0 = extractAscent(kids[1]);
                double idxDes0 = extractDescent(kids[1]);
                double idxBase0 = 0;

                /* ---------- 4. 缩放后指数尺寸 ---------- */
                const double scale = 0.7;
                double idxW = idxW0 * scale;
                double idxAsc = idxAsc0 * scale;
                double idxDes = idxDes0 * scale;

                /* ---------- 5. 根号几何参数 ---------- */
                const double pad = 2.0;   // 内边距
                const double bar = 1.2;   // 横线超出系数
                const double vgap = 2.0;   // 横线与内容顶部间距
                const double tick = 6.0;   // 勾线水平段
                const double idxGap = 1.0;  // 指数与勾线空隙

                /* ---------- 6. 整体盒尺寸（以被开方内容基线为 0） ---------- */
                double bodyAsc = innerAsc + vgap + pad;   // 被开方内容顶部到基线
                double bodyDes = innerDes + pad;          // 被开方内容底部到基线
                double totalH = bodyAsc + bodyDes;
                double totalW = tick + innerW * bar + pad * 2;

                /* ---------- 7. 指数位置（基线对齐整体基线） ---------- */
                double idxX = -idxW - idxGap;   // 指数左上角 x
                /* 补偿缩放导致的基线偏移：先平移 -idxBase0*scale，再整体放到勾线左侧 */
                double idxY = -idxAsc - idxBase0 * scale;

                /* ---------- 8. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"mroot\" data-w=\"" << totalW
                    << "\" data-h=\"" << totalH
                    << "\" data-asc=\"" << bodyAsc
                    << "\" data-desc=\"" << bodyDes
                    << "\" data-baseline=\"0\">"
                    /* 指数：基线对齐整体基线（y = 0） */
                    << "<g transform=\"translate(" << idxX << "," << idxY << ") scale(" << scale << ")\">"
                    << kids[1] << "</g>"
                    /* 根号勾线 + 横线（相对于基线） */
                    << "<path d=\"M0," << bodyAsc
                    << " L" << tick << "," << -vgap
                    << " L" << tick + innerW * bar << "," << -vgap
                    << "\" stroke=\"black\" fill=\"none\" stroke-width=\"1\"/>"
                    /* 被开方内容：左上角对齐勾线右侧，基线保持 y = 0 */
                    << "<g transform=\"translate(" << tick << ",0)\">"
                    << kids[0] << "</g>"
                    << "</g>";
                return oss.str();
            });
        /* 属性 */
        registerAttr("mathcolor", [](const XMLAttribute* a, Style& st) {
            st.fill = a->Value();
            });
        registerAttr("mathsize", [](const XMLAttribute* a, Style& st) {
            st.fontSize = a->Value();
            });
        registerAttr("mathvariant", [](const XMLAttribute* a, Style& st) {
            const char* v = a->Value();
            if (!strcmp(v, "italic")) st.fontStyle = "italic";
            else if (!strcmp(v, "bold")) st.fontWeight = "bold";
            });

/* ---------- mtd ---------- */
        registerTag("mtd",
            [this](const XMLElement* e, const Style& st) -> std::string {
                std::ostringstream inner;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    inner << renderElement(c, st);

                std::string s = inner.str();
                std::ostringstream os;
                os << "<g data-w=\"" << extractWidth(s)
                    << "\" data-asc=\"" << extractAscent(s)
                    << "\" data-desc=\"" << extractDescent(s) << "\">"
                    << s << "</g>";
                return os.str();
            });

        /* ---------- mtr ---------- */
        registerTag("mtr",
            [this](const XMLElement* e, const Style& st) -> std::string {
                std::ostringstream oss;
                for (const XMLElement* c = e->FirstChildElement("mtd"); c; c = c->NextSiblingElement("mtd"))
                    oss << renderElement(c, st);   // 每个 <mtd> 已自带 metrics
                return oss.str();
            });
        registerTag("mtable",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 解析属性 ---------- */
                const double defaultGap = 8.0;
                std::vector<double> colGap = { defaultGap };
                std::vector<double> rowGap = { defaultGap };

                auto parseList = [](const char* s, std::vector<double>& v, double def)
                    {
                        if (!s) return;
                        std::stringstream ss(s);
                        v.clear();
                        for (std::string t; std::getline(ss, t, ' '); )
                            v.push_back(std::stod(t));
                        if (v.empty()) v.push_back(def);
                    };
                parseList(e->Attribute("columnspacing"), colGap, defaultGap);
                parseList(e->Attribute("rowspacing"), rowGap, defaultGap);

                /* ---------- 2. 收集所有单元格 ---------- */
                std::vector<std::vector<std::string>> grid;   // 每行每列的 SVG 片段
                std::vector<std::vector<double>> wGrid, hGrid;
                size_t rows = 0, cols = 0;

                for (const XMLElement* r = e->FirstChildElement("mtr"); r; r = r->NextSiblingElement("mtr")) {
                    grid.emplace_back();
                    wGrid.emplace_back();
                    hGrid.emplace_back();
                    auto& rowCells = grid.back();
                    auto& rowW = wGrid.back();
                    auto& rowH = hGrid.back();

                    for (const XMLElement* c = r->FirstChildElement("mtd"); c; c = c->NextSiblingElement("mtd")) {
                        std::string cell = renderElement(c, st);   // 已带 data-w / data-asc / data-desc
                        rowCells.push_back(cell);
                        rowW.push_back(extractWidth(cell));
                        rowH.push_back(extractAscent(cell) - extractDescent(cell));
                    }
                    rows++;
                    cols = std::max(cols, rowCells.size());
                }
                if (rows == 0) return "<!-- empty mtable -->";

                /* ---------- 3. 统一列宽、行高 ---------- */
                std::vector<double> colW(cols, 0.0);
                std::vector<double> rowH(rows, 0.0);

                for (size_t r = 0; r < rows; ++r)
                    for (size_t c = 0; c < grid[r].size(); ++c)
                        colW[c] = std::max(colW[c], wGrid[r][c]);

                for (size_t r = 0; r < rows; ++r) {
                    double h = 0.0;
                    for (size_t c = 0; c < grid[r].size(); ++c)
                        h = std::max(h, hGrid[r][c]);
                    rowH[r] = h;
                }

                /* ---------- 4. 整体尺寸 ---------- */
                double totalW = std::accumulate(colW.begin(), colW.end(), 0.0)
                    + (cols ? (cols - 1) * colGap[0] : 0.0);
                double totalH = std::accumulate(rowH.begin(), rowH.end(), 0.0)
                    + (rows ? (rows - 1) * rowGap[0] : 0.0);

                double asc = totalH * 0.5;
                double des = -(totalH - asc);

                /* ---------- 5. 绝对坐标摆放 ---------- */
                std::ostringstream os;
                os << "<g class=\"mtable\" data-w=\"" << totalW
                    << "\" data-asc=\"" << asc
                    << "\" data-desc=\"" << des << "\">";

                double y = -asc;
                for (size_t r = 0; r < rows; ++r) {
                    double x = 0.0;
                    for (size_t c = 0; c < grid[r].size(); ++c) {
                        double dy = (rowH[r] - hGrid[r][c]) * 0.5;   // 垂直居中
                        os << "<g transform=\"translate(" << x << "," << (y + dy + extractAscent(grid[r][c])) << ")\">"
                            << grid[r][c]
                            << "</g>";
                        x += colW[c] + (c + 1 < cols ? colGap[0] : 0.0);
                    }
                    y += rowH[r] + (r + 1 < rows ? rowGap[0] : 0.0);
                }
                os << "</g>";
                return os.str();
            });
      
        registerTag("mlabeledtr",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> cells;
                for (const XMLElement* c = e->FirstChildElement("mtd");
                    c; c = c->NextSiblingElement("mtd"))
                    cells.push_back(renderElement(c, st));

                if (cells.empty()) return "<!-- empty mlabeledtr -->";

                /* 列间距 */
                double colGap = 4.0;
                if (const char* gap = e->Attribute("columnspacing"))
                    colGap = std::stod(gap);

                /* 用 hbox 横向排布，但手动计算基线对齐 */
                double totalW = 0;
                double maxAsc = 0;
                double maxDes = 0;
                for (const auto& cell : cells) {
                    totalW += extractWidth(cell);
                    maxAsc = std::max(maxAsc, extractAscent(cell));
                    maxDes = std::max(maxDes, extractDescent(cell));
                }
                totalW += colGap * (cells.size() - 1);

                std::ostringstream oss;
                oss << "<g class=\"mlabeledtr\" data-w=\"" << totalW
                    << "\" data-h=\"" << (maxAsc + maxDes)
                    << "\" data-asc=\"" << maxAsc
                    << "\" data-desc=\"" << maxDes
                    << "\" data-baseline=\"0\">";

                double x = 0;
                for (const auto& cell : cells) {
                    double dx = x;
                    /* 垂直按基线对齐：单元格内部基线 0 → 行基线 0 */
                    double dy = maxAsc - extractAscent(cell);
                    oss << "<g transform=\"translate(" << dx << "," << dy << ")\">"
                        << cell << "</g>";
                    x += extractWidth(cell) + colGap;
                }
                oss << "</g>";
                return oss.str();
            });
        registerTag("mmultiscripts",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 收集节点 ---------- */
                std::vector<const XMLElement*> nodes;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    nodes.push_back(c);
                if (nodes.empty()) return "";

                /* ---------- 2. 分区 ---------- */
                size_t split = nodes.size();
                for (size_t i = 0; i < nodes.size(); ++i)
                    if (std::string(nodes[i]->Name()) == "mprescripts") { split = i; break; }

                const XMLElement* baseNode = nodes[0];
                std::vector<const XMLElement*> postNodes(nodes.begin() + 1, nodes.begin() + split);   // post: sub1 sup1 sub2 sup2 ...
                std::vector<const XMLElement*> preNodes(nodes.begin() + split + 1, nodes.end());      // pre : sup1 sub1 sup2 sub2 ...

                /* ---------- 3. 主字符 ---------- */
                std::string baseSVG = renderElement(baseNode, st);
                double baseW = extractWidth(baseSVG);
                double baseAsc = extractAscent(baseSVG);
                double baseDes = extractDescent(baseSVG);

                /* ---------- 4. 常量 ---------- */
                const double scale = 0.7;
                const double kern = 1.0;
                const double supGap = 1.5;
                const double subGap = 1.0;
                const double pairGap = 1.0;

                /* ---------- 5. 工具：一次性渲染并缓存尺寸 ---------- */
                struct ScriptRec {
                    std::string svg;
                    double w, asc, des;   // asc>0, des<0
                };
                auto makeRec = [&](const XMLElement* el) -> ScriptRec
                    {
                        if (!el || std::string(el->Name()) == "none")
                            return { "", 0, 0, 0 };

                        std::string raw = renderElement(el, st);
                        double w = extractWidth(raw) * scale;
                        double asc = extractAscent(raw) * scale;
                        double des = extractDescent(raw) * scale;   // 负值

                        std::ostringstream os;
                        os << "<g data-w=\"" << w
                            << "\" data-asc=\"" << asc
                            << "\" data-desc=\"" << des << "\">"
                            << "<g transform=\"scale(" << scale << ")\">"
                            << raw << "</g></g>";

                        return { os.str(), w, asc, des };
                    };

                /* ---------- 6. 收集列 ---------- */
                std::vector<ScriptRec> preSub, preSup, postSub, postSup;
                double preW = 0;
                double postW = 0;

                /* pre 区段：sup1 sub1 sup2 sub2 ... */
                for (size_t i = 0; i + 1 < preNodes.size(); i += 2)
                {
                    ScriptRec sup = makeRec(preNodes[i]);
                    ScriptRec sub = makeRec(preNodes[i + 1]);
                    preSup.push_back(sup);
                    preSub.push_back(sub);
                    preW += std::max(sup.w, sub.w) + pairGap;
                }
                if (!preSup.empty()) preW -= pairGap;

                /* post 区段：sub1 sup1 sub2 sup2 ... */
                for (size_t i = 0; i + 1 < postNodes.size(); i += 2)
                {
                    ScriptRec sub = makeRec(postNodes[i]);
                    ScriptRec sup = makeRec(postNodes[i + 1]);
                    postSub.push_back(sub);
                    postSup.push_back(sup);
                    postW += std::max(sub.w, sup.w) + pairGap;
                }
                if (!postSub.empty()) postW -= pairGap;

                /* ---------- 7. 整体盒尺寸 ---------- */
                double totalW = preW + kern + baseW + kern + postW;

                double maxSup = baseAsc;
                double minSub = baseDes;
                auto update = [&](const ScriptRec& r)
                    {
                        maxSup = std::max(maxSup, baseAsc+r.asc);
                        minSub = std::min(minSub, baseDes+r.des);
                    };
                for (const auto& r : preSup) update(r);
                for (const auto& r : preSub) update(r);
                for (const auto& r : postSub) update(r);
                for (const auto& r : postSup) update(r);

                /* ---------- 8. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"mmultiscripts\" data-w=\"" << totalW
                    << "\" data-asc=\"" << maxSup
                    << "\" data-desc=\"" << minSub << "\">";

                double x = 0;

                /* pre 列（从右向左排） */
                for (int i = (int)preSup.size() - 1; i >= 0; --i)
                {
                    const ScriptRec& sup = preSup[i];
                    const ScriptRec& sub = preSub[i];
                    double w = std::max(sup.w, sub.w);
                    
                    double ySup = (supGap - sup.des);
                    double ySub = -(subGap + sub.asc);   // sub.des 负值
                    oss << "<g transform=\"translate(" << x << ",0)\">"
                        << "<g transform=\"translate(0," << ySup << ")\">" << sup.svg << "</g>"
                        << "<g transform=\"translate(0," << ySub << ")\">" << sub.svg << "</g>"
                        << "</g>";
                    x += w + pairGap;
                }

                /* 主字符 */
                oss << "<g transform=\"translate(" << x << ",0)\">" << baseSVG << "</g>";
                x += baseW + kern;

                /* post 列（从左向右排） */
                for (size_t i = 0; i < postSub.size(); ++i)
                {
                    const ScriptRec& sub = postSub[i];
                    const ScriptRec& sup = postSup[i];
                    double w = std::max(sub.w, sup.w);

                    double ySup = -(supGap + sup.asc);
                    double ySub = (subGap - sub.des);
                    oss << "<g transform=\"translate(" << x << ",0)\">"
                        << "<g transform=\"translate(0," << ySub << ")\">" << sub.svg << "</g>"
                        << "<g transform=\"translate(0," << ySup << ")\">" << sup.svg << "</g>"
                        << "</g>";
                    x += w + pairGap;
                }

                oss << "</g>";
                return oss.str();
            });
            registerTag("munder",
                [this](const XMLElement* e, const Style& st) -> std::string
                {
                    std::vector<std::string> kids;
                    for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                        kids.push_back(renderElement(c, st));
                    if (kids.size() != 2) return "<!-- munder needs 2 children -->";

                    const double em = std::stof(st.fontSize);
                    const double gap = 0.2 * em;
                    const double scale = 0.7;

                    const std::string& base = kids[0];
                    const std::string& sub = kids[1];

                    double baseW = extractWidth(base);
                    double baseAsc = extractAscent(base);
                    double baseDes = extractDescent(base);

                    double subW = extractWidth(sub) * scale;
                    double subAsc = extractAscent(sub) * scale;
                    double subDes = extractDescent(sub) * scale; 
        

                    /* ---------- 整体盒尺寸（以主字符基线为 0） ---------- */
                    double totalW = std::max(baseW, subW);
                    double baseX = (totalW - baseW) / 2.0;
                    double subX = (totalW - subW) / 2.0;

                    /* 下标位置：主字符底部 + 间隙，再补偿下标自身基线 */
                    double subY = (subAsc + gap - baseDes);

                    double totalAsc = baseAsc;                       // 最上沿
                    double totalDes = baseDes - gap - (subAsc - subDes); // 最下沿
     

                    std::ostringstream oss;
                    oss << "<g class=\"munder\" data-w=\"" << totalW
                        << "\" data-asc=\"" << totalAsc
                        << "\" data-desc=\"" << totalDes << "\" >";
                    oss << "<g transform=\"translate(" << baseX << ",0)\">" << base << "</g>";
                    oss << "<g transform=\"translate(" << subX << "," << subY << ")" << " scale(" << scale <<")\">" << sub << "</g>";
                    oss << "</g>";
                    return oss.str();
                });
            registerTag("munderover",
                [this](const XMLElement* e, const Style& st) -> std::string
                {
                    std::vector<std::string> kids;
                    for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                        kids.push_back(renderElement(c, st));
                    if (kids.size() != 3) return "<!-- munderover needs 3 children -->";

                    const double em = std::stof(st.fontSize);
                    const double gap = 0.2 * em;
                    const double scale = 0.7;

                    const std::string& base = kids[0];
                    const std::string& sub = kids[1];
                    const std::string& sup = kids[2];

                    /* 主字符 */
                    double baseW = extractWidth(base);
                    double baseAsc = extractAscent(base);
                    double baseDes = extractDescent(base);

                    /* 下标 */
                    double subW = extractWidth(sub) * scale;
                    double subAsc = extractAscent(sub) * scale;
                    double subDes = extractDescent(sub) * scale;


                    /* 上标 */
                    double supW = extractWidth(sup) * scale;
                    double supAsc = extractAscent(sup) * scale;
                    double supDes = extractDescent(sup) *scale;
  

                    /* ---------- 整体盒尺寸（以主字符基线为 0） ---------- */
                    double totalW = std::max({ baseW, subW, supW });
                    double baseX = (totalW - baseW) / 2.0;
                    double subX = (totalW - subW) / 2.0;
                    double supX = (totalW - supW) / 2.0;

                    /* 上标位置：主字符顶部上方 gap，再补偿上标自身基线 */
                    double supY = -(-supDes + baseAsc + gap);

                    /* 下标位置：主字符底部下方 gap，再补偿下标自身基线 */
                    double subY = (subAsc + gap - baseDes);

                    double totalAsc = baseAsc + gap + (supAsc - supDes); // 最上沿
                    double totalDes = baseDes - gap - (subAsc - subDes); // 最下沿
                    double totalH = totalAsc + totalDes;

                    std::ostringstream oss;
                    oss << "<g class=\"munderover\" data-w=\"" << totalW
                        << "\" data-asc=\"" << totalAsc
                        << "\" data-desc=\"" << totalDes << "\">";
                    oss << "<g transform=\"translate(" << supX << "," << supY << ") scale(" << scale<<  ")\">" << sup << "</g>";
                    oss << "<g transform=\"translate(" << baseX << ",0)\">" << base << "</g>";
                    oss << "<g transform=\"translate(" << subX << "," << subY << ") scale(" << scale << ")\">" << sub << "</g>";
                    oss << "</g>";
                    return oss.str();
                });
        // ------------------------------------------------------------------
// 1. mover  (上划线 / 下划线 / 上下箭头)
// 语法: <mover> base overscript </mover>
// ------------------------------------------------------------------
            registerTag("mover",
                [this](const XMLElement* e, const Style& st) -> std::string
                {
                    std::vector<std::string> kids;
                    for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                        kids.push_back(renderElement(c, st));
                    if (kids.size() != 2) return "<!-- mover needs 2 children -->";

                    const double scale = 0.7;
                    const double gap = 0.2 * std::stof(st.fontSize); // 0.2 em

                    const std::string& base = kids[0];
                    const std::string& over = kids[1];

                    /* 主字符 */
                    double baseW = extractWidth(base);
                    double baseAsc = extractAscent(base);
                    double baseDes = extractDescent(base);

                    /* 上标原始尺寸 */
                    double overW0 = extractWidth(over);
                    double overAsc0 = extractAscent(over);
                    double overDes0 = extractDescent(over);


                    /* 缩放后尺寸 */
                    double overW = overW0 * scale;
                    double overAsc = overAsc0 * scale;
                    double overDes = overDes0 * scale;

                    /* ---------- 整体盒尺寸（以主字符基线为 0） ---------- */
                    double totalW = std::max(baseW, overW);
                    double baseX = 0;                      // 主字符左上角 x
                    double overX = (totalW - overW) / 2.0;   // 上标居中

                    /* 上标位置：主字符顶部上方 gap，再补偿上标自身基线 */
                    double overY = -(-overDes + baseAsc + gap);

                    double totalAsc = baseAsc + gap + (overAsc - overDes); // 最上沿
                    double totalDes = baseDes;                           // 最下沿
                    double totalH = totalAsc + totalDes;

                    std::ostringstream oss;
                    oss << "<g class=\"mover\" data-w=\"" << totalW
                        << "\" data-asc=\"" << totalAsc
                        << "\" data-desc=\"" << totalDes
                        << "\">";
                    /* 主字符：基线 y=0 */
                    oss << "<g transform=\"translate(" << baseX << ",0)\">" << base << "</g>";
                    /* 上标：基线补偿后摆放 */
                    oss << "<g transform=\"translate(" << overX << "," << overY << ") scale(" << scale << ")\">"
                        << over << "</g>";
                    oss << "</g>";
                    return oss.str();
                });
        // ------------------------------------------------------------------
// 2. mpadded  (人工设置宽度/高度/深度)
// 语法: <mpadded width="..." height="..." depth="..."> child </mpadded>
// 单位：em，可省略符号
// ------------------------------------------------------------------
            registerTag("mpadded",
                [this](const XMLElement* e, const Style& st) -> std::string
                {
                    const XMLElement* child = e->FirstChildElement();
                    if (!child) return "";

                    std::string inner = renderElement(child, st);
                    double em = std::stof(st.fontSize);

                    /* 解析属性，缺省用子元素自身尺寸 */
                    double w = getDimAttr(e, "width", extractWidth(inner) / em) * em;
                    double asc = getDimAttr(e, "height", extractAscent(inner) / em) * em;
                    double des = getDimAttr(e, "depth", extractDescent(inner) / em) * em;

                    /* 子元素基线到盒上下沿的距离 */
                    double innerAsc = extractAscent(inner);
                    double innerDes = extractDescent(inner);

                    /* 居中放置：水平居中，垂直按基线对齐 */
                    double dx = (w - extractWidth(inner)) * 0.5;
                    double dy = asc - innerAsc;   // 使子元素基线落在 y=0

                    std::ostringstream oss;
                    oss << "<g class=\"mpadded\" data-w=\"" << w
                        << "\" data-asc=\"" << asc
                        << "\" data-desc=\"" << des
                        << "0\">"
                        << "<g transform=\"translate(" << dx << "," << dy << ")\">"
                        << inner
                        << "</g>"
                        << "</g>";
                    return oss.str();
                });

        // ------------------------------------------------------------------
        // 3. mphantom  (占位但不显示)
        // 语法: <mphantom> child </mphantom>
        // 与 mpadded 类似，但把内容设为透明
        // ------------------------------------------------------------------
            registerTag("mphantom",
                [this](const XMLElement* e, const Style& st) -> std::string
                {
                    const XMLElement* child = e->FirstChildElement();
                    if (!child) return "";

                    std::string inner = renderElement(child, st);

                    std::ostringstream oss;
                    oss << "<g class=\"mphantom\" data-w=\"" << extractWidth(inner)
                        << "\" data-asc=\"" << extractAscent(inner)
                        << "\" data-desc=\"" << extractDescent(inner) << "\">"
                        << "<g fill=\"transparent\">"
                        << inner
                        << "</g>"
                        << "</g>";
                    return oss.str();
                });

        // ----------------------------------------------------------
// 1. <none>  —— 空占位，宽 0，高/深 0
// ----------------------------------------------------------
            registerTag("none",
                [](const XMLElement*, const Style&) -> std::string
                {
                    return R"(<g class="none" data-w="0"  data-asc="0" data-desc="0" />)";
                });

            // ----------------------------------------------------------
            // mprescripts —— 占位节点，尺寸 0，基线 0
            // ----------------------------------------------------------
            registerTag("mprescripts",
                [](const XMLElement*, const Style&) -> std::string
                {
                    return R"(<g class="mprescripts" data-w="0"  data-asc="0" data-desc="0" />)";
                });

            // ----------------------------------------------------------
            // mspace —— 只产生水平间距
            // width 支持 2.5、2.5em、2.5ex 等写法
            // ----------------------------------------------------------
            registerTag("mspace",
                [](const XMLElement* e, const Style& st) -> std::string
                {
                    const char* w = e->Attribute("width");
                    double width = 0.0;

                    if (w) {
                        std::string s = w;
                        if (s.find("em") != std::string::npos) {
                            width = std::stod(s) * std::stof(st.fontSize);
                        }
                        else if (s.find("ex") != std::string::npos) {
                            width = std::stod(s) * std::stof(st.fontSize) * 0.5; // 1ex ≈ 0.5em
                        }
                        else {
                            width = std::stod(s);          // 纯数字，默认单位 em
                        }
                    }

                    std::ostringstream oss;
                    oss << "<g class=\"mspace\" data-w=\"" << width
                        << "\"  data-asc=\"0\" data-desc=\"0\" />";
                    return oss.str();
                });
    }
};

/* ---------- 单例 ---------- */
MathML2SVG& MathML2SVG::instance() {
    static MathML2SVG inst;
    return inst;
}
MathML2SVG::MathML2SVG() : pImpl(std::make_unique<Impl>()) {}
MathML2SVG::~MathML2SVG() = default;

/* 暴露接口 */
std::string MathML2SVG::convert(const std::string& mathml) {
    return pImpl->convert(mathml);
}
void MathML2SVG::registerTag(const std::string& tag, RenderFn fn) {
    pImpl->registerTag(tag, fn);
}
void MathML2SVG::registerAttr(const std::string& attr, AttrFn fn) {
    pImpl->registerAttr(attr, fn);
}



std::wstring GdiTextMeasurer::makeKey(const std::wstring& name, float size, int style) {
    return name + L'|' + std::to_wstring(size) + L'|' + std::to_wstring(style);
}

GdiTextMeasurer& GdiTextMeasurer::instance() {
    static GdiTextMeasurer inst;
    return inst;
}

GdiTextMeasurer::GdiTextMeasurer() {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
}

GdiTextMeasurer::~GdiTextMeasurer() {
    // 所有 unique_ptr 会自动释放
}

GdiTextMeasurer::Size
GdiTextMeasurer::measure(const std::wstring& text,
    const std::wstring& fontName,
    float               fontSizePx,
    Gdiplus::FontStyle  style)
{
    std::wstring key = makeKey(fontName, fontSizePx, style);
    std::lock_guard lg(mtx_);

    auto& slot = cache_[key];
    if (!slot.font) {
        slot.family = std::make_unique<Gdiplus::FontFamily>(fontName.c_str());
        slot.font = std::make_unique<Gdiplus::Font>(slot.family.get(),
            fontSizePx,
            style,
            Gdiplus::UnitPixel);
    }

    HDC hdc = GetDC(nullptr);
    Gdiplus::Graphics g(hdc);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    // 测量
    Gdiplus::RectF bounds;
    g.MeasureString(text.c_str(), -1, slot.font.get(),
        Gdiplus::PointF(0, 0), &bounds);

    // 计算 ascent（像素）
    INT emHeight = slot.family->GetEmHeight(style);
    INT ascent = slot.family->GetCellAscent(style);
    float ascentPx = fontSizePx * ascent / emHeight;

    ReleaseDC(nullptr, hdc);
    return { bounds.Width, bounds.Height, ascentPx };
}

std::string GdiTextMeasurer::outlineToSVG(const std::wstring& text,
    const std::wstring& fontName,
    float fontSizePx,
    const std::string& fill)
{
    /* 1. 复用已有缓存字体 */
    auto& cached = cache_[makeKey(fontName, fontSizePx, Gdiplus::FontStyleRegular)];
    if (!cached.font) {
        cached.family = std::make_unique<Gdiplus::FontFamily>(fontName.c_str());
        cached.font = std::make_unique<Gdiplus::Font>(cached.family.get(),
            fontSizePx,
            Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
    }

    /* 2. 生成 GraphicsPath */
    Gdiplus::GraphicsPath path;
    path.AddString(text.c_str(), static_cast<int>(text.size()),
        cached.family.get(), Gdiplus::FontStyleRegular,
        fontSizePx, Gdiplus::PointF(0, 0), nullptr);

    /* 3. 转 SVG path */
    std::ostringstream d;
    d << "<path fill=\"" << fill << "\" d=\"";
    Gdiplus::PathData pd;
    if (path.GetPathData(&pd) != Gdiplus::Ok || pd.Count == 0) {
        d << "\"/>";
        return d.str();
    }

    for (int i = 0; i < pd.Count; ) {
        BYTE type = pd.Types[i] & Gdiplus::PathPointTypePathTypeMask;

        switch (type) {
        case Gdiplus::PathPointTypeStart:
            d << "M" << pd.Points[i].X << " " << pd.Points[i].Y;
            ++i;
            break;

        case Gdiplus::PathPointTypeLine:
            d << "L" << pd.Points[i].X << " " << pd.Points[i].Y;
            ++i;
            break;

        case Gdiplus::PathPointTypeBezier:
            if (i + 2 < pd.Count) {   // 确保有 3 个点
                d << "C"
                    << pd.Points[i].X << " " << pd.Points[i].Y << " "
                    << pd.Points[i + 1].X << " " << pd.Points[i + 1].Y << " "
                    << pd.Points[i + 2].X << " " << pd.Points[i + 2].Y;
                i += 3;
            }
            else {
                i += 1;               // 防御：点数不足，跳过
            }
            break;

        default:
            ++i;
            break;
        }

        if (pd.Types[i - 1] & Gdiplus::PathPointTypeCloseSubpath)
            d << "Z";
    }

    d << "\"/>";
    return d.str();
}

FreeTypeTextMeasurer& FreeTypeTextMeasurer::instance() {
    static FreeTypeTextMeasurer inst;
    return inst;
}

FreeTypeTextMeasurer::FreeTypeTextMeasurer() {
    if (FT_Init_FreeType(&ft_)) throw std::runtime_error("FT_Init_FreeType failed");
}

FreeTypeTextMeasurer::~FreeTypeTextMeasurer() {
    for (auto& kv : cache_) FT_Done_Face(kv.second.face);
    FT_Done_FreeType(ft_);
}

static std::wstring makeKey(const std::wstring& name, int style) {
    return name + L'|' + std::to_wstring(style);
}


FT_Face FreeTypeTextMeasurer::loadFace(const std::wstring& fontName, int style) {
    // 简单映射：Windows 字体目录
    wchar_t winFontPath[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, 0, winFontPath);

    std::wstring file = winFontPath;
    file += L"\\";
    if (fontName == L"Times New Roman") file += (style & 1) ? L"timesbd.ttf" : L"times.ttf";
    else if (fontName == L"Arial")      file += (style & 1) ? L"arialbd.ttf" : L"arial.ttf";
    else                                file += fontName + L".ttf";

    FT_Face face;
    if (FT_New_Face(instance().ft_, w2a(file).c_str(), 0, &face)) return nullptr;
    return face;
}

FreeTypeTextMeasurer::CachedFace& FreeTypeTextMeasurer::getFace(const std::wstring& fontName, int style) {
    std::wstring key = makeKey(fontName, style);
    std::lock_guard<std::mutex> lock(mtx_);
    auto& slot = cache_[key];
    if (!slot.face) {
        slot.face = loadFace(fontName, style);
        if (!slot.face) slot.face = loadFace(L"Times New Roman", 0); // fallback
        slot.emSize = slot.face->units_per_EM;
    }
    return slot;
}

FreeTypeTextMeasurer::Size
FreeTypeTextMeasurer::measure(const std::wstring& text,
    const std::wstring& fontName,
    float               fontSizePx,
    int                 style) {
    auto& slot = getFace(fontName, style);
    FT_Face face = slot.face;
    float scale = fontSizePx / slot.emSize;

    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fontSizePx);

    float width = 0.f;
    float maxY = -FLT_MAX, minY = FLT_MAX;

    for (wchar_t ch : text) {
        FT_UInt idx = FT_Get_Char_Index(face, ch);
        if (FT_Load_Glyph(face, idx, FT_LOAD_DEFAULT)) continue;
        FT_GlyphSlot g = face->glyph;

        width += (g->advance.x >> 6);   // 26.6 固定小数 → 像素
        if (g->metrics.horiBearingY > maxY) maxY = g->metrics.horiBearingY >> 6;
        if (g->metrics.horiBearingY - g->metrics.height < minY)
            minY = (g->metrics.horiBearingY - g->metrics.height) >> 6;
    }

    Size sz;
    sz.width = width;
    sz.height = (maxY - minY);
    sz.ascent = maxY;
    sz.descent = minY;         // baseline → bottom（minY 为负值）
    return sz;
}



