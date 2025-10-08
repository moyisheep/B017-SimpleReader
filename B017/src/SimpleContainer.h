#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl.h>
#include <wrl/implements.h>   // 关键
#include <d2d1_3.h>        // ID2D1DeviceContext / ID2D1Bitmap1

#include <dwrite_1.h>   // 需要 IDWriteTextFormat1
#include <d2d1_1.h>       // D2D 1.1
#include <d3d11.h>        // D3D11
#include <dxgi1_2.h>  // DXGI 1.2
#include <dwrite_3.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <litehtml.h>
#include <string>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <stdint.h>
#include <array>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
using Microsoft::WRL::ComPtr;

#include <lunasvg/lunasvg.h>
#include "3rdParty/stb_image.h"
#include "FontKey.h"
#include "MemFile.h"
#include "a2w_w2a.h"

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
    uint32_t   offset; // 在整篇纯文本中的偏移
};

// 一行文本的所有字符
using LineBoxes = std::vector<CharBox>;

// ---------- 字体缓存 ----------
struct FontPair {
    ComPtr<IDWriteTextFormat> format;
    litehtml::font_description descr;
    std::wstring familyName;
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
    std::wstring familyName;
    ComPtr<IDWriteTextFormat> fmt;
    ComPtr<IDWriteFont> font;
};
class FontCache {
public:
    FontCache();
    ~FontCache() = default;
    // 主入口：根据 litehtml 描述 + 可选私有集合，返回 TextFormat
    FontCachePair*
        get(std::wstring& familyName, const litehtml::font_description& descr, IDWriteFontCollection* sysColl = nullptr);
    ComPtr<IDWriteFontCollection> CreatePrivateCollectionFromFile(IDWriteFactory* dw, const wchar_t* path);

    void clear();
private:


    // 内部：真正创建
    FontCachePair*
        create(std::wstring& familyName, const litehtml::font_description& descr, IDWriteFontCollection* sysColl);

    // 工具：在指定集合里找家族
    bool findFamily(IDWriteFontCollection* coll,
        const std::wstring& name,
        Microsoft::WRL::ComPtr<IDWriteFontFamily>& family,
        UINT32& index);

    std::unordered_map<std::wstring, FontCachePair*> m_map;
    mutable std::shared_mutex              m_mtx;
    Microsoft::WRL::ComPtr<IDWriteFactory>   m_dw;
    std::unordered_map<std::wstring, ComPtr<IDWriteFontCollection>> collCache;
    FileCollectionLoader* m_loader;

};

// ------------------------------------------------------------------
struct LayoutKey {
    std::wstring txt;
    std::wstring  fontKey;
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
            std::wstring txt = k.txt + k.fontKey + std::to_wstring(k.maxW);

            return std::hash<std::wstring>{}(txt);
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


// ---------- LiteHtml 容器 ----------
class SimpleContainer : public litehtml::document_container {
public:
    SimpleContainer(int w, int h, HWND hwnd);

    void BuildFontList();

    ~SimpleContainer();


    litehtml::pixel_t	get_default_font_size() const override;
    const char* get_default_font_name() const override;



    void	get_viewport(litehtml::position& viewport) const override;
    void	set_caption(const char* caption) override;
    void	set_base_url(const char* base_url) override;
    void	set_cursor(const char* cursor) override;

    void	import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& baseurl) override;


    void	link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override;


    void	transform_text(litehtml::string& text, litehtml::text_transform tt) override;

    litehtml::element::ptr	create_element(const char* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) override;

    void	get_media_features(litehtml::media_features& media) const override;
    void	get_language(litehtml::string& language, litehtml::string& culture) const override;

    // 事件
    void	on_anchor_click(const char* url, const litehtml::element::ptr& el) override;
    bool	on_element_click(const litehtml::element::ptr& /*el*/) override;
    void	on_mouse_event(const litehtml::element::ptr& el, litehtml::mouse_event event) override;

    // litehtml 新增
    //void	split_text(const char* text, const std::function<void(const char*)>& on_word, const std::function<void(const char*)>& on_space) override;
   // litehtml::string resolve_color(const litehtml::string& /*color*/) const { return litehtml::string(); }

    litehtml::pixel_t	pt_to_px(float pt) const override;


    // 渲染后端需要实现的
    void draw_text(litehtml::uint_ptr hdc, const char* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;

    void draw_image(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const std::string& url, const std::string& base_url) override;
    void draw_solid_fill(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::web_color& color) override;
    void draw_linear_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::linear_gradient& gradient) override;
    void draw_radial_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::radial_gradient& gradient) override;
    void draw_conic_gradient(litehtml::uint_ptr hdc, const litehtml::background_layer& layer, const litehtml::background_layer::conic_gradient& gradient) override;
    void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;
    void	draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;
   
    litehtml::uint_ptr	create_font(const litehtml::font_description& descr, const litehtml::document* doc, litehtml::font_metrics* fm) override;
    void				delete_font(litehtml::uint_ptr hFont) override;
    litehtml::pixel_t	text_width(const char* text, litehtml::uint_ptr hFont) override;
    void build_rounded_rect_path(ComPtr<ID2D1GeometrySink>& sink, const litehtml::position& pos, const litehtml::border_radiuses& bdr);
    void	set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius) override;
    void	del_clip() override;
    void	load_image(const char* src, const char* baseurl, bool redraw_on_ready) override;
    void	get_image_size(const char* src, const char* baseurl, litehtml::size& sz) override;

    void BeginDraw();
    void EndDraw();
    void resize(int width, int height);

    bool isImageCached(std::string src);

    void addImageCache(std::string hash, std::string svg);
    void clear();


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
    void on_lbutton_up(int x, int y);
    void on_mouse_move(int x, int y);
    void on_mouse_wheel(int delta);
    void copy_to_clipboard();
    void present(float x, float y, litehtml::position* clip);

    void clear_font_cache() { m_layoutCache.clear(); m_fontCache.clear(); }
    ComPtr<ID2D1Bitmap1> m_offscreenBmp;   // 离屏位图
    bool                 m_offscreenDirty = true; // 是否需要重绘
private:

    float m_px_per_pt{ 96.0f / 72.0f };   // 默认 96 DPI

    std::vector<RECT> get_selection_rows() const;



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
    void record_char_boxes(ID2D1DeviceContext* rt, IDWriteTextLayout* layout, const std::wstring& wtxt, const litehtml::position& pos);

    std::vector<std::wstring> split_font_list(const std::string& src);

    bool is_all_zero(const litehtml::border_radiuses& r);


    // 自动 AddRef/Release

    Microsoft::WRL::ComPtr<IDWriteFontCollection> m_privateFonts;  // 新增
    std::vector<std::wstring> m_tempFontFiles;
    std::unordered_map<std::string, ComPtr<ID2D1Bitmap>> m_d2dBitmapCache;

    static std::wstring toLower(std::wstring s);
    //static std::optional<std::wstring> mapStatic(const std::wstring& key);
    float m_baselineY = 0;
    std::vector<ComPtr<ID2D1Layer>>  m_clipStack;  // 新增
    ComPtr<IDWriteFontCollection> m_sysFontColl;
    static std::wstring normalize_quotes(const std::wstring& src);
    ComPtr<ID2D1SolidColorBrush> getBrush(litehtml::uint_ptr hdc, const litehtml::web_color& c);

    ComPtr<IDWriteTextLayout> getLayout(const std::wstring& txt, litehtml::uint_ptr hFont, float maxW);
    ComPtr<ID2D1Bitmap> getBitmap(litehtml::uint_ptr hdc, std::string url);
    void draw_decoration(litehtml::uint_ptr hdc, const FontPair* fp, litehtml::web_color color, const litehtml::position& pos, IDWriteTextLayout* layout);


    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> m_brushPool;
    FontCache m_fontCache;
    LayoutCache m_layoutCache;

    ComPtr<IDWriteFactory>    m_dwrite;

    ComPtr<IDWriteTextAnalyzer> m_analyzer;

    std::unordered_map<std::string, ComPtr<ID2D1Bitmap>> m_d2dBmpCache;
    std::mutex m_imgCacheMutex;
    Microsoft::WRL::ComPtr<ID2D1Device>      m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_dc;   // 取代原来的 m_rt
    Microsoft::WRL::ComPtr<IDXGISwapChain1>   m_swapChain;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_targetBmp;
    Microsoft::WRL::ComPtr<IDXGISurface> m_backBuffer;

};
enum class ImgFmt { PNG, JPEG, BMP, GIF, TIFF, SVG, UNKNOWN };
static ImgFmt detect_fmt(const uint8_t* d, size_t n, const wchar_t* ext);

static ImageFrame decode_img(const MemFile& mf, const wchar_t* ext);
