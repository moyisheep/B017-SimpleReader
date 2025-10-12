#pragma once
#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <regex>

#include <tinyxml2.h>

#include "IFileProvider.h"
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

class EPUBParser 
{
public:
    EPUBParser();
    ~EPUBParser();
 
    bool load(std::shared_ptr<IFileProvider> fp);
    std::wstring get_title();
    std::wstring get_href(int spine_id);
    std::wstring get_author();
    std::wstring get_chapter_name_by_id(int spine_id);
    bool is_toc_item(int spine_id);
    void clear();
private:
    void parse_ocf(void);                       // 主解析入口
    void parse_opf(void);   // 解析 OPF
    void parse_toc(void);                        // 解析 TOC

    std::wstring resolve_path(std::wstring base_url, std::wstring href);
    bool is_xhtml(const std::wstring& file_path);
    static std::wstring extract_text(const tinyxml2::XMLElement* a);
    static std::wstring url_decode(const std::wstring& in);

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

