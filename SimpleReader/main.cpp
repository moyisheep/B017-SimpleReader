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

std::shared_ptr<Book>  g_book;

std::future<void> g_parse_task;

std::unique_ptr<VirtualDoc> g_vd;
//static float g_scrollY = 0.0f;   // 当前像素偏移
 std::atomic<float> g_offsetY{ 0.0f };
//std::vector<FontItem> g_fontList;




AppStates g_states;
AppSettings g_cfg;







std::unique_ptr<AppBootstrap> g_bootstrap;
std::unique_ptr<ReadingRecorder> g_recorder;

 MMRESULT g_tickTimer = 0;   // 0 表示当前没有定时器
 MMRESULT g_flushTimer = 0;

 MMRESULT g_updateTimer = 0;

 MMRESULT g_scrollTimer = 0;
 MMRESULT g_framerateTimer = 0;
std::atomic<float> g_velocity{ 0 };     // 像素/秒


int g_center_offset = 0;

std::string g_globalCSS = "";

 int   g_splitX = 200;       // 当前 TOC 宽度（初始值）
 bool  g_dragging = false;     // 是否正在拖动
 bool  g_imageview_dragging = false;
 POINT g_imageview_drag_pos{ 0,0 };
 bool g_mouse_tracked = false;


 int g_imageviewRenderW = 0;
// 全局
std::unique_ptr<TocPanel> g_toc;
std::unique_ptr<ScrollBarEx> g_scrollbar;

//std::vector<std::wstring> g_fontNames;       // 保存字体名
// 1. 在全局或合适位置声明
    // 整篇文档的所有行


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












//static std::wstring a2w(const std::string& s)
//{
//    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
//    return converter.from_bytes(s);
//}













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
 std::unordered_map<int, std::wstring> g_statusBuf;














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




// ---------- 入口 ----------
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int n)
{
    // 1. 分配控制台
    //AllocConsole();

    //// 2. 重定向标准输出到控制台
    //freopen("CONOUT$", "w", stdout);
    //freopen("CONOUT$", "w", stderr);
    //freopen("CONIN$", "r", stdin);
    //// 3. 现在可以使用 printf

 
    freopen("stdout.txt", "w", stdout);
    freopen("stdout.txt", "w", stderr);
    printf("Hello from Win32 Window!\n");
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







namespace fs = std::filesystem;



std::wstring GetMainFontNameFromTextLayout(
    ComPtr<IDWriteTextLayout> pTextLayout

) {
    //Timer timer("    GetMainFontNameFromTextLayout");
    std::wstring fontName = L"";
    if (pTextLayout == nullptr) {
        return fontName;
    }

    HRESULT hr = S_OK;
    WCHAR fontFamilyName[100];
    UINT32 fontFamilyNameLength = 0;

    // 获取第一个字符的字体名称
    hr = pTextLayout->GetFontFamilyNameLength(0, &fontFamilyNameLength);
    if (FAILED(hr) || fontFamilyNameLength == 0) {
        return fontName;
    }

    hr = pTextLayout->GetFontFamilyName(
        0,
        fontFamilyName,
        fontFamilyNameLength + 1
    );

    if (SUCCEEDED(hr)) {
        fontName = fontFamilyName;
    }

    return fontName;
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









static bool is_word_boundary(wchar_t ch)
{
    return iswspace(ch) || iswpunct(ch) || ch == L'\r' || ch == L'\n';
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


