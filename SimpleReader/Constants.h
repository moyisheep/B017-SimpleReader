#pragma once


#include <string>
#include <atomic>
#include <vector>
#include <memory>
#include <future>
#include <unordered_map>

#define NOMINMAX
#include <Windows.h>
#include <CommCtrl.h>
#include <mmsystem.h>
#include <d2d1.h>

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


const wchar_t MAIN_CLASS[] = L"SimpleEPUBReader";
const wchar_t IMAGEVIEW_CLASS[] = L"Imageview";
const wchar_t HOMEPAGE_CLASS[] = L"HomepageClass";
const wchar_t VIEW_CLASS[] = L"ViewClass";
const wchar_t SCROLLBAR_CLASS[] = L"ScrollBarEx";
const wchar_t TOC_CLASS[] = L"TocPanelClass";
const wchar_t TOOLTIP_CLASS[] = L"TooltipClass";

// 可随时改
static UINT g_frame_count = 0;

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




enum class ImgFmt { PNG, JPEG, BMP, GIF, TIFF, SVG, UNKNOWN };


struct ImageFrame
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;          // 每行字节数
    std::vector<uint8_t> rgba;     // 连续像素，8-bit * 4
    std::vector<uint8_t> raw_data;     // 连续像素，8-bit * 4
};



struct FontItem
{
    std::wstring familyName;   // 字体原名（en-us）
    std::wstring displayName;  // 中文名（zh-cn），没有就用 familyName
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


class SimpleContainer;
class VirtualDoc;
class Book;
struct AppStates;
struct AppSettings;
class TocPanel;
class ScrollBarEx;
class AppBootstrap;
class ReadingRecorder;

extern HWND  g_hToc;    // 侧边栏 TreeView
extern HIMAGELIST g_hImg ;   // 图标(可选)
extern HWND      g_hWnd ;
extern HWND g_hStatus ;   // 状态栏句柄
extern HWND g_hView ;
extern HWND g_hTooltip ;
extern HWND g_hImageview ;
extern HWND g_hViewScroll ;
extern HWND g_hHomepage ;

extern HINSTANCE g_hInst;
extern std::shared_ptr<SimpleContainer> g_cMain;
extern std::shared_ptr<SimpleContainer> g_cTooltip;
extern std::shared_ptr<SimpleContainer> g_cImage;
extern std::shared_ptr<SimpleContainer> g_cHome;

extern std::shared_ptr<Book>  g_book;

extern std::future<void> g_parse_task;

extern std::unique_ptr<VirtualDoc> g_vd;

extern  std::atomic<float> g_offsetY;

extern AppStates g_states;
extern AppSettings g_cfg;

extern std::unique_ptr<AppBootstrap> g_bootstrap;
extern std::unique_ptr<ReadingRecorder> g_recorder;

extern  MMRESULT g_tickTimer;   // 0 表示当前没有定时器
extern  MMRESULT g_flushTimer;

extern  MMRESULT g_updateTimer;

extern  MMRESULT g_scrollTimer;
extern  MMRESULT g_framerateTimer ;
extern std::atomic<float> g_velocity;     // 像素/秒

extern int g_center_offset;

extern std::string g_globalCSS ;

extern  int   g_splitX ;       // 当前 TOC 宽度（初始值）
extern  bool  g_dragging ;     // 是否正在拖动
extern  bool  g_imageview_dragging;
extern  POINT g_imageview_drag_pos;
extern  bool g_mouse_tracked ;

extern  int g_imageviewRenderW ;

extern std::unique_ptr<TocPanel> g_toc;
extern std::unique_ptr<ScrollBarEx> g_scrollbar;
extern std::unordered_map<int, std::wstring> g_statusBuf;