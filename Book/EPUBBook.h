#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <filesystem>
#include <fstream>
#include <array>




#include <miniz/miniz.h>
#include <tinyxml2.h>
#include <blake3.h>

#include "Book.h"

struct FontKey {
    std::string family;
    int          weight;
    bool         italic;
    int          size;          // px
    bool operator==(const FontKey& o) const noexcept = default;
};
namespace std {
    template<>
    struct hash<FontKey> {
        size_t operator()(const FontKey& k) const noexcept {
            std::string txt;
            txt += k.family;
            txt += std::to_string(k.weight);
            txt += std::to_string(k.italic);
            txt += std::to_string(k.size);
            return std::hash<std::string>{}(txt);
        }
    };
}


// ---------- EPUB 零解压 ----------
class EPUBBook: public Book 
{
public:



    // -------------- EPUBBook 内部新增成员 --------------

    bool load(const std::string& epub_path) override;
    std::vector<uint8_t> get_binary(std::string base_url, std::string url) override;



    std::string get_string(const std::string& path) override;


    std::string get_book_path() override;
    std::string get_current_dir() override;
    std::string get_chapter_name_by_id(int spine_id) override;

    std::string get_title() override;
    std::string get_author() override;
    std::string get_version() override;

    std::vector<OCFRef>& get_spine() override;
    OCFPackage& get_ocf_package() override;


 
    bool has_script() override;
    bool has_font() override;
    bool has_css() override;


   // bool is_toc_item(int spine_id);


    void clear() override;



    std::string resolve_path(std::string base_url, std::string href) override;

    EPUBBook() noexcept {}
    ~EPUBBook();
private:
    OCFPackage m_ocf_pkg;                     // 解析结果
    std::string m_current_book_path = "";
    std::string m_current_html_path = "";
    mz_zip_archive zip = {};
    std::map<std::string, std::vector<uint8_t>> m_cache;


private:
    std::vector<uint8_t> read_zip(std::string file_name);

    static std::string url_decode(const std::string& in);

    bool is_xhtml(const std::string& file_path);


    void parse_ocf(void);                       // 主解析入口
    void parse_opf(void);   // 解析 OPF
    void parse_toc(void);                        // 解析 TOC

    static std::string extract_text(const tinyxml2::XMLElement* a);
    // 递归解析 EPUB3-Nav <ol>
    void parse_nav_list(tinyxml2::XMLElement* ol, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);


    // 递归解析 NCX <navPoint>
    void parse_ncx_points(tinyxml2::XMLElement* navPoint, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);


};
