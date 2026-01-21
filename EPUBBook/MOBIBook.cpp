#include "MOBIBook.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace mobi {

    // ==================== 辅助函数 ====================

    uint32_t MobiBook::readUInt32(const uint8_t* data) {
        return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    }

    uint16_t MobiBook::readUInt16(const uint8_t* data) {
        return (data[0] << 8) | data[1];
    }

    bool MobiBook::readFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        file_data_.resize(size);
        if (!file.read(reinterpret_cast<char*>(file_data_.data()), size)) {
            file_data_.clear();
            return false;
        }

        return true;
    }

    std::string MobiBook::normalizePath(const std::string& path) {
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');

        // 移除多余的斜杠
        size_t pos;
        while ((pos = result.find("//")) != std::string::npos) {
            result.erase(pos, 1);
        }

        return result;
    }

    // ==================== 主要解析方法 ====================

    bool MobiBook::load(const std::string& mobi_path) {
        clear();

        if (!readFile(mobi_path)) {
            return false;
        }

        // 添加：基本文件大小检查
        if (file_data_.size() < 100) { // 或更具体的阈值
            return false;
        }

        book_path_ = normalizePath(mobi_path);

        // 获取当前目录
        size_t pos = book_path_.find_last_of('/');
        if (pos != std::string::npos) {
            current_dir_ = book_path_.substr(0, pos + 1);
        }
        else {
            current_dir_ = "./";
        }


        // 解析PDB头
        if (!parsePDBHeader()) {
            return false;
        }

        // 解析记录索引
        if (!parseRecordIndices()) {
            return false;
        }

        if (m_record_info_list.empty() || m_record_info_list[0].data_offset >= file_data_.size()) {
            return false;
        }

        // 解析记录0（PalmDOC/MOBI头）
        if (!parseRecord0()) {
            return false;
        }

        // 解析特殊记录（FLIS/FCIS/EOF）
        parseSpecialRecords();


        // 解析MOBI记录
        //bool mobi_found = false;
        //for (uint32_t i = 0; i < m_record_info_list.size(); i++) {
        //    if (parseMobiRecord(i)) {
        //        mobi_found = true;
        //        break;
        //    }
        //}

        //if (!mobi_found) {
        //    return false;
        //}

        // 解析EXTH元数据
        parseExthMetadata();

        // 获取完整书名
        //std::string full_name = getFullName();
        //if (!full_name.empty()) {
        //    title_ = full_name;
        //}

        // 解析内容记录
        if (!parseContentRecords()) {
            return false;
        }

        // 设置OCF包信息
        setupOCFPackage();

        is_loaded_ = true;
        return true;
    }

    bool MobiBook::parsePDBHeader() {
        if (file_data_.size() < sizeof(PDBHeader)) {
            return false;
        }

        const PDBHeader* header = reinterpret_cast<const PDBHeader*>(file_data_.data());
        if(!header)
        {
            return false;
        }

        m_pdb_header = swapPDBHeader(*header);
         //检查是否为PDB格式
        if (std::string(header->type, 4) != "BOOK" &&
            std::string(header->type, 4) != "TEXt" &&
            std::string(header->type, 4) != "MOBI") {
            return false;
        }

        return true;
    }

    bool MobiBook::parseRecordIndices() {
        if (file_data_.size() < sizeof(PDBHeader) + sizeof(RecordIndex)) {
            return false;
        }

     
        uint32_t num_records = m_pdb_header.num_records;

        // 记录索引从PDB头之后开始
        const uint8_t* index_data = file_data_.data() + sizeof(PDBHeader);

        m_record_info_list.resize(num_records);
        for (uint32_t i = 0; i < num_records; i++) {
            const RecordInfo* raw_idx = reinterpret_cast<const RecordInfo*>(
                index_data + i * sizeof(RecordInfo));

            m_record_info_list[i] = swapRecordIndex(*raw_idx);
        }

        return true;
    }

    bool MobiBook::parseMobiRecord(uint32_t record_index) {
        if (record_index >= m_record_info_list.size()) {
            return false;
        }

        const RecordInfo& record = m_record_info_list[record_index];
        if (record.data_offset >= file_data_.size()) {
            return false;
        }

        const uint8_t* record_data = file_data_.data() + record.data_offset;

        // 1. 读取原始大端序数据到结构体
        MobiHeader mobi_header_raw;
        memcpy(&mobi_header_raw, record_data, sizeof(MobiHeader));

        // 2. 转换为小端序
        MobiHeader mobi_header = swapMobiHeader(mobi_header_raw);

        // 3. 使用转换后的结构体（不需要再调用 readUInt32）
        mobi_version_ = mobi_header.file_version;
        text_encoding_ = mobi_header.text_encoding;

        // 压缩类型在记录属性中
        compression_type_ = (record.attributes >> 9) & 0x07;

        // 检查 MOBI 版本以确定字段大小
        if (mobi_version_ >= 6) {
            // 注意：您原始代码中的 first_content_record 可能字段位置不对
            // MOBI头中没有 first_content_record 字段，需要检查您的结构体定义
        }

        full_name_offset_ = mobi_header.full_name_offset;
        full_name_length_ = mobi_header.full_name_length;

        // 解析EXTH记录
        if (mobi_header.exth_flags & 0x40) {  // 有EXTH记录
            const uint8_t* exth_data = record_data + mobi_header.header_length;

            // 检查 EXTH 标识符
            uint32_t exth_identifier = readUInt32(exth_data);  // 这里可以继续用 readUInt32

            if (exth_identifier == 0x45585448) {  // "EXTH"
                // 读取 EXTH 头并进行字节序转换
                ExthHeader exth_header_raw;
                memcpy(&exth_header_raw, exth_data, sizeof(ExthHeader));
                ExthHeader exth_header = swapExthHeader(exth_header_raw);

                parseExthRecord(exth_data + 12, exth_header.header_length - 12);
            }
        }

        version_ = "MOBI v" + std::to_string(mobi_version_);

        return true;
    }

    bool MobiBook::parseExthRecord(const uint8_t* data, uint32_t length) {
        uint32_t pos = 0;

        while (pos + 8 <= length) {
            uint32_t record_type = readUInt32(data + pos);
            uint32_t record_length = readUInt32(data + pos + 4);

            if (record_length < 8 || pos + record_length > length) {
                break;
            }

            // 记录数据
            uint32_t data_length = record_length - 8;
            std::string record_data(reinterpret_cast<const char*>(data + pos + 8), data_length);

            // 去除末尾的空字符
            while (!record_data.empty() && record_data.back() == '\0') {
                record_data.pop_back();
            }

            exth_records_[record_type] = record_data;

            pos += record_length;
        }

        return true;
    }

    void MobiBook::parseExthMetadata() {
        // EXTH记录类型定义
        const uint32_t EXTH_TITLE = 503;
        const uint32_t EXTH_AUTHOR = 100;
        const uint32_t EXTH_PUBLISHER = 101;
        const uint32_t EXTH_ISBN = 104;
        const uint32_t EXTH_DESCRIPTION = 103;
        const uint32_t EXTH_SUBJECT = 105;

        // 提取元数据
        if (exth_records_.find(EXTH_TITLE) != exth_records_.end()) {
            title_ = exth_records_[EXTH_TITLE];
        }

        if (exth_records_.find(EXTH_AUTHOR) != exth_records_.end()) {
            author_ = exth_records_[EXTH_AUTHOR];
        }

        if (exth_records_.find(EXTH_PUBLISHER) != exth_records_.end()) {
            publisher_ = exth_records_[EXTH_PUBLISHER];
        }

        if (exth_records_.find(EXTH_ISBN) != exth_records_.end()) {
            isbn_ = exth_records_[EXTH_ISBN];
        }
    }

    std::string MobiBook::getFullName() {
        if (full_name_offset_ == 0 || full_name_length_ == 0) {
            return "";
        }

        const uint8_t* record_data = file_data_.data() + m_record_info_list[0].data_offset;
        const uint8_t* name_data = record_data + full_name_offset_;

        return decodeText(name_data, full_name_length_);
    }

    bool MobiBook::parseContentRecords() {
        if (first_content_record_ >= m_record_info_list.size() ||
            last_content_record_ >= m_record_info_list.size() ||
            first_content_record_ > last_content_record_) {
            return false;
        }

        content_records_.clear();
        image_records_.clear();

        for (uint16_t i = first_content_record_; i <= last_content_record_; i++) {
            const RecordInfo& record = m_record_info_list[i];

            if (record.data_offset >= file_data_.size()) {
                continue;
            }

            const uint8_t* record_data = file_data_.data() + record.data_offset;

            // 检查是否为图像记录
            bool is_image = false;
            if (i >= 1) {  // 通常第一个图像记录在内容记录之后
                // 检查常见图像标识符
                if (record_data[0] == 0xFF && record_data[1] == 0xD8) {  // JPEG
                    is_image = true;
                }
                else if (record_data[0] == 0x89 && record_data[1] == 0x50 &&
                    record_data[2] == 0x4E && record_data[3] == 0x47) {  // PNG
                    is_image = true;
                }
                else if (record_data[0] == 0x47 && record_data[1] == 0x49 &&
                    record_data[2] == 0x46) {  // GIF
                    is_image = true;
                }
            }

            // 计算记录长度
            uint32_t record_length = 0;
            if (i + 1 < m_record_info_list.size()) {
                record_length = m_record_info_list[i + 1].data_offset - record.data_offset;
            }
            else {
                record_length = file_data_.size() - record.data_offset;
            }

            std::vector<uint8_t> data(record_data, record_data + record_length);

            if (is_image) {
                image_records_.push_back(data);
            }
            else {
                content_records_.push_back(data);
            }
        }

        return !content_records_.empty();
    }

    // ==================== 文本处理 ====================

    std::string MobiBook::decodeText(const uint8_t* data, uint32_t length) {
        if (length == 0 || data == nullptr) {
            return "";
        }

        // 根据编码类型解码
        switch (text_encoding_) {
        case 1252:  // CP1252
            return std::string(reinterpret_cast<const char*>(data), length);

        case 65001:  // UTF-8
            return std::string(reinterpret_cast<const char*>(data), length);

        case 1200:  // UTF-16 LE
        case 1201:  // UTF-16 BE
            // 简化处理：转换为UTF-8（实际应完整处理）
        {
            std::string result;
            for (uint32_t i = 0; i < length; i += 2) {
                if (i + 1 < length) {
                    uint16_t ch;
                    if (text_encoding_ == 1200) {  // LE
                        ch = data[i] | (data[i + 1] << 8);
                    }
                    else {  // BE
                        ch = (data[i] << 8) | data[i + 1];
                    }

                    if (ch < 0x80) {
                        result += static_cast<char>(ch);
                    }
                    else if (ch < 0x800) {
                        result += static_cast<char>(0xC0 | (ch >> 6));
                        result += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                    else {
                        result += static_cast<char>(0xE0 | (ch >> 12));
                        result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (ch & 0x3F));
                    }
                }
            }
            return result;
        }

        default:
            // 默认按Latin-1处理
            return std::string(reinterpret_cast<const char*>(data), length);
        }
    }

    std::vector<uint8_t> MobiBook::decompressPalmDoc(const uint8_t* data, uint32_t length) {
        std::vector<uint8_t> result;

        if (data == nullptr || length == 0) {
            return result;
        }

        uint32_t pos = 0;
        while (pos < length) {
            uint8_t c = data[pos++];

            if (c >= 0x01 && c <= 0x08) {
                // 复制后面的c个字节
                for (uint8_t i = 0; i < c && pos < length; i++) {
                    result.push_back(data[pos++]);
                }
            }
            else if (c >= 0x09 && c <= 0x7F) {
                // 直接复制
                result.push_back(c);
            }
            else if (c >= 0x80 && c <= 0xBF) {
                // 2字节编码
                if (pos >= length) break;

                uint8_t next = data[pos++];
                uint8_t distance = ((c << 2) & 0x1C) | ((next >> 6) & 0x03);
                uint8_t length2 = (next & 0x3F) + 3;

                // 从结果中复制
                if (distance > result.size()) {
                    // 错误情况，跳过
                    continue;
                }

                for (uint8_t i = 0; i < length2; i++) {
                    result.push_back(result[result.size() - distance]);
                }
            }
            else if (c >= 0xC0 && c <= 0xFF) {
                // 1字节编码
                uint8_t length2 = (c & 0x1F) + 3;
                uint8_t distance = data[pos++];

                // 从结果中复制
                if (distance > result.size()) {
                    // 错误情况，跳过
                    continue;
                }

                for (uint8_t i = 0; i < length2; i++) {
                    result.push_back(result[result.size() - distance]);
                }
            }
        }

        return result;
    }

    std::string MobiBook::encodingToString(uint32_t encoding) {
        switch (encoding) {
        case 1252: return "CP1252";
        case 65001: return "UTF-8";
        case 1200: return "UTF-16 LE";
        case 1201: return "UTF-16 BE";
        default: return "Unknown (" + std::to_string(encoding) + ")";
        }
    }

    // ==================== OCF包设置 ====================

    void MobiBook::setupOCFPackage() {
        ocf_package_.rootfile = book_path_;
        ocf_package_.opf_dir = current_dir_;
        ocf_package_.version = "1.0";

        // 清空现有数据
        spine_.clear();
        ocf_package_.manifest.clear();
        ocf_package_.toc.clear();
        ocf_package_.meta.clear();

        // 添加元数据
        ocf_package_.meta["title"] = title_;
        ocf_package_.meta["author"] = author_;
        ocf_package_.meta["publisher"] = publisher_;
        ocf_package_.meta["isbn"] = isbn_;
        ocf_package_.meta["version"] = version_;
        ocf_package_.meta["encoding"] = encodingToString(text_encoding_);

        // 添加内容文件到manifest和spine
        for (size_t i = 0; i < content_records_.size(); i++) {
            std::string id = "content_" + std::to_string(i);
            std::string href = "content_" + std::to_string(i) + ".html";

            // 添加到manifest
            OCFItem item;
            item.id = id;
            item.href = href;
            item.media_type = "application/xhtml+xml";
            ocf_package_.manifest.push_back(item);

            // 添加到spine
            OCFRef ref;
            ref.idref = id;
            ref.href = href;
            ref.linear = "yes";
            spine_.push_back(ref);

            // 添加到目录
            OCFNavPoint nav;
            nav.label = "Chapter " + std::to_string(i + 1);
            nav.href = href;
            nav.order = static_cast<int>(i + 1);
            ocf_package_.toc.push_back(nav);
        }

        // 添加图像文件到manifest
        for (size_t i = 0; i < image_records_.size(); i++) {
            std::string id = "image_" + std::to_string(i);
            std::string href = "image_" + std::to_string(i);

            // 确定图像类型
            std::string media_type = "image/jpeg";  // 默认JPEG
            const auto& img_data = image_records_[i];
            if (img_data.size() > 4) {
                if (img_data[0] == 0x89 && img_data[1] == 0x50 &&
                    img_data[2] == 0x4E && img_data[3] == 0x47) {
                    media_type = "image/png";
                    href += ".png";
                }
                else if (img_data[0] == 0x47 && img_data[1] == 0x49 &&
                    img_data[2] == 0x46) {
                    media_type = "image/gif";
                    href += ".gif";
                }
                else {
                    href += ".jpg";
                }
            }

            OCFItem item;
            item.id = id;
            item.href = href;
            item.media_type = media_type;
            ocf_package_.manifest.push_back(item);
        }

        // 设置TOC路径
        ocf_package_.toc_path = "toc.ncx";
    }

    bool MobiBook::parseRecord0() {
        if (m_record_info_list.empty()) {
            std::cerr << "record info list is empty" << std::endl;
            return false;
        }

        // 第一个记录的索引
        const RecordInfo& record0 = m_record_info_list[0];

        
        if (record0.data_offset + sizeof(PalmDocHeader) > file_data_.size()) {
            std::cerr << "Record 0 data size is not enough" << std::endl;
            return false;
        }

        // 解析 PalmDOC 头
        const PalmDocHeader* raw_palm_doc_header = reinterpret_cast<const PalmDocHeader*>(
            file_data_.data() + record0.data_offset);
        auto palm_doc_header = swapPalmDocHeader(*raw_palm_doc_header);
     
        std::cout << "\n=== PalmDOC Header Info ===" << std::endl;
        std::cout << "Compression: " << palm_doc_header.compression << " (";
        switch (palm_doc_header.compression) {
        case COMPRESSION_NONE:
            std::cout << "No";
            break;
        case COMPRESSION_PALMDOC:
            std::cout << "PalmDOC";
            break;
        case COMPRESSION_HUFFCDIC:
            std::cout << "HUFF/CDIC";
            break;
        default:
            std::cout << "Unknown";
            break;
        }
        std::cout << ")" << std::endl;

        std::cout << "Text Length: " << palm_doc_header.text_length << " Bytes" << std::endl;
        std::cout << "Record Count: " << palm_doc_header.record_count << std::endl;
        std::cout << "Record Size: " << palm_doc_header.record_size << " Bytes" << std::endl;

        std::cout << "Encryption Type: " << palm_doc_header.encryption_type << " (";
        switch (palm_doc_header.encryption_type) {
        case ENCRYPTION_NONE:
            std::cout << "None";
            break;
        case ENCRYPTION_OLD_MOBI:
            std::cout << "Old Mobipocket Encryption";
            break;
        case ENCRYPTION_MOBI:
            std::cout << "Mobipocket Encryption";
            break;
        default:
            std::cout << "Unknown Encryption";
            break;
        }
        std::cout << ")" << std::endl;

        // 检查是否有 MOBI 头
        uint32_t mobi_header_offset = record0.data_offset + sizeof(PalmDocHeader);

        if (mobi_header_offset + 4 > file_data_.size()) {
            std::cout << "No MOBI header" << std::endl;
            return true;  // 可能是一个简单的PalmDOC文件
        }

        const char* mobi_identifier = reinterpret_cast<const char*>(
            file_data_.data() + mobi_header_offset);

        if (std::memcmp(mobi_identifier, "MOBI", 4) != 0) {
            std::cout << "No MOBI header（no match）" << std::endl;
            return true;  // 可能没有MOBI头
        }

        std::cout << "\n=== MOBI Header Info ===" << std::endl;

        // 解析 MOBI 头
        const MobiHeader* raw_mobi_header = reinterpret_cast<const MobiHeader*>(
            file_data_.data() + mobi_header_offset);
        auto m_mobi_header = swapMobiHeader(*raw_mobi_header);
     
        std::cout << "MOBI Length: " << m_mobi_header.header_length << " Bytes" << std::endl;

        std::cout << "MOBI Type: " << m_mobi_header.mobi_type << " (";
        switch (m_mobi_header.mobi_type) {
        case MOBI_TYPE_BOOK:
            std::cout << "Mobipocket Book";
            break;
        case MOBI_TYPE_PALMDOC:
            std::cout << "PalmDoc Book";
            break;
        case MOBI_TYPE_AUDIO:
            std::cout << "Audio";
            break;
        case MOBI_TYPE_KINDLEGEN1:
            std::cout << "KindleGen 1.2";
            break;
        case MOBI_TYPE_KF8:
            std::cout << "KF8 (KindleGen 2)";
            break;
        case MOBI_TYPE_NEWS:
            std::cout << "News";
            break;
        case MOBI_TYPE_NEWS_FEED:
            std::cout << "News Feed";
            break;
        case MOBI_TYPE_NEWS_MAGAZINE:
            std::cout << "News Magazine";
            break;
        default:
            std::cout << "Unknown Type";
            break;
        }
        std::cout << ")" << std::endl;

        std::cout << "Text Encoding: " << m_mobi_header.text_encoding << " (";
        switch (m_mobi_header.text_encoding) {
        case ENCODING_CP1252:
            std::cout << "CP1252/WinLatin1";
            break;
        case ENCODING_UTF8:
            std::cout << "UTF-8";
            break;
        default:
            std::cout << "Unknown Encoding";
            break;
        }
        std::cout << ")" << std::endl;

        std::cout << "Unique ID: " << m_mobi_header.unique_id << std::endl;
        std::cout << "File Version: " << m_mobi_header.file_version << std::endl;
        std::cout << "Locale: " << m_mobi_header.locale << std::endl;

        if (m_mobi_header.full_name_offset > 0 && m_mobi_header.full_name_length > 0) {
            uint32_t name_offset = record0.data_offset + m_mobi_header.full_name_offset;
            if (name_offset + m_mobi_header.full_name_length <= file_data_.size()) {
                std::string full_name(reinterpret_cast<const char*>(
                    file_data_.data() + name_offset), m_mobi_header.full_name_length);
                std::cout << "Full Name: " << full_name << std::endl;
            }
        }

        std::cout << "First Image Index: " << m_mobi_header.first_image_index << std::endl;

        // 检查是否有EXTH头
        if (m_mobi_header.exth_flags & 0x40) {
            std::cout << "\n=== EXTH Header Info ===" << std::endl;

            uint32_t exth_header_offset = mobi_header_offset + m_mobi_header.header_length;

            if (exth_header_offset + sizeof(ExthHeader) <= file_data_.size()) {
                const ExthHeader* raw_exth_header = reinterpret_cast<const ExthHeader*>(
                    file_data_.data() + exth_header_offset);

                auto exth_header = swapExthHeader(*raw_exth_header);
             
                if (std::memcmp(exth_header.identifier, "EXTH", 4) == 0) {
                    std::cout << "EXTH Header Length: " << exth_header.header_length << " Bytes" << std::endl;
                    std::cout << "EXTH Record Count: " << exth_header.record_count << std::endl;

                    // 解析EXTH记录
                    parseExthRecords(exth_header_offset + sizeof(ExthHeader),
                        exth_header.header_length - sizeof(ExthHeader),
                        exth_header.record_count);
                }
            }
        }

        return true;
    }

    bool MobiBook::parseExthRecords(uint32_t start_offset, uint32_t data_length, uint32_t record_count) {
        if (start_offset + data_length > file_data_.size()) {
            std::cerr << "EXTH data out of range" << std::endl;
            return false;
        }

        uint32_t current_offset = start_offset;
        uint32_t records_parsed = 0;

        std::cout << "EXTH record details:" << std::endl;

        while (records_parsed < record_count && current_offset + 8 <= start_offset + data_length) {
            const ExthRecord* raw_record = reinterpret_cast<const ExthRecord*>(
                file_data_.data() + current_offset);

            auto record = swapExthRecord(*raw_record);
          

            std::cout << "  Record " << records_parsed + 1 << " - Type: " << record.type
                << ", Length: " << record.length;

            if (record.length < 8) {
                std::cout << " (Length Invalid)" << std::endl;
                break;
            }

            // 解析记录数据
            uint32_t data_length = record.length - 8;
            if (current_offset + record.length <= start_offset + data_length) {
                std::string record_data(reinterpret_cast<const char*>(
                    file_data_.data() + current_offset + 8), data_length);

                // 输出常见EXTH记录类型的信息
                switch (record.type) {
                case EXTH_AUTHOR:
                    std::cout << ", Author: " << record_data;
                    break;
                case EXTH_PUBLISHER:
                    std::cout << ", Publisher: " << record_data;
                    break;
                case EXTH_DESCRIPTION:
                    std::cout << ", Description: " << (data_length > 50 ? record_data.substr(0, 47) + "..." : record_data);
                    break;
                case EXTH_SUBJECT:
                    std::cout << ", Subject: " << record_data;
                    break;
                case EXTH_LANGUAGE:
                    std::cout << ", Language: " << record_data;
                    break;
                case EXTH_CREATOR_SOFTWARE:
                    std::cout << ", Creator Software: ";
                    if (data_length >= 4) {
                        uint32_t creator = *reinterpret_cast<const uint32_t*>(record_data.data());
                        switch (creator) {
                        case 1: std::cout << "mobigen"; break;
                        case 2: std::cout << "Mobipocket Creator"; break;
                        case 200: std::cout << "kindlegen (Windows)"; break;
                        case 201: std::cout << "kindlegen (Linux)"; break;
                        case 202: std::cout << "kindlegen (Mac)"; break;
                        default: std::cout << "Unknown (" << creator << ")"; break;
                        }
                    }
                    break;
                default:
                    std::cout << ", data: " << (data_length > 50 ? record_data.substr(0, 47) + "..." : record_data);
                    break;
                }
            }

            std::cout << std::endl;

            current_offset += record.length;
            records_parsed++;
        }

        return records_parsed == record_count;
    }
    bool MobiBook::parseSpecialRecords() {
        if (m_record_info_list.size() < 3) {
            std::cout << "记录数量不足以包含特殊记录" << std::endl;
            return false;
        }

        std::cout << "\n=== Special Records ===" << std::endl;

        // 检查最后几个记录是否为FLIS/FCIS/EOF
        size_t last_index = m_record_info_list.size() - 1;

        // 检查EOF记录
        const RecordInfo& eof_record = m_record_info_list[last_index];
        if (eof_record.data_offset + 4 <= file_data_.size()) {
            const EofRecord* eof = reinterpret_cast<const EofRecord*>(
                file_data_.data() + eof_record.data_offset);

            if (eof->byte0 == 0xE9 && eof->byte1 == 0x8E &&
                eof->byte2 == 0x0D && eof->byte3 == 0x0A) {
                std::cout << "Found EOF record (record " << last_index << ")" << std::endl;
            }
        }

        // 检查FCIS记录
        if (last_index >= 2) {
            const RecordInfo& fcis_record = m_record_info_list[last_index - 1];
            if (fcis_record.data_offset + 4 <= file_data_.size()) {
                const char* identifier = reinterpret_cast<const char*>(
                    file_data_.data() + fcis_record.data_offset);

                if (std::memcmp(identifier, "FCIS", 4) == 0) {
                    std::cout << "Found FCIS record (record " << last_index - 1 << ")" << std::endl;

                    const FcisRecord* fcis = reinterpret_cast<const FcisRecord*>(
                        file_data_.data() + fcis_record.data_offset);
                    std::cout << "  FCIS Text Length: " << fcis->text_length << " Bytes" << std::endl;
                }
            }
        }

        // 检查FLIS记录
        if (last_index >= 3) {
            const RecordInfo& flis_record = m_record_info_list[last_index - 2];
            if (flis_record.data_offset + 4 <= file_data_.size()) {
                const char* identifier = reinterpret_cast<const char*>(
                    file_data_.data() + flis_record.data_offset);

                if (std::memcmp(identifier, "FLIS", 4) == 0) {
                    std::cout << "Found FLIS record (record " << last_index - 2 << ")" << std::endl;
                }
            }
        }

        return true;
    }
    // ==================== Book接口实现 ====================

    std::vector<uint8_t> MobiBook::get_binary(std::string base_url, std::string url) {
        std::vector<uint8_t> result;

        if (url.empty()) {
            return result;
        }

        std::string full_path = resolve_path(base_url, url);

        // 如果是MOBI文件本身
        if (full_path == book_path_) {
            return file_data_;
        }

        // 检查是否是内容文件
        for (size_t i = 0; i < content_records_.size(); i++) {
            std::string expected_href = "content_" + std::to_string(i) + ".html";
            if (full_path.find(expected_href) != std::string::npos) {
                // 处理压缩
                std::vector<uint8_t> decompressed;
                if (compression_type_ == 2) {  // PalmDOC压缩
                    decompressed = decompressPalmDoc(
                        content_records_[i].data(),
                        static_cast<uint32_t>(content_records_[i].size())
                    );
                }
                else {
                    decompressed = content_records_[i];
                }

                // 转换为HTML
                std::string html_content = "<!DOCTYPE html>\n";
                html_content += "<html>\n";
                html_content += "<head>\n";
                html_content += "<meta charset=\"utf-8\">\n";
                html_content += "<title>" + title_ + "</title>\n";
                html_content += "</head>\n";
                html_content += "<body>\n";

                if (i == 0) {
                    html_content += "<h1>" + title_ + "</h1>\n";
                    if (!author_.empty()) {
                        html_content += "<h2>Author: " + author_ + "</h2>\n";
                    }
                }

                html_content += "<div>\n";

                // 解码文本
                std::string text = decodeText(
                    decompressed.data(),
                    static_cast<uint32_t>(decompressed.size())
                );

                // 简单格式化
                std::stringstream ss(text);
                std::string line;
                while (std::getline(ss, line)) {
                    if (!line.empty()) {
                        html_content += "<p>" + line + "</p>\n";
                    }
                }

                html_content += "</div>\n";
                html_content += "</body>\n";
                html_content += "</html>\n";

                result.resize(html_content.size());
                std::memcpy(result.data(), html_content.data(), html_content.size());
                return result;
            }
        }

        // 检查是否是图像文件
        for (size_t i = 0; i < image_records_.size(); i++) {
            std::string expected_href = "image_" + std::to_string(i);
            if (full_path.find(expected_href) != std::string::npos) {
                return image_records_[i];
            }
        }

        return result;
    }

    std::string MobiBook::get_string(const std::string& path) {
        auto binary = get_binary("", path);
        if (!binary.empty()) {
            return std::string(reinterpret_cast<char*>(binary.data()), binary.size());
        }
        return "";
    }

    std::string MobiBook::get_chapter_name_by_id(int spine_id) {
        if (spine_id >= 0 && spine_id < static_cast<int>(ocf_package_.toc.size())) {
            return ocf_package_.toc[spine_id].label;
        }
        return "Chapter " + std::to_string(spine_id + 1);
    }

    void MobiBook::clear() {
        book_path_.clear();
        current_dir_.clear();
        title_.clear();
        author_.clear();
        publisher_.clear();
        isbn_.clear();
        version_.clear();
        file_data_.clear();
        m_record_info_list.clear();
        exth_records_.clear();
        content_records_.clear();
        image_records_.clear();
        spine_.clear();
        ocf_package_ = OCFPackage();

        mobi_version_ = 0;
        text_encoding_ = 65001;
        compression_type_ = 2;
        first_content_record_ = 0;
        last_content_record_ = 0;
        full_name_offset_ = 0;
        full_name_length_ = 0;

        is_loaded_ = false;
    }

    std::string MobiBook::resolve_path(std::string base_url, std::string href) {
        if (href.empty()) return "";

        // 绝对路径
        if (href[0] == '/' ||
            href.find("://") != std::string::npos ||
            (href.length() > 1 && href[1] == ':')) {
            return normalizePath(href);
        }

        // 处理base_url
        std::string base_dir = current_dir_;
        if (!base_url.empty()) {
            size_t pos = base_url.find_last_of('/');
            if (pos != std::string::npos) {
                base_dir = base_url.substr(0, pos + 1);
            }
        }

        // 构建完整路径
        std::string result = base_dir + href;
        result = normalizePath(result);

        // 处理相对路径
        std::vector<std::string> parts;
        std::stringstream ss(result);
        std::string part;

        while (std::getline(ss, part, '/')) {
            if (part == "..") {
                if (!parts.empty()) {
                    parts.pop_back();
                }
            }
            else if (part != "." && !part.empty()) {
                parts.push_back(part);
            }
        }

        // 重新组合
        result.clear();
        for (size_t i = 0; i < parts.size(); i++) {
            if (i > 0) result += "/";
            result += parts[i];
        }

        return result;
    }

    // 在实现文件中
    uint16_t MobiBook::swapUint16(uint16_t value) {
        return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
    }

    uint32_t MobiBook::swapUint32(uint32_t value) {
        return ((value & 0xFF000000) >> 24) |
            ((value & 0x00FF0000) >> 8) |
            ((value & 0x0000FF00) << 8) |
            ((value & 0x000000FF) << 24);
    }

    int32_t MobiBook::swapInt32(int32_t value) {
        uint32_t uval = static_cast<uint32_t>(value);
        uint32_t swapped = swapUint32(uval);
        return static_cast<int32_t>(swapped);
    }

    PalmDocHeader MobiBook::swapPalmDocHeader(const PalmDocHeader& header) {
        PalmDocHeader swapped = header;
        swapped.compression = swapUint16(header.compression);
        swapped.unused = swapUint16(header.unused);
        swapped.text_length = swapUint32(header.text_length);
        swapped.record_count = swapUint16(header.record_count);
        swapped.record_size = swapUint16(header.record_size);
        swapped.encryption_type = swapUint16(header.encryption_type);
        swapped.unknown = swapUint16(header.unknown);
        return swapped;
    }

    MobiHeader MobiBook::swapMobiHeader(const MobiHeader& header) {
        MobiHeader swapped = header;
        // 注意：identifier[4] 是字符，不需要交换

        swapped.header_length = swapUint32(header.header_length);
        swapped.mobi_type = swapUint32(header.mobi_type);
        swapped.text_encoding = swapUint32(header.text_encoding);
        swapped.unique_id = swapUint32(header.unique_id);
        swapped.file_version = swapUint32(header.file_version);
        swapped.orthographic_index = swapUint32(header.orthographic_index);
        swapped.inflection_index = swapUint32(header.inflection_index);
        swapped.index_names = swapUint32(header.index_names);
        swapped.index_keys = swapUint32(header.index_keys);
        swapped.extra_index0 = swapUint32(header.extra_index0);
        swapped.extra_index1 = swapUint32(header.extra_index1);
        swapped.extra_index2 = swapUint32(header.extra_index2);
        swapped.extra_index3 = swapUint32(header.extra_index3);
        swapped.extra_index4 = swapUint32(header.extra_index4);
        swapped.extra_index5 = swapUint32(header.extra_index5);
        swapped.first_non_book_index = swapUint32(header.first_non_book_index);
        swapped.full_name_offset = swapUint32(header.full_name_offset);
        swapped.full_name_length = swapUint32(header.full_name_length);
        swapped.locale = swapUint32(header.locale);
        swapped.input_language = swapUint32(header.input_language);
        swapped.output_language = swapUint32(header.output_language);
        swapped.min_version = swapUint32(header.min_version);
        swapped.first_image_index = swapUint32(header.first_image_index);
        swapped.huffman_record_offset = swapUint32(header.huffman_record_offset);
        swapped.huffman_record_count = swapUint32(header.huffman_record_count);
        swapped.huffman_table_offset = swapUint32(header.huffman_table_offset);
        swapped.huffman_table_length = swapUint32(header.huffman_table_length);
        swapped.exth_flags = swapUint32(header.exth_flags);
        // unknown32[32] 是字节数组，不需要交换
        swapped.unknown_a4 = swapUint32(header.unknown_a4);
        swapped.drm_offset = swapUint32(header.drm_offset);
        swapped.drm_count = swapUint32(header.drm_count);
        swapped.drm_size = swapUint32(header.drm_size);
        swapped.drm_flags = swapUint32(header.drm_flags);
        return swapped;
    }

    PDBHeader MobiBook::swapPDBHeader(const PDBHeader& header) {
        PDBHeader swapped = header;
        // name[32] 是字符数组，不需要交换

        swapped.attributes = swapUint16(header.attributes);
        swapped.version = swapUint16(header.version);
        swapped.creation_time = swapUint32(header.creation_time);
        swapped.modification_time = swapUint32(header.modification_time);
        swapped.backup_time = swapUint32(header.backup_time);
        swapped.modification_number = swapUint32(header.modification_number);
        swapped.app_info_offset = swapUint32(header.app_info_offset);
        swapped.sort_info_offset = swapUint32(header.sort_info_offset);
        // type[4] 和 creator[4] 是字符数组，不需要交换
        swapped.unique_id_seed = swapUint32(header.unique_id_seed);
        swapped.next_record_list_id = swapUint32(header.next_record_list_id);
        swapped.num_records = swapUint16(header.num_records);

        return swapped;
    }

    RecordIndex MobiBook::swapRecordIndex(const RecordIndex& index) {
        RecordIndex swapped = index;
        swapped.offset = swapUint32(index.offset);
        // attributes 是单字节，不需要交换
        // unique_id[3] 是字节数组，不需要交换
        return swapped;
    }

    RecordInfo MobiBook::swapRecordIndex(const RecordInfo& index) {
        RecordInfo swapped = index;
        swapped.data_offset = swapUint32(index.data_offset);
        // attributes 是单字节，不需要交换
        // unique_id[3] 是字节数组，不需要交换
        return swapped;
    }

    ExthHeader MobiBook::swapExthHeader(const ExthHeader& header) {
        ExthHeader swapped = header;
        // identifier[4] 是字符数组，不需要交换
        swapped.header_length = swapUint32(header.header_length);
        swapped.record_count = swapUint32(header.record_count);
        return swapped;
    }

    ExthRecord MobiBook::swapExthRecord(const ExthRecord& record) {
        ExthRecord swapped = record;
        swapped.type = swapUint32(record.type);
        swapped.length = swapUint32(record.length);
        // 注意：数据部分不会在这里交换，需要在读取时单独处理
        return swapped;
    }

    FlisRecord MobiBook::swapFlisRecord(const FlisRecord& record) {
        FlisRecord swapped = record;
        // identifier[4] 是字符数组，不需要交换
        swapped.unknown_4 = swapUint32(record.unknown_4);
        swapped.unknown_8 = swapUint16(record.unknown_8);
        swapped.unknown_a = swapUint16(record.unknown_a);
        swapped.unknown_c = swapUint32(record.unknown_c);
        swapped.unknown_10 = swapUint32(record.unknown_10);
        swapped.unknown_14 = swapUint16(record.unknown_14);
        swapped.unknown_16 = swapUint16(record.unknown_16);
        swapped.unknown_18 = swapUint32(record.unknown_18);
        swapped.unknown_1c = swapUint32(record.unknown_1c);
        swapped.unknown_20 = swapUint32(record.unknown_20);
        return swapped;
    }

    FcisRecord MobiBook::swapFcisRecord(const FcisRecord& record) {
        FcisRecord swapped = record;
        // identifier[4] 是字符数组，不需要交换
        swapped.unknown_4 = swapUint32(record.unknown_4);
        swapped.unknown_8 = swapUint32(record.unknown_8);
        swapped.unknown_c = swapUint32(record.unknown_c);
        swapped.unknown_10 = swapUint32(record.unknown_10);
        swapped.text_length = swapUint32(record.text_length);
        swapped.unknown_18 = swapUint32(record.unknown_18);
        swapped.unknown_1c = swapUint32(record.unknown_1c);
        swapped.unknown_20 = swapUint32(record.unknown_20);
        swapped.unknown_24 = swapUint16(record.unknown_24);
        swapped.unknown_26 = swapUint16(record.unknown_26);
        swapped.unknown_28 = swapUint32(record.unknown_28);
        return swapped;
    }

    ImageRecordIndex MobiBook::swapImageRecordIndex(const ImageRecordIndex& index) {
        ImageRecordIndex swapped = index;
        swapped.offset = swapUint32(index.offset);
        swapped.length = swapUint32(index.length);
        swapped.width = swapUint16(index.width);
        swapped.height = swapUint16(index.height);
        // type 是单字节，不需要交换
        return swapped;
    }
} // namespace mobi