#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <memory>

#define NOMINMAX
#include <Windows.h>
#include <wrl/client.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <d3d11.h>

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <blake3.h>
#include <lunasvg/lunasvg.h>   


#include "litehtml.h"
#include "Book.h"
#include "EPUBBook.h"
#include "Timer.h"
#include "AppSettings.h"
#include "AppBootstrap.h"
#include "Constants.h"


using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;






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
    void clear_background();
    void build_font_index(const std::string& tempDir);
    std::string blake3_hex(const std::vector<uint8_t>& data);
    void set_book(std::shared_ptr<Book>& book) { m_book = book; }
    std::wstring m_sel_text = L"";
    std::unordered_map<FontKey, std::vector<std::string>> m_fontBin;
    std::vector<std::string> get_font_path();
private:

    float m_dpi_x = 96.0f;
    float m_dpi_y = 96.0f;

    std::shared_ptr<Book> m_book;
    std::vector<RECT> get_selection_rows() const;
    std::wstring get_selection_text() const;
    std::vector<std::string> m_font_path = {};



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
    ComPtr<ID2D1SolidColorBrush> m_backgroundBrush;
    ComPtr<ID2D1SolidColorBrush> m_debugBrush;
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

    IDWriteTextLayout* getLayout(const std::string& txt, litehtml::uint_ptr hFont);
    ComPtr<ID2D1Bitmap> getBitmap(litehtml::uint_ptr hdc, std::string url);
    //void draw_decoration(litehtml::uint_ptr hdc, const FontPair* fp, litehtml::web_color color, const litehtml::position& pos, IDWriteTextLayout* layout);


    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> m_brushPool;
    //FontCache m_fontCache;
    //LayoutCache m_layoutCache;
    std::unordered_map<std::string, IDWriteTextLayout*> m_layoutCache;
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
