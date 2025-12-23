#include "main.h"


std::unique_ptr<TimerOutput> g_timerOutput = std::make_unique<TimerOutput>();
std::unique_ptr<Timer> g_timer = nullptr;


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
constexpr UINT STATUSBAR_HOVER_TEXT = 11;
constexpr UINT STATUSBAR_HOVER_FONT = 12;

// 可随时改
static UINT g_frame_count = 0;



AppStates g_states;
AppSettings g_cfg;

enum class ImgFmt { PNG, JPEG, BMP, GIF, TIFF, SVG, UNKNOWN };

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
bool IsMouseOverWindow(HWND hWnd) {
    POINT ptCursor;
    GetCursorPos(&ptCursor); // 获取鼠标的屏幕坐标

    RECT rectWindow;
    GetWindowRect(hWnd, &rectWindow); // 获取窗口的屏幕坐标矩形

    return PtInRect(&rectWindow, ptCursor);
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
static int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
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
static std::string make_temp_dir()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + g_cfg.temp_dir + L"\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    return fs::path(dir).generic_string();
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
class InMemoryFontFileLoader
    : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IDWriteInMemoryFontFileLoader>
{
public:
    // IDWriteFontFileLoader
    IFACEMETHODIMP CreateStreamFromKey(
        const void* /*key*/, UINT32 /*size*/, IDWriteFontFileStream** /*stream*/) override
    {
        return E_NOTIMPL;   // 用不到
    }

    // IDWriteInMemoryFontFileLoader
    IFACEMETHODIMP CreateInMemoryFontFileReference(
        IDWriteFactory* factory,
        const void* data,
        UINT32 size,
        IUnknown* owner,
        IDWriteFontFile** fontFile) override
    {
        return factory->CreateCustomFontFileReference(data, size, this, fontFile);
    }

    STDMETHODIMP_(UINT32) GetFileCount() override
    {
        // 简单计数即可；可按需要维护实际数量
        return 1;
    }
};
class MemoryFontLoader : public IDWriteFontCollectionLoader,
    public IDWriteFontFileEnumerator
{
public:
    // 静态工厂：一次性把若干内存字体打包成私有集合
    static HRESULT CreateCollection(
        IDWriteFactory* dwrite,
        const std::vector<std::pair<std::wstring, std::vector<uint8_t>>>& fonts,
        IDWriteFontCollection** out);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override { return 1; }
    IFACEMETHODIMP_(ULONG) Release() override { return 1; }

    // IDWriteFontCollectionLoader
    IFACEMETHODIMP CreateEnumeratorFromKey(
        IDWriteFactory* factory,
        const void* collectionKey, UINT32 collectionKeySize,
        IDWriteFontFileEnumerator** enumerator) override;

    // IDWriteFontFileEnumerator
    IFACEMETHODIMP MoveNext(BOOL* hasCurrentFile) override;
    IFACEMETHODIMP GetCurrentFontFile(IDWriteFontFile** fontFile) override;

private:
    // 私有构造，只能由 CreateCollection 调用
    MemoryFontLoader(IDWriteFactory* f,
        const std::vector<std::vector<uint8_t>>& blobs)
        : factory_(f), blobs_(blobs), idx_(0) {
    }

    // 内部辅助：把一段内存封装成 IDWriteFontFile
    static HRESULT CreateInMemoryFontFile(IDWriteFactory* factory,
        const void* data,
        UINT32 size,
        IDWriteFontFile** out);

    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    std::vector<std::vector<uint8_t>> blobs_;
    size_t idx_;
    Microsoft::WRL::ComPtr<IDWriteFontFile> current_;
};
class TempFileEnumerator
    : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IDWriteFontFileEnumerator>
{
public:
    TempFileEnumerator() = default;
    TempFileEnumerator(IDWriteFactory* factory,
        const std::vector<std::wstring>& paths)
        : m_factory(factory), m_paths(paths) {
    }
    HRESULT RuntimeClassInitialize(IDWriteFactory* f,
        const std::vector<std::wstring>& paths)
    {
        m_factory = f;
        m_paths = paths;
        return S_OK;
    }
    IFACEMETHODIMP MoveNext(BOOL* hasCurrentFile) override
    {
        *hasCurrentFile = (m_idx < m_paths.size());
        if (*hasCurrentFile) ++m_idx;
        return S_OK;
    }

    IFACEMETHODIMP GetCurrentFontFile(IDWriteFontFile** fontFile) override
    {
        if (m_idx == 0 || m_idx > m_paths.size()) return E_FAIL;
        return m_factory->CreateFontFileReference(m_paths[m_idx - 1].c_str(), nullptr, fontFile);
    }

private:
    Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
    std::vector<std::wstring> m_paths;
    size_t m_idx = 0;
};
class TempFileFontLoader
    : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IDWriteFontCollectionLoader>
{
public:
    TempFileFontLoader() = default;
    explicit TempFileFontLoader(IDWriteFactory* f) : m_factory(f) {}
    static HRESULT CreateCollection(
        IDWriteFactory* dwrite,
        const std::vector<std::pair<std::wstring, std::vector<uint8_t>>>& fonts,
        IDWriteFontCollection** out,
        std::vector<std::wstring>& tempPaths)
    {
        if (!dwrite || !out) return E_INVALIDARG;

        // 注册（只一次）
        static bool reg = false;
        static Microsoft::WRL::ComPtr<TempFileFontLoader> g_loader;
        if (!reg)
        {
            Microsoft::WRL::MakeAndInitialize<TempFileFontLoader>(&g_loader, dwrite);
            dwrite->RegisterFontCollectionLoader(g_loader.Get());
            reg = true;
        }
        // 写临时文件
        std::vector<std::wstring> paths;
        for (const auto& [name, blob] : fonts)
        {
            wchar_t tmpPath[MAX_PATH]{};
            GetTempPathW(MAX_PATH, tmpPath);
            wchar_t tmpFile[MAX_PATH]{};
            PathCombineW(tmpFile, tmpPath, PathFindFileNameW(name.c_str()));
            HANDLE h = CreateFileW(tmpFile, GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE) continue;
            DWORD written = 0;
            WriteFile(h, blob.data(), (DWORD)blob.size(), &written, nullptr);
            CloseHandle(h);
            paths.push_back(tmpFile);
        }

        // 把路径数组整体作为 key
        // key = 连续 wchar_t 字符串数组，每个以 '\0' 结尾
        std::vector<wchar_t> key;
        for (const auto& p : paths)
        {
            key.insert(key.end(), p.begin(), p.end());
            key.push_back(L'\0');
        }
        key.push_back(L'\0');   // 双零结束

        HRESULT hr = dwrite->CreateCustomFontCollection(
            g_loader.Get(),
            key.data(),
            static_cast<UINT32>(key.size() * sizeof(wchar_t)),
            out);

        if (SUCCEEDED(hr))
            tempPaths.swap(paths);
        return hr;
    }

    // IDWriteFontCollectionLoader
    IFACEMETHODIMP CreateEnumeratorFromKey(
        IDWriteFactory* factory,
        void const* collectionKey,
        UINT32 collectionKeySize,
        IDWriteFontFileEnumerator** fontFileEnumerator) override
    {
        *fontFileEnumerator = nullptr;

        // 把 key 还原成路径列表
        std::vector<std::wstring> paths;
        const wchar_t* p = static_cast<const wchar_t*>(collectionKey);
        const wchar_t* end = p + collectionKeySize / sizeof(wchar_t);
        while (*p && p < end)
        {

            paths.emplace_back(p);
            p += wcslen(p) + 1;
        }

        return Microsoft::WRL::MakeAndInitialize<TempFileEnumerator>(
            fontFileEnumerator,
            factory,
            paths);
    }

    // 用 RuntimeClassInitialize 接收额外参数
    HRESULT RuntimeClassInitialize(IDWriteFactory* f)
    {
        m_factory = f;
        return S_OK;
    }
private:

    Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
};







static ImgFmt detect_fmt(const uint8_t* d, size_t n, const char* ext)
{

    if (n >= 4 && memcmp(d, "\x89PNG", 4) == 0) return ImgFmt::PNG;
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xD8)   return ImgFmt::JPEG;
    if (n >= 2 && d[0] == 'B' && d[1] == 'M')      return ImgFmt::BMP;
    if (n >= 6 && memcmp(d, "GIF87a", 6) == 0)    return ImgFmt::GIF;
    if (n >= 6 && memcmp(d, "GIF89a", 6) == 0)    return ImgFmt::GIF;
    if (n >= 4 && memcmp(d, "MM\x00*", 4) == 0)   return ImgFmt::TIFF;
    if (n >= 4 && memcmp(d, "II*\x00", 4) == 0)   return ImgFmt::TIFF;
    if (ext && strcmp(ext, "svg") == 0)       return ImgFmt::SVG;

    return ImgFmt::UNKNOWN;
}


void SavePixelsToPNG(const BYTE* pixels, UINT w, UINT h, UINT stride, const wchar_t* file)
{
    ComPtr<IWICImagingFactory> wic;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));

    ComPtr<IWICBitmap> wicBmp;
    wic->CreateBitmapFromMemory(
        w, h,
        GUID_WICPixelFormat32bppPBGRA,
        stride,
        stride * h,
        const_cast<BYTE*>(pixels),
        &wicBmp);

    ComPtr<IWICBitmapEncoder> enc;
    wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);

    ComPtr<IWICStream> stream;
    wic->CreateStream(&stream);
    stream->InitializeFromFilename(file, GENERIC_WRITE);
    enc->Initialize(stream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    enc->CreateNewFrame(&frame, &props);
    frame->Initialize(props.Get());
    frame->SetSize(w, h);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    frame->SetPixelFormat(&format);
    frame->WriteSource(wicBmp.Get(), nullptr);
    frame->Commit();
    enc->Commit();
}

void PrintFontFamilies(ComPtr<IDWriteFontCollection> fontCollection)
{
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    UINT32 familyCount = fontCollection->GetFontFamilyCount();
    char buf[512];

    for (UINT32 i = 0; i < familyCount; ++i)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
        fontCollection->GetFontFamily(i, &family);

        Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
        family->GetFamilyNames(&names);

        UINT32 idx = 0;
        UINT32 len = 0;
        BOOL exists = FALSE;
        names->FindLocaleName(L"en-us", &idx, &exists);
        if (!exists) idx = 0;

        names->GetStringLength(idx, &len);
        std::wstring wname(len + 1, 0);
        names->GetString(idx, wname.data(), len + 1);

        // 转成 UTF-8
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
        std::string name = conv.to_bytes(wname.c_str());
        DumpHex(L"[actual]   ", wname);
        snprintf(buf, sizeof(buf), "[Font] %s\n", name.c_str());
        OutputDebugStringA(buf);
    }

    ::CoUninitialize();
}
void PrintSystemFontFamilies()
{
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // 1. 拿 IDWriteFactory
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()));

    // 2. 拿系统字体集合
    Microsoft::WRL::ComPtr<IDWriteFontCollection> sysFonts;
    factory->GetSystemFontCollection(&sysFonts);

    UINT32 familyCount = sysFonts->GetFontFamilyCount();
    char buf[512];

    for (UINT32 i = 0; i < familyCount; ++i)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
        sysFonts->GetFontFamily(i, &family);

        Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
        family->GetFamilyNames(&names);

        UINT32 idx = 0;
        UINT32 len = 0;
        BOOL exists = FALSE;
        names->FindLocaleName(L"en-us", &idx, &exists);
        if (!exists) idx = 0;

        names->GetStringLength(idx, &len);
        std::wstring wname(len + 1, 0);
        names->GetString(idx, wname.data(), len + 1);

        // 转成 UTF-8
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
        std::string name = conv.to_bytes(wname.c_str());

        snprintf(buf, sizeof(buf), "[SystemFont] %s\n", name.c_str());
        OutputDebugStringA(buf);
    }

    ::CoUninitialize();
}
// 把 ID2D1Bitmap 保存为 PNG，返回 true 表示成功
bool DumpBitmap(ID2D1Bitmap* bmp, const wchar_t* file)
{
    if (!bmp || !file) return false;

    // 1. 尺寸
    D2D1_SIZE_U sz = bmp->GetPixelSize();
    if (sz.width == 0 || sz.height == 0) return false;

    // 2. 创建 WIC 工厂
    ComPtr<IWICImagingFactory> wic;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
    if (FAILED(hr)) return false;

    // 3. 创建 WIC 位图（32bpp PBGRA）
    ComPtr<IWICBitmap> wicBmp;
    hr = wic->CreateBitmap(
        sz.width, sz.height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &wicBmp);
    if (FAILED(hr)) return false;

    // 4. 创建临时 D2D WIC RenderTarget，把 bmp 画进去
    ComPtr<ID2D1Factory> d2dFactory;
    bmp->GetFactory(&d2dFactory);

    ComPtr<ID2D1RenderTarget> rt;
    hr = d2dFactory->CreateWicBitmapRenderTarget(
        wicBmp.Get(),
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED)),
        &rt);
    if (FAILED(hr)) return false;



    // 5. 编码 PNG
    ComPtr<IWICStream> stream;
    hr = wic->CreateStream(&stream);
    if (FAILED(hr)) return false;

    hr = stream->InitializeFromFilename(file, GENERIC_WRITE);
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapEncoder> encoder;
    hr = wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) return false;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) return false;

    hr = frame->Initialize(props.Get());
    if (FAILED(hr)) return false;

    hr = frame->SetSize(sz.width, sz.height);
    if (FAILED(hr)) return false;

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) return false;

    hr = frame->WriteSource(wicBmp.Get(), nullptr);
    if (FAILED(hr)) return false;

    hr = frame->Commit();
    if (FAILED(hr)) return false;

    hr = encoder->Commit();
    return SUCCEEDED(hr);
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
static inline std::string trim_any(const std::string& s,
    const char* ws = " \t\"'")
{
    if (s.empty()) return s;
    size_t first = s.find_first_not_of(ws);
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(ws);
    return s.substr(first, last - first + 1);
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
                    g_cfg.font_name = w2a(g_fontList[idx].familyName);   // 立即保存
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
                    g_cfg.font_name = w2a(g_fontList[idx].familyName);   // 立即保存
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


static ImageFrame decode_img(const MemFile& mf, const char* ext)
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

        lunasvg::Bitmap svgBmp = doc->renderToBitmap(g_cfg.document_width*2);
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
        g_vd->OnTreeSelChanged(w2a(url));  // 现在安全地在主线程执行
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
   
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);   // ±120
            float factor = (delta > 0) ? 1.1f : 0.9f;     // 放大 / 缩小系数

            // 2. 更新全局缩放
            g_cMain->clear_selection();
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
        float pxPerLine = g_cfg.font_size * g_cfg.line_height;
        float pxDelta = -zDelta / 120.0f * pxPerLine * 3.0f;   // 负号：上滚为负
        if (g_cMain) { g_cMain->on_mouse_wheel(-pxDelta); }
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
            //OutputDebugStringA("[View] WM_PAINT\n");
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
            //OutputDebugStringA("[ImageView] WM_PAINT\n");
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
        g_bootstrap->hide_imageview();
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
            //OutputDebugStringA("[Tooltip] WM_PAINT\n");
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
    wcscpy_s(lf.lfFaceName, a2w(g_cfg.font_name).c_str());   // 默认字体
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
        g_cfg.font_name = w2a(fontName);

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
                if (!fs::exists(file))
                {
                    std::string txt = "[EPUBBook] 文件不存在: " + w2a(file) + "\n";
                    OutputDebugStringA(txt.c_str());

                    return ;
                }
                if (!g_book->load(w2a(file))) 
                { 
                    std::string txt = "[EPUBBook] 打开失败: " + w2a(file) + "\n";
                    OutputDebugStringA(txt.c_str());
                    return ; 
                }
                if (g_cfg.enableEPUBFonts) { g_book->build_epub_font_index(make_temp_dir()); }
                if (g_toc)
                {
                    g_toc->Load(g_book->m_ocf_pkg);                 // 代替 EPUBBook::LoadToc()
                }
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
        g_vd->clear();
        g_recorder->flush();
        g_recorder->openBook(g_book->get_book_path());
   
        DumpBookRecord();

        
        // 更新设置
        auto& record = g_recorder->m_book_record;
        auto spine_id = record.lastSpineId;
        g_offsetY.store(record.lastOffset, std::memory_order_relaxed) ;
        g_cfg.font_size = record.fontSize > 0 ? record.fontSize:g_cfg.default_font_size;
        g_cfg.line_height = record.lineHeightMul > 0 ? record.lineHeightMul : g_cfg.default_line_height;
        g_cfg.document_width = record.docWidth > 0 ? record.docWidth : g_cfg.default_document_width;
    
        int spine_size = g_book->m_ocf_pkg.spine.size();
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

        g_cfg.font_name = record.fontName;
        g_vd->load_book();
        g_vd->load_html(g_book->m_ocf_pkg.spine[spine_id].href);

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
            if (g_book && g_book->m_fontBin.empty()) {  g_book->build_epub_font_index(make_temp_dir()); }
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
            if (IsMouseOverWindow(g_hView)) {
                // 鼠标在主窗口上
                g_bootstrap->copy_to_clipboard(g_hWnd, a2w(g_cMain->m_sel_text));
            }

            if (IsMouseOverWindow(g_hToc)) {
                // 鼠标在目录窗口上
                g_bootstrap->copy_to_clipboard(g_hWnd, a2w(g_toc->m_sel_text));
            }
       
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
        case ID_BACKGROUND_COLOR_DEFAULT:
            g_cfg.background_color = g_cfg.default_background_color;
            if (g_vd) { g_vd->reload(); }
            break;
        case ID_BACKGROUND_COLOR_BEIGE:
            g_cfg.background_color = {246.0f/255.0f, 243.0f/255.0f, 233.0f/255.0f, 1.0f};
            if (g_vd) { g_vd->reload(); }
            break;

        case ID_RESET_ALL:
            g_cMain->m_zoom_factor = 1.0f;
            g_cfg.font_name = g_cfg.default_font_name;
            g_cfg.font_size = g_cfg.default_font_size;
            g_cfg.document_width = g_cfg.default_document_width;
            g_cfg.line_height = g_cfg.default_line_height;
            g_cfg.background_color = g_cfg.default_background_color;
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
        log << std::to_string(nowUs()) << " " << message << std::endl;
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
    CreateMutex(nullptr, TRUE, L"SimpleReader_SingleInstance");
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
 
    // ---------- 4. 首次启动时如有文件立即加载 ----------
    if (firstFile && fs::exists(firstFile))
        PostMessage(g_hWnd, WM_EPUB_OPEN, 0, (LPARAM)firstFile);


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
void VirtualDoc::OnTreeSelChanged(std::string href)
{
    if (href.empty()) return;

    // 清除高亮选区
    m_container->clear_selection();

    /* 1. 分离文件路径与锚点 */

    size_t pos = href.find('#');
    std::string file_path = (pos == std::string::npos) ? href : href.substr(0, pos);
    int spine_id = get_id_by_href(file_path);
    m_anchor_id = (pos == std::string::npos) ? "" :
        href.substr(pos + 1);

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

void SimpleContainer::load_image(const char* src, const char* baseurl, bool redraw_on_ready)
{
    Timer t("  load_image");
    if (!src) { return; }
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    if (m_img_cache.contains(src)) return;

    if(g_book)
    {
        std::string path{ src };
        MemFile mf = g_book->get_binary( g_book->get_current_dir(), path);
        if (mf.data.empty())
        {
            OutputDebugStringA(("EPUB not found: " + path + "\n").c_str());
            return;
        }


        if(fs::path(path).extension().generic_string() == ".svg")
        {
            auto frame = decode_img(mf, "svg");
            if(!frame.rgba.empty())
            {
                frame.raw_data = std::move(mf.data);
                m_img_cache.emplace(src, std::move(frame));
            }
            return;
        }
        ImageFrame frame{};
 
        int w, h, comp;
        if (stbi_info_from_memory(mf.data.data(), static_cast<int>(mf.data.size()),
            &w, &h, &comp)) {
            frame.width = w;
            frame.height = h;
            frame.raw_data = std::move(mf.data);
        }
        if (!frame.raw_data.empty())
        {
            m_img_cache.emplace(src, std::move(frame));
        }
        else
        {
            OutputDebugStringA(("EPUB decode failed: " + path + "\n").c_str());
        }
        return;
    }

}





void SimpleContainer::get_image_size(const char* src, const char* baseurl, litehtml::size& sz) 
{
    Timer t("  get_image_size");
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    if (!m_img_cache.contains(src)) { sz.width = sz.height = 0; return;}
    auto img = m_img_cache[src];
    sz.width = img.width;
    sz.height = img.height;

}

// get_client_rect -> get_viewport
void SimpleContainer::get_viewport(litehtml::position& client) const
{
    Timer t("  get_viewport");
    // 1. 取客户区物理像素
    RECT rc{};
    GetClientRect(m_hwnd, &rc);


    // 4. 逻辑像素
    int width = static_cast<int>(g_cfg.document_width * m_zoom_factor);
    int height = static_cast<int>((rc.bottom- rc.top) / m_zoom_factor);

    client = litehtml::position(0, 0, width, height);
}



litehtml::element::ptr
SimpleContainer::create_element(const char* tag,
    const litehtml::string_map& attrs,
    const std::shared_ptr<litehtml::document>& doc)
{
    Timer t("  create_element");
        return nullptr;   // 让 litehtml 自己建别的节点

}



void SimpleContainer::import_css(litehtml::string& text,
    const litehtml::string& url,
    litehtml::string& baseurl)
{
    Timer t("  import_css");

    if (!g_cfg.enableCSS) {
        //text.clear();           // 禁用所有外部/内部 CSS
        return;
    }
    if (auto it = m_css_cache.find(url); it != m_css_cache.end())
    {
        text = it->second;
        return;
    }
    if (baseurl.empty())
    {
        std::string wdir = g_book->get_current_dir();
        baseurl = wdir.empty() ? "" : wdir;
    }
    if(g_book)
    {
        auto mf = g_book->get_binary(baseurl, url);
        if (!mf.data.empty())
        {
            // 直接填到 text（litehtml 期望 UTF-8）
            auto css = std::string(mf.data.begin(), mf.data.end());
            m_css_cache.emplace(url, css);
            text = css;
        }
        else
        {
            OutputDebugStringW((L"CSS not found: " + a2w(url) + L"\n").c_str());
        }
    }


    std::string wdir = g_book->get_current_dir();
    std::string wpath = wdir.empty() ? "" : g_book->resolve_path(wdir, url);

    baseurl = fs::path(wpath).parent_path().generic_string();

    // baseurl 保持原样即可
}





// ---------- 2. 标题 ----------------------------------------------------
void SimpleContainer::set_caption(const char* cap)
{
    Timer t("  set_caption");
    if (cap && g_hWnd) {
        std::string title;
        auto t = g_book->get_title();
        if (!t.empty()) { title += t + " - "; }
        auto a = g_book->get_author();
        if (!a.empty()) { title += a + " - "; }
        title += g_cfg.appName;
        SetWindowTextW(g_hWnd, a2w(title).c_str());
        //OutputDebugStringW((a2w(cap)+L"\n").c_str());
    }
}

// ---------- 3. base url -------------------------------------------------
void SimpleContainer::set_base_url(const char* base)
{
    Timer t("  set_base_url");
    return;
}

// ---------- 4. 链接注册 --------------------------------------------------
void SimpleContainer::link(const std::shared_ptr<litehtml::document>& doc,
    const litehtml::element::ptr& el)
{
    Timer t("  link");
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
    if (!url || !*url) return;

    std::string_view sv{ url };
    if (sv.starts_with('#'))
    { 
        /* 锚点 */ 
        std::wstring cssSel = a2w(url + 1);   // 去掉开头的 '#'  

        PostMessageW(g_hView, WM_EPUB_ANCHOR,
            reinterpret_cast<WPARAM>(_wcsdup(cssSel.c_str())), 0);
    }
    else if (sv.starts_with("http") || sv.starts_with("mailto:")) { /* 外部 */ }
    else 
    { 
        /* 章节跳转 */
        std::string href = g_book->resolve_path(g_book->get_current_dir(), url);
        wchar_t* url_copy = _wcsdup(a2w(href).c_str());
        PostMessageW(g_hView, WM_EPUB_NAVIGATE,
            reinterpret_cast<WPARAM>(url_copy), 0);
    }

}

bool SimpleContainer::on_element_click(const litehtml::element::ptr& el)
{
    OutputDebugStringA(el->get_tagName());
    OutputDebugStringA("\n");
    el->set_pseudo_class(litehtml::_hover_, true);
    if (std::strcmp(el->get_tagName(), "img") == 0 && g_cfg.enableClickPreview && !g_bootstrap->find_link_in_chain(el))
    {   

        g_bootstrap->show_imageview(el);
    }
    return true;
}
void SimpleContainer::on_mouse_event(const litehtml::element::ptr& el,
    litehtml::mouse_event event)
{
    if (!g_cfg.enableHoverPreview) return;

    if (event == litehtml::mouse_event::mouse_event_enter)
    {
        
        if (!el) return;
        auto link = g_bootstrap->find_link_in_chain(el);

        std::string html;
        if (!link) { return ; }
   
        const char* href_raw = link->get_attr("href");
        if (!href_raw) { return; }
        m_sel_text = std::string(href_raw);
        std::string id = g_bootstrap->extract_anchor(href_raw);
        if (id.empty()) {  return; }
        html = g_bootstrap->html_of_anchor_paragraph(g_cMain->m_doc.get(), id);
  
        if (g_bootstrap) { g_bootstrap->delayed_show_tooltip(std::move(html), g_cfg.tooltip_width, g_cfg.tooltip_delay_ms); }
    }
    else
    {
        if (g_bootstrap) { g_bootstrap->cancel_delayed_tooltip(); }   // 先杀旧计时器
    }
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
    Timer t("  set_cursor");
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
    Timer t("  transform_text");
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
    Timer t("  get_media_features");
    // 1. 窗口客户区（物理像素）
    RECT rc;
    GetClientRect(m_hwnd, &rc);


    // 4. 逻辑像素
    int width = static_cast<int>(g_cfg.document_width * m_zoom_factor);
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
    Timer t("  get_language");
    language = "en";
    culture = "US";
    // 真正 EPUB 可从 OPF <dc:language> 读
}


void SimpleContainer::init_dpi() {

    m_d2dFactory->GetDesktopDpi(&m_dpi_x, &m_dpi_y);  // 使用D2D的DPI
}

// 保持 pt_to_px 不变
litehtml::pixel_t SimpleContainer::pt_to_px(float pt) const {
    // 乘法 + 位移，比 MulDiv 更快
    Timer t("  pt_to_px");
    return pt * m_dpi_x / 72.0f;
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
  
            MemFile mf = g_book->get_binary(g_book->get_current_dir(), src);
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
            

                MemFile mf = g_book->get_binary(g_book->get_current_dir(), imgRel);
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
         std::string dir = make_temp_dir();
         replace_svg_with_img(html, dir);
     } 

}






 std::string SimpleContainer::normalize_quotes(const std::string& src)
{
    std::string out;
    out.reserve(src.size());
    for (char ch : src)
    {
        switch (ch)
        {
        case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0xFF07:
            out.push_back('\''); break;
        case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0xFF02:
            out.push_back('\"'); break;
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

ComPtr<IDWriteTextLayout> SimpleContainer::getLayout(const std::string& txt,
    litehtml::uint_ptr hFont,
    float maxW)
{
 
    // 1. 先替换花引号
    auto* fp = reinterpret_cast<FontPair*>(hFont);
    if (!fp->format) { return nullptr; }
    std::string clean = normalize_quotes(txt);
    LayoutKey k{ clean, fp->descr.hash() + fp->familyName, maxW };
    auto layout = m_layoutCache.get(k);    // 原来是 m_layoutCache.find(k)->second
    if (layout) return layout;

    std::vector<std::string> faces;
    if (!fp->descr.family.empty() && !g_cfg.enableCustomFont)
    {
        faces = split_font_list(fp->descr.family);
    }
    else
    {
        faces.push_back(g_cfg.font_name);
    }

    // 默认字体兜底

    faces.push_back(g_cfg.default_font_name);
    FontCachePair* fcp;
   for(auto& name: faces)
   {
       fcp = m_fontCache.get(name, fp->descr, m_sysFontColl.Get());
       if (!fcp->font || !fcp->fmt) { continue; }
       BOOL exists = false;
       for (auto& w: clean)
       {
           fcp->font->HasCharacter(w, &exists);
           if (!exists) { continue; }
       }
       if (exists) { break; }
   }
   if (!fcp->fmt) { return nullptr; }
   std::wstring wclean = a2w(clean);
   m_dwrite->CreateTextLayout(wclean.c_str(), (UINT32)wclean.size(),
        fcp->fmt.Get(), maxW, 512.f, &layout);
 
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
    const std::string& txt,
    const std::string& familyName,
    const litehtml::position& pos)
{
    std::wstring wtxt = a2w(txt);
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
        cb.familyName = a2w(familyName);
        line.push_back(cb);
    }
    m_lines.emplace_back(std::move(line));

    // 同时累积纯文本
    m_plainText += txt + " ";
}

std::string GetFontNameFromTextFormat(ComPtr<IDWriteTextFormat> textFormat) {
    if (!textFormat) return "";

    // 使用足够大的缓冲区
    wchar_t buffer[128];
    HRESULT hr = textFormat->GetFontFamilyName(buffer, ARRAYSIZE(buffer));

    if (SUCCEEDED(hr)) {
        // 找到第一个 null 字符
        size_t len = 0;
        while (len < ARRAYSIZE(buffer) && buffer[len] != L'\0') {
            len++;
        }

        std::wstring fontName(buffer, len);
        return w2a(fontName);
    }

    return "";
}
void SimpleContainer::draw_text(litehtml::uint_ptr hdc,
    const char* text,
    litehtml::uint_ptr hFont,
    litehtml::web_color color,
    const litehtml::position& pos)
{
    Timer t("  draw_text");
    if (!text || !*text || !hFont) return;
    auto* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
    FontPair* fp = reinterpret_cast<FontPair*>(hFont);
    if (!fp) return;

    // 1. 画刷
    auto brush = getBrush(hdc, color);
    if (!brush) return;

    // 2. 文本

    
    float maxW = 8192.0f;
    auto layout = getLayout(text,  hFont, maxW);
    if (!layout) return;
    record_char_boxes(rt, layout.Get(), text, GetFontNameFromTextFormat(fp->format), pos);
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
    std::lock_guard<std::mutex> lock(m_imgCacheMutex);
    auto it = m_img_cache.find(url);
  
    if (it == m_img_cache.end()) { return nullptr; }
 
    ImageFrame& frame  = it->second;

    if(frame.rgba.empty())
    {
        if (!g_book) { return nullptr; }
        
      
        auto dot = url.find_last_of('.');
        std::string ext = fs::path(url).extension().generic_string();


        frame = decode_img(MemFile{frame.raw_data}, ext.empty() ? nullptr : ext.c_str());
        if (!frame.rgba.empty())
        {
            m_img_cache.emplace(url, frame);
        }
        else
        {
            OutputDebugStringA(("EPUB decode failed: " + std::string(url) + "\n").c_str());
            return nullptr;
        }

        
    }
  
    if (frame.rgba.empty()) return nullptr;


    /* ---------- 2. 取/建 D2D 位图 ---------- */
    ComPtr<ID2D1Bitmap> bmp = m_d2dBmpCache[url];   // 引用现有或新建
    if (!bmp) {
        auto* rt = reinterpret_cast<ID2D1DeviceContext*>(hdc);
        D2D1_BITMAP_PROPERTIES bp =
            D2D1::BitmapProperties(
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_PREMULTIPLIED));
        rt->CreateBitmap(
            D2D1::SizeU(frame.width, frame.height),
            frame.rgba.data(),
            frame.stride,
            bp,
            &bmp);
        if (!bmp) return nullptr;
        m_d2dBmpCache.emplace(url, bmp);
  
    }
    return bmp;
}
// ----------------------------------------------------------
// 主函数：draw_image
// ----------------------------------------------------------
void SimpleContainer::draw_image(litehtml::uint_ptr hdc,
    const litehtml::background_layer& layer,
    const std::string& url,
    const std::string& base_url)
{
    Timer t("  draw_image");
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
    Timer t("  draw_solid_fill");
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
    Timer t("  draw_linear_gradient");
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
    Timer t("  draw_radial_gradient");
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
    a = fmodf(a, 2.0f * float(M_PI));
    return a < 0 ? a + 2.0f * float(M_PI) : a;
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
    Timer t("  draw_conic_gradient");
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
            angle += float(M_PI);                  // 0..2π
            angle = normalize_angle(angle + g.angle); // 支持全局旋转
            float t = angle / (2.0f * float(M_PI));   // 0..1

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
    Timer t("  draw_list_marker");
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
    Timer ti("  draw_borders");
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



std::vector<std::string>
SimpleContainer::split_font_list(const std::string& src) 
{
    std::vector<std::string> out;
    std::string token;
    for (size_t i = 0, n = src.size(); i < n; ++i)
    {
        if (src[i] == ',')
        {
            token = trim_any(token);
            if (!token.empty()) {
                out.emplace_back(token);
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
        out.emplace_back(token);
    }
    return out;
};


litehtml::uint_ptr SimpleContainer::create_font(const litehtml::font_description& descr,
    const litehtml::document* doc,
    litehtml::font_metrics* fm)
{
    Timer t("  create_font");
    if (!m_dwrite || !fm) return 0;

    /*----------------------------------------------------------
      1. 把 font-family 字符串拆成单个字体名
    ----------------------------------------------------------*/
    std::vector<std::string> faces;
    if (!descr.family.empty() && !g_cfg.enableCustomFont)
    {
        faces = split_font_list(descr.family);
    }
    else
    {
        faces.push_back(g_cfg.font_name);
    }

    // 默认字体兜底
    
    faces.push_back(g_cfg.default_font_name);
    std::string family_name = "";
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
    Timer t("  delete_font");
    if (!h) return;
    //auto* fp = reinterpret_cast<FontPair*>(h);
    //delete fp;              // 4. 真正释放
}

litehtml::pixel_t SimpleContainer::text_width(const char* text,
    litehtml::uint_ptr hFont)
{
    Timer t("  text_width");
    if (!text || !*text || !hFont) return 0;

    // 1. 创建 TextLayout
 
    float maxW = 8192.0f;
    auto layout = getLayout(text, hFont, maxW);
    if (!layout) { return 0; }


    // 3. 取逻辑宽度（已含空白、连字、kerning）
    DWRITE_TEXT_METRICS tm{};
    HRESULT hr = layout->GetMetrics(&tm);
    if (FAILED(hr)) { return 0; }

 
    // 4. DPI → 物理像素（Win7 也支持）
         
    float physical = tm.widthIncludingTrailingWhitespace * m_dpi_x / 96.0f;

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
    Timer t("  set_clip");
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
    Timer t("  del_clip");
    if (m_clipStack.empty()) return;
    if (m_clipStack.back())
        m_dc->PopLayer();           // 圆角
    else
        m_dc->PopAxisAlignedClip(); // 矩形
    m_clipStack.pop_back();
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









// -------------------------------------------------
// 静态工厂
// -------------------------------------------------
HRESULT MemoryFontLoader::CreateCollection(
    IDWriteFactory* dwrite,
    const std::vector<std::pair<std::wstring, std::vector<uint8_t>>>& fonts,
    IDWriteFontCollection** out)
{
    if (!dwrite || !out) return E_INVALIDARG;

    // 1. 注册 loader（只注册一次）
    static bool registered = false;
    if (!registered)
    {
        Microsoft::WRL::ComPtr<MemoryFontLoader> stub(new MemoryFontLoader(nullptr, {}));
        HRESULT hr = dwrite->RegisterFontCollectionLoader(stub.Get());
        if (FAILED(hr)) return hr;
        registered = true;
    }

    // 2. 把 vector<blob> 打包成一块连续内存
    std::vector<std::vector<uint8_t>> blobs;
    blobs.reserve(fonts.size());
    for (const auto& [name, data] : fonts)
        blobs.emplace_back(data);

    // 3. 创建自定义集合
    return dwrite->CreateCustomFontCollection(
        static_cast<IDWriteFontCollectionLoader*>(nullptr),   // 用 key 区分
        blobs.data(),
        static_cast<UINT32>(blobs.size() * sizeof(blobs[0])),
        out);
}

// -------------------------------------------------
// IUnknown
// -------------------------------------------------
HRESULT MemoryFontLoader::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == __uuidof(IUnknown) ||
        riid == __uuidof(IDWriteFontCollectionLoader))
    {
        *ppv = static_cast<IDWriteFontCollectionLoader*>(this);
    }
    else if (riid == __uuidof(IDWriteFontFileEnumerator))
    {
        *ppv = static_cast<IDWriteFontFileEnumerator*>(this);
    }
    else
    {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    return S_OK;
}

// -------------------------------------------------
// IDWriteFontCollectionLoader
// -------------------------------------------------
HRESULT MemoryFontLoader::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    const void* collectionKey, UINT32 collectionKeySize,
    IDWriteFontFileEnumerator** enumerator)
{
    if (!factory || !enumerator) return E_INVALIDARG;

    // collectionKey 指向 vector<vector<uint8_t>>
    const auto* blobs = reinterpret_cast<const std::vector<uint8_t>*>(collectionKey);
    size_t count = collectionKeySize / sizeof(std::vector<uint8_t>);
    if (!blobs || count == 0) return E_INVALIDARG;

    Microsoft::WRL::ComPtr<MemoryFontLoader> loader(
        new MemoryFontLoader(factory, std::vector<std::vector<uint8_t>>(blobs, blobs + count)));
    *enumerator = loader.Detach();
    return S_OK;
}

// -------------------------------------------------
// IDWriteFontFileEnumerator
// -------------------------------------------------
HRESULT MemoryFontLoader::MoveNext(BOOL* hasCurrentFile)
{
    if (!hasCurrentFile) return E_INVALIDARG;
    *hasCurrentFile = FALSE;

    if (idx_ < blobs_.size())
    {
        HRESULT hr = CreateInMemoryFontFile(factory_.Get(),
            blobs_[idx_].data(),
            static_cast<UINT32>(blobs_[idx_].size()),
            &current_);
        *hasCurrentFile = SUCCEEDED(hr);
        ++idx_;
        return hr;
    }
    return S_OK;
}

HRESULT MemoryFontLoader::GetCurrentFontFile(IDWriteFontFile** fontFile)
{
    if (!fontFile) return E_INVALIDARG;
    *fontFile = current_.Get();
    if (*fontFile) (*fontFile)->AddRef();
    return S_OK;
}




HRESULT MemoryFontLoader::CreateInMemoryFontFile(
    IDWriteFactory* factory,
    const void* data,
    UINT32 size,
    IDWriteFontFile** out)
{
    using Microsoft::WRL::MakeAndInitialize;

    Microsoft::WRL::ComPtr<InMemoryFontFileLoader> loader;
    HRESULT hr = MakeAndInitialize<InMemoryFontFileLoader>(&loader);
    if (FAILED(hr)) return hr;

    hr = factory->RegisterFontFileLoader(loader.Get());
    if (FAILED(hr) && hr != DWRITE_E_ALREADYREGISTERED) return hr;

    return loader->CreateInMemoryFontFileReference(
        factory,
        data,
        size,
        nullptr,
        out);
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
        fs::path dbPath = documents_dir() / g_cfg.appName / "data";
        g_recorder = std::make_unique<ReadingRecorder>(dbPath); 
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
        g_toc->SetOnNavigate([](const std::string& href) {
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

    // 清除高亮选择的文字
    clear_selection();
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

    m_dc->SetDpi(m_dpi_x, m_dpi_y);
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





/* ---------- 实现 ---------- */

std::string AppBootstrap::extract_anchor(const char* href)
{
    if (!href) return "";
    const char* p = std::strrchr(href, '#');
    return p ? (p + 1) : "";

}
litehtml::element::ptr AppBootstrap::find_link_in_chain(litehtml::element::ptr start)
{
    for (auto cur = start; cur; cur = cur->parent())
    {
        const char* tag = cur->get_tagName();
        if (std::strcmp(tag, "p") == 0) break;
        if (std::strcmp(tag, "a") == 0) return cur;
    }
    return nullptr;
}

 bool AppBootstrap::skip_attr(const std::string& val)
{
    if (val.empty()) return true;
    if (val == "0")  return true;
    return std::all_of(val.begin(), val.end(),
        [](unsigned char c) { return std::isspace(c); });
}

 std::string AppBootstrap::get_html(litehtml::element::ptr el)
{
    if (!el) return "";
    std::string out;
    out += "<";
    out += el->get_tagName();

    // 常见属性名，按需增删
    static const char* attr_names[] = {
        "id","class","style","title","alt","href","src","type","name","value"
    };

    for (const char* name : attr_names)
    {
        const char* val = el->get_attr(name);
        if (val && !skip_attr(val))
        {
            out += " ";
            out += name;
            out += "=\"";
            out += val;
            out += "\"";
        }
    }

    out += ">";

    for (auto child : el->children())
    {
        if (child->is_text())
        {
            std::string txt;
            child->get_text(txt);
            out += txt;
        }
        else
        {
            out += get_html(child);
        }
    }

    out += "</";
    out += el->get_tagName();
    out += ">";
    return out;
}


 // 从一段 HTML 中提取第一个 <img> 的 outerHTML；没有则返回空串
 static std::string extract_first_img(const std::string& html)
 {
     GumboOutput* out = gumbo_parse(html.c_str());
     std::string result;

     // 深度优先找 <img>
     std::function<void(const GumboNode*)> dfs = [&](const GumboNode* node)
         {
             if (!result.empty()) return;           // 已找到
             if (node->type != GUMBO_NODE_ELEMENT) return;

             const GumboElement& elem = node->v.element;
             if (elem.tag == GUMBO_TAG_IMG)
             {
                 gumbo_serialize(node, result);
                 return;
             }
             for (unsigned int i = 0; i < elem.children.length; ++i)
                 dfs(static_cast<GumboNode*>(elem.children.data[i]));
         };
     dfs(out->root);

     gumbo_destroy_output(&kGumboDefaultOptions, out);
     return result;
 }
// 3. 核心函数：用 select_one 找 id，再向上找 <p>
std::string AppBootstrap::html_of_anchor_paragraph(litehtml::document* doc, const std::string& anchorId)
{
    if (anchorId.empty()) return "";
    // 构造 CSS 选择器
    std::string sel = "[id=\"" + anchorId + "\"]";
    //std::string sel = "#" + anchorId;
    auto target = doc->root()->select_one(sel);
    if (!target) return "";

    // 向上找最近的 <p>
    auto p = target;
    while (p)
    {
        if (std::strcmp(p->get_tagName(), "figure") == 0 || std::strcmp(p->get_tagName(), "img") == 0) { break; }
        const char* cls = p->get_attr("class");
        if (!cls ||
            (std::strcmp(cls, "duokan-footnote-item") != 0 &&
                std::strcmp(cls, "fig") != 0 &&
                std::strcmp(cls, "figimage") != 0 &&
                std::strcmp(cls, "figure") != 0 &&
                std::strcmp(cls, "reflist") != 0 &&
                std::strcmp(cls, "illustype_image") != 0))
        {
            p = p->parent();
        }
        else
        {
            break;          // 找到目标 class
        }
    }

    if (!p) return "";          // 兜底：直接返回自身

    std::string inner = get_html(p);

    // 用 gumbo 处理
    std::string imgOnly = extract_first_img(inner);
    if (!imgOnly.empty())
    {
        return "<style>img{display:block;width:100%;height:auto;}a{color:#3182ce;text-decoration:none;} </style>" + imgOnly;
    }
    else
    {
        return "<style>img{display:block;width:100%;height:auto;}a{color:#3182ce;text-decoration:none;} </style>" + inner;
    }
}


std::string AppBootstrap::get_html_of_image(litehtml::element::ptr start)
{
    if (!start && std::strcmp(start->get_tagName(), "img") != 0 ) { return ""; }
    std::string inner = get_html(start);
    return "<style>img{display:block;width:100%;height:auto;}</style>" + inner;
   
}
void AppBootstrap::show_imageview(const litehtml::element::ptr& el)
{
    std::string html = get_html_of_image(el);
    if (html.empty()) { return; }
    POINT pt;
    GetCursorPos(&pt);   // pt.x, pt.y 为屏幕坐标
    //ScreenToClient(g_hView, &pt);   // 现在 pt.x, pt.y 是相对于窗口客户区的坐标


    g_cImage->m_doc = litehtml::document::createFromString(
        { html.c_str(), litehtml::encoding::utf_8 }, g_cImage.get());
    int width = g_cfg.tooltip_width;
    g_cImage->m_doc->render(width);

    int height = g_cImage->m_doc->height();
    auto tip_x = pt.x - width/2;
    auto tip_y = pt.y - height/2;


 
    DWORD style = GetWindowLong(g_hImageview, GWL_STYLE);
    DWORD exStyle = GetWindowLong(g_hImageview, GWL_EXSTYLE);
    UINT dpi = GetDpiForWindow(g_hImageview);
    RECT r{ 0, 0, width, height };
    AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
    g_cImage->resize(width, height);
    SetWindowPos(g_hImageview, HWND_TOPMOST,
        tip_x, tip_y,
        r.right - r.left, r.bottom - r.top,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);

    g_imageviewRenderW = width;

    InvalidateRect(g_hImageview, nullptr, true);
}
void AppBootstrap::show_tooltip(const std::string txt, int width)
{
    auto html = txt;
    //OutputDebugStringA(("[show_tooltip] " + std::to_string(x) + " " + std::to_string(y) + "\n").c_str());
    if (html.empty()) {  return; }

    //if (g_vd && !g_vd->m_blocks.empty())
    //{
    //    html = "<html>" + g_vd->m_blocks.back().head + "<body>" + html + "</body></html>";
    //}
    POINT pt;
    GetCursorPos(&pt);   




    g_cTooltip->m_doc = litehtml::document::createFromString(
        { html.c_str(), litehtml::encoding::utf_8 }, g_cTooltip.get());

    g_cTooltip->m_doc->render(width);
    int height = g_cTooltip->m_doc->height();


    int tip_x = pt.x - width / 2;
    int tip_y = pt.y - height - 20;
    if (tip_y < 0) { tip_y = pt.y + 20; }
    DWORD style = GetWindowLong(g_hTooltip, GWL_STYLE);
    DWORD exStyle = GetWindowLong(g_hTooltip, GWL_EXSTYLE);
    UINT dpi = GetDpiForWindow(g_hTooltip);
    RECT r{ 0, 0, width, height };
    AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, dpi);
    g_cTooltip->resize(width, height);
    SetWindowPos(g_hTooltip, HWND_TOPMOST,
        tip_x, tip_y,
        r.right - r.left, r.bottom - r.top,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);



    InvalidateRect(g_hTooltip, nullptr, true);

}
void AppBootstrap::hide_imageview()
{

    if (g_hImageview && IsWindowVisible(g_hImageview))
    {
        ShowWindow(g_hImageview, SW_HIDE);
        if (g_cImage && g_cImage->m_doc)
        {
            g_cImage->m_doc.reset();
        }

    }
}
void AppBootstrap::hide_tooltip()
{

    if (g_hTooltip  && IsWindowVisible(g_hTooltip) )
    {
        ShowWindow(g_hTooltip, SW_HIDE);

    }
}



// 把单个 element 序列化成 HTML
static void element_to_html(const litehtml::element::ptr& el,
    std::string& out)
{
    if (!el) return;

    if (el->is_text())
    {
        // 文本节点
        std::string txt;
        el->get_text(txt);
        out += txt;
        return;
    }

    // 开始标签
    out += "<";
    out += el->get_tagName();

    // 属性
    auto attrs = el->dump_get_attrs();
    for (const auto& [name, val] : attrs)
    {
        out += " " + name + "=\"" + val + "\"";
    }

    if (el->children().empty())
    {
        // 自闭合
        out += " />";
    }
    else
    {
        out += ">";

        // 子节点
        for (const auto& child : el->children())
            element_to_html(child, out);

        // 结束标签
        out += "</";
        out += el->get_tagName();
        out += ">";
    }
}

// 根据锚点 id 返回对应元素的 HTML
std::string AppBootstrap::get_anchor_html(litehtml::document* doc,
    const std::string& anchor)
{
    if (!doc || anchor.empty()) return {};

    litehtml::element::ptr root = doc->root();
    if (!root) return {};

    litehtml::element::ptr el =
        root->select_one(("#" + anchor).c_str());
    if (!el) return {};

    std::string html;
    element_to_html(el, html);
    return html;
}








SimpleContainer::SimpleContainer(int w, int h, HWND hwnd):
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
    init_dpi();
    const float scale = m_dpi_x / 96.0f;


    // 2) 创建 D3D11 设备（flag 选 D3D11_CREATE_DEVICE_BGRA_SUPPORT）
    Microsoft::WRL::ComPtr<ID3D11Device>        d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dCtx;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT ,
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
 
    m_dc->SetDpi(m_dpi_x, m_dpi_y);
    

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

}

void SimpleContainer::BuildFontList()
{
    g_fontList.clear();

    auto sysColl = m_sysFontColl;
    if (!sysColl)
        return;

    UINT32 count = sysColl->GetFontFamilyCount();
    for (UINT32 i = 0; i < count; ++i)
    {
        ComPtr<IDWriteFontFamily> family;
        if (FAILED(sysColl->GetFontFamily(i, &family)))
            continue;

        ComPtr<IDWriteLocalizedStrings> names;
        if (FAILED(family->GetFamilyNames(&names)))
            continue;

        // 1. 取英文字体原名
        UINT32 idx = 0;
        BOOL exists = FALSE;
        names->FindLocaleName(L"en-us", &idx, &exists);
        if (!exists) idx = 0; // fallback

        UINT32 len = 0;
        names->GetStringLength(idx, &len);
        std::wstring familyName(len + 1, 0);
        names->GetString(idx, familyName.data(), len + 1);

        // 2. 尝试取中文名
        std::wstring displayName = familyName; // 默认
        names->FindLocaleName(L"zh-cn", &idx, &exists);
        if (exists)
        {
            names->GetStringLength(idx, &len);
            displayName.resize(len);
            names->GetString(idx, displayName.data(), len + 1);
        }

        g_fontList.push_back({ familyName, displayName });
    }

    // 可选：按 displayName 排序
    std::sort(g_fontList.begin(), g_fontList.end(),
        [](const FontItem& a, const FontItem& b)
        { return a.displayName <  b.displayName; });
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
    Timer t("  get_default_font_size");
    return g_cfg.font_size;
}
const char* SimpleContainer::get_default_font_name() const
{
    Timer t("  get_default_font_name");
    return g_cfg.default_font_name.c_str();
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
FontCache::get(std::string& familyName, const litehtml::font_description& descr,
     IDWriteFontCollection* sysColl) {
    // 1. 构造键

    std::string search_key = familyName + descr.hash();
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
     FontCache::create(std::string& familyName, const litehtml::font_description& descr, IDWriteFontCollection* sysColl)
 {



     /* ---------- 1. 候选列表（路径优先） ---------- */
     std::vector<std::string> tryNames{ familyName };
     if (g_book && g_cfg.enableEPUBFonts)
     {
         FontKey exact{ familyName, descr.weight, descr.style, 0 };
         if (auto it = g_book->m_fontBin.find(exact) ; it != g_book->m_fontBin.end())
             tryNames.insert(tryNames.end(), it->second.begin(), it->second.end());
         else
         {
             for (const auto& kv : g_book->m_fontBin)
                 if (kv.first.family == familyName)
                     tryNames.insert(tryNames.end(), kv.second.begin(), kv.second.end());
         }

     }

     /* ---------- 2. 工具：一次性生成 metrics ---------- */


     /* ---------- 3. 路径字体（私有集合） ---------- */
     if(g_cfg.enableEPUBFonts)
     {
         for (std::string& name : tryNames)
         {
             if (name.find(':') == std::string_view::npos) continue;
             auto& coll = collCache[name];
             if (!coll)
             {
                 coll = CreatePrivateCollectionFromFile(m_dw.Get(), a2w(name).c_str());
                 if (coll) { collCache.emplace(name, coll); }
             }
             if (!coll) continue;

             UINT32 familyCount = coll->GetFontFamilyCount();
             if (familyCount == 0) continue;

             ComPtr<IDWriteFontFamily> family;
             if (FAILED(coll->GetFontFamily(0, &family))) continue;

             ComPtr<IDWriteFont> font;
             if (FAILED(family->GetFirstMatchingFont(
                 static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
                 DWRITE_FONT_STRETCH_NORMAL,
                 descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                 &font))) continue;

             wchar_t realName[LF_FACESIZE]{};
             {
                 ComPtr<IDWriteLocalizedStrings> names;
                 if (SUCCEEDED(family->GetFamilyNames(&names)))
                 {
                     UINT32 idx = 0, len = 0;
                     BOOL exists = FALSE;
                     names->FindLocaleName(L"en-us", &idx, &exists);
                     if (!exists) idx = 0;
                     names->GetStringLength(idx, &len);
                     if (len < LF_FACESIZE)
                         names->GetString(idx, realName, len + 1);
                 }
             }
            if (!realName[0]) { continue; }
            ComPtr<IDWriteTextFormat> fmt;
            if (SUCCEEDED(m_dw->CreateTextFormat(
                realName,
                coll.Get(),
                static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
                descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                static_cast<float>(descr.size), L"en-us", &fmt)))
            return new FontCachePair{ familyName,  fmt, font };

         }
     }


     /* ---------- 4. 系统字体 ---------- */
     if (sysColl)
     {
         for (std::string& name : tryNames)
         {
             UINT32 index = 0;
             BOOL exists = FALSE;
             sysColl->FindFamilyName(a2w(name).c_str(), &index, &exists);
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
                 a2w(name).c_str(), sysColl,
                 static_cast<DWRITE_FONT_WEIGHT>(descr.weight),
                 descr.style ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                 DWRITE_FONT_STRETCH_NORMAL,
                 static_cast<float>(descr.size), L"en-us", &fmt))) {
                 continue;
             }
 
            return new FontCachePair{ familyName, fmt , font};

         }
     }

     /* ---------- 5. 失败 ---------- */
     return new FontCachePair{ familyName, nullptr, nullptr};
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
    const std::string& target,
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

        std::wstring buf(len + 1, '\0');
        if (FAILED(names->GetString(0, buf.data(), len + 1)))
            continue;
        buf.resize(len);

        if (strcmp(w2a(buf).c_str(), target.c_str()) == 0)
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

    m_spine = m_book->m_ocf_pkg.spine;
}


// ---------- 分页 ----------
std::string VirtualDoc::get_href_by_id(int id)
{

    if (id < m_spine.size() && id >= 0)
    {
        return m_spine[id].href;
    }
    return "";
}

int VirtualDoc::get_id_by_href(std::string& href)
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

void VirtualDoc::load_html(std::string& href)
{

    auto id = get_id_by_href(href);
    if(id < 0)
    {
        OutputDebugStringA(href.c_str());
        OutputDebugStringA(" 未找到\n");
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
 
        std::string href = get_href_by_id(spine_id);
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

    //OutputDebugStringA("[before] ");
    //OutputDebugStringA(std::to_string(offsetY).c_str());
    //OutputDebugStringA("\n");


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
    SetStatus(STATUSBAR_FONT_NAME, (L"自定义字体：" + a2w(g_cfg.font_name)).c_str());
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
//bool VirtualDoc::insert_chapter(int spine_id, bool isPushBack)
//{
//    int id = spine_id;
//    if (id < 0 || id >= static_cast<int>(m_spine.size()) || exists(id)) return false;
//    auto start_load_time = nowUs();
//
//    if (!load_by_id(id, isPushBack))
//    {
//        return false;
//    }
//
//
//    // 2. 组装 HTML
//    float height = 0.0f;
//    HtmlBlock& target = isPushBack ? m_blocks.front() : m_blocks.back();
//
//
//    std::string html = "";
//    for (auto& hb : m_blocks)
//    {
//        html += "<html>" + hb.head + "<body>";
//        for (auto& b : hb.body_blocks) html += b.html;
//        html += "</body></html>";
//    }
//
//    /* ---------- 4. render ---------- */
//
//    std::string css = g_globalCSS;
//    css += ":root,body,p,li,div,h1,h2,h3,h4,h5,h6,span, ul{line-height:" + std::to_string(g_cfg.line_height) + ";}\n";
//
//
//    auto end_load_time = nowUs();
//    auto start_create_time = nowUs();
//
//    m_doc = litehtml::document::createFromString(
//        { html.c_str(), litehtml::encoding::utf_8 }, m_container.get(), litehtml::master_css, css);
//
//    auto end_create_time = nowUs();
//    auto start_render_time = nowUs();
//
//    m_doc->render(g_cfg.document_width);
//
//    auto end_render_time = nowUs();
//
//    /* ---------- 5. 计算高度 ---------- */
//
//
//    height = m_doc->height() - m_height;
//
//    target.height = height;
//    m_height = m_doc->height();
//    float delta = isPushBack ? height : 0.0f;
//
//    std::string txt = "=========" + g_book->get_title() + "==========\n";
//    txt += "加载耗时（s）：" + std::to_string((end_load_time - start_load_time) / 1000000.0f) + "\n";
//    txt += "创建耗时（s）：" + std::to_string((end_create_time - start_create_time) / 1000000.0f) + "\n";
//    txt += "渲染耗时（s）：" + std::to_string((end_render_time - start_render_time) / 1000000.0f) + "\n";
//    OutputDebugStringA(txt.c_str());
//
//    PostMessage(g_hWnd, WM_EPUB_CACHE_UPDATED, 0, static_cast<LPARAM>(delta));
//    return true;
//}
//bool VirtualDoc::insert_prev_chapter() 
//{
//      int id = m_blocks.empty()? 0:m_blocks.front().spine_id - 1;
//      return insert_chapter(id, false);
//}
//bool VirtualDoc::insert_next_chapter() 
//{
//     int id = m_blocks.empty() ? 0 : m_blocks.back().spine_id + 1;
//     return insert_chapter(id, true);
//}
bool VirtualDoc::insert_chapter(int spine_id, bool isPushBack)
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

        //OutputDebugStringA("[VirtualDod thread] 开始更新\n");
        // 1. 耗时 IO
                /* ---------- 2. 耗时 IO ---------- */


        g_timer = std::make_unique<Timer>("总耗时（s）");
        //// ********************测试开始**********************
        //LogToFile(m_book->get_title());
        //auto start = nowUs();
        //for (int i = 0; i<m_spine.size(); i++)
        //{
        //    auto now = nowUs();
        //    if (!load_by_id(i, true))
        //    {
        //        continue;
        //    }
        //    // 2. 组装 HTML
        //    float height = 0.0f;
        //    HtmlBlock& target = m_blocks.back();


        //    std::string html = "";
   
        //        html += "<html>" + target.head + "<body>";
        //        for (auto& b : target.body_blocks) html += b.html;
        //        html += "</body></html>";
      

        //    /* ---------- 4. render ---------- */

        //    std::string css = g_globalCSS;
        //    css += ":root,body,p,li,div,h1,h2,h3,h4,h5,h6,span, ul{line-height:" + std::to_string(g_cfg.line_height) + ";}\n";

        //    m_doc = litehtml::document::createFromString(
        //        { html.c_str(), litehtml::encoding::utf_8 }, m_container.get(), litehtml::master_css, css);
        //    m_doc->render(g_cfg.document_width);
        //    std::string message = std::to_string(i) + " " + "height=" + std::to_string(m_doc->height()) + " ";
        //    message += "time=" + std::to_string((nowUs() - now) / 1000) + "ms";
        //    LogToFile(message);

        //}
        //std::string total_time = "total_time=" + std::to_string((nowUs() - start) / 1000) + "ms";
        //LogToFile(total_time);
        //LogToFile(" ");
        //// ********************测试结束**********************

        std::string txt = "=========" + g_book->get_title() + "==========\n";
        OutputDebugStringA(txt.c_str());
        auto start_load_time = nowUs();

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
        

        auto end_load_time = nowUs();
 
        txt = "加载耗时（s）：" + std::to_string((end_load_time - start_load_time) / 1000000.0f) + "\n";
        txt += "---------------------------------\n";
        OutputDebugStringA(txt.c_str());
        auto start_create_time = nowUs();

        m_doc = litehtml::document::createFromString(
            { html.c_str(), litehtml::encoding::utf_8 }, m_container.get(), litehtml::master_css, css);
        
        auto end_create_time = nowUs();
        g_timerOutput->print();
        txt = "创建耗时（s）：" + std::to_string((end_create_time - start_create_time) / 1000000.0f) + "\n";
        txt += "---------------------------------\n";
        OutputDebugStringA(txt.c_str());

        auto start_render_time = nowUs();

        m_doc->render(g_cfg.document_width);


        
        /* ---------- 5. 计算高度 ---------- */


        height = m_doc->height() - m_height;
 
        target.height = height;
        m_height = m_doc->height();
        float delta = task.insertAtFront ? height : 0.0f;
 

        auto end_render_time = nowUs();
        g_timerOutput->print();
        txt = "渲染耗时（s）：" + std::to_string((end_render_time - start_render_time) / 1000000.0f) + "\n";
        txt += "---------------------------------\n";
        OutputDebugStringA(txt.c_str());

        PostMessage(g_hWnd, WM_EPUB_CACHE_UPDATED, 0, static_cast<LPARAM>(delta));
        //OutputDebugStringA("[VirtualDod thread] 更新结束\n");
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
        std::string href = n.nav->href;
        size_t pos = href.find('#');
        std::string pure = pos == std::string::npos ? href : href.substr(0, pos);
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
    //if (line == m_selLine) { m_curHover = -1; if (m_hTip) { ShowWindow(m_hTip, SW_HIDE); }return; }

    m_curHover = line;
    SetCursor(LoadCursor(nullptr, IDC_HAND));
    std::wstring wtxt = a2w(m_nodes[m_visible[line]].nav->label);
    m_sel_text = w2a(wtxt);
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

        SetWindowTextW(m_hTip, wtxt.c_str());

        
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
        a2w(g_cfg.default_font_name).c_str());   // 字体名

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
    //OutputDebugStringA("[TocPanel] WM_PAINT\n");
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
        std::wstring wlabel = a2w(n.nav->label);
        TextOutW(hdc, textLeft, r.top + 2,
            wlabel.c_str(),
            static_cast<int>(wlabel.size()));
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

    m_selLine = line;
    InvalidateRect(m_hwnd, nullptr, false);
    UpdateWindow(m_hwnd);
    if (m_onNavigate) m_onNavigate(n.nav->href);
    
    if (!n.childIdx.empty())
    {
        Toggle(line);
    }

}
float TocPanel::getAnchorOffsetY(const std::string& href)
{
    if (!g_cMain || !g_cMain->m_doc) { return 0; }
    size_t pos = href.find('#');
    std::string pure = pos == std::string::npos ? href : href.substr(0, pos);
    std::string anchor = pos == std::string::npos ? "" : href.substr(pos+1);
 
    if (!anchor.empty()) {
        std::string cssSel = "[id=\"" + anchor + "\"]";
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

            std::string href = m_nodes[i].nav->href;
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
    //OutputDebugStringA("[ScrollBarEx] WM_PAINT\n");
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
    m_sel_rects = {};
    m_selStart = m_selEnd = -1;
    UpdateCache();
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
            m_sel_rects = {};
            for (const auto& row : get_selection_rows())
            {

                D2D1_RECT_F r = D2D1::RectF(
                    row.left, row.top, row.right, row.bottom);
                m_sel_rects.push_back(r);
            }
            InvalidateRect(m_hwnd, nullptr, false);
        }

    }
    for (const auto& line : m_lines)
        for (const auto& cb : line)
            if (x >= cb.rect.left && x <= cb.rect.right &&
                y >= cb.rect.top && y <= cb.rect.bottom)
            {
                
                std::wstring txt = L"当前指向的文字：" + std::wstring(1, cb.ch) + L", 对应字体：" + cb.familyName;
                SetStatus(STATUSBAR_HOVER_TEXT, txt.c_str());
                return;
            }
}

void SimpleContainer::on_mouse_wheel(float delta)
{
    if (m_selStart != m_selEnd && m_selStart >= 0 && m_selEnd >= 0)
    {

        for ( auto& r : m_sel_rects)
        {
            r.bottom += delta;
            r.top += delta;
        }
    }

}

void SimpleContainer::on_lbutton_up()
{

    m_sel_text = get_selection_text();
    m_selecting = false;

    m_currentCursor = IDC_ARROW;
    SetCursor(LoadCursor(nullptr, m_currentCursor));
    InvalidateRect(g_hView, nullptr, false);
}
void SimpleContainer::copy_to_clipboard()
{
    //if (m_sel_text.empty()) return;

    //const size_t bufSize = (m_sel_text.size() + 1) * sizeof(wchar_t);
    //if (HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bufSize)) {
    //    if (wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem))) {
    //        // 直接复制整个字符串内容
    //        wcscpy_s(dst, m_sel_text.size() + 1, m_sel_text.c_str());
    //        GlobalUnlock(hMem);

    //        if (OpenClipboard(g_hWnd)) {
    //            EmptyClipboard();
    //            SetClipboardData(CF_UNICODETEXT, hMem);
    //            CloseClipboard();
    //        }
    //        else {
    //            GlobalFree(hMem);
    //        }
    //    }
    //    else {
    //        GlobalFree(hMem);
    //    }
    //}
}

std::vector<RECT> SimpleContainer::get_selection_rows() const
{
    std::vector<RECT> rows;
    if (m_selStart == m_selEnd) return rows;

    const size_t start = std::min(m_selStart, m_selEnd);
    const size_t end = std::max(m_selStart, m_selEnd);

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

std::string SimpleContainer::get_selection_text() const
{
    if (m_selStart == m_selEnd)
        return "";

    // 确保选区不越界
    const size_t start = std::min(m_selStart, m_selEnd);
    const size_t end = std::min(std::max(m_selStart, m_selEnd), static_cast<int64_t>(m_plainText.size()));

    if (start >= end)
        return "";

    // 直接使用substr安全地获取子字符串
    return m_plainText.substr(start, end - start);
}

void SimpleContainer::present(float x, float y, litehtml::position* clip)
{
    auto start_draw_time = nowUs();

    m_lines.clear();
    m_plainText.clear();

    m_dc->BeginDraw();
    m_dc->Clear(g_cfg.background_color);

    // 保存原始矩阵
    m_dc->GetTransform(&m_oldMatrix);

    // 缩放
    m_dc->SetTransform(
        D2D1::Matrix3x2F::Scale(
            m_zoom_factor,
            m_zoom_factor,
            D2D1::Point2F(0.0f, 0.0f)));

    // 绘制 html
    m_dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
 
    m_doc->draw(getContext(), x, y, clip);

    // 高亮选中行
    if (!m_selBrush)
    {
        m_dc->CreateSolidColorBrush(
            g_cfg.highlight_color_d2d,
            &m_selBrush);
    }
    if (m_selStart != m_selEnd && m_selBrush && m_selStart >= 0 && m_selEnd >= 0)
    {
        //m_dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (const auto& r : m_sel_rects)
        {
            m_dc->FillRectangle(r, m_selBrush.Get());
        }
    }

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

    auto end_draw_time = nowUs();
    g_timerOutput->print();
    std::string txt = "绘制耗时（s）：" + std::to_string((end_draw_time - start_draw_time) / 1000000.0f) + "\n";
    txt += "---------------------------------\n";
    OutputDebugStringA(txt.c_str());
    
    g_timer.reset();
    
    g_timerOutput->print();
    txt = "==================================\n";
    OutputDebugStringA(txt.c_str());
}


static bool is_word_boundary(wchar_t ch)
{
    return iswspace(ch) || iswpunct(ch) || ch == L'\r' || ch == L'\n';
}


void SimpleContainer::clear_selection()
{
    m_selStart = m_selEnd = -1;
    m_selecting = false;
    m_sel_delta = 0;
    m_sel_rects = {};
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
    if (m_selStart != m_selEnd && m_selStart >= 0 && m_selEnd >= 0)
    {

        for (const auto& row : get_selection_rows())
        {

            D2D1_RECT_F r = D2D1::RectF(
                row.left, row.top, row.right, row.bottom);
            m_sel_rects.push_back(r);
        }
    }
    m_sel_text = get_selection_text();
    UpdateCache();
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
//        UBRK_LINE, nullptr,
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
//    if (m_selStart != m_selEnd && m_selStart >= 0 && m_selEnd >= 0)
//    {
//
//        for (const auto& row : get_selection_rows())
//        {
//
//            D2D1_RECT_F r = D2D1::RectF(
//                row.left, row.top, row.right, row.bottom);
//            m_sel_rects.push_back(r);
//        }
//    }
//    m_sel_text = get_selection_text();
//    //UpdateCache();
//}


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






// 2. 在 EPUBBook.cpp 里写
void AppBootstrap::delayed_show_tooltip(std::string txt, unsigned width, unsigned delayMs)
{
    if (m_tooltipTimer)
    {
        timeKillEvent(m_tooltipTimer);
        m_tooltipTimer = 0;
    }

    auto* p = new TooltipPayload{ std::move(txt), width };

    // 3. 用 + 号把 lambda 转成 C 函数指针
    using CB = void CALLBACK(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    static CB* const proc = +[](UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR)
        {
            auto* t = reinterpret_cast<TooltipPayload*>(dwUser);
            g_bootstrap->show_tooltip(std::move(t->html), std::move(t->width));
            delete t;
        };

    m_tooltipTimer = timeSetEvent(delayMs, 1, proc,
        reinterpret_cast<DWORD_PTR>(p), TIME_ONESHOT);
}

void AppBootstrap::cancel_delayed_tooltip()
{
    if (m_tooltipTimer)
    {
        timeKillEvent(m_tooltipTimer);
        m_tooltipTimer = 0;
    }
    hide_tooltip();   // 立即隐藏已显示的 tooltip
}

void TocPanel::copy_to_clipboard()
{
    //if (m_sel_text.empty()) return;

    //const size_t bufSize = (m_sel_text.size() + 1) * sizeof(wchar_t);
    //if (HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bufSize)) {
    //    if (wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem))) {
    //        // 直接复制整个字符串内容
    //        wcscpy_s(dst, m_sel_text.size() + 1, m_sel_text.c_str());
    //        GlobalUnlock(hMem);

    //        if (OpenClipboard(g_hWnd)) {
    //            EmptyClipboard();
    //            SetClipboardData(CF_UNICODETEXT, hMem);
    //            CloseClipboard();
    //        }
    //        else {
    //            GlobalFree(hMem);
    //        }
    //    }
    //    else {
    //        GlobalFree(hMem);
    //    }
    //}
}

void AppBootstrap::copy_to_clipboard(HWND hwnd, std::wstring txt)
{
    if (txt.empty()) return;

    const size_t bufSize = (txt.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bufSize)) {
        if (wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem))) {
            // 直接复制整个字符串内容
            wcscpy_s(dst, txt.size() + 1, txt.c_str());
            GlobalUnlock(hMem);

            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                SetClipboardData(CF_UNICODETEXT, hMem);
                CloseClipboard();
            }
            else {
                GlobalFree(hMem);
            }
        }
        else {
            GlobalFree(hMem);
        }
    }
}


Timer::Timer(const std::string& name):
    name_(name), start_(std::chrono::high_resolution_clock::now()) 
{
    //std::cout << name_ << " started...\n";
}
Timer::~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
    g_timerOutput->add(name_, duration.count());
    //std::string txt = name_ + ":" + std::to_string(duration.count() / 1000000.0f) + " s\n";
    //OutputDebugStringA(txt.c_str());
    //std::cout << name_ << " ended. Duration: " << duration.count() << " us\n";
}

void TimerOutput::add(std::string name, uint64_t duration)
{
    for(auto&m: m_map)
    {
        if (m.name == name) 
        { 
            m.duration += duration; 
            m.times += 1;
            return;
        }
 
    }

    m_map.push_back(data{name, duration, 1});
}

void TimerOutput::print()
{
    for (auto& m :m_map)
    {
        // 对齐和填充格式化
        std::string txt = std::format("{:<30}: {:>8.9f} 秒, {:>6} 次",
            m.name,
            m.duration / 1000000000.0,  // 纳秒转秒
            m.times);

        // 如果需要添加换行符
        txt += "\n";
        OutputDebugStringA(txt.c_str());
    }
    clear();
}

void TimerOutput::clear()
{
    m_map = {};
}
