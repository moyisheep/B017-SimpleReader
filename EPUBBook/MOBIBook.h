#pragma once

#include <string>

#pragma pack(push, 1)
struct PDB_Header {
    char     name[32];      // 文件名（空字符填充）
    uint16_t attributes;    // 文件属性
    uint16_t version;       // 版本号
    uint32_t createTime;    // 创建时间（Palm时间戳）
    uint32_t modifyTime;    // 修改时间
    uint32_t backupTime;    // 备份时间
    uint32_t modNumber;     // 修改编号
    uint32_t appInfoID;     // 应用信息ID
    uint32_t sortInfoID;    // 排序信息ID
    char     type[4];       // 文件类型（"BOOK"或"TEXt"）
    char     creator[4];    // 创建者标识（"MOBI"）
    uint32_t uid;           // 唯一ID
    uint32_t nextRecord;    // 下一条记录ID
    uint16_t numRecords;    // 记录总数
};

struct PalmDOC_Header
{
    uint16_t compression;
    uint16_t unused;
    uint32_t text_length;
    uint16_t record_count;
    uint16_t record_size;
    uint32_t current_position;
    uint16_t encryption_type;
    uint16_t unknown;
};
struct MOBI_Header {
    // --- 基础标识 ---
    uint32_t identifier;          // 固定值 0x4D4F4249 ("MOBI")
    uint32_t header_length;        // MOBI头总长度（通常≥0xE4）

    // --- 书籍属性 ---
    uint32_t mobi_type;            // 文件类型：2=MOBI, 3=KF8
    uint32_t text_encoding;        // 编码：65001=UTF-8, 1252=Latin1
    uint32_t unique_id;            // 书籍唯一标识符
    uint32_t file_version;         // 格式版本：6=MOBI7, 8=KF8

    // --- 文本信息 ---
    uint32_t ortographic_index;    // 正字法记录号（未使用）
    uint32_t inflection_index;     // 变形记录号（未使用）
    uint32_t index_names;          // 索引名称记录号
    uint32_t index_keys;           // 索引键记录号
    uint32_t extra_index0;         // 额外索引0
    uint32_t extra_index1;         // 额外索引1
    uint32_t extra_index2;         // 额外索引2
    uint32_t extra_index3;         // 额外索引3
    uint32_t extra_index4;         // 额外索引4
    uint32_t extra_index5;         // 额外索引5

    // --- 元数据 ---
    uint32_t first_non_book_index;   // 首个非书籍内容记录号
    uint32_t full_name_offset;      // 书名在记录中的偏移
    uint32_t full_name_length;      // 书名长度
    uint32_t locale;              // 地区代码（en=0x09, zh=0x0804）
    uint32_t input_language;       // 输入语言代码
    uint32_t output_language;      // 输出语言代码
    uint32_t min_version;          // 最低支持Kindle版本
    uint32_t first_image_index;     // 首张图片记录号
    uint32_t huffman_record_offset;       // Huffman压缩记录号
    uint32_t huffman_record_count;        // Huffman记录数
    uint32_t huffman_table_offset;  // Huffman表偏移
    uint32_t huffman_table_length;  // Huffman表长度

    // --- DRM 相关 ---(存在问题 待修复）
    uint32_t exth_flags;           // EXTH头标志位（位0=存在EXTH）
    uint8_t  unknown1[8];         // 保留字段
    uint32_t drm_offset;           // DRM信息偏移（相对于MOBI头）
    uint32_t drm_count;            // DRM条目数
    uint32_t drm_size;             // DRM数据总大小
    uint32_t drm_flags;            // DRM标志位

    // --- 内容位置 ---
    uint32_t first_content_record_number;  // 首内容记录号（通常1）
    uint32_t last_content_record_number;   // 末内容记录号
    uint32_t unknown2;            // 
    uint32_t fcis_record_number;          // FCIS记录号（KF8）
    uint32_t fcis_record_count;           // FCIS记录数（通常1）
    uint32_t flis_record_number;          // FLIS记录号（KF8）
    uint32_t flis_record_count;           // FLIS记录数（通常1）
    uint32_t unknown3[8];         // 

    // --- KF8 扩展 ---
    uint32_t kf8BoundaryOffset;   // KF8内容起始偏移（相对记录0）
    uint32_t kf8Unknown1;         // 
    uint32_t kf8StartOffset;      // KF8 ZIP容器起始偏移
    uint32_t kf8EndOffset;        // KF8 ZIP容器结束偏移
};



struct EXTHHeader {
    uint32_t identifier;       // 固定值 0x45585448 ("EXTH")
    uint32_t header_length;           // EXTH头总长度（含记录）
    uint32_t record_count;      // 记录项数量

    // 动态记录项数组（需根据recordCount解析）
    struct EXTHRecord {
        uint32_t type;         // 记录类型（见下表）
        uint32_t length;       // 数据长度（含type和length字段）
        uint8_t  data[];       // 变长数据（实际长度=length-8）
    };
};

// 常见EXTH记录类型枚举
enum EXTHRecordType {
    EXTH_AUTHOR = 100,   // 作者
    EXTH_PUBLISHER = 101,   // 出版商
    EXTH_DESCRIPTION = 103,   // 描述
    EXTH_ISBN = 104,   // ISBN
    EXTH_SUBJECT = 105,   // 主题分类
    EXTH_PUBDATE = 106,   // 出版日期（YYYY-MM-DD）
    EXTH_REVIEW = 107,   // 评论
    EXTH_CONTRIBUTOR = 108,   // 贡献者
    EXTH_RIGHTS = 109,   // 版权信息
    EXTH_COVER_OFFSET = 201,   // 封面图片偏移（二进制）
    EXTH_THUMB_OFFSET = 202,   // 缩略图偏移
    EXTH_CDETYPE = 501,   // 内容类型（"EBOK"=电子书）
    EXTH_DOCTYPE = 503,   // 文档类型（"EBOK"）
    EXTH_ASIN = 113,   // Amazon商品编号
    EXTH_FONTSIGN = 906,   // 字体签名（DRM相关）
};

#pragma pack(pop)