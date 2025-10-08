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




// -------------- 新增数据结构 --------------
struct OCFItem {
    std::wstring id, href, media_type, properties;
};
struct OCFRef {
    std::wstring idref, href, linear = L"yes";
};
struct OCFNavPoint {
    std::wstring label, href;
    int order = 0;
};
struct OCFPackage {
    std::wstring rootfile;                // OPF 绝对路径
    std::wstring opf_dir;                 // 目录，带 '/'
    std::vector<OCFItem>   manifest;
    std::vector<OCFRef>    spine;
    std::vector<OCFNavPoint> toc;
    std::map<std::wstring, std::wstring> meta;
    std::wstring toc_path;
};


//  file system
class IFileProvider {
public:
    virtual ~IFileProvider() = default;
    virtual bool load(const std::wstring& file_path) = 0;
    // 按路径返回原始二进制
    virtual MemFile get(std::wstring path) = 0;
    virtual std::wstring find(const std::wstring& path) = 0;
};

class ZipFileProvider : public IFileProvider
{
public:
    ZipFileProvider();
    ~ZipFileProvider();
    bool load(const std::wstring& file_path) override;
    MemFile get(std::wstring path)  override;
private:
    mz_zip_archive m_zip = {};
};

class LocalFileProvider : public IFileProvider
{
public:
    bool load(const std::wstring& file_path) override { return true; };
    // 按路径返回原始二进制
    MemFile get(std::wstring path)  override;
};

class EPUBParser
{
public:
    EPUBParser();
    ~EPUBParser();
    bool load(std::shared_ptr<IFileProvider> fp);
private:
    bool parse_ocf();
    bool parse_opf();
    bool parse_toc();
    static std::wstring extract_text(const tinyxml2::XMLElement* a);

    // 递归解析 EPUB3-Nav <ol>
    void parse_nav_list(tinyxml2::XMLElement* ol, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);


    // 递归解析 NCX <navPoint>
    void parse_ncx_points(tinyxml2::XMLElement* navPoint, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);
    std::shared_ptr<IFileProvider> m_fp;
    OCFPackage m_ocf_pkg;
};
// ---------- EPUB 零解压 ----------
class EPUBBook {
public:
    mz_zip_archive zip = {};
    std::map<std::wstring, MemFile> m_cache;
    OCFPackage ocf_pkg_;                     // 解析结果

    // -------------- EPUBBook 内部新增成员 --------------

    void parse_ocf_(void);                       // 主解析入口
    void parse_opf_(void);   // 解析 OPF
    void parse_toc_(void);                        // 解析 TOC



    std::wstring get_chapter_name_by_id(int spine_id);
    //void OnTreeSelChanged(const wchar_t* href);
    bool load(const std::wstring& epub_path);
    std::wstring get_current_dir();
    MemFile read_zip(std::wstring file_name);
    std::string load_html(const std::wstring& path);

    //void load_all_fonts(void);



    static std::wstring extract_text(const tinyxml2::XMLElement* a);

    // 递归解析 EPUB3-Nav <ol>
    void parse_nav_list(tinyxml2::XMLElement* ol, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);


    // 递归解析 NCX <navPoint>
    void parse_ncx_points(tinyxml2::XMLElement* navPoint, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);

    std::string extract_anchor(const char* href);

    litehtml::element::ptr find_link_in_chain(litehtml::element::ptr start);

    static bool skip_attr(const std::string& val);

    static std::string get_html(litehtml::element::ptr el);

    std::string html_of_anchor_paragraph(litehtml::document* doc, const std::string& anchorId);

    std::string get_html_of_image(litehtml::element::ptr start);

    void show_imageview(const litehtml::element::ptr& el);

    std::string get_title();
    std::string get_author();

    void delayed_show_tooltip(std::string txt, unsigned width = 300, unsigned delayMs = 300);
    void cancel_delayed_tooltip();
    MemFile get_binary(std::wstring base_url, std::wstring url);
    bool is_toc_item(int spine_id);
    void show_tooltip(const std::string html, int width);
    void hide_imageview();
    void hide_tooltip();
    static std::string get_anchor_html(litehtml::document* doc, const std::string& anchor);
    void clear();
    void LoadToc();


    //void build_epub_font_index();
    std::unordered_map<FontKey, std::vector<std::wstring>> m_fontBin;
    std::wstring resolve_path(std::wstring base_url, std::wstring href);
    std::wstring get_book_path();
    EPUBBook() noexcept {}
    ~EPUBBook();
private:
    struct TooltipPayload { std::string html; unsigned width; };
    //MMRESULT m_tooltipTimer = 0;
    static std::wstring url_decode(const std::wstring& in);


    std::wstring m_current_book_path = L"";
    std::wstring m_current_html_path = L"";

};
