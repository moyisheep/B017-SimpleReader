#pragma once
// main.cpp  ——  优化后完整单文件
#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _USE_MATH_DEFINES
#include "resource.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <memory>
#include <string>
#include <future>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <set>
#include <codecvt>
#include <locale>
#include <cctype>
#include <queue>
#include <robuffer.h>   // IBufferByteAccess
#include <new>
#include <iostream>
#include <numeric>
#include <mutex>

#include <wrl/client.h>
#include <wrl.h>
#include <wrl/implements.h>   // 关键
#include <windows.h>
#include <windowsx.h>   // 加这一行
#include <mmsystem.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commdlg.h>   // OPENFILENAMEW, GetOpenFileNameW
#include <shobjidl.h> // 包含任务对话框头文件
#include <objidl.h>
#include <Shlobj.h>      // SHGetKnownFolderPath
#include <KnownFolders.h>
#include <gdiplus.h>
#include <wininet.h>
#include <wincodec.h>
#include <atomic>
#include <condition_variable>
#include <array>
#include <shared_mutex>
#include <cstdint>
#include <cwctype>
#include <functional>
#include <cstring>
#include <stack>

#include <dwrite_3.h>
#include <d2d1_3.h>        // ID2D1DeviceContext / ID2D1Bitmap1
#include <d2d1_1.h>       // D2D 1.1
#include <d3d11.h>        // D3D11
#include <dxgi1_2.h>  // DXGI 1.2
#include <d2d1.h>
#include <d2d1helper.h>   // 保险起见，再带一次


#include <miniz/miniz.h>
#include <tinyxml2.h>
#include <lunasvg/lunasvg.h>   

#include <litehtml.h>
#include <gumbo.h>
#include <sqlite3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <blake3.h>
#include <boost/algorithm/string.hpp>

using namespace Gdiplus;



#ifndef HR
#define HR(hr)  do { HRESULT _hr_ = (hr); if(FAILED(_hr_)) return 0; } while(0)
#endif


#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "windowscodecs.lib")

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")



#include "MML2SVG.h"
#include "ReadingRecorder.h"
#include "EPUBBook.h"




class Timer {
public:
    // 构造函数开始计时
    Timer(const std::string& name = "Timer");


    // 析构函数结束计时并输出结果
    ~Timer();

    // 禁止拷贝和赋值
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // 可以移动构造
    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

private:
    std::string name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

using Microsoft::WRL::ComPtr;

namespace fs = std::filesystem;

struct ImageFrame
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;          // 每行字节数
    std::vector<uint8_t> rgba;     // 连续像素，8-bit * 4
    std::vector<uint8_t> raw_data;     // 连续像素，8-bit * 4
};


// 一个字符在窗口坐标系中的包围盒
struct CharBox
{
    wchar_t  ch;
    D2D1_RECT_F rect;   // 左上角 (x,y) 右下角 (x+width,y+height)
    std::wstring familyName;
    size_t   offset; // 在整篇纯文本中的偏移
};

// 一行文本的所有字符
using LineBoxes = std::vector<CharBox>;

// ---------- 字体缓存 ----------
struct FontPair {
    ComPtr<IDWriteTextFormat2> format;
    litehtml::font_description descr;
    std::string familyName;
};

// -------------- 运行时策略 -----------------
enum class Renderer { GDI, D2D};

class AppBootstrap {
public:
    AppBootstrap();
    ~AppBootstrap();
    struct script_info
    {
        litehtml::element::ptr el;   // 只需要保留节点指针
    };

    void enableJS();
    void disableJS();
    void bind_host_objects();   // 新增
    //void make_tooltip_backend();
    void run_pending_scripts();

    
    void show_tooltip(const std::string html, int width);
    void hide_imageview();
    void hide_tooltip();
    void delayed_show_tooltip(std::string txt, unsigned width = 300, unsigned delayMs = 300);
    void cancel_delayed_tooltip();
    std::string extract_anchor(const char* href);

    litehtml::element::ptr find_link_in_chain(litehtml::element::ptr start);

    static bool skip_attr(const std::string& val);

    static std::string get_html(litehtml::element::ptr el);

    std::string html_of_anchor_paragraph(litehtml::document* doc, const std::string& anchorId);

    std::string get_html_of_image(litehtml::element::ptr start);

    void show_imageview(const litehtml::element::ptr& el);
    static std::string get_anchor_html(litehtml::document* doc, const std::string& anchor);
    struct TooltipPayload { std::string html; unsigned width; };
    MMRESULT m_tooltipTimer = 0;


    std::vector<script_info> m_pending_scripts;

    void copy_to_clipboard(HWND hwnd, std::wstring txt);

    //std::unique_ptr<js_runtime> m_jsrt;   // 替换裸 duk_context*
};





class FileCollectionLoader : public IDWriteFontCollectionLoader
{
    LONG ref_ = 1;
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteFontCollectionLoader))
        {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }

    // IDWriteFontCollectionLoader
    IFACEMETHODIMP CreateEnumeratorFromKey(
        IDWriteFactory* factory,
        void const* key, UINT32 keySize,
        IDWriteFontFileEnumerator** ppEnumerator) override
    {
        *ppEnumerator = new FileEnumerator(
            factory,
            reinterpret_cast<IDWriteFontFile* const*>(key),
            keySize / sizeof(IDWriteFontFile*));
        return *ppEnumerator ? S_OK : E_OUTOFMEMORY;
    }

private:
    class FileEnumerator : public IDWriteFontFileEnumerator
    {
        IDWriteFactory* fac_;
        std::vector<ComPtr<IDWriteFontFile>> files_;
        UINT32 idx_ = 0;
        LONG ref_ = 1;
    public:
        FileEnumerator(IDWriteFactory* f, IDWriteFontFile* const* files, UINT32 n)
            : fac_(f), files_(files, files + n) {
        }

        IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteFontFileEnumerator))
            {
                *ppv = this; AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }
        IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
        IFACEMETHODIMP_(ULONG) Release() override
        {
            ULONG r = InterlockedDecrement(&ref_);
            if (r == 0) delete this;
            return r;
        }

        IFACEMETHODIMP MoveNext(BOOL* hasCurrent) override
        {
            *hasCurrent = idx_ < files_.size();
            return S_OK;
        }
        IFACEMETHODIMP GetCurrentFontFile(IDWriteFontFile** file) override
        {
            *file = idx_ < files_.size() ? files_[idx_++].Get() : nullptr;
            if (*file) (*file)->AddRef();
            return S_OK;
        }
    };
};
// 全局缓存（也可放 D2DBackend 内）

struct FontCachePair 
{
    std::string familyName;
    ComPtr<IDWriteTextFormat2> fmt;
    ComPtr<IDWriteFont> font;
};
class FontCache {
public:
    FontCache();
    ~FontCache() = default;
    // 主入口：根据 litehtml 描述 + 可选私有集合，返回 TextFormat
    FontCachePair*
        get(std::string& familyName, const litehtml::font_description& descr, IDWriteFontCollection* sysColl = nullptr);
    ComPtr<IDWriteFontCollection> CreatePrivateCollectionFromFile(IDWriteFactory* dw, const wchar_t* path);

    void clear();
private:


    // 内部：真正创建
    FontCachePair*
        create(std::string& familyName, const litehtml::font_description& descr, IDWriteFontCollection* sysColl);

    // 工具：在指定集合里找家族
    bool findFamily(IDWriteFontCollection* coll,
        const std::string& name,
        Microsoft::WRL::ComPtr<IDWriteFontFamily>& family,
        UINT32& index);

    std::unordered_map<std::string, FontCachePair*> m_map;
    mutable std::shared_mutex              m_mtx;
    Microsoft::WRL::ComPtr<IDWriteFactory3>   m_dw;
    std::unordered_map<std::string, ComPtr<IDWriteFontCollection>> collCache;
    FileCollectionLoader* m_loader;

};








struct AppSettings {
    bool enableCSS = true;   // 默认启用 css
    bool enableJS = false;   // 默认禁用 JS
    bool enableEPUBFonts = true;
    bool enableGlobalCSS = true;
    bool enableScrollAnimation = false;

    bool enableHoverPreview = true;
    bool enableClickPreview = true;
    bool enableCustomFont = false;
    bool enableFontRealtimePreview = true;
    bool displayTOC = true;
    bool displayStatusBar = true;
    bool displayMenuBar = true;
    bool displayScrollBar = true;
    bool displayToolbar = true;
    bool displayFrameRate = true;


    int record_update_interval_ms = 1000;
    int record_flush_interval_ms = 10 * 1000;
    int tooltip_delay_ms = 300;
    int update_interval_ms = 20;
    int scroll_update_interval_ms = 16;

    int font_size = 16;
    float line_height = 1.5f; //倍数
    int document_width = 600;

    int default_font_size = 16;
    float default_line_height = 1.5;
    int default_document_width = 800;
    std::string font_name = "Georgia";
    float zoom_factor = 1.0f;
    Renderer fontRenderer = Renderer::D2D;
    std::string default_font_name = "Microsoft YaHei";

    std::wstring temp_dir = L"SimpleReaderTemp";

    int tooltip_width = 500;
    std::string appName = "Simple Reader";
    int split_space_height = 300; // 单位:px
    std::string default_serif = "Georgia";
    std::string default_sans_serif = "Verdana";
    std::string default_monospace = "Consolas";



    // 1) GDI+ 颜色（A=255 不透明）
    Gdiplus::Color scrollbar_slider_color{ 255, 238, 165, 102 };
    Gdiplus::Color scrollbar_dot_color_highlight{ 255, 238, 165, 102 };
    Gdiplus::Color scrollbar_dot_color{ 209, 202, 197, 80 };
    COLORREF highlight_color_cr = RGB(238, 165, 102);  // #eea566
    COLORREF hover_color_cr = RGB(240, 240, 240);  
    // 2) D2D1 颜色（保持原透明度 0.4，可按需改）
    D2D1::ColorF highlight_color_d2d{
        238.0f / 255.0f,  // R
        165.0f / 255.0f,  // G
        102.0f / 255.0f,  // B
        0.4f              // A
    };
    D2D1::ColorF default_background_color{
        255.0f / 255.0f,  // R
        255.0f / 255.0f,  // G
        255.0f / 255.0f,  // B
        1.0f              // A
    };
    D2D1::ColorF background_color{
    255.0f / 255.0f,  // R
    255.0f / 255.0f,  // G
    255.0f / 255.0f,  // B
    1.0f              // A
    };

};
struct AppStates {
    // ---- 取消令牌 ----
    std::shared_ptr<std::atomic_bool> cancelToken;

    // ---- 状态机 ----

    bool isLoaded = false;
    // 工具：生成新令牌，旧令牌立即失效
    void newCancelToken() {
        if (cancelToken) cancelToken->store(true);
        cancelToken = std::make_shared<std::atomic_bool>(false);
    }
};

// ------------------------------------------------------------------
struct LayoutKey {
    std::string txt;
    std::string  fontKey;
    float        maxW;

    bool operator==(const LayoutKey& o) const noexcept {
        return txt == o.txt && fontKey == o.fontKey && maxW == o.maxW;
    }
};

// ------------------------------------------------------------------
namespace std {
    template<>
    struct hash<LayoutKey> {
        size_t operator()(const LayoutKey& k) const noexcept {
            std::string txt = k.txt + k.fontKey + std::to_string(k.maxW);

            return std::hash<std::string>{}(txt);
        }
    };
}


class LayoutCache {
public:
    LayoutCache() = default;   // 补上

    using Map = std::unordered_map<LayoutKey,
        Microsoft::WRL::ComPtr<IDWriteTextLayout>,
        std::hash<LayoutKey>>;

    void set(const LayoutKey& k, const Microsoft::WRL::ComPtr<IDWriteTextLayout>& layout) {
        std::lock_guard<std::mutex> lk(mtx_);
        map_[k] = layout;        // ComPtr 直接拷贝，引用计数自动管理
    }

    Microsoft::WRL::ComPtr<IDWriteTextLayout> get(const LayoutKey& k) const {
        Timer timer("    LayoutCache::get");
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = map_.find(k);
        return it != map_.end() ? it->second : nullptr;
    }

    void clear() noexcept {
        Map empty;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            map_.swap(empty);
        }
    }

    LayoutCache(const LayoutCache&) = delete;
    LayoutCache& operator=(const LayoutCache&) = delete;
private:
    mutable std::mutex mtx_;
    Map                map_;
};
struct FontItem
{
    std::wstring familyName;   // 字体原名（en-us）
    std::wstring displayName;  // 中文名（zh-cn），没有就用 familyName
};


// ---------- LiteHtml 容器 ----------
class SimpleContainer : public litehtml::document_container {
public:
    SimpleContainer(int w, int h, HWND hwnd);
    ~SimpleContainer();


    // createFromString
    litehtml::element::ptr	create_element(const char* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) override;
    void	get_media_features(litehtml::media_features& media) const override;
    void	import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override;
    litehtml::pixel_t	get_default_font_size() const override;
    const char* get_default_font_name() const override;

  

    std::vector<std::string> get_font_alias(std::string fontName);

    void InitDefaultFont();

    bool AddPrivateFont(const wchar_t* path);

    bool BuildFontCollection();

    litehtml::uint_ptr	create_font(const litehtml::font_description& descr, const litehtml::document* doc, litehtml::font_metrics* fm) override;
    litehtml::pixel_t	text_width(const char* text, litehtml::uint_ptr hFont) override;
    litehtml::pixel_t	pt_to_px(float pt) const override;
    void	load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;
    void	set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
    void	set_caption(const char* caption) override;
    void	transform_text(litehtml::string& text, litehtml::text_transform tt) override;



    // render 
    void	get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;
    void	get_viewport(litehtml::position& viewport) const override;



    // draw
    void del_clip() override;
    void delete_font(litehtml::uint_ptr hFont) override;
    void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;
    void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const std::string& url, const std::string& base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::web_color& color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::radial_gradient& gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::conic_gradient& gradient) override;
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;
    void	draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;





    // 事件
    void	on_anchor_click(const char* url, const litehtml::element::ptr& el) override;
    bool	on_element_click(const litehtml::element::ptr& /*el*/) override;
    void	on_mouse_event(const litehtml::element::ptr& el, litehtml::mouse_event event) override;
    void	set_base_url(const char* base_url) override;
    void	link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override;
    void	set_cursor(const char* cursor) override;
    void	get_language(litehtml::string& language, litehtml::string& culture) const override;


    // litehtml 新增
    //void	split_text(const char* text, const std::function<void(const char*)>& on_word, const std::function<void(const char*)>& on_space) override;
   // litehtml::string resolve_color(const litehtml::string& /*color*/) const { return litehtml::string(); }

    

    void build_rounded_rect_path(ComPtr<ID2D1GeometrySink>& sink, const litehtml::position& pos, const litehtml::border_radiuses& bdr);


    void resize(int width, int height);

    bool isImageCached(std::string src);

    void addImageCache(std::string hash, std::string svg);

    void clear();
    std::vector<FontItem> getFontList();
 
    void init_dpi();

    LPCWSTR m_currentCursor = IDC_IBEAM;
    std::unordered_map<std::string, ImageFrame> m_img_cache;
    std::unordered_map<std::string, std::string> m_css_cache;
    std::unordered_map<std::string, litehtml::element::ptr> m_anchor_map;
    litehtml::document::ptr m_doc;
    float m_line_height = 1.0f;
    float m_zoom_factor = 1.0f;
    int width()  const { return m_w; }
    int height() const { return m_h; }
    litehtml::uint_ptr getContext();
    void clear_selection();

    void on_lbutton_dblclk(int x, int y);
    void on_lbutton_up();
    void on_lbutton_down(int x, int y);
    void on_mouse_move(int x, int y);
    void on_mouse_wheel(float delta);
    void copy_to_clipboard();
    void present(float x, float y, litehtml::position* clip);
    void BuildFontList();

    void clear_font_cache() { m_layoutCache.clear();   m_textWidthCache.clear(); }

    std::wstring m_sel_text = L"";

private:

    float m_dpi_x = 96.0f;
    float m_dpi_y = 96.0f;


    std::vector<RECT> get_selection_rows() const;
    std::wstring get_selection_text() const;



    //ComPtr<ID2D1HwndRenderTarget> m_rt;


    int  m_w, m_h;

    D2D1_MATRIX_3X2_F m_oldMatrix{};
    HWND m_hwnd = nullptr;
    ComPtr<ID2D1Factory1> m_d2dFactory = nullptr;   // 原来是 ID2D1Factory = nullptr;

    int64_t hit_test(float x, float y);

    bool   m_selecting = false;

    // 当前选区
    int64_t m_selStart = -1;   // 字符级偏移
    int64_t m_selEnd = -1;   // 同上
    std::vector<LineBoxes> m_lines;
    std::wstring           m_plainText;       // 整篇纯文本

    ComPtr<ID2D1SolidColorBrush> m_selBrush;
    void record_char_boxes(
        ComPtr<IDWriteTextLayout> layout,
        const std::wstring& wtxt,
        const std::wstring& familyName, 
        const litehtml::position& pos);

    std::vector<std::string> split_font_list(const std::string& src);

    bool is_all_zero(const litehtml::border_radiuses& r);


    // 自动 AddRef/Release

    Microsoft::WRL::ComPtr<IDWriteFontCollection> m_privateFonts;  // 新增
    std::vector<std::wstring> m_tempFontFiles;
    //std::unordered_map<std::string, ComPtr<ID2D1Bitmap>> m_d2dBitmapCache;

    static std::wstring toLower(std::wstring s);
    //static std::optional<std::wstring> mapStatic(const std::wstring& key);
    float m_baselineY = 0;
    std::vector<ComPtr<ID2D1Layer>>  m_clipStack;  // 新增
    //ComPtr<IDWriteFontCollection> m_systemFonts;
    static std::wstring normalize_quotes(const std::wstring& src);
    ComPtr<ID2D1SolidColorBrush> getBrush(litehtml::uint_ptr hdc, const litehtml::web_color& c);

    ComPtr<IDWriteTextLayout> getLayout(const std::string& txt,  litehtml::uint_ptr hFont);
    ComPtr<ID2D1Bitmap> getBitmap(litehtml::uint_ptr hdc, std::string url);
    //void draw_decoration(litehtml::uint_ptr hdc, const FontPair* fp, litehtml::web_color color, const litehtml::position& pos, IDWriteTextLayout* layout);


    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> m_brushPool;
    //FontCache m_fontCache;
    //LayoutCache m_layoutCache;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDWriteTextLayout>> m_layoutCache;
    std::unordered_map<std::string, float> m_textWidthCache;
    ComPtr<IDWriteFactory3>    m_dwrite;

    ComPtr<IDWriteTextAnalyzer> m_analyzer;

    std::unordered_map<std::string, ComPtr<ID2D1Bitmap>> m_d2dBmpCache;
    std::mutex m_imgCacheMutex;
    Microsoft::WRL::ComPtr<ID2D1Device>      m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_dc;   // 取代原来的 m_rt
    Microsoft::WRL::ComPtr<IDXGISwapChain1>   m_swapChain;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_targetBmp;
    Microsoft::WRL::ComPtr<IDXGISurface> m_backBuffer;

    float m_sel_delta = 0;
    std::vector<D2D1_RECT_F> m_sel_rects = {};
    ComPtr<FileCollectionLoader> m_loader;
    std::vector<ComPtr<IDWriteFontFile>> m_defaultFontFiles; // 存储所有字体文件
    std::vector<ComPtr<IDWriteFontFile>> m_privateFontFiles; // 存储所有字体文件
    std::vector<FontItem> m_fontList;
};



struct ScrollPosition
{
    int spine_id = 0;
    float offset = 0.0f;
    float height = 0.0f;
};


struct BodyBlock {
    int spine_id = 0;
    int block_id = 0;
    std::string html;
    float height = 0.0f; // 未渲染前默认 -1

};

struct HtmlBlock {
    int spine_id;
    float height = 0.0f;
    std::string head;
    std::vector<BodyBlock> body_blocks;
};




class VirtualDoc {
public:
    VirtualDoc();
    ~VirtualDoc();
    void load_book();
    void OnTreeSelChanged(std::string href);
    void update_doc(int client_h);
    void load_html(std::string& href);
    void clear();
    ScrollPosition get_scroll_position();
    void set_scroll_position(ScrollPosition sp);
    std::vector<HtmlBlock> m_blocks;
    float get_height_by_id(int spine_id);
    void reload();
    bool exists(int spine_id);
    //void draw(int x, int y, int w, int h, float offsetY);
    litehtml::document::ptr m_doc;
    std::atomic<bool>        m_isReloading{ false }; 
    std::atomic<bool>        m_isAnchor{ false };
    float m_percent = 0.0;
    float  m_height = 0.0f;
    std::string m_anchor_id = "";
    std::atomic<bool>        m_workerBusy{ false }; // 是否正在干活
private:
    HtmlBlock get_html_block(std::string html, int spine_id);
    void merge_block(HtmlBlock& dst, HtmlBlock& src, bool isAddToBottom = true);
    int get_id_by_href(std::string& href);
    std::string get_href_by_id(int spine_id);
    std::string get_head(std::string& html);
    std::vector<BodyBlock> get_body_blocks(std::string& html, int spine_id = 0, size_t max_chunk_bytes = 4*1024);
    void serialize_node(const GumboNode* node, std::ostream& out);
    bool gumbo_tag_is_void(GumboTag tag);
    void serialize_element(const GumboElement& el, std::ostream& out);




    bool insert_next_chapter();

    void workerLoop();


    float get_height();
    bool insert_chapter(int spine_id, bool isPushBack=true);
    //bool insert_chapter(int spine_id);
    bool insert_prev_chapter();


    bool load_by_id(int spine_id, bool isPushBack);
    struct DocCache
    {
        litehtml::document::ptr doc;
        float height;
        int spine_id;
    };
    std::vector<OCFRef> m_spine;
    std::shared_ptr<EPUBBook> m_book;
    std::shared_ptr<SimpleContainer> m_container;
    //std::vector<DocCache> m_doc_cache;

    // 放在 VirtualDoc 内，仅这 5 个
    std::thread              m_worker;          // 后台线程
    std::mutex               m_taskMtx;         // 任务队列锁
    std::condition_variable  m_taskCv;          // 任务通知
    struct Task {
        int  chapterId;
        bool insertAtFront;   // true=prev, false=next
    };
    std::queue<Task>         m_taskQueue;       // 待处理任务

    std::condition_variable m_cvFinish;
    std::atomic<bool> m_cancelFlag{ false };
};





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
    struct Node : TreeNode{};

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


struct GetDocParam
{
    int        client_h;
    int        scrollY;
    int        offsetY;
    HWND       notify_hwnd;   // 通知窗口
};


class AccelManager {
public:
    explicit AccelManager(HWND h) : m_hwnd(h) {}

    // 添加/更新一条快捷键
    void set(WORD cmd, BYTE fVirt, WORD key) {
        // 先删除同命令的旧项
        erase(cmd);
        m_entries.push_back({ fVirt, key, cmd });
        rebuild();
    }
    void add(WORD cmd, BYTE fVirt, WORD key) {
        m_entries.push_back({ fVirt, key, cmd });
        rebuild();
    }
    // 删除某命令
    void erase(WORD cmd) {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [=](const ACCEL& a) { return a.cmd == cmd; }),
            m_entries.end());
        rebuild();
    }

    // 在消息循环里调用
    bool translate(MSG* m) {
        return m_hAccel && TranslateAccelerator(m_hwnd, m_hAccel, m);
    }

private:
    void rebuild() {
        if (m_hAccel) DestroyAcceleratorTable(m_hAccel);
        m_hAccel = m_entries.empty()
            ? nullptr
            : CreateAcceleratorTable(m_entries.data(),
                static_cast<int>(m_entries.size()));
    }

    std::vector<ACCEL> m_entries;
    HACCEL m_hAccel = nullptr;
    HWND m_hwnd;
};


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


struct BmpHeader {
    uint16_t bfType = 0x4D42;          // 'BM'
    uint32_t bfSize = 0;
    uint16_t bfReserved1 = 0;
    uint16_t bfReserved2 = 0;
    uint32_t bfOffBits = 54;           // 54 = sizeof(BmpHeader) + sizeof(BmpInfo)
};
struct BmpInfo {
    uint32_t biSize = 40;
    int32_t  biWidth = 0;
    int32_t  biHeight = 0;
    uint16_t biPlanes = 1;
    uint16_t biBitCount = 32;
    uint32_t biCompression = 0;      // BI_RGB
    uint32_t biSizeImage = 0;
    int32_t  biXPelsPerMeter = 0;
    int32_t  biYPelsPerMeter = 0;
    uint32_t biClrUsed = 0;
    uint32_t biClrImportant = 0;
};
//
//namespace mathml2tex {
//
//    /* 唯一对外接口 */
//    std::string convert(const std::string& mathml);
//
//} // namespace mathml2tex

//enum PuncClass {
//    PC_NORMAL,   // 普通字符
//    PC_LEFT,   // 左引号、左括号、书名号前半
//    PC_RIGHT,  // 右引号、右括号、句末标点
//    PC_MIDDLE, // 破折号、省略号（不可拆）
//};
//
//static PuncClass classify(UChar32 cp) {
//    switch (cp) {
//        /* ---------- 左半部分 ---------- */
//    case 0x3008: case 0x300A: case 0x300C: case 0x300E: // 〈 《 「 『
//    case 0xFF08:                                        // （
//    case 0x3010: case 0x3014: case 0x3016: case 0x3018: // 【 〔 〖 〘
//    case 0xFF3B: case 0xFF5B: case 0xFF5F:              // ［ ｛ ｟
//    case 0x2018: case 0x201C:                           // ‘ “
//        return PC_LEFT;
//
//        /* ---------- 右半部分 ---------- */
//    case 0x3001: case 0x3002:                           // 、 。
//    case 0xFF01: case 0xFF1F:                           // ！ ？
//    case 0xFF0C: case 0xFF1B: case 0xFF1A:              // ， ； ：
//    case 0x3009: case 0x300B: case 0x300D: case 0x300F: // 〉 》 」 』
//    case 0xFF09:                                        // ）
//    case 0x3011: case 0x3015: case 0x3017: case 0x3019: // 】 〕 〗 〙
//    case 0xFF3D: case 0xFF5D: case 0xFF60:              // ］ ｝ ｠
//    case 0x2019: case 0x201D:                           // ’ ”
//        return PC_RIGHT;
//
//        /* ---------- 中间整体 ---------- */
//    case 0x2014:                                        // —  破折号
//    case 0x2026:                                        // …  省略号
//    case 0x2025:                                        // ‥  二点省略
//        return PC_MIDDLE;
//
//    default:
//        return PC_NORMAL;
//    }
//}


class BusyGuard {
public:
    explicit BusyGuard(std::atomic<bool>& flag) : m_flag(flag) {
        m_flag.store(true, std::memory_order_relaxed);
    }
    ~BusyGuard() {
        //OutputDebugStringA("BusyGuard::~BusyGuard() - workerBusy = false\n");
        m_flag.store(false, std::memory_order_relaxed);
    }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;
private:
    std::atomic<bool>& m_flag;
};






class TimerOutput
{
public:
    TimerOutput() { m_map = {}; }
    ~TimerOutput(){}
    void add(std::string name, uint64_t duration);
    void print();

    void clear();
    void start(std::string info="timer") { timer = std::make_unique<Timer>(info); }
    void end() { if (timer) { timer.reset(); print(); } }
private:
    std::string format_duration(double seconds);
    struct data
    {
        std::string name = "";
        uint64_t duration = 0;
        uint64_t times = 0;
    };
    std::unique_ptr<Timer> timer;
    std::vector<data> m_map;
};


// 创建自定义字体加载器
class CustomFontCollectionLoader : public IDWriteFontCollectionLoader
{
public:
    // IUnknown接口
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&refCount); }
    STDMETHOD_(ULONG, Release)()
    {
        ULONG newCount = InterlockedDecrement(&refCount);
        if (newCount == 0) delete this;
        return newCount;
    }

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject)
    {
        if (riid == __uuidof(IDWriteFontCollectionLoader) ||
            riid == __uuidof(IUnknown))
        {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    // IDWriteFontCollectionLoader接口
    STDMETHOD(CreateEnumeratorFromKey)(
        IDWriteFactory* factory,
        const void* collectionKey,
        UINT32 collectionKeySize,
        IDWriteFontFileEnumerator** fontFileEnumerator)
    {
        // 实现字体文件枚举器
        *fontFileEnumerator = nullptr;
        return E_NOTIMPL;
    }

private:
    ULONG refCount = 1;
};

bool IsMouseOverWindow(HWND hWnd);