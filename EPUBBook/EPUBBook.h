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


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Shlwapi.h>

#include <miniz/miniz.h>
#include <tinyxml2.h>
#include <blake3.h>

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


struct FontKey {
    std::wstring family;
    int          weight;
    bool         italic;
    int          size;          // px
    bool operator==(const FontKey& o) const noexcept = default;
};
namespace std {
    template<>
    struct hash<FontKey> {
        size_t operator()(const FontKey& k) const noexcept {
            std::wstring txt;
            txt += k.family;
            txt += std::to_wstring(k.weight);
            txt += std::to_wstring(k.italic);
            txt += std::to_wstring(k.size);
            return std::hash<std::wstring>{}(txt);
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

    bool is_xhtml(const std::wstring& file_path);

    void load_all_fonts(void);

    static std::wstring blake3_hex(const std::vector<uint8_t>& data);



    static std::wstring extract_text(const tinyxml2::XMLElement* a);

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


    MemFile get_binary(std::wstring base_url, std::wstring url);
    bool is_toc_item(int spine_id);


    void clear();



    void build_epub_font_index(std::wstring tempDir);
    std::unordered_map<FontKey, std::vector<std::wstring>> m_fontBin;
    std::wstring resolve_path(std::wstring base_url, std::wstring href);
    std::wstring get_book_path();
    EPUBBook() noexcept {}
    ~EPUBBook();
private:

    static std::wstring url_decode(const std::wstring& in);


    std::wstring m_current_book_path = L"";
    std::wstring m_current_html_path = L"";

};
