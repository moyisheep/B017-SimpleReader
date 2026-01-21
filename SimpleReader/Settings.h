#pragma once
#include <string>

struct RGBA
{
    int red;
    int green;
    int blue;
    int alpha;
};
class Settings {
public:
    Settings() = default;
public:
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

    std::string default_font_name = "Microsoft YaHei";

    std::wstring temp_dir = L"SimpleReaderTemp";

    int tooltip_width = 500;
    std::string appName = "Simple Reader";
    int split_space_height = 300; // 单位:px
    std::string default_serif = "Georgia";
    std::string default_sans_serif = "Verdana";
    std::string default_monospace = "Consolas";



    // 1) GDI+ 颜色（A=255 不透明）
    RGBA scrollbar_slider_color{ 255, 238, 165, 102 };
    RGBA scrollbar_dot_color_highlight{ 255, 238, 165, 102 };
    RGBA scrollbar_dot_color{ 209, 202, 197, 80 };
    RGBA highlight_color_cr{ 238, 165, 102, 255 };  // #eea566
    RGBA hover_color_cr{ 240, 240, 240, 255 };
    // 2) D2D1 颜色（保持原透明度 0.4，可按需改）
    RGBA highlight_color_d2d{238, 165, 102,  102};

    RGBA default_background_color{ 255, 255, 255, 255 };
    RGBA background_color{ 255, 255, 255, 255 };


};