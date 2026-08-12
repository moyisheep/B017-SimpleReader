#pragma once

#include <string>
#define NOMINMAX
#include <Windows.h>
#include <Shlwapi.h>

#include <boost/algorithm/string.hpp>
#include <lunasvg\lunasvg.h>

#include "litehtml.h"
#include "EPUBBook.h"
#include "ReadingRecorder.h"
#include "Tools.h"


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




