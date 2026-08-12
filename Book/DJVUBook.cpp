#include "DJVUBook.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <zlib.h>

// DjVu内部数据结构
struct DjVuPage {
    std::string id;
    uint32_t width;
    uint32_t height;
    uint32_t dpi;
    std::string compression;
    std::vector<uint8_t> data;
    std::vector<uint8_t> thumb_data;
    std::string text;
};

struct DjVuSharedDict {
    std::string id;
    std::vector<uint8_t> data;
};

struct DjVuAnnotation {
    std::string type;
    std::string value;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct DjVuBook::Internal {
    std::string book_path;
    std::string current_dir;
    OCFPackage package;
    std::vector<OCFRef> spine;
    std::map<std::string, std::vector<uint8_t>> binary_cache;
    std::string title;
    std::string author;
    std::string publisher;
    std::string date;
    std::string language;
    std::string version;

    // DjVu特定数据
    std::vector<DjVuPage> pages;
    std::vector<DjVuSharedDict> shared_dicts;
    std::map<int, std::vector<DjVuAnnotation>> annotations;
    std::vector<std::string> chunk_order;
    std::map<std::string, std::vector<uint8_t>> chunk_data;

    // 文件偏移信息
    std::map<std::string, uint64_t> chunk_offsets;
    std::map<std::string, uint32_t> chunk_sizes;

    Internal() : version("DJVU/1.0") {}

    void clear() {
        book_path.clear();
        current_dir.clear();
        package = OCFPackage();
        spine.clear();
        binary_cache.clear();
        title.clear();
        author.clear();
        publisher.clear();
        date.clear();
        language.clear();
        pages.clear();
        shared_dicts.clear();
        annotations.clear();
        chunk_order.clear();
        chunk_data.clear();
        chunk_offsets.clear();
        chunk_sizes.clear();
    }
};

DjVuBook::DjVuBook() : impl(std::make_unique<Internal>()) {}

DjVuBook::~DjVuBook() = default;

bool DjVuBook::load(const std::string & book_path) {
    clear();

    if (book_path.empty()) {
        return false;
    }

    impl->book_path = book_path;

    // 设置当前目录
    size_t last_slash = book_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        impl->current_dir = book_path.substr(0, last_slash + 1);
    }
    else {
        impl->current_dir = "./";
    }

    // 解析DjVu文件
    if (!parse_djvu_file(book_path)) {
        return false;
    }

    // 构建OCF包结构
    impl->package.version = impl->version;
    impl->package.rootfile = book_path;
    impl->package.opf_dir = impl->current_dir;

    // 添加主文档到manifest
    OCFItem main_item;
    main_item.id = "djvu_main";
    main_item.href = book_path.substr(last_slash + 1);
    main_item.media_type = "image/vnd.djvu";
    impl->package.manifest.push_back(main_item);

    // 添加元数据
    impl->package.meta["dc:title"] = impl->title;
    impl->package.meta["dc:creator"] = impl->author;
    impl->package.meta["dc:publisher"] = impl->publisher;
    impl->package.meta["dc:date"] = impl->date;
    impl->package.meta["dc:language"] = impl->language;

    // 构建spine和页面manifest
    for (size_t i = 0; i < impl->pages.size(); i++) {
        // 添加到manifest
        OCFItem page_item;
        page_item.id = "page_" + std::to_string(i + 1);
        page_item.href = "#page=" + std::to_string(i + 1);
        page_item.media_type = "image/vnd.djvu";
        impl->package.manifest.push_back(page_item);

        // 添加到spine
        OCFRef ref;
        ref.idref = page_item.id;
        ref.href = page_item.href;
        ref.linear = "yes";
        impl->spine.push_back(ref);
        impl->package.spine.push_back(ref);

        // 添加到目录
        OCFNavPoint nav_point;
        nav_point.label = "Page " + std::to_string(i + 1);
        if (!impl->pages[i].text.empty()) {
            // 使用文本前50个字符作为标签
            std::string preview = impl->pages[i].text.substr(0, 50);
            if (preview.length() == 50) preview += "...";
            nav_point.label = preview;
        }
        nav_point.href = "#page=" + std::to_string(i + 1);
        nav_point.order = static_cast<int>(i + 1);
        impl->package.toc.push_back(nav_point);
    }

    impl->package.toc_path = "#toc";

    return true;
}

bool DjVuBook::parse_djvu_file(const std::string & path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // 检查文件大小
    file.seekg(0, std::ios::end);
    uint64_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size < 12) { // 最小IFF文件头大小
        return false;
    }

    // 读取IFF文件标识
    char header[4];
    file.read(header, 4);
    std::string file_type(header, 4);
    file.read(header, 4);
    if (file_type != "AT&T" && file_type != "FORM" && file_type != "DJVM" && file_type != "DJVU") {
        // 可能没有IFF头，直接按单页DjVu处理
        file.seekg(0, std::ios::beg);
        return parse_djvu_chunk(file, static_cast<uint32_t>(file_size));
    }

    // 读取文件大小
    uint32_t total_size = read_uint32(file);

    // 读取格式类型
    file.read(header, 4);
    std::string format(header, 4);

    if (format == "DJVM") {
        // 多页DjVu文档
        impl->version = "DJVM/1.0";
        return parse_form_chunk(file, total_size - 4);
    }
    else if (format == "DJVU") {
        // 单页DjVu文档
        impl->version = "DJVU/1.0";
        return parse_djvu_chunk(file, total_size - 4);
    }

    return false;
}

bool DjVuBook::parse_form_chunk(std::ifstream & file, uint32_t size) {
    uint64_t start_pos = file.tellg();
    uint64_t end_pos = start_pos + size;

    while (file.tellg() < static_cast<int64_t>(end_pos)) {
        char chunk_id[4];
        file.read(chunk_id, 4);
        std::string id(chunk_id, 4);

        uint32_t chunk_size = read_uint32(file);

        impl->chunk_order.push_back(id);
        impl->chunk_offsets[id] = file.tellg();
        impl->chunk_sizes[id] = chunk_size;

        if (id == "INCL") {
            // 包含的子文档
            std::string doc_id = read_string(file);
            uint32_t offset = read_uint32(file);
            uint32_t length = read_uint32(file);

            // 记录子文档信息
            uint64_t save_pos = file.tellg();
            file.seekg(offset, std::ios::beg);

            char sub_header[4];
            file.read(sub_header, 4);
            std::string sub_type(sub_header, 4);

            uint32_t sub_size = read_uint32(file);

            if (sub_type == "DJVU") {
                parse_djvu_chunk(file, sub_size);
            }

            file.seekg(save_pos, std::ios::beg);
        }
        else if (id == "DJVI") {
            // 共享字典
            parse_form_chunk(file, chunk_size);
        }
        else if (id == "INFO") {
            parse_info_chunk(file, chunk_size);
        }
        else if (id == "BG44" || id == "BG2K" || id == "BGjp") {
            parse_bg44_chunk(file, chunk_size);
        }
        else if (id == "Sjbz") {
            parse_sjbz_chunk(file, chunk_size);
        }
        else if (id == "FG44" || id == "FG2K") {
            parse_fg44_chunk(file, chunk_size);
        }
        else if (id == "Djbz") {
            parse_djbz_chunk(file, chunk_size);
        }
        else if (id == "ANTa" || id == "ANTz") {
            parse_ant_chunk(file, chunk_size);
        }
        else if (id == "TXTa" || id == "TXTz") {
            parse_txta_chunk(file, chunk_size);
        }
        else if (id == "NAVM") {
            parse_metadata(file);
        }
        else {
            // 跳过未知块
            file.seekg(chunk_size, std::ios::cur);
        }

        // 对齐到偶数边界
        if (chunk_size % 2 != 0) {
            file.seekg(1, std::ios::cur);
        }
    }

    return true;
}

bool DjVuBook::parse_info_chunk(std::ifstream & file, uint32_t size) {
    DjVuPage page;

    page.width = read_uint16(file);
    page.height = read_uint16(file);
    page.dpi = read_uint16(file);

    uint8_t version = read_uint8(file);
    uint8_t flags = read_uint8(file);

    if (flags & 0x80) {
        page.compression = "JB2";
    }
    else {
        page.compression = "BITONAL";
    }

    // 记录页面信息
    page.id = "page_" + std::to_string(impl->pages.size() + 1);
    impl->pages.push_back(page);

    return true;
}

bool DjVuBook::parse_djvu_chunk(std::ifstream & file, uint32_t size) {
    uint64_t start_pos = file.tellg();
    uint64_t end_pos = start_pos + size;

    DjVuPage page;
    page.id = "page_" + std::to_string(impl->pages.size() + 1);

    while (file.tellg() < static_cast<int64_t>(end_pos)) {
        char chunk_id[4];
        file.read(chunk_id, 4);
        std::string id(chunk_id, 4);

        uint32_t chunk_size = read_uint32(file);

        if (id == "INFO") {
            page.width = read_uint16(file);
            page.height = read_uint16(file);
            page.dpi = read_uint16(file);
            uint8_t version = read_uint8(file);
            uint8_t flags = read_uint8(file);

            if (flags & 0x80) {
                page.compression = "JB2";
            }
            else {
                page.compression = "BITONAL";
            }
        }
        else if (id == "BG44" || id == "BG2K") {
            // 背景层
            std::vector<uint8_t> data = read_bytes(file, chunk_size);
            page.data.insert(page.data.end(), data.begin(), data.end());
        }
        else if (id == "FG44" || id == "FG2K") {
            // 前景层
            std::vector<uint8_t> data = read_bytes(file, chunk_size);
            page.data.insert(page.data.end(), data.begin(), data.end());
        }
        else if (id == "Sjbz" || id == "JB2") {
            // JB2编码数据
            std::vector<uint8_t> data = read_bytes(file, chunk_size);
            page.data.insert(page.data.end(), data.begin(), data.end());
        }
        else if (id == "TXTa" || id == "TXTz") {
            // 文本层
            std::vector<uint8_t> data = read_bytes(file, chunk_size);
            page.text = extract_text_from_chunk(data);
        }
        else if (id == "ANTa" || id == "ANTz") {
            // 注释层
            std::vector<uint8_t> data = read_bytes(file, chunk_size);
            // 简单提取文本注释
            std::string text(reinterpret_cast<char*>(data.data()),
                std::min<size_t>(data.size(), 1000));
            if (!page.text.empty()) page.text += "\n";
            page.text += text;
        }
        else {
            // 跳过其他块
            file.seekg(chunk_size, std::ios::cur);
        }

        // 对齐到偶数边界
        if (chunk_size % 2 != 0) {
            file.seekg(1, std::ios::cur);
        }
    }

    impl->pages.push_back(page);
    return true;
}

bool DjVuBook::parse_bg44_chunk(std::ifstream & file, uint32_t size) {
    if (impl->pages.empty()) return false;

    std::vector<uint8_t> data = read_bytes(file, size);
    impl->pages.back().data.insert(impl->pages.back().data.end(),
        data.begin(), data.end());
    return true;
}

bool DjVuBook::parse_sjbz_chunk(std::ifstream & file, uint32_t size) {
    if (impl->pages.empty()) return false;

    std::vector<uint8_t> data = read_bytes(file, size);
    impl->pages.back().data.insert(impl->pages.back().data.end(),
        data.begin(), data.end());
    return true;
}

bool DjVuBook::parse_fg44_chunk(std::ifstream & file, uint32_t size) {
    if (impl->pages.empty()) return false;

    std::vector<uint8_t> data = read_bytes(file, size);
    impl->pages.back().data.insert(impl->pages.back().data.end(),
        data.begin(), data.end());
    return true;
}

bool DjVuBook::parse_djbz_chunk(std::ifstream & file, uint32_t size) {
    // 共享字典，通常用于JB2压缩
    DjVuSharedDict dict;
    dict.id = "dict_" + std::to_string(impl->shared_dicts.size() + 1);
    dict.data = read_bytes(file, size);
    impl->shared_dicts.push_back(dict);
    return true;
}

bool DjVuBook::parse_ant_chunk(std::ifstream & file, uint32_t size) {
    if (impl->pages.empty()) return false;

    std::vector<uint8_t> data = read_bytes(file, size);

    // 简单解析注释
    std::string annotation(reinterpret_cast<char*>(data.data()),
        std::min<size_t>(data.size(), 1000));

    // 查找文本注释
    size_t text_pos = annotation.find("text");
    if (text_pos != std::string::npos) {
        size_t start = annotation.find('"', text_pos);
        size_t end = annotation.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string text = annotation.substr(start + 1, end - start - 1);
            if (!impl->pages.back().text.empty()) impl->pages.back().text += "\n";
            impl->pages.back().text += text;
        }
    }

    return true;
}

bool DjVuBook::parse_txta_chunk(std::ifstream & file, uint32_t size) {
    if (impl->pages.empty()) return false;

    std::vector<uint8_t> data = read_bytes(file, size);
    impl->pages.back().text = extract_text_from_chunk(data);
    return true;
}

bool DjVuBook::parse_metadata(std::ifstream & file) {
    try {
        // 读取元数据长度
        uint32_t meta_size = read_uint32(file);
        if (meta_size == 0) return false;

        // 读取元数据内容
        std::vector<uint8_t> meta_data = read_bytes(file, meta_size);
        std::string metadata(reinterpret_cast<char*>(meta_data.data()), meta_data.size());

        // 简单解析元数据
        size_t title_pos = metadata.find("<dc:title>");
        if (title_pos != std::string::npos) {
            size_t title_end = metadata.find("</dc:title>", title_pos);
            if (title_end != std::string::npos) {
                impl->title = metadata.substr(title_pos + 10, title_end - title_pos - 10);
            }
        }

        size_t author_pos = metadata.find("<dc:creator>");
        if (author_pos != std::string::npos) {
            size_t author_end = metadata.find("</dc:creator>", author_pos);
            if (author_end != std::string::npos) {
                impl->author = metadata.substr(author_pos + 12, author_end - author_pos - 12);
            }
        }

        size_t publisher_pos = metadata.find("<dc:publisher>");
        if (publisher_pos != std::string::npos) {
            size_t publisher_end = metadata.find("</dc:publisher>", publisher_pos);
            if (publisher_end != std::string::npos) {
                impl->publisher = metadata.substr(publisher_pos + 14, publisher_end - publisher_pos - 14);
            }
        }

        size_t date_pos = metadata.find("<dc:date>");
        if (date_pos != std::string::npos) {
            size_t date_end = metadata.find("</dc:date>", date_pos);
            if (date_end != std::string::npos) {
                impl->date = metadata.substr(date_pos + 9, date_end - date_pos - 9);
            }
        }

        size_t lang_pos = metadata.find("<dc:language>");
        if (lang_pos != std::string::npos) {
            size_t lang_end = metadata.find("</dc:language>", lang_pos);
            if (lang_end != std::string::npos) {
                impl->language = metadata.substr(lang_pos + 13, lang_end - lang_pos - 13);
            }
        }

    }
    catch (...) {
        return false;
    }

    return true;
}

std::string DjVuBook::extract_text_from_chunk(const std::vector<uint8_t>&data) {
    if (data.empty()) return "";

    std::string result;

    // 检查是否是压缩文本(TXTz)
    if (data.size() > 4 && std::string(reinterpret_cast<const char*>(data.data()), 4) == "TXTz") {
        // 简单的文本提取 - 跳过前4个字节的标识
        for (size_t i = 4; i < data.size(); i++) {
            if (data[i] >= 32 && data[i] <= 126) { // 可打印ASCII字符
                result += static_cast<char>(data[i]);
            }
            else if (data[i] == '\n' || data[i] == '\r' || data[i] == '\t') {
                result += ' ';
            }
        }
    }
    else {
        // 直接文本(TXTa)
        for (uint8_t byte : data) {
            if (byte >= 32 && byte <= 126) { // 可打印ASCII字符
                result += static_cast<char>(byte);
            }
            else if (byte == '\n' || byte == '\r' || byte == '\t') {
                result += ' ';
            }
        }
    }

    // 清理多余空格
    std::string cleaned;
    bool last_was_space = false;
    for (char c : result) {
        if (c == ' ') {
            if (!last_was_space) {
                cleaned += c;
                last_was_space = true;
            }
        }
        else {
            cleaned += c;
            last_was_space = false;
        }
    }

    return cleaned;
}

std::vector<uint8_t> DjVuBook::get_binary(std::string base_url, std::string url) {
    std::vector<uint8_t> result;

    // 如果是页面请求
    if (url.find("#page=") == 0) {
        std::string page_num_str = url.substr(6);
        try {
            int page_num = std::stoi(page_num_str) - 1;
            if (page_num >= 0 && page_num < static_cast<int>(impl->pages.size())) {
                // 返回页面数据或占位符
                if (!impl->pages[page_num].data.empty()) {
                    return impl->pages[page_num].data;
                }
                else {
                    // 生成简单占位符图像数据
                    std::string placeholder = "P1\n# DjVu Page " + std::to_string(page_num + 1) + "\n";
                    placeholder += std::to_string(impl->pages[page_num].width) + " " +
                        std::to_string(impl->pages[page_num].height) + "\n";
                    result.assign(placeholder.begin(), placeholder.end());
                }
            }
        }
        catch (...) {
            // 转换失败
        }
        return result;
    }

    // 尝试读取文件
    std::string full_path = resolve_path(base_url, url);

    // 检查是否在缓存中
    auto it = impl->binary_cache.find(full_path);
    if (it != impl->binary_cache.end()) {
        return it->second;
    }

    // 读取文件
    std::ifstream file(full_path, std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        result.resize(size);
        file.read(reinterpret_cast<char*>(result.data()), size);

        // 缓存结果
        impl->binary_cache[full_path] = result;
    }

    return result;
}

std::string DjVuBook::get_string(const std::string & path) {
    std::string result;

    // 如果是页面文本内容请求
    if (path.find("#page=") == 0) {
        std::string page_num_str = path.substr(6);
        try {
            int page_num = std::stoi(page_num_str) - 1;
            if (page_num >= 0 && page_num < static_cast<int>(impl->pages.size())) {
                if (!impl->pages[page_num].text.empty()) {
                    return impl->pages[page_num].text;
                }
                else {
                    return "Page " + std::to_string(page_num + 1) + " (No text available)";
                }
            }
        }
        catch (...) {
            // 转换失败
        }
        return "Page content not available";
    }

    // 尝试读取文本文件
    std::ifstream file(path);
    if (file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        result = buffer.str();
    }

    return result;
}

std::string DjVuBook::get_book_path() {
    return impl->book_path;
}

std::string DjVuBook::get_current_dir() {
    return impl->current_dir;
}

std::string DjVuBook::get_chapter_name_by_id(int spine_id) {
    if (spine_id >= 0 && spine_id < static_cast<int>(impl->spine.size())) {
        std::string label = "Page " + std::to_string(spine_id + 1);
        if (spine_id < static_cast<int>(impl->pages.size())) {
            if (!impl->pages[spine_id].text.empty()) {
                std::string preview = impl->pages[spine_id].text.substr(0, 50);
                if (preview.length() == 50) preview += "...";
                label = preview;
            }
        }
        return label;
    }
    return "";
}

std::string DjVuBook::get_title() {
    return impl->title;
}

std::string DjVuBook::get_author() {
    return impl->author;
}

std::string DjVuBook::get_version() {
    return impl->version;
}

std::vector<OCFRef>& DjVuBook::get_spine() {
    return impl->spine;
}

OCFPackage& DjVuBook::get_ocf_package() {
    return impl->package;
}

bool DjVuBook::has_script() {
    return false;
}

bool DjVuBook::has_font() {
    // 检查是否有共享字典
    return !impl->shared_dicts.empty();
}

bool DjVuBook::has_css() {
    return false;
}

void DjVuBook::clear() {
    impl->clear();
}

std::string DjVuBook::resolve_path(std::string base_url, std::string href) {
    if (href.empty()) return base_url;

    if (href[0] == '#') {
        return base_url + href;
    }

    if (href.find("://") != std::string::npos) {
        return href;
    }

    if (href[0] == '/') {
        std::string base_dir = impl->current_dir;
        if (!base_dir.empty() && base_dir.back() != '/') {
            base_dir += '/';
        }
        return base_dir + href.substr(1);
    }

    std::string base_dir = base_url;
    size_t last_slash = base_dir.find_last_of("/");
    if (last_slash != std::string::npos) {
        base_dir = base_dir.substr(0, last_slash + 1);
    }

    return base_dir + href;
}

// 辅助读取函数
uint32_t DjVuBook::read_uint32(std::ifstream & file) {
    uint32_t value = 0;
    file.read(reinterpret_cast<char*>(&value), 4);
    // 转换为大端序
    return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
        ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

uint16_t DjVuBook::read_uint16(std::ifstream & file) {
    uint16_t value = 0;
    file.read(reinterpret_cast<char*>(&value), 2);
    // 转换为大端序
    return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

uint8_t DjVuBook::read_uint8(std::ifstream & file) {
    uint8_t value = 0;
    file.read(reinterpret_cast<char*>(&value), 1);
    return value;
}

std::string DjVuBook::read_string(std::ifstream & file) {
    std::string result;
    char ch;
    while (file.get(ch) && ch != '\0') {
        result += ch;
    }
    return result;
}

std::vector<uint8_t> DjVuBook::read_bytes(std::ifstream & file, uint32_t count) {
    std::vector<uint8_t> result(count);
    if (count > 0) {
        file.read(reinterpret_cast<char*>(result.data()), count);
    }
    return result;
}

std::string DjVuBook::bytes_to_hex(const std::vector<uint8_t>&bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}