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
    std::string version;
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

    OCFPackage m_ocf_pkg;                     // 解析结果

    // -------------- EPUBBook 内部新增成员 --------------

    bool load(const std::string& epub_path);
    MemFile get_binary(std::string base_url, std::string url);
   

    MemFile read_zip(std::string file_name);

    std::string load_html(const std::string& path);



    void load_all_fonts(void);


    std::string get_book_path();
    std::string get_current_dir();
    std::string get_chapter_name_by_id(int spine_id);

    std::string get_title();
    std::string get_author();
    std::string get_version();
 
    bool has_script();
    bool has_font();
    bool has_css();


    bool is_toc_item(int spine_id);


    void clear();



    void build_epub_font_index(std::string tempDir);
    std::unordered_map<FontKey, std::vector<std::string>> m_fontBin;
    std::string resolve_path(std::string base_url, std::string href);

    EPUBBook() noexcept {}
    ~EPUBBook();
private:
    std::string m_current_book_path = "";
    std::string m_current_html_path = "";
    mz_zip_archive zip = {};
    std::map<std::string, MemFile> m_cache;


    static std::string url_decode(const std::string& in);
    static std::string blake3_hex(const std::vector<uint8_t>& data);
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
