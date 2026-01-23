#include "MOBIBook.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace mobi {

    // ==================== 辅助函数 ====================


    bool MobiBook::readFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        m_file_data.resize(size);
        if (!file.read(reinterpret_cast<char*>(m_file_data.data()), size)) {
            m_file_data.clear();
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
        if (m_file_data.size() < 100) { // 或更具体的阈值
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

        printPDBHeader();

        // 解析记录索引
        if (!parseRecordIndices()) {
            return false;
        }

        if (m_record_info_list.empty() || m_record_info_list[0].data_offset >= m_file_data.size()) {
            return false;
        }

        //printRecordInfoList();
        //printRecordInfoStat();

        // 解析记录0（PalmDOC/MOBI头）
        if (!parseRecord0()) {
            return false;
        }
        printRecord0();
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
        //parseExthMetadata();

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
        if (m_file_data.size() < sizeof(PDBHeader)) {
            return false;
        }

        const PDBHeader* header = reinterpret_cast<const PDBHeader*>(m_file_data.data());
        if (!header)
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
        if (m_file_data.size() < sizeof(PDBHeader) + sizeof(RecordInfo)) {
            return false;
        }


        uint32_t num_records = m_pdb_header.num_records;

        // 记录索引从PDB头之后开始
        const uint8_t* index_data = m_file_data.data() + sizeof(PDBHeader);

        m_record_info_list.resize(num_records);
        for (uint32_t i = 0; i < num_records; i++) {
            const RecordInfo* raw_idx = reinterpret_cast<const RecordInfo*>(
                index_data + i * sizeof(RecordInfo));

            m_record_info_list[i] = swapRecordInfo(*raw_idx);
        }

        return true;
    }



    std::string MobiBook::getFullName() const
    {
        // 解析全名
        if (m_mobi_header.full_name_offset > 0 && m_mobi_header.full_name_length > 0) {
            uint32_t name_offset = m_palm_doc_header_offset + m_mobi_header.full_name_offset;
            if (name_offset + m_mobi_header.full_name_length <= m_file_data.size()) {
                return std::string(reinterpret_cast<const char*>(
                    m_file_data.data() + name_offset), m_mobi_header.full_name_length);
            }
        }
        return "";
    }

    bool MobiBook::parseContentRecords() {
        if (m_mobi_header.first_content_record >= m_record_info_list.size() ||
            m_mobi_header.last_content_record >= m_record_info_list.size() ||
            m_mobi_header.first_content_record > m_mobi_header.last_content_record) {
            return false;
        }

        content_records_.clear();
        image_records_.clear();

        for (uint16_t i = m_mobi_header.first_content_record; i <= m_mobi_header.last_content_record; i++) {
            const RecordInfo& record = m_record_info_list[i];

            if (record.data_offset >= m_file_data.size()) {
                continue;
            }

            const uint8_t* record_data = m_file_data.data() + record.data_offset;

            // 检查是否为图像记录
            bool is_image = false;
            if (i >= m_mobi_header.first_image_index) {  // 通常第一个图像记录在内容记录之后
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
                record_length = m_file_data.size() - record.data_offset;
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

    //std::string MobiBook::decodeText(const uint8_t* data, uint32_t length) {
    //    if (length == 0 || data == nullptr) {
    //        return "";
    //    }

    //    // 根据编码类型解码
    //    switch (text_encoding_) {
    //    case 1252:  // CP1252
    //        return std::string(reinterpret_cast<const char*>(data), length);

    //    case 65001:  // UTF-8
    //        return std::string(reinterpret_cast<const char*>(data), length);

    //    case 1200:  // UTF-16 LE
    //    case 1201:  // UTF-16 BE
    //        // 简化处理：转换为UTF-8（实际应完整处理）
    //    {
    //        std::string result;
    //        for (uint32_t i = 0; i < length; i += 2) {
    //            if (i + 1 < length) {
    //                uint16_t ch;
    //                if (text_encoding_ == 1200) {  // LE
    //                    ch = data[i] | (data[i + 1] << 8);
    //                }
    //                else {  // BE
    //                    ch = (data[i] << 8) | data[i + 1];
    //                }

    //                if (ch < 0x80) {
    //                    result += static_cast<char>(ch);
    //                }
    //                else if (ch < 0x800) {
    //                    result += static_cast<char>(0xC0 | (ch >> 6));
    //                    result += static_cast<char>(0x80 | (ch & 0x3F));
    //                }
    //                else {
    //                    result += static_cast<char>(0xE0 | (ch >> 12));
    //                    result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
    //                    result += static_cast<char>(0x80 | (ch & 0x3F));
    //                }
    //            }
    //        }
    //        return result;
    //    }

    //    default:
    //        // 默认按Latin-1处理
    //        return std::string(reinterpret_cast<const char*>(data), length);
    //    }
    //}

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


    // ==================== OCF包设置 ====================
    std::string MobiBook::getExthRecord(ExthRecordType type)
    {
        if (m_exth_records.find(type) != m_exth_records.end())
        {
            return m_exth_records[type];
        }
        return "";
    };
    void MobiBook::setupOCFPackage()
    {
        m_ocf_package.rootfile = book_path_;
        m_ocf_package.opf_dir = current_dir_;
        m_ocf_package.version = m_mobi_header.file_version;

        // 清空现有数据
        m_ocf_package.spine.clear();
        m_ocf_package.manifest.clear();
        m_ocf_package.toc.clear();
        m_ocf_package.meta.clear();

        // 添加元数据
        m_ocf_package.meta["title"] = getFullName();
        m_ocf_package.meta["author"] = getExthRecord(EXTH_AUTHOR);
        m_ocf_package.meta["publisher"] = getExthRecord(EXTH_PUBLISHER);
        m_ocf_package.meta["isbn"] = getExthRecord(EXTH_ISBN);
        m_ocf_package.meta["version"] = m_mobi_header.file_version;
        m_ocf_package.meta["encoding"] = getEncodingName(m_mobi_header.text_encoding);

        // 添加内容文件到manifest和spine
        for (size_t i = 0; i < content_records_.size(); i++) {
            std::string id = "content_" + std::to_string(i);
            std::string href = "content_" + std::to_string(i) + ".html";

            // 添加到manifest
            OCFItem item;
            item.id = id;
            item.href = href;
            item.media_type = "application/xhtml+xml";
            m_ocf_package.manifest.push_back(item);

            // 添加到spine
            OCFRef ref;
            ref.idref = id;
            ref.href = href;
            ref.linear = "yes";
            m_ocf_package.spine.push_back(ref);

            // 添加到目录
            OCFNavPoint nav;
            nav.label = "Chapter " + std::to_string(i + 1);
            nav.href = href;
            nav.order = 0;
            m_ocf_package.toc.push_back(nav);
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
            m_ocf_package.manifest.push_back(item);
        }

        // 设置TOC路径
        m_ocf_package.toc_path = "toc.ncx";
    }



    bool MobiBook::parseExthRecords(uint32_t start_offset, uint32_t data_length, uint32_t record_count) {
        if (start_offset + data_length > m_file_data.size()) {
            std::cerr << "EXTH data out of range" << std::endl;
            return false;
        }

        uint32_t current_offset = start_offset;
        uint32_t records_parsed = 0;

        while (records_parsed < record_count && current_offset + 8 <= start_offset + data_length) {
            const ExthRecord* raw_record = reinterpret_cast<const ExthRecord*>(
                m_file_data.data() + current_offset);

            auto record = swapExthRecord(*raw_record);


            if (record.length < 8) {
                break;
            }

            // 解析记录数据
            uint32_t length = record.length - 8;
            if (current_offset + record.length <= start_offset + data_length)
            {
                std::string record_data(reinterpret_cast<const char*>(
                    m_file_data.data() + current_offset + 8), length);
                m_exth_records[record.type] = record_data;
            }

            std::cout << std::endl;

            current_offset += record.length;
            records_parsed++;
        }

        return records_parsed == record_count;
    }

    // 解析函数（只解析，不打印）
    bool MobiBook::parseRecord0() {
        if (m_record_info_list.empty()) {
            std::cerr << "record info list is empty" << std::endl;
            return false;
        }

        const RecordInfo& record0 = m_record_info_list[0];

        if (record0.data_offset + sizeof(PalmDocHeader) > m_file_data.size()) {
            std::cerr << "Record 0 data size is not enough" << std::endl;
            return false;
        }

        // 解析 PalmDOC 头
        m_palm_doc_header_offset = record0.data_offset;
        const PalmDocHeader* raw_palm_doc_header = reinterpret_cast<const PalmDocHeader*>(
            m_file_data.data() + m_palm_doc_header_offset);
        m_palm_doc_header = swapPalmDocHeader(*raw_palm_doc_header);

        // 检查是否有 MOBI 头
        m_mobi_header_offset = m_palm_doc_header_offset + sizeof(PalmDocHeader);

        if (m_mobi_header_offset + 4 > m_file_data.size()) {
            return true;  // 可能是一个简单的PalmDOC文件
        }

        const char* mobi_identifier = reinterpret_cast<const char*>(
            m_file_data.data() + m_mobi_header_offset);

        if (std::memcmp(mobi_identifier, "MOBI", 4) != 0) {
            return true;  // 可能没有MOBI头
        }

        // 解析 MOBI 头
        const MobiHeader* raw_mobi_header = reinterpret_cast<const MobiHeader*>(
            m_file_data.data() + m_mobi_header_offset);
        m_mobi_header = swapMobiHeader(*raw_mobi_header);



        // 检查是否有EXTH头
        if (m_mobi_header.exth_flags & 0x40) {
            m_exth_header_offset = m_mobi_header_offset + m_mobi_header.header_length;

            if (m_exth_header_offset + sizeof(ExthHeader) <= m_file_data.size()) {
                const ExthHeader* raw_exth_header = reinterpret_cast<const ExthHeader*>(
                    m_file_data.data() + m_exth_header_offset);

                auto exth_header = swapExthHeader(*raw_exth_header);

                if (std::memcmp(exth_header.identifier, "EXTH", 4) == 0) {
                    m_exth_header = exth_header;

                    // 解析EXTH记录
                    parseExthRecords(m_exth_header_offset + sizeof(ExthHeader),
                        exth_header.header_length - sizeof(ExthHeader),
                        exth_header.record_count);
                }
            }
        }

        return true;
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
        if (eof_record.data_offset + 4 <= m_file_data.size()) {
            const EofRecord* eof = reinterpret_cast<const EofRecord*>(
                m_file_data.data() + eof_record.data_offset);

            if (eof->byte0 == 0xE9 && eof->byte1 == 0x8E &&
                eof->byte2 == 0x0D && eof->byte3 == 0x0A) {
                std::cout << "Found EOF record (record " << last_index << ")" << std::endl;
            }
        }

        // 检查FCIS记录
        if (last_index >= 2) {
            const RecordInfo& fcis_record = m_record_info_list[last_index - 1];
            if (fcis_record.data_offset + 4 <= m_file_data.size()) {
                const char* identifier = reinterpret_cast<const char*>(
                    m_file_data.data() + fcis_record.data_offset);

                if (std::memcmp(identifier, "FCIS", 4) == 0) {
                    std::cout << "Found FCIS record (record " << last_index - 1 << ")" << std::endl;

                    const FcisRecord* fcis = reinterpret_cast<const FcisRecord*>(
                        m_file_data.data() + fcis_record.data_offset);
                    std::cout << "  FCIS Text Length: " << fcis->text_length << " Bytes" << std::endl;
                }
            }
        }

        // 检查FLIS记录
        if (last_index >= 3) {
            const RecordInfo& flis_record = m_record_info_list[last_index - 2];
            if (flis_record.data_offset + 4 <= m_file_data.size()) {
                const char* identifier = reinterpret_cast<const char*>(
                    m_file_data.data() + flis_record.data_offset);

                if (std::memcmp(identifier, "FLIS", 4) == 0) {
                    std::cout << "Found FLIS record (record " << last_index - 2 << ")" << std::endl;
                }
            }
        }

        return true;
    }

    // 字节序转换函数实现
    IndxHeader MobiBook::swapIndxHeader(const IndxHeader& header) {
        IndxHeader swapped = header;
        swapped.header_length = swapUint32(header.header_length);
        swapped.index_type = swapUint32(header.index_type);
        swapped.unknown_c = swapUint32(header.unknown_c);
        swapped.unknown_10 = swapUint32(header.unknown_10);
        swapped.idxt_offset = swapUint32(header.idxt_offset);
        swapped.index_record_count = swapUint32(header.index_record_count);
        swapped.index_encoding = swapUint32(header.index_encoding);
        swapped.index_language = swapUint32(header.index_language);
        swapped.total_index_count = swapUint32(header.total_index_count);
        swapped.ordt_offset = swapUint32(header.ordt_offset);
        swapped.ligt_offset = swapUint32(header.ligt_offset);
        swapped.unknown_30 = swapUint32(header.unknown_30);
        swapped.unknown_34 = swapUint32(header.unknown_34);
        return swapped;
    }

    TagxHeader MobiBook::swapTagxHeader(const TagxHeader& header) {
        TagxHeader swapped = header;
        swapped.header_length = swapUint32(header.header_length);
        swapped.control_byte_count = swapUint32(header.control_byte_count);
        return swapped;
    }





    // 辅助函数：获取索引类型名称
    std::string MobiBook::getIndexTypeName(uint32_t type) const {
        switch (type) {
        case 0: return "Normal Index";
        case 2: return "Inflections Index";
        default: return "Unknown";
        }
    }

    // 辅助函数：根据编码代码获取编码名称
    std::string MobiBook::getEncodingNameFromCode(uint32_t encoding) const {
        switch (encoding) {
        case 1252: return "CP1252 (WinLatin1)";
        case 65001: return "UTF-8";
        default: return "Unknown";
        }
    }



    // 读取可变宽度整数
    std::vector<uint8_t> MobiBook::readVariableWidthInteger(const uint8_t* data, uint32_t& offset, bool forward_encoded) const {
        std::vector<uint8_t> bytes;

        if (forward_encoded) {
            // 前向编码：只有LSB有bit 8设置
            while (true) {
                if (offset >= m_file_data.size()) break;
                uint8_t byte = data[offset++];
                bytes.push_back(byte);
                if (!(byte & VAR_INT_CONTINUE_MASK_FORWARD)) {
                    break;
                }
            }
        }
        else {
            // 后向编码：只有MSB有bit 8设置
            // 需要先读取所有字节，然后反转
            while (true) {
                if (offset >= m_file_data.size()) break;
                uint8_t byte = data[offset++];
                bytes.push_back(byte);
                if (!(byte & VAR_INT_CONTINUE_MASK_BACKWARD)) {
                    break;
                }
            }
            // 反转字节顺序用于解码
            std::reverse(bytes.begin(), bytes.end());
        }

        return bytes;
    }

    // 解码可变宽度整数
    uint32_t MobiBook::decodeVariableWidthInteger(const std::vector<uint8_t>& bytes) const {
        uint32_t result = 0;

        for (uint8_t byte : bytes) {
            result = (result << 7) | (byte & 0x7F);
        }

        return result;
    }




    // ==================== Book接口实现 ====================

    std::vector<uint8_t> MobiBook::get_binary(std::string base_url, std::string url)
    {
        std::vector<uint8_t> result;
        // 移除可能的查询参数和片段标识符
        size_t query_pos = url.find('?');
        if (query_pos != std::string::npos) {
            url = url.substr(0, query_pos);
        }

        size_t fragment_pos = url.find('#');
        if (fragment_pos != std::string::npos) {
            url = url.substr(0, fragment_pos);
        }

        // 1. 处理内容文件（HTML/XHTML章节）
        if (url.find("content_") == 0 && url.find(".html") != std::string::npos) {
            // 提取索引号，如 content_0.html -> 0
            std::string index_str = url.substr(8); // "content_"
            index_str = index_str.substr(0, index_str.find('.'));

            try {
                size_t index = std::stoul(index_str);

                if (index < content_records_.size()) {
                    // 获取对应的HTML内容
                    return content_records_[index];
                }

            }
            catch (const std::exception& e) {
                // 解析失败，返回错误页面
                std::cerr << "Error parsing content index: " << e.what() << std::endl;
                return {};
            }
        }
        // 2. 处理图像文件
        else if (url.find("image_") == 0) {
            // 提取索引号，如 image_0.jpg -> 0
            std::string index_str = url.substr(6); // "image_"

            // 移除扩展名
            size_t dot_pos = index_str.find('.');
            if (dot_pos != std::string::npos) {
                index_str = index_str.substr(0, dot_pos);
            }

            try {
                size_t index = std::stoul(index_str);

                if (index < image_records_.size()) {
                    return image_records_[index];
                }

            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing image index: " << e.what() << std::endl;
                return {};
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
        if (spine_id >= 0 && spine_id < static_cast<int>(m_ocf_package.toc.size())) {
            return m_ocf_package.toc[spine_id].label;
        }
        return "Chapter " + std::to_string(spine_id + 1);
    }

    void MobiBook::clear() {
        book_path_.clear();
        current_dir_.clear();

        m_file_data.clear();
        m_record_info_list.clear();
        m_exth_records.clear();
        content_records_.clear();
        image_records_.clear();

        m_ocf_package = OCFPackage();


        m_palm_doc_header_offset = 0;
        m_mobi_header_offset = 0;
        m_exth_header_offset = 0;
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

    // 交换64位整数的字节序
    uint64_t MobiBook::swapUint64(uint64_t value) {
        return ((value & 0x00000000000000FFULL) << 56) |
            ((value & 0x000000000000FF00ULL) << 40) |
            ((value & 0x0000000000FF0000ULL) << 24) |
            ((value & 0x00000000FF000000ULL) << 8) |
            ((value & 0x000000FF00000000ULL) >> 8) |
            ((value & 0x0000FF0000000000ULL) >> 24) |
            ((value & 0x00FF000000000000ULL) >> 40) |
            ((value & 0xFF00000000000000ULL) >> 56);
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

        // 条件字段
        if (header.header_length >= 228) {
            swapped.unknown_b8 = swapUint64(header.unknown_b8);
        }

        if (header.header_length >= 244) {
            swapped.first_content_record = swapUint16(header.first_content_record);
            swapped.last_content_record = swapUint16(header.last_content_record);
            swapped.unknown_c4 = swapUint32(header.unknown_c4);
            swapped.fcis_record = swapUint32(header.fcis_record);
            swapped.fcis_count = swapUint32(header.fcis_count);
            swapped.flis_record = swapUint32(header.flis_record);
            swapped.flis_count = swapUint32(header.flis_count);
            swapped.unknown_d8 = swapUint64(header.unknown_d8);
            swapped.unknown_e0 = swapUint32(header.unknown_e0);
            swapped.first_compilation_section = swapUint32(header.first_compilation_section);
            swapped.num_compilation_sections = swapUint32(header.num_compilation_sections);
            swapped.unknown_ec = swapUint32(header.unknown_ec);
            swapped.extra_record_data_flags = swapUint32(header.extra_record_data_flags);
        }

        if (header.header_length >= 248) {
            swapped.indx_record_offset = swapUint32(header.indx_record_offset);
            swapped.unknown_f8 = swapUint32(header.unknown_f8);
            swapped.unknown_fc = swapUint32(header.unknown_fc);
        }

        if (header.header_length >= 256) {
            swapped.unknown_100 = swapUint32(header.unknown_100);
            swapped.unknown_104 = swapUint32(header.unknown_104);
            swapped.unknown_108 = swapUint32(header.unknown_108);
            swapped.unknown_10b = swapUint32(header.unknown_10b);
        }

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



    RecordInfo MobiBook::swapRecordInfo(const RecordInfo& index) {
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
    std::string MobiBook::convertPalmTime(uint32_t palm_time) const {
        if (palm_time == 0) return "Not set";

        const uint32_t PALM_TO_UNIX_OFFSET = 2082844800u;
        time_t unix_time = (time_t)palm_time - PALM_TO_UNIX_OFFSET;

        if (unix_time < 0) return "Invalid time";

        // 使用安全的 localtime_s 替代 localtime
        struct tm timeinfo;
        errno_t err = localtime_s(&timeinfo, &unix_time);

        if (err != 0) {
            return "Time conversion error";
        }

        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return std::string(buffer);
    }
    void MobiBook::printPDBHeader() const {
        if (!m_file_data.size()) {
            std::cout << "No PDB data loaded." << std::endl;
            return;
        }

        constexpr int label_width = 25;
        constexpr int value_width = 30;

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PDB HEADER INFORMATION" << std::endl;
        std::cout << std::string(60, '=') << std::endl << std::endl;

        // 1. 基本信息
        std::cout << "BASIC INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        std::string db_name(m_pdb_header.name, sizeof(m_pdb_header.name));
        size_t null_pos = db_name.find('\0');
        if (null_pos != std::string::npos) {
            db_name = db_name.substr(0, null_pos);
        }

        std::string type_str(m_pdb_header.type, sizeof(m_pdb_header.type));
        null_pos = type_str.find('\0');
        if (null_pos != std::string::npos) {
            type_str = type_str.substr(0, null_pos);
        }

        std::string creator_str(m_pdb_header.creator, sizeof(m_pdb_header.creator));
        null_pos = creator_str.find('\0');
        if (null_pos != std::string::npos) {
            creator_str = creator_str.substr(0, null_pos);
        }

        printField("Database Name:", db_name, label_width);
        printField("Type:", type_str + " " + getTypeDescription(type_str), label_width);
        printField("Creator:", creator_str + " " + getCreatorDescription(creator_str), label_width);
        printField("Version:", std::to_string(m_pdb_header.version), label_width);
        printField("Modification Number:", std::to_string(m_pdb_header.modification_number), label_width);

        std::cout << std::endl;

        // 2. 属性信息
        std::cout << "ATTRIBUTES" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        std::cout << std::left << std::setw(label_width) << "Attributes Value:"
            << "0x" << std::hex << std::setw(4) << std::setfill('0')
            << m_pdb_header.attributes << std::dec << std::setfill(' ') << std::endl;

        std::cout << std::left << std::setw(label_width) << "Attribute Flags:";

        std::vector<std::string> flags;
        if (m_pdb_header.attributes & 0x0002) flags.push_back("Read-Only");
        if (m_pdb_header.attributes & 0x0004) flags.push_back("Dirty AppInfo");
        if (m_pdb_header.attributes & 0x0008) flags.push_back("Backup");
        if (m_pdb_header.attributes & 0x0010) flags.push_back("OK to Install Newer");
        if (m_pdb_header.attributes & 0x0020) flags.push_back("Force Reset");
        if (m_pdb_header.attributes & 0x0040) flags.push_back("No Beam");
        if (m_pdb_header.attributes & 0x0080) flags.push_back("Stream");
        if (m_pdb_header.attributes & 0x0100) flags.push_back("Hidden");
        if (m_pdb_header.attributes & 0x0200) flags.push_back("Launchable Data");
        if (m_pdb_header.attributes & 0x0400) flags.push_back("Copy Protected");
        if (m_pdb_header.attributes & 0x0800) flags.push_back("OK to Install Older");

        if (flags.empty()) {
            std::cout << "[None]";
        }
        else {
            for (size_t i = 0; i < flags.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << flags[i];
            }
        }
        std::cout << std::endl << std::endl;

        // 3. 时间信息
        std::cout << "TIMESTAMP INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Creation Time:",
            convertPalmTime(m_pdb_header.creation_time) +
            " (Palm: " + std::to_string(m_pdb_header.creation_time) + ")",
            label_width);

        printField("Modification Time:",
            convertPalmTime(m_pdb_header.modification_time) +
            " (Palm: " + std::to_string(m_pdb_header.modification_time) + ")",
            label_width);

        printField("Last Backup Time:",
            convertPalmTime(m_pdb_header.backup_time) +
            " (Palm: " + std::to_string(m_pdb_header.backup_time) + ")",
            label_width);

        std::cout << std::endl;

        // 4. 偏移信息
        std::cout << "OFFSET INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("App Info Offset:", m_pdb_header.app_info_offset, label_width);
        if (m_pdb_header.app_info_offset == 0) {
            printField("", "[No Application Info]", label_width);
        }
        else if (m_pdb_header.app_info_offset < m_file_data.size()) {
            printField("", "[√ Application Info present]", label_width);
        }
        else {
            printField("", "[× Application Info offset out of range]", label_width);
        }

        printHexDecField("Sort Info Offset:", m_pdb_header.sort_info_offset, label_width);
        if (m_pdb_header.sort_info_offset == 0) {
            printField("", "[No Sort Info]", label_width);
        }
        else if (m_pdb_header.sort_info_offset < m_file_data.size()) {
            printField("", "[√ Sort Info present]", label_width);
        }
        else {
            printField("", "[× Sort Info offset out of range]", label_width);
        }

        std::cout << std::endl;

        // 5. ID信息
        std::cout << "IDENTIFIERS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("Unique ID Seed:", m_pdb_header.unique_id_seed, label_width);
        printHexDecField("Next Record List ID:", m_pdb_header.next_record_list_id, label_width);

        if (m_pdb_header.next_record_list_id != 0) {
            std::cout << std::left << std::setw(label_width) << "Warning:"
                << "Expected 0, got " << m_pdb_header.next_record_list_id << std::endl;
        }

        std::cout << std::endl;

        // 6. 记录和文件信息
        std::cout << "FILE & RECORD INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Number of Records:", std::to_string(m_pdb_header.num_records), label_width);

        // 文件大小
        std::string file_size_str = formatFileSize(m_file_data.size());
        printField("File Size:", file_size_str, label_width);

        // 验证文件大小是否足够
        size_t header_size = 78; // PDB头部大小
        size_t record_info_table_size = m_pdb_header.num_records * 8; // 每个记录信息8字节
        size_t min_required_size = header_size + record_info_table_size;

        printField("Minimum Required:", formatFileSize(min_required_size), label_width);

        if (m_file_data.size() < min_required_size) {
            std::cout << std::left << std::setw(label_width) << "STATUS:"
                << "× INVALID - File truncated!" << std::endl;
            std::cout << std::left << std::setw(label_width) << "Missing Bytes:"
                << formatFileSize(min_required_size - m_file_data.size()) << std::endl;
        }
        else {
            std::cout << std::left << std::setw(label_width) << "STATUS:"
                << "√ VALID" << std::endl;
            std::cout << std::left << std::setw(label_width) << "Header Overhead:"
                << formatFileSize(header_size) << std::endl;
            std::cout << std::left << std::setw(label_width) << "Record Info Table:"
                << formatFileSize(record_info_table_size) << std::endl;
            std::cout << std::left << std::setw(label_width) << "Data Space:"
                << formatFileSize(m_file_data.size() - min_required_size) << std::endl;
        }

        // 如果有AppInfo和SortInfo，计算实际数据偏移
        uint32_t first_data_offset = min_required_size;
        if (m_pdb_header.app_info_offset > min_required_size &&
            m_pdb_header.app_info_offset < m_file_data.size()) {
            first_data_offset = std::min(first_data_offset, m_pdb_header.app_info_offset);
        }
        if (m_pdb_header.sort_info_offset > min_required_size &&
            m_pdb_header.sort_info_offset < m_file_data.size()) {
            first_data_offset = std::min(first_data_offset, m_pdb_header.sort_info_offset);
        }

        if (first_data_offset > min_required_size) {
            std::cout << std::left << std::setw(label_width) << "Padding After Header:"
                << formatFileSize(first_data_offset - min_required_size) << std::endl;
        }

        std::cout << std::endl << std::string(60, '=') << std::endl << std::endl;
    }
    void MobiBook::printRecordInfoStat() const
    {
        if (m_record_info_list.empty()) {
            std::cout << "No record information parsed. Call parseRecordIndices() first." << std::endl;
            return;
        }
        // 添加简要统计
        std::cout << std::endl << "=== Record Statistics ===" << std::endl;
        std::cout << "Total records: " << m_record_info_list.size() << std::endl;
        std::cout << std::endl;
        // 计算有效记录数量（In Use标志）
        int in_use_count = 0;
        int secret_count = 0;
        int dirty_count = 0;
        int delete_on_hotsync_count = 0;

        for (const auto& record : m_record_info_list) {
            if (record.attributes & 0x20) in_use_count++;
            if (record.attributes & 0x10) secret_count++;
            if (record.attributes & 0x40) dirty_count++;
            if (record.attributes & 0x80) delete_on_hotsync_count++;
        }

        std::cout << "Records 'In Use': " << in_use_count << std::endl;
        std::cout << "Secret records: " << secret_count << std::endl;
        std::cout << "Dirty records: " << dirty_count << std::endl;
        std::cout << "Delete on HotSync: " << delete_on_hotsync_count << std::endl;

        // 计算总数据大小
        uint64_t total_data_size = 0;
        for (size_t i = 0; i < m_record_info_list.size(); i++) {
            const RecordInfo& record = m_record_info_list[i];
            uint32_t record_size = 0;

            if (i + 1 < m_record_info_list.size()) {
                record_size = m_record_info_list[i + 1].data_offset - record.data_offset;
            }
            else if (record.data_offset < m_file_data.size()) {
                record_size = static_cast<uint32_t>(m_file_data.size() - record.data_offset);
            }

            total_data_size += record_size;
        }

        std::cout << "Total data size: " << total_data_size << " bytes" << std::endl;
        std::cout << "Average record size: "
            << (m_record_info_list.size() > 0 ? total_data_size / m_record_info_list.size() : 0)
            << " bytes" << std::endl;
    }
    void MobiBook::printRecordInfoList() const {
        if (m_record_info_list.empty()) {
            std::cout << "No record information parsed. Call parseRecordIndices() first." << std::endl;
            return;
        }

        std::cout << "=== Record Information List ===" << std::endl;
        std::cout << "Total records: " << m_record_info_list.size() << std::endl;
        std::cout << std::endl;

        // 显示表格标题
        std::cout << std::left << std::setw(8) << "Index"
            << std::setw(12) << "Offset(hex)"
            << std::setw(10) << "Offset(dec)"
            << std::setw(12) << "Attributes"
            << std::setw(12) << "Unique ID"
            << std::setw(15) << "Record Size"
            << std::endl;

        std::cout << std::string(65, '-') << std::endl;

        for (size_t i = 0; i < m_record_info_list.size(); i++) {
            const RecordInfo& record = m_record_info_list[i];

            // 计算记录大小（下一个记录的偏移减当前偏移）
            uint32_t record_size = 0;
            if (i + 1 < m_record_info_list.size()) {
                record_size = m_record_info_list[i + 1].data_offset - record.data_offset;
            }
            else {
                // 最后一个记录：文件大小减去偏移
                if (record.data_offset < m_file_data.size()) {
                    record_size = static_cast<uint32_t>(m_file_data.size() - record.data_offset);
                }
            }

            // 构建唯一ID（3字节）
            uint32_t unique_id = (static_cast<uint32_t>(record.unique_id[0]) << 16) |
                (static_cast<uint32_t>(record.unique_id[1]) << 8) |
                static_cast<uint32_t>(record.unique_id[2]);

            // 分析属性
            std::string attribute_str;
            uint8_t category = record.attributes & 0x0F; // 低4位是类别

            // 添加属性标记
            if (record.attributes & 0x10) attribute_str += "S"; // Secret
            if (record.attributes & 0x20) attribute_str += "U"; // In Use
            if (record.attributes & 0x40) attribute_str += "D"; // Dirty
            if (record.attributes & 0x80) attribute_str += "X"; // Delete on HotSync

            // 如果没有任何标记，显示类别
            if (attribute_str.empty()) {
                attribute_str = std::to_string(static_cast<int>(category));
            }
            else {
                // 包含类别信息
                attribute_str = std::to_string(static_cast<int>(category)) + ":" + attribute_str;
            }

            // 输出记录信息
            std::cout << std::left << std::setw(8) << i
                << "0x" << std::hex << std::setw(10) << std::right << record.data_offset
                << std::setw(12) << std::dec << record.data_offset
                << std::setw(12) << attribute_str
                << "0x" << std::hex << std::setw(10) << std::right << unique_id << std::dec
                << std::setw(15) << record_size
                << std::endl;

            // 添加警告信息（如果需要）
            if (record.data_offset >= m_file_data.size()) {
                std::cout << "  WARNING: Offset beyond file size!" << std::endl;
            }
            else if (record.data_offset + record_size > m_file_data.size()) {
                std::cout << "  WARNING: Record extends beyond file end!" << std::endl;
            }
        }

        // 添加简要统计
        std::cout << std::endl << "=== Record Statistics ===" << std::endl;

        // 计算有效记录数量（In Use标志）
        int in_use_count = 0;
        int secret_count = 0;
        int dirty_count = 0;
        int delete_on_hotsync_count = 0;

        for (const auto& record : m_record_info_list) {
            if (record.attributes & 0x20) in_use_count++;
            if (record.attributes & 0x10) secret_count++;
            if (record.attributes & 0x40) dirty_count++;
            if (record.attributes & 0x80) delete_on_hotsync_count++;
        }

        std::cout << "Records 'In Use': " << in_use_count << std::endl;
        std::cout << "Secret records: " << secret_count << std::endl;
        std::cout << "Dirty records: " << dirty_count << std::endl;
        std::cout << "Delete on HotSync: " << delete_on_hotsync_count << std::endl;

        // 计算总数据大小
        uint64_t total_data_size = 0;
        for (size_t i = 0; i < m_record_info_list.size(); i++) {
            const RecordInfo& record = m_record_info_list[i];
            uint32_t record_size = 0;

            if (i + 1 < m_record_info_list.size()) {
                record_size = m_record_info_list[i + 1].data_offset - record.data_offset;
            }
            else if (record.data_offset < m_file_data.size()) {
                record_size = static_cast<uint32_t>(m_file_data.size() - record.data_offset);
            }

            total_data_size += record_size;
        }

        std::cout << "Total data size: " << total_data_size << " bytes" << std::endl;
        std::cout << "Average record size: "
            << (m_record_info_list.size() > 0 ? total_data_size / m_record_info_list.size() : 0)
            << " bytes" << std::endl;
    }

    // 打印PalmDOC头信息
    void MobiBook::printPalmDocHeader() const {
        constexpr int label_width = 25;
        constexpr int value_width = 30;

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "PALMDOC HEADER INFORMATION" << std::endl;
        std::cout << std::string(60, '=') << std::endl << std::endl;

        std::cout << "HEADER STRUCTURE" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("Header Offset:", m_palm_doc_header_offset, label_width);
        printHexDecField("Header Length:", 16, label_width); // PalmDoc头部固定16字节

        std::cout << std::endl;

        // 压缩信息
        std::cout << "COMPRESSION SETTINGS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Compression Type:",
            std::to_string(m_palm_doc_header.compression) +
            " - " + getCompressionName(m_palm_doc_header.compression),
            label_width);

        printField("Compression Status:",
            m_palm_doc_header.compression == COMPRESSION_NONE ?
            "None (Uncompressed)" : "Compressed",
            label_width);

        std::cout << std::endl;

        // 文本信息
        std::cout << "TEXT INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Text Length:", formatFileSize(m_palm_doc_header.text_length), label_width);

        // 计算压缩率（如果有）
        size_t uncompressed_size = m_palm_doc_header.text_length;
        size_t compressed_size = 0;

        // 估算压缩后大小
        for (size_t i = 0; i < m_record_info_list.size(); i++) {
            const RecordInfo& record = m_record_info_list[i];
            uint32_t record_size = 0;

            if (i + 1 < m_record_info_list.size()) {
                record_size = m_record_info_list[i + 1].data_offset - record.data_offset;
            }
            else if (record.data_offset < m_file_data.size()) {
                record_size = static_cast<uint32_t>(m_file_data.size() - record.data_offset);
            }
            compressed_size += record_size;
        }

        if (uncompressed_size > 0 && compressed_size > 0) {
            double compression_ratio = static_cast<double>(compressed_size) / uncompressed_size;
            double compression_percent = (1.0 - compression_ratio) * 100;

            std::ostringstream oss;
            oss << formatFileSize(compressed_size) << " (";
            oss << std::fixed << std::setprecision(1) << compression_percent << "% smaller)";

            printField("Compressed Size:", oss.str(), label_width);
        }

        std::cout << std::endl;

        // 记录信息
        std::cout << "RECORD INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Record Count:", std::to_string(m_palm_doc_header.record_count), label_width);

        // 验证记录数
        if (m_record_info_list.size() > 0) {
            bool matches = static_cast<uint32_t>(m_record_info_list.size()) == m_palm_doc_header.record_count;
            std::string status = matches ? "√ MATCH" : "× MISMATCH";
            printField("Actual Records:", std::to_string(m_record_info_list.size()) + " " + status, label_width);
        }

        printField("Record Size:", formatFileSize(m_palm_doc_header.record_size), label_width);

        std::cout << std::endl;

        // 加密和杂项
        std::cout << "SECURITY & MISC" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Encryption Type:",
            std::to_string(m_palm_doc_header.encryption_type) +
            " - " + getEncryptionName(m_palm_doc_header.encryption_type),
            label_width);

        std::cout << std::endl;

        // 验证信息
        std::cout << "VALIDATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        bool is_valid = validatePalmDocHeader();
        printField("Header Integrity:", is_valid ? "√ VALID" : "× INVALID", label_width);

        if (m_palm_doc_header.record_count == 0) {
            printField("Warning:", "Record count is 0", label_width);
        }

        if (m_palm_doc_header.text_length == 0) {
            printField("Warning:", "Text length is 0", label_width);
        }

        std::cout << std::endl << std::string(60, '=') << std::endl << std::endl;
    }

    void MobiBook::printMobiHeader() const {
        constexpr int label_width = 25;
        constexpr int value_width = 40;

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "MOBI HEADER INFORMATION" << std::endl;
        std::cout << std::string(60, '=') << std::endl << std::endl;

        std::cout << "BASIC INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("Header Offset:", m_mobi_header_offset, label_width);
        printHexDecField("Header Length:", m_mobi_header.header_length, label_width);
        printHexDecField("Mobi Type:", m_mobi_header.mobi_type, label_width);

        printField("Mobi Type Name:", getMobiTypeName(m_mobi_header.mobi_type), label_width);

        printField("Text Encoding:",
            std::to_string(m_mobi_header.text_encoding) +
            " - " + getEncodingName(m_mobi_header.text_encoding),
            label_width);

        std::cout << std::endl;

        // 标识信息
        std::cout << "IDENTIFIERS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("Unique ID:", m_mobi_header.unique_id, label_width);
        printHexDecField("File Version:", m_mobi_header.file_version, label_width);


        printField("Full Name:", getFullName(), label_width);


        // 本地化信息
        std::cout << std::endl;
        std::cout << "LOCALIZATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        std::string locale_info = "0x" +
            std::to_string(m_mobi_header.locale) + " - " +
            getLocaleName(m_mobi_header.locale);
        printField("Locale:", locale_info, label_width);

        std::cout << std::endl;

        // 文本信息
        std::cout << "TEXT INFORMATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printField("Text Length:", formatFileSize(m_palm_doc_header.text_length), label_width);
        printHexDecField("First Content Record:", m_mobi_header.first_content_record, label_width);
        printHexDecField("First Image Record:", m_mobi_header.first_image_index, label_width);
        printHexDecField("First Non-Text Record:", m_mobi_header.first_non_book_index, label_width);

        std::cout << std::endl;

        // 链接和目录信息
        std::cout << "LINKS & NAVIGATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("EXTH Flags:", m_mobi_header.exth_flags, label_width);

        bool has_exth = (m_mobi_header.exth_flags & 0x40) != 0;
        printField("Has EXTH Header:", has_exth ? "√ YES" : "× NO", label_width);

        printHexDecField("DRM Offset:", m_mobi_header.drm_offset, label_width);
        printHexDecField("DRM Count:", m_mobi_header.drm_count, label_width);
        printHexDecField("DRM Size:", m_mobi_header.drm_size, label_width);

        if (m_mobi_header.drm_offset > 0 || m_mobi_header.drm_count > 0 || m_mobi_header.drm_size > 0) {
            printField("DRM Protection:", "⚠️ Protected", label_width);
        }

        std::cout << std::endl;

        // 杂项信息
        std::cout << "MISCELLANEOUS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("Extra Record Data Flags:", m_mobi_header.extra_record_data_flags, label_width);
        printHexDecField("INDX Offset:", m_mobi_header.indx_record_offset, label_width);
        printHexDecField("Last Content Record:", m_mobi_header.last_content_record, label_width);
        printHexDecField("FCIS Record:", m_mobi_header.fcis_record, label_width);
        printHexDecField("FLIS Record:", m_mobi_header.flis_record, label_width);

        std::cout << std::endl;

        // 验证信息
        std::cout << "VALIDATION" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        bool has_required_fields = validateMobiHeader();
        printField("Header Validation:", has_required_fields ? "√ VALID" : "× INVALID", label_width);

        // 检查版本兼容性
        if (m_mobi_header.file_version >= 8) {
            printField("File Version:", "KF8/New Format (Version " +
                std::to_string(m_mobi_header.file_version) + ")", label_width);
        }
        else if (m_mobi_header.file_version >= 6) {
            printField("File Version:", "Old Format (Version " +
                std::to_string(m_mobi_header.file_version) + ")", label_width);
        }

        std::cout << std::endl << std::string(60, '=') << std::endl << std::endl;
    }

    void MobiBook::printExthHeader() const {
        constexpr int label_width = 25;

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "EXTH HEADER INFORMATION" << std::endl;
        std::cout << std::string(60, '=') << std::endl << std::endl;

        if (m_exth_records.empty()) {
            std::cout << "No EXTH records found." << std::endl;
            return;
        }

        std::cout << "HEADER OVERVIEW" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        printHexDecField("Header Offset:", m_exth_header_offset, label_width);
        printHexDecField("Header Length:", m_exth_header.header_length, label_width);
        printField("Record Count:", std::to_string(m_exth_header.record_count), label_width);

        // 计算总数据大小
        size_t total_data_size = 0;
        for (const auto& pair : m_exth_records) {
            total_data_size += pair.second.size();
        }

        printField("Total Data Size:", formatFileSize(total_data_size), label_width);

        std::cout << std::endl;

        // 打印EXTH记录
        printExthRecords();

        std::cout << std::endl << std::string(60, '=') << std::endl << std::endl;
    }

    void MobiBook::printExthRecords() const {
        if (m_exth_records.empty()) return;

        std::cout << "EXTH RECORDS DETAIL" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        constexpr int type_width = 15;
        constexpr int length_width = 20;
        constexpr int name_width = 35;

        std::cout << std::left
            << std::setw(type_width) << "Type"
            << std::setw(name_width) << "Name"
            << std::setw(length_width) << "Length"
            << "Value" << std::endl;

        std::cout << std::string(70, '-') << std::endl;

        for (const auto& pair : m_exth_records) {
            uint32_t type = pair.first;
            const std::string& data = pair.second;

            std::string type_name = getExthTypeName(type);
            std::string formatted_value = formatExthValue(type, data);

            std::cout << std::left
                << std::setw(type_width) << type
                << std::setw(name_width) << type_name
                << std::setw(length_width) << formatFileSize(data.size());

            // 截断长文本
            if (formatted_value.length() > 50) {
                std::cout << formatted_value.substr(0, 47) << "...";
            }
            else {
                std::cout << formatted_value;
            }
            std::cout << std::endl;
        }
    }



    // 统一的打印函数，可以调用所有打印函数
    void MobiBook::printRecord0() const {
        // 先检查是否有数据
        if (m_record_info_list.empty()) {
            std::cout << "No record data available. Call parseRecordIndices() first." << std::endl;
            return;
        }

        // 打印PalmDOC头（如果有）
        if (m_palm_doc_header.compression != 0) {
            printPalmDocHeader();
        }

        // 检查是否有MOBI头
        const RecordInfo& record0 = m_record_info_list[0];
        uint32_t mobi_header_offset = record0.data_offset + sizeof(PalmDocHeader);

        if (mobi_header_offset + 4 <= m_file_data.size()) {
            const char* mobi_identifier = reinterpret_cast<const char*>(
                m_file_data.data() + mobi_header_offset);

            if (std::memcmp(mobi_identifier, "MOBI", 4) == 0) {
                printMobiHeader();

                // 检查是否有EXTH头
                if (m_mobi_header.exth_flags & 0x40) {
                    printExthHeader();
                }
            }
            else {
                std::cout << "\nNote: No MOBI header found (this may be a simple PalmDOC file)" << std::endl;
            }
        }
    }

    std::string MobiBook::getTypeDescription(const std::string& type) const {
        if (type == "BOOK") return "(Generic Book)";
        if (type == "TEXt") return "(Plain Text)";
        if (type == "MOBI") return "(Mobipocket)";
        if (type == "TEXtREAd") return "(TealDoc)";
        if (type == "DB99") return "(iSilo)";
        if (type == "DATA") return "(Database)";
        if (type == "PNRd") return "(Palm Reader)";
        if (type == "vIMG") return "(Images)";
        if (type == "zTXT") return "(Palm ZTXT)";
        return "(Unknown Type)";
    }

    std::string MobiBook::getCreatorDescription(const std::string& creator) const {
        if (creator == "MOBI") return "(Mobipocket)";
        if (creator == "REAd") return "(Palm Reader)";
        if (creator == "TlDc") return "(TealDoc)";
        if (creator == "SilX") return "(iSilo)";
        if (creator == "appl") return "(Palm Application)";
        if (creator == "PmDB") return "(Palm Database)";
        if (creator == "PNRd") return "(Palm Reader)";
        if (creator == "GmEP") return "(Palm Game)";
        return "(Unknown Creator)";
    }

    void MobiBook::printField(const std::string& label, const std::string& value, int label_width) const {
        std::cout << std::left << std::setw(label_width) << label << value << std::endl;
    }

    void MobiBook::printHexDecField(const std::string& label, uint32_t value, int label_width) const {
        std::cout << std::left << std::setw(label_width) << label
            << "0x" << std::hex << std::setw(8) << std::setfill('0') << value
            << " (" << std::dec << value << ")" << std::setfill(' ') << std::endl;
    }

    std::string MobiBook::formatFileSize(size_t bytes) const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);

        if (bytes < 1024) {
            oss << bytes << " B";
        }
        else if (bytes < 1024 * 1024) {
            oss << bytes / 1024.0 << " KB";
        }
        else if (bytes < 1024 * 1024 * 1024) {
            oss << bytes / (1024.0 * 1024.0) << " MB";
        }
        else {
            oss << bytes / (1024.0 * 1024.0 * 1024.0) << " GB";
        }

        oss << " (" << bytes << " bytes)";
        return oss.str();
    }

    // 辅助函数实现
    std::string MobiBook::getCompressionName(uint16_t compression) const {
        switch (compression) {
        case COMPRESSION_NONE: return "No Compression";
        case COMPRESSION_PALMDOC: return "PalmDOC/LZ77";
        case COMPRESSION_HUFFCDIC: return "Huffman/CDIC";
        default: return "Unknown (" + std::to_string(compression) + ")";
        }
    }

    std::string MobiBook::getEncryptionName(uint16_t  encryption) const {
        switch (encryption) {
        case ENCRYPTION_NONE: return "No Encryption";
        case ENCRYPTION_OLD_MOBI: return "Old Mobipocket";
        case ENCRYPTION_MOBI: return "Mobipocket";
        default: return "Unknown (" + std::to_string(encryption) + ")";
        }
    }

    std::string MobiBook::getMobiTypeName(uint32_t  mobi_type) const {
        switch (mobi_type) {
        case MOBI_TYPE_BOOK: return "Mobipocket Book";
        case MOBI_TYPE_PALMDOC: return "PalmDoc Book";
        case MOBI_TYPE_AUDIO: return "Audio Book";
        case MOBI_TYPE_KINDLEGEN1: return "KindleGen 1.x";
        case MOBI_TYPE_KF8: return "KF8 (Kindle Format 8)";
        case MOBI_TYPE_NEWS: return "News";
        case MOBI_TYPE_NEWS_FEED: return "News Feed";
        case MOBI_TYPE_NEWS_MAGAZINE: return "News Magazine";
        case MOBI_TYPE_PICS: return "PICS";
        case MOBI_TYPE_WORD: return "WORD";
        case MOBI_TYPE_XLS: return "XLS";
        case MOBI_TYPE_PPT: return "PPT";
        case MOBI_TYPE_TEXT: return "TEXT";
        case MOBI_TYPE_HTML: return "HTML";
        default: return "Unknown (" + std::to_string(mobi_type) + ")";
        }
    }

    std::string MobiBook::getEncodingName(uint32_t  encoding) const
    {
        switch (encoding) {
            // 拉丁语系编码
        case ENCODING_LATIN1:      return "ISO-8859-1 (Latin-1)";
        case ENCODING_CP1250:      return "Windows-1250 (Central European)";
        case ENCODING_CP1252:      return "Windows-1252 (Western European)";
        case ENCODING_CP1254:      return "Windows-1254 (Turkish)";
        case ENCODING_CP1257:      return "Windows-1257 (Baltic)";
        case ENCODING_CP1258:      return "Windows-1258 (Vietnamese)";

            // 西里尔语系编码
        case ENCODING_CP1251:      return "Windows-1251 (Cyrillic)";
        case ENCODING_ISO_8859_5:  return "ISO-8859-5 (Cyrillic)";
        case ENCODING_MAC_CYRILLIC:return "Mac Cyrillic";

            // 希腊语编码
        case ENCODING_CP1253:      return "Windows-1253 (Greek)";
        case ENCODING_ISO_8859_7:  return "ISO-8859-7 (Greek)";
        case ENCODING_MAC_GREEK:   return "Mac Greek";

            // 中东语言编码
        case ENCODING_CP1255:      return "Windows-1255 (Hebrew)";
        case ENCODING_CP1256:      return "Windows-1256 (Arabic)";
        case ENCODING_ISO_8859_6:  return "ISO-8859-6 (Arabic)";
        case ENCODING_ISO_8859_8:  return "ISO-8859-8 (Hebrew)";
        case ENCODING_MAC_ARABIC:  return "Mac Arabic";
        case ENCODING_MAC_HEBREW:  return "Mac Hebrew";

            // 日语编码
        case ENCODING_JIS:         return "JIS (Japanese)";
        case ENCODING_SJIS:        return "Shift-JIS (Japanese)";
        case ENCODING_MAC_JAPANESE:return "Mac Japanese";

            // 中文编码
        case ENCODING_GB2312:      return "GB2312 (Simplified Chinese)";
        case ENCODING_BIG5:        return "Big5 (Traditional Chinese)";
        case ENCODING_MAC_TRADCHINESE: return "Mac Traditional Chinese";

            // 韩语编码
        case ENCODING_EUC_KR:      return "EUC-KR (Korean)";
        case ENCODING_MAC_KOREAN:  return "Mac Korean";

            // Unicode 编码
        case ENCODING_UTF8:        return "UTF-8 (Unicode)";
        case ENCODING_UTF16:       return "UTF-16";
        case ENCODING_UCS2:        return "UCS-2";
        case ENCODING_UTF16BE:     return "UTF-16 Big Endian";

            // 其他 ISO 编码
        case ENCODING_ISO_8859_2:  return "ISO-8859-2 (Latin-2)";
        case ENCODING_ISO_8859_3:  return "ISO-8859-3 (Latin-3)";
        case ENCODING_ISO_8859_4:  return "ISO-8859-4 (Latin-4)";
        case ENCODING_ISO_8859_9:  return "ISO-8859-9 (Latin-5)";
        case ENCODING_ISO_8859_10: return "ISO-8859-10 (Latin-6)";
        case ENCODING_ISO_8859_11: return "ISO-8859-11 (Thai)";
        case ENCODING_ISO_8859_13: return "ISO-8859-13 (Latin-7)";
        case ENCODING_ISO_8859_14: return "ISO-8859-14 (Latin-8)";
        case ENCODING_ISO_8859_15: return "ISO-8859-15 (Latin-9)";
        case ENCODING_ISO_8859_16: return "ISO-8859-16 (Latin-10)";

            // Mac 编码
        case ENCODING_MAC_ROMAN:   return "Mac Roman";
        case ENCODING_MAC_DEVANAGARI: return "Mac Devanagari";
        case ENCODING_MAC_GURMUKHI:   return "Mac Gurmukhi";

            // 默认情况
        default:
            // 检查是否在 10000-20000 的 Mac 编码范围内
            if (encoding >= 10000 && encoding <= 20000) {
                return "Mac Encoding (" + std::to_string(encoding) + ")";
            }
            // 检查是否在 1250-1258 的 Windows 编码范围内
            else if (encoding >= 1250 && encoding <= 1258) {
                return "Windows-" + std::to_string(encoding);
            }
            // 检查是否在 65000-66000 的 UTF 范围
            else if (encoding >= 65000 && encoding <= 66000) {
                return "UTF Encoding (" + std::to_string(encoding) + ")";
            }
            else {
                return "Unknown Encoding (" + std::to_string(encoding) + ")";
            }
        }
    }

    // ==================== getLocaleName 函数 ====================
    std::string MobiBook::getLocaleName(uint32_t  locale) const {
        switch (locale) {
            // 英语
        case LOCALE_EN_US: return "en-US (English, United States)";
        case LOCALE_EN_GB: return "en-GB (English, United Kingdom)";
        case LOCALE_EN_CA: return "en-CA (English, Canada)";
        case LOCALE_EN_AU: return "en-AU (English, Australia)";
        case LOCALE_EN_NZ: return "en-NZ (English, New Zealand)";
        case LOCALE_EN_IE: return "en-IE (English, Ireland)";
        case LOCALE_EN_IN: return "en-IN (English, India)";
        case LOCALE_EN_SG: return "en-SG (English, Singapore)";

            // 简体中文
        case LOCALE_ZH_CN: return "zh-CN (Chinese Simplified, China)";
        case LOCALE_ZH_SG: return "zh-SG (Chinese Simplified, Singapore)";

            // 繁体中文
        case LOCALE_ZH_TW: return "zh-TW (Chinese Traditional, Taiwan)";
        case LOCALE_ZH_HK: return "zh-HK (Chinese Traditional, Hong Kong)";
        case LOCALE_ZH_MO: return "zh-MO (Chinese Traditional, Macau)";

            // 日语
        case LOCALE_JA_JP: return "ja-JP (Japanese, Japan)";

            // 韩语
        case LOCALE_KO_KR: return "ko-KR (Korean, Korea)";

            // 法语
        case LOCALE_FR_FR: return "fr-FR (French, France)";
        case LOCALE_FR_CA: return "fr-CA (French, Canada)";
        case LOCALE_FR_CH: return "fr-CH (French, Switzerland)";
        case LOCALE_FR_BE: return "fr-BE (French, Belgium)";

            // 德语
        case LOCALE_DE_DE: return "de-DE (German, Germany)";
        case LOCALE_DE_AT: return "de-AT (German, Austria)";
        case LOCALE_DE_CH: return "de-CH (German, Switzerland)";

            // 西班牙语
        case LOCALE_ES_ES: return "es-ES (Spanish, Spain)";
        case LOCALE_ES_MX: return "es-MX (Spanish, Mexico)";
        case LOCALE_ES_AR: return "es-AR (Spanish, Argentina)";

            // 意大利语
        case LOCALE_IT_IT: return "it-IT (Italian, Italy)";
        case LOCALE_IT_CH: return "it-CH (Italian, Switzerland)";

            // 葡萄牙语
        case LOCALE_PT_BR: return "pt-BR (Portuguese, Brazil)";
        case LOCALE_PT_PT: return "pt-PT (Portuguese, Portugal)";

            // 俄语
        case LOCALE_RU_RU: return "ru-RU (Russian, Russia)";

            // 阿拉伯语
        case LOCALE_AR_SA: return "ar-SA (Arabic, Saudi Arabia)";
        case LOCALE_AR_EG: return "ar-EG (Arabic, Egypt)";

            // 印地语
        case LOCALE_HI_IN: return "hi-IN (Hindi, India)";

            // 其他亚洲语言
        case LOCALE_TH_TH: return "th-TH (Thai, Thailand)";
        case LOCALE_VI_VN: return "vi-VN (Vietnamese, Vietnam)";

            // 北欧语言
        case LOCALE_SV_SE: return "sv-SE (Swedish, Sweden)";
        case LOCALE_NB_NO: return "nb-NO (Norwegian Bokmål, Norway)";
        case LOCALE_DA_DK: return "da-DK (Danish, Denmark)";
        case LOCALE_FI_FI: return "fi-FI (Finnish, Finland)";

            // 中东欧语言
        case LOCALE_PL_PL: return "pl-PL (Polish, Poland)";
        case LOCALE_CS_CZ: return "cs-CZ (Czech, Czech Republic)";
        case LOCALE_HU_HU: return "hu-HU (Hungarian, Hungary)";
        case LOCALE_RO_RO: return "ro-RO (Romanian, Romania)";
        case LOCALE_EL_GR: return "el-GR (Greek, Greece)";
        case LOCALE_TR_TR: return "tr-TR (Turkish, Turkey)";

            // 希伯来语
        case LOCALE_HE_IL: return "he-IL (Hebrew, Israel)";

            // 荷兰语
        case LOCALE_NL_NL: return "nl-NL (Dutch, Netherlands)";
        case LOCALE_NL_BE: return "nl-BE (Dutch, Belgium)";



        default: {
            uint16_t primary = (locale >> 10) & 0x3FF;  // 主语言ID
            uint16_t sub = locale & 0x3FF;              // 子语言ID

            std::ostringstream oss;
            oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << locale;

            // 尝试解析未知locale的结构
            if (primary > 0) {
                oss << " [Lang: 0x" << std::hex << std::setw(3) << std::setfill('0') << primary;
                if (sub > 0) {
                    oss << ", Sub: 0x" << std::hex << std::setw(3) << std::setfill('0') << sub;
                }
                oss << "]";
            }

            return oss.str();
        }
        }
    }

    // ==================== getExthTypeName 函数 ====================
    std::string MobiBook::getExthTypeName(uint32_t  type) const {
        switch (type) {
            // 0-99: 基础信息
        case EXTH_AUTHOR: return "Author";
        case EXTH_PUBLISHER: return "Publisher";
        case EXTH_IMPRINT: return "Imprint";
        case EXTH_DESCRIPTION: return "Description";
        case EXTH_ISBN: return "ISBN";
        case EXTH_SUBJECT: return "Subject/Categories";
        case EXTH_PUBLISHING_DATE: return "Publishing Date";
        case EXTH_REVIEW: return "Review";
        case EXTH_CONTRIBUTOR: return "Contributor";
        case EXTH_RIGHTS: return "Rights";
        case EXTH_SUBJECT_CODE: return "Subject Code";
        case EXTH_TYPE: return "Type";
        case EXTH_SOURCE: return "Source";
        case EXTH_ASIN: return "ASIN (Amazon)";

            // 100-199: 元数据和阅读信息
        case EXTH_VERSION_NUMBER: return "Version Number";
        case EXTH_SAMPLE: return "Sample";
        case EXTH_START_READING: return "Start Reading Location";
        case EXTH_ADULT: return "Adult Content";
        case EXTH_RETAIL_PRICE: return "Retail Price";
        case EXTH_RETAIL_PRICE_CURRENCY: return "Retail Price Currency";
        case EXTH_KF8_BOUNDARY_OFFSET: return "KF8 Boundary Offset";
        case EXTH_FIXED_LAYOUT: return "Fixed Layout";
        case EXTH_BOOK_TYPE: return "Book Type";
        case EXTH_ORIENTATION_LOCK: return "Orientation Lock";
        case EXTH_COUNT_OF_RESOURCES: return "Count of Resources";
        case EXTH_ORIGINAL_RESOLUTION: return "Original Resolution";
        case EXTH_ZERO_GUTTER: return "Zero Gutter";
        case EXTH_ZERO_MARGIN: return "Zero Margin";
        case EXTH_METADATA_RESOURCE_URI: return "Metadata Resource URI";
        case EXTH_UNKNOWN_131: return "Unknown (131)";
        case EXTH_UNKNOWN_132: return "Unknown (132)";

            // 200-299: 封面和软件信息
        case EXTH_DICTIONARY_SHORT_NAME: return "Dictionary Short Name";
        case EXTH_COVER_OFFSET: return "Cover Offset";
        case EXTH_THUMB_OFFSET: return "Thumbnail Offset";
        case EXTH_HAS_FAKE_COVER: return "Has Fake Cover";
        case EXTH_CREATOR_SOFTWARE: return "Creator Software";
        case EXTH_CREATOR_MAJOR_VERSION: return "Creator Major Version";
        case EXTH_CREATOR_MINOR_VERSION: return "Creator Minor Version";
        case EXTH_CREATOR_BUILD_NUMBER: return "Creator Build Number";
        case EXTH_WATERMARK: return "Watermark";
        case EXTH_TAMPER_PROOF_KEYS: return "Tamper Proof Keys";

            // 300-399: 字体和排版
        case EXTH_FONT_SIGNATURE: return "Font Signature";

            // 400-499: DRM和限制
        case EXTH_CLIPPING_LIMIT: return "Clipping Limit";
        case EXTH_PUBLISHER_LIMIT: return "Publisher Limit";
        case EXTH_TTS_FLAG: return "Text-to-Speech Flag";
        case EXTH_RENT_BORROW_FLAG: return "Rent/Borrow Flag";
        case EXTH_RENT_BORROW_EXPIRATION: return "Rent/Borrow Expiration";
        case EXTH_UNKNOWN_407: return "Unknown (407)";

            // 500-599: CDE和更新信息
        case EXTH_CDE_TYPE: return "CDE Type";
        case EXTH_LAST_UPDATE_TIME: return "Last Update Time";
        case EXTH_UPDATED_TITLE: return "Updated Title";
        case EXTH_ASIN2: return "ASIN (Alternate)";

            // 520-529: 语言和排版
        case EXTH_LANGUAGE: return "Language";
        case EXTH_WRITING_MODE: return "Writing Mode";

            // 530-549: 构建信息和时间戳
        case EXTH_CREATOR_BUILD_NUMBER2: return "Creator Build Number (Alternate)";
        case EXTH_UNKNOWN_536: return "Unknown (536)";
        case EXTH_UNIX_TIMESTAMP: return "Unix Timestamp";
        case EXTH_IN_MEMORY: return "In-Memory Flag";

            // 常见的其他记录类型（您的原始代码中的部分）

        case 403: return "Last Update Time (Alternate)";
        case 505: return "Language Pronunciation";
        case 506: return "TTS License";

        default: {
            // 尝试根据类型范围分类
            if (type < 100) return "Basic Metadata (" + std::to_string(type) + ")";
            else if (type < 200) return "Metadata and Reading (" + std::to_string(type) + ")";
            else if (type < 300) return "Cover and Software (" + std::to_string(type) + ")";
            else if (type < 400) return "Font and Layout (" + std::to_string(type) + ")";
            else if (type < 500) return "DRM and Restrictions (" + std::to_string(type) + ")";
            else if (type < 600) return "CDE and Updates (" + std::to_string(type) + ")";
            else return "Unknown EXTH Record (" + std::to_string(type) + ")";
        }
        }
    }

    // 验证函数
    bool MobiBook::validatePalmDocHeader() const {
        return m_palm_doc_header.compression <= 2 &&
            m_palm_doc_header.encryption_type <= 2 &&
            m_palm_doc_header.record_size > 0;
    }

    bool MobiBook::validateMobiHeader() const {
        return m_mobi_header.header_length >= 228 &&  // MOBI头部最小长度
            m_mobi_header.first_content_record < m_record_info_list.size();
    }
    std::string MobiBook::toHexString(uint32_t value) const {
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << value;
        return oss.str();
    }

    // ==================== formatExthValue 函数 ====================
    std::string MobiBook::formatExthValue(uint32_t type, const std::string& data) const
    {
        // 特殊处理某些类型的值
        switch (type)
        {
        case EXTH_LANGUAGE:
            if (data.length() >= 2) {
                std::string lang_code = data.substr(0, 2);
                // 这里可以添加语言代码到名称的映射
                return data + " (" + lang_code + ")";
            }
            break;

        case EXTH_ADULT:
            if (!data.empty() && data[0] == '1') {
                return "Yes (Adult Content)";
            }
            else if (!data.empty() && data[0] == '0') {
                return "No";
            }
            break;

        case EXTH_TTS_FLAG:
            if (!data.empty()) {
                if (data[0] == '1') return "Enabled";
                else if (data[0] == '0') return "Disabled";
            }
            break;

        case EXTH_FIXED_LAYOUT:
            if (!data.empty()) {
                if (data[0] == '1') return "Fixed Layout";
                else if (data[0] == '0') return "Reflowable";
            }
            break;

        case EXTH_ORIENTATION_LOCK:
            if (data == "portrait") return "Portrait Only";
            else if (data == "landscape") return "Landscape Only";
            else if (data == "none") return "No Lock";
            break;

        case EXTH_RETAIL_PRICE_CURRENCY:
            if (data == "USD") return "US Dollar";
            else if (data == "EUR") return "Euro";
            else if (data == "GBP") return "British Pound";
            else if (data == "JPY") return "Japanese Yen";
            else if (data == "CNY") return "Chinese Yuan";
            break;
        }

        // 默认返回原始数据，但做适当清理
        std::string result = data;
        // 移除末尾的空字符
        while (!result.empty() && result.back() == '\0') {
            result.pop_back();
        }

        // 如果数据看起来像数字，显示数值
        if (!result.empty() && result.length() <= 8) {
            bool is_numeric = true;
            for (char c : result) {
                if (c != '\0' && !std::isdigit(static_cast<unsigned char>(c))) {
                    is_numeric = false;
                    break;
                }
            }
            if (is_numeric) {
                uint32_t num = 0;
                for (char c : result) {
                    if (c != '\0') {
                        num = num * 256 + static_cast<unsigned char>(c);
                    }
                }
                result = std::to_string(num) + " [0x" + toHexString(num) + "]";
            }
        }

        return result.empty() ? "(empty)" : result;
    }

    uint32_t MobiBook::getRecordLength(uint32_t record_index) const {
        if (record_index >= m_record_info_list.size()) {
            return 0;
        }

        uint32_t next_offset = 0;
        if (record_index + 1 < m_record_info_list.size()) {
            // 使用下一个记录的偏移量计算当前记录长度
            next_offset = m_record_info_list[record_index + 1].data_offset;
        }
        else {
            // 如果是最后一个记录，使用文件结束位置
            next_offset = static_cast<uint32_t>(m_file_data.size());
        }

        uint32_t current_offset = m_record_info_list[record_index].data_offset;
        return next_offset - current_offset;
    }

    // 读取后向编码的可变宽度整数（用于尾部条目）
    uint32_t MobiBook::readBackwardVarWidthInt(const uint8_t* data, uint32_t data_size, uint32_t& offset) {
        std::vector<uint8_t> bytes;
        uint32_t start_offset = offset;

        // 向后读取直到遇到结束标记
        while (offset < data_size) {
            uint8_t byte = data[offset++];
            bytes.push_back(byte);

            // 后向编码：只有MSB的bit 8（即LSB）为1表示继续
            // 实际上应该是检查bit 0 (LSB)
            if (!(byte & 0x01)) {
                break;
            }
        }

        // 需要反转字节数组
        std::reverse(bytes.begin(), bytes.end());

        // 解码可变宽度整数（每字节7位）
        uint32_t result = 0;
        for (uint8_t byte : bytes) {
            result = (result << 7) | ((byte >> 1) & 0x7F);  // 注意：这里需要移位，因为bit 0是继续标记
        }

        return result;
    }

    // 读取前向编码的可变宽度整数（用于文本长度）
    uint32_t MobiBook::readForwardVarWidthInt(const uint8_t* data, uint32_t data_size, uint32_t& offset) {
        uint32_t result = 0;

        while (offset < data_size) {
            uint8_t byte = data[offset++];
            result = (result << 7) | (byte & 0x7F);

            // 前向编码：MSB为0表示结束
            if (!(byte & 0x80)) {
                break;
            }
        }

        return result;
    }


} // namespace mobi