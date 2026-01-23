#ifndef DJVU_BOOK_H
#define DJVU_BOOK_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "Book.h"

class DjVuBook : public Book {
public:
    DjVuBook();
    virtual ~DjVuBook();

    virtual bool load(const std::string& epub_path) override;
    virtual std::vector<uint8_t> get_binary(std::string base_url, std::string url) override;
    virtual std::string get_string(const std::string& path) override;
    virtual std::string get_book_path() override;
    virtual std::string get_current_dir() override;
    virtual std::string get_chapter_name_by_id(int spine_id) override;
    virtual std::string get_title() override;
    virtual std::string get_author() override;
    virtual std::string get_version() override;
    virtual std::vector<OCFRef>& get_spine() override;
    virtual OCFPackage& get_ocf_package() override;
    virtual bool has_script() override;
    virtual bool has_font() override;
    virtual bool has_css() override;
    virtual void clear() override;
    virtual std::string resolve_path(std::string base_url, std::string href) override;

private:
    struct Internal;
    std::unique_ptr<Internal> impl;

    bool parse_djvu_file(const std::string& path);
    bool parse_iff_chunk(std::ifstream& file, uint32_t size);
    bool parse_form_chunk(std::ifstream& file, uint32_t size);
    bool parse_info_chunk(std::ifstream& file, uint32_t size);
    bool parse_djvu_chunk(std::ifstream& file, uint32_t size);
    bool parse_bg44_chunk(std::ifstream& file, uint32_t size);
    bool parse_sjbz_chunk(std::ifstream& file, uint32_t size);
    bool parse_fg44_chunk(std::ifstream& file, uint32_t size);
    bool parse_fg4x_chunk(std::ifstream& file, uint32_t size, const std::string& id);
    bool parse_djbz_chunk(std::ifstream& file, uint32_t size);
    bool parse_ant_chunk(std::ifstream& file, uint32_t size);
    bool parse_txta_chunk(std::ifstream& file, uint32_t size);
    bool parse_metadata(std::ifstream& file);
    std::string read_string(std::ifstream& file);
    std::vector<uint8_t> read_bytes(std::ifstream& file, uint32_t count);
    uint32_t read_uint32(std::ifstream& file);
    uint16_t read_uint16(std::ifstream& file);
    uint8_t read_uint8(std::ifstream& file);
    std::string bytes_to_hex(const std::vector<uint8_t>& bytes);
    std::string extract_text_from_chunk(const std::vector<uint8_t>& data);
};

#endif // DJVU_BOOK_H