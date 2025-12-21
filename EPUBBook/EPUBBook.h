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
    std::vector<OCFItem>   manifest;
    std::vector<OCFRef>    spine;
    std::vector<OCFNavPoint> toc;
    std::map<std::string, std::string> meta;
    std::string toc_path;
};


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

struct MemFile {
    std::vector<uint8_t> data;
    const char* begin() const { return reinterpret_cast<const char*>(data.data()); }
    size_t      size()  const { return data.size(); }
};
// ---------- EPUB 零解压 ----------
class EPUBBook {
public:
    mz_zip_archive zip = {};
    std::map<std::string, MemFile> m_cache;
    OCFPackage ocf_pkg_;                     // 解析结果

    // -------------- EPUBBook 内部新增成员 --------------

    void parse_ocf_(void);                       // 主解析入口
    void parse_opf_(void);   // 解析 OPF
    void parse_toc_(void);                        // 解析 TOC



    std::string get_chapter_name_by_id(int spine_id);
    //void OnTreeSelChanged(const wchar_t* href);
    bool load(const std::string& epub_path);
    std::string get_current_dir();
    MemFile read_zip(std::string file_name);
    std::string load_html(const std::string& path);

    bool is_xhtml(const std::string& file_path);

    void load_all_fonts(void);

    static std::string blake3_hex(const std::vector<uint8_t>& data);



    static std::string extract_text(const tinyxml2::XMLElement* a);

    // 递归解析 EPUB3-Nav <ol>
    void parse_nav_list(tinyxml2::XMLElement* ol, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);


    // 递归解析 NCX <navPoint>
    void parse_ncx_points(tinyxml2::XMLElement* navPoint, int level,
        const std::string& opf_dir,
        std::vector<OCFNavPoint>& out);



    std::string get_title();
    std::string get_author();


    MemFile get_binary(std::string base_url, std::string url);
    bool is_toc_item(int spine_id);


    void clear();



    void build_epub_font_index(std::string tempDir);
    std::unordered_map<FontKey, std::vector<std::string>> m_fontBin;
    std::string resolve_path(std::string base_url, std::string href);
    std::string get_book_path();
    EPUBBook() noexcept {}
    ~EPUBBook();
private:

    static std::string url_decode(const std::string& in);


    std::string m_current_book_path = "";
    std::string m_current_html_path = "";

};
