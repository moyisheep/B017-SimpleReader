#pragma once
#include <unordered_map>
#include <string>
#include <vector>

// -------------- 新增数据结构 --------------
struct OCFItem {
    std::string id, href, media_type, properties;
};
struct OCFRef {
    std::string idref, href, linear = "yes";
};
struct OCFNavPoint {
    std::string label, href;
    int order = 0;
};
struct OCFPackage {
    std::string rootfile;                // OPF 绝对路径
    std::string opf_dir;                 // 目录，带 '/'
    std::string version;
    std::vector<OCFItem>   manifest;
    std::vector<OCFRef>    spine;
    std::vector<OCFNavPoint> toc;
    std::unordered_map<std::string, std::string> meta;
    std::string toc_path;
};


class Book
{
public:
    virtual bool load(const std::string& epub_path) = 0;
    virtual std::vector<uint8_t> get_binary(std::string base_url, std::string url) = 0;



    virtual std::string get_string(const std::string& path) = 0;


    virtual std::string get_book_path() = 0;
    virtual std::string get_current_dir() = 0;
    virtual std::string get_chapter_name_by_id(int spine_id) = 0;

    virtual std::string get_title() = 0;
    virtual std::string get_author() = 0;
    virtual std::string get_version() = 0;

    virtual std::vector<OCFRef>& get_spine() = 0;
    virtual OCFPackage& get_ocf_package() = 0;


    virtual bool has_script() = 0;
    virtual bool has_font() = 0;
    virtual bool has_css() = 0;
    virtual void clear() = 0;
    virtual std::string resolve_path(std::string base_url, std::string href) = 0;
};