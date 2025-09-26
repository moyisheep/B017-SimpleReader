#pragma once
// main.cpp  ——  优化后完整单文件
#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "resource.h"
#include <windows.h>
#include <windowsx.h>   // 加这一行

#include <mmsystem.h>

#include <commctrl.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#include <cmath>
#include <memory>
#include <string>
#include <future>
#include <unordered_map>
#include <algorithm>



#include <algorithm>





#include <objidl.h>
#include <filesystem>
#include <gdiplus.h>

using namespace Gdiplus;

#include <shlwapi.h>
#include <regex>

#include <sqlite3.h>
#include <wininet.h>



#include <unordered_set>
#include <chrono>
#include <thread>
#include <set>


#include <codecvt>
#include <locale>



#include <wincodec.h>


#include <cctype>

#include <queue>
#include <robuffer.h>   // IBufferByteAccess
#include <new>

#include <iostream>


#ifndef HR
#define HR(hr)  do { HRESULT _hr_ = (hr); if(FAILED(_hr_)) return 0; } while(0)
#endif


#include <atomic>


#include <condition_variable>
#include <array>


#include <cstdint>
#include <cwctype>

#include <functional>

#include <cstring>
#include <stack>
#include <blake3.h>
#include <boost/algorithm/string.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include FT_OUTLINE_H
#include FT_GLYPH_H
#include <Shlobj.h>      // SHGetKnownFolderPath
#include <KnownFolders.h>
#include <numeric>
#include <commdlg.h>   // OPENFILENAMEW, GetOpenFileNameW
#include <shobjidl.h> // 包含任务对话框头文件
#include <mutex>
#define STB_IMAGE_IMPLEMENTATION

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "freetype.lib")

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

#include <gumbo.h>
#include <tinyxml2.h>

#include "MemFile.h"
#include "EPUBBook.h"
#include "SimpleContainer.h"
namespace fs = std::filesystem;




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


    std::vector<script_info> m_pending_scripts;

    //std::unique_ptr<js_runtime> m_jsrt;   // 替换裸 duk_context*
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
    std::wstring font_name = L"Georgia";
    float zoom_factor = 1.0f;

    std::wstring default_font_name = L"Microsoft YaHei";

    std::wstring temp_dir = L"epub_book";

    int tooltip_width = 500;
    std::string appName = "Simple EPUB Reader";
    int split_space_height = 300; // 单位:px
    std::wstring default_serif = L"Georgia";
    std::wstring default_sans_serif = L"Verdana";
    std::wstring default_monospace = L"Consolas";



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
    void OnTreeSelChanged(std::wstring href);
    void update_doc(int client_h);
    void load_html(std::wstring& href);
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
    int get_id_by_href(std::wstring& href);
    std::wstring get_href_by_id(int spine_id);
    std::string get_head(std::string& html);
    std::vector<BodyBlock> get_body_blocks(std::string& html, int spine_id = 0, size_t max_chunk_bytes = 4*1024);
    void serialize_node(const GumboNode* node, std::ostream& out);
    bool gumbo_tag_is_void(GumboTag tag);
    void serialize_element(const GumboElement& el, std::ostream& out);




    bool insert_next_chapter();

    void workerLoop();


    float get_height();
    bool insert_chapter(int spine_id);
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





struct BookRecord {
    int64_t id = -1;                       // 数据库主键；-1 表示未找到
    std::string path;
    std::string title;
    std::string author;
    int         openCount = 0;
    int         totalWords = 0;
    int         lastSpineId = 0;
    int         lastOffset = 0;
    int         fontSize = 0;
    float       lineHeightMul = 0.0f;
    int         docWidth = 0;
    int         totalTime = 0;        // 累计阅读秒数
    int64_t     lastOpenTimestamp = 0;        // 微秒
    bool        enableCSS = true;
    bool        enableGlobalCSS = true;
    bool        enableCustomFont = false;
    std::string   fontName = "Verdana";
    float zoomFactor;

};
struct SettingRecord
{
    bool enableLoadEPUBFonts = true;
    bool enableScrollAnimation = false;
    bool enableHoverPreview = true;
    bool enableClickPreview = true;
    bool enableFontRealtimePreview = true;
    bool displayTOC = true;
    bool displayStatusBar = true;
    bool displayScrollBar = true;
    bool displayFrameRate = true;
};
struct timeFragment
{
    std::string path;
    std::string title;
    std::string author;
    int       spine_id;
    std::string chapter;
    int64_t timestamp;
};

class ReadingRecorder {
public:
    ReadingRecorder();
    ~ReadingRecorder();

    void openBook(const std::string absolutePath); // 返回记录（读或建）
    void flush();
    void flushSettingRecord();
    // 一次性写回
    void flushBookRecord();
    void flushTimeRecord();
    void updateRecord();
    int64_t getTotalTime();
    int64_t getBookTotalTime() const;

    BookRecord m_book_record;
    SettingRecord m_setting_record;
private:
    void initDB();

    bool loadSettings();



    sqlite3* m_dbBook = nullptr;
    sqlite3* m_dbTime = nullptr;
    sqlite3* m_dbSetting = nullptr;

    std::vector<timeFragment> m_time_frag;



};


class TocPanel
{
public:
    using OnNavigate = std::function<void(const std::wstring& href)>;
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

    float getAnchorOffsetY(const std::wstring& href);
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


class MathML2SVG {
public:
    static MathML2SVG& instance();

    // 删除拷贝/移动
    MathML2SVG(const MathML2SVG&) = delete;
    MathML2SVG& operator=(const MathML2SVG&) = delete;
    MathML2SVG(MathML2SVG&&) = delete;
    MathML2SVG& operator=(MathML2SVG&&) = delete;

    /* 业务接口 */
    std::string convert(const std::string& mathml);

    struct Style {
        std::string fontSize = "20";
        std::wstring fontFamily = L"Cambria Math";
        std::string fill = "#000000";
        std::string fontStyle;
        std::string fontWeight;
    };
    /* 扩展点 */
    using AttrMap = std::unordered_map<std::string, std::string>;
    using RenderFn = std::function<std::string(const tinyxml2::XMLElement*, const Style&)>;
    using AttrFn = void(*)(const class tinyxml2::XMLAttribute*, class Style&);

    void registerTag(const std::string& tag, RenderFn  fn);
    void registerAttr(const std::string& attr, AttrFn fn);

private:
    MathML2SVG();
    ~MathML2SVG();

    class Impl;
    std::unique_ptr<Impl> pImpl;

};


class GdiTextMeasurer {
public:
    static std::wstring makeKey(const std::wstring& name, float size, int style);
    static GdiTextMeasurer& instance();   // Meyers 单例

    // 返回文本的像素宽高
    struct Size {
        float width = 0.f;
        float height = 0.f;
        float ascent = 0.f;   // 新增
    };
    Size measure(const std::wstring& text,
        const std::wstring& fontName,
        float               fontSizePx,
        Gdiplus::FontStyle  style = Gdiplus::FontStyleRegular);

    std::string outlineToSVG(const std::wstring& text,
        const std::wstring& fontName,
        float               fontSizePx,
        const std::string& fill = "black");
private:
    GdiTextMeasurer();
    ~GdiTextMeasurer();
    GdiTextMeasurer(const GdiTextMeasurer&) = delete;
    GdiTextMeasurer& operator=(const GdiTextMeasurer&) = delete;

    struct CachedFont {
        std::unique_ptr<Gdiplus::FontFamily> family;
        std::unique_ptr<Gdiplus::Font>       font;
    };

    std::unordered_map<std::wstring, CachedFont> cache_;
    std::mutex                                   mtx_;
};

class FreeTypeTextMeasurer {
public:
    static FreeTypeTextMeasurer& instance();   // Meyers 单例
    struct Size {
        float width = 0.f;
        float height = 0.f;
        float ascent = 0.f;   // baseline → top
        float descent = 0.f;  // baseline → bottom
    };
    Size measure(const std::wstring& text,
        const std::wstring& fontName,
        float               fontSizePx,
        int                 style = 0);   // 0=Regular, 1=Bold, 2=Italic

    std::string outlineToSVG(const std::wstring& text,
        const std::wstring& fontName,
        float               fontSizePx,
        const std::string& fill = "black");
private:
    struct CachedFace {
        FT_Face face = nullptr;
        float   emSize = 0.f;      // units_per_EM
    };
    FreeTypeTextMeasurer();
    ~FreeTypeTextMeasurer();
    static FT_Face loadFace(const std::wstring& fontName, int style);
    CachedFace& getFace(const std::wstring& fontName, int style);
    FreeTypeTextMeasurer(const FreeTypeTextMeasurer&) = delete;
    FreeTypeTextMeasurer& operator=(const FreeTypeTextMeasurer&) = delete;


    std::unordered_map<std::wstring, CachedFace> cache_;
    std::mutex                                   mtx_;

    FT_Library ft_ = nullptr;
};

class BusyGuard {
public:
    explicit BusyGuard(std::atomic<bool>& flag) : m_flag(flag) {
        m_flag.store(true, std::memory_order_relaxed);
    }
    ~BusyGuard() {
        OutputDebugStringA("BusyGuard::~BusyGuard() - workerBusy = false\n");
        m_flag.store(false, std::memory_order_relaxed);
    }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;
private:
    std::atomic<bool>& m_flag;
};

struct FontItem
{
    std::wstring familyName;   // 字体原名（en-us）
    std::wstring displayName;  // 中文名（zh-cn），没有就用 familyName
};