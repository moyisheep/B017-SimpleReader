#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <regex>



#include <gumbo.h>
#include <miniz/miniz.h>
#include <tinyxml2.h>
#include <litehtml.h>



#include "MemFile.h"
#include "FontKey.h"
#include "a2w_w2a.h"
#include "IFileProvider.h"
#include "EPUBParser.h"






//  file system




// ---------- EPUB 零解压 ----------
class EPUBBook {
public:
    EPUBBook() noexcept {}
    ~EPUBBook();
    mz_zip_archive zip = {};
    std::map<std::wstring, MemFile> m_cache;


    // -------------- EPUBBook 内部新增成员 --------------






    //void OnTreeSelChanged(const wchar_t* href);
    bool load(const std::wstring& epub_path);
    std::wstring get_current_dir();
    MemFile read_zip(std::wstring file_name);
    std::string load_html(const std::wstring& path);

    //void load_all_fonts(void);





    std::string extract_anchor(const char* href);

    litehtml::element::ptr find_link_in_chain(litehtml::element::ptr start);

    static bool skip_attr(const std::string& val);

    static std::string get_html(litehtml::element::ptr el);

    std::string html_of_anchor_paragraph(litehtml::document* doc, const std::string& anchorId);

    std::string get_html_of_image(litehtml::element::ptr start);

    void show_imageview(const litehtml::element::ptr& el);



    void delayed_show_tooltip(std::string txt, unsigned width = 300, unsigned delayMs = 300);
    void cancel_delayed_tooltip();
   

    void show_tooltip(const std::string html, int width);
    void hide_imageview();
    void hide_tooltip();
    static std::string get_anchor_html(litehtml::document* doc, const std::string& anchor);
    void clear();
    void LoadToc();


    //void build_epub_font_index();
    std::unordered_map<FontKey, std::vector<std::wstring>> m_fontBin;

    std::wstring get_book_path();

private:
    struct TooltipPayload { std::string html; unsigned width; };
    //MMRESULT m_tooltipTimer = 0;



    std::wstring m_current_book_path = L"";
    std::wstring m_current_html_path = L"";

};
