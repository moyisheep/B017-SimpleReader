#pragma once

#include <string>

#pragma pack(push, 1)
struct PDBHeader {
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


struct MOBIHeader {
    // --- 基础标识 ---
    uint32_t identifier;          // 固定值 0x4D4F4249 ("MOBI")
    uint32_t headerLength;        // MOBI头总长度（通常≥0xE4）

    // --- 书籍属性 ---
    uint32_t mobiType;            // 文件类型：2=MOBI, 3=KF8
    uint32_t textEncoding;        // 编码：65001=UTF-8, 1252=Latin1
    uint32_t uniqueID;            // 书籍唯一标识符
    uint32_t fileVersion;         // 格式版本：6=MOBI7, 8=KF8

    // --- 文本信息 ---
    uint32_t ortographicIndex;    // 正字法记录号（未使用）
    uint32_t inflectionIndex;     // 变形记录号（未使用）
    uint32_t indexNames;          // 索引名称记录号
    uint32_t indexKeys;           // 索引键记录号
    uint32_t extraIndex0;         // 额外索引0
    uint32_t extraIndex1;         // 额外索引1
    uint32_t extraIndex2;         // 额外索引2
    uint32_t extraIndex3;         // 额外索引3
    uint32_t extraIndex4;         // 额外索引4
    uint32_t extraIndex5;         // 额外索引5

    // --- 元数据 ---
    uint32_t firstNonBookIndex;   // 首个非书籍内容记录号
    uint32_t fullNameOffset;      // 书名在记录中的偏移
    uint32_t fullNameLength;      // 书名长度
    uint32_t locale;              // 地区代码（en=0x09, zh=0x0804）
    uint32_t inputLanguage;       // 输入语言代码
    uint32_t outputLanguage;      // 输出语言代码
    uint32_t minVersion;          // 最低支持Kindle版本
    uint32_t firstImageIndex;     // 首张图片记录号
    uint32_t huffmanRecord;       // Huffman压缩记录号
    uint32_t huffmanCount;        // Huffman记录数
    uint32_t huffmanTableOffset;  // Huffman表偏移
    uint32_t huffmanTableLength;  // Huffman表长度

    // --- DRM 相关 ---
    uint32_t exthFlags;           // EXTH头标志位（位0=存在EXTH）
    uint8_t  unknown1[8];         // 保留字段
    uint32_t drmOffset;           // DRM信息偏移（相对于MOBI头）
    uint32_t drmCount;            // DRM条目数
    uint32_t drmSize;             // DRM数据总大小
    uint32_t drmFlags;            // DRM标志位

    // --- 内容位置 ---
    uint32_t firstContentRecord;  // 首内容记录号（通常1）
    uint32_t lastContentRecord;   // 末内容记录号
    uint32_t unknown2;            // 
    uint32_t fcisRecord;          // FCIS记录号（KF8）
    uint32_t fcisCount;           // FCIS记录数（通常1）
    uint32_t flisRecord;          // FLIS记录号（KF8）
    uint32_t flisCount;           // FLIS记录数（通常1）
    uint32_t unknown3[8];         // 

    // --- KF8 扩展 ---
    uint32_t kf8BoundaryOffset;   // KF8内容起始偏移（相对记录0）
    uint32_t kf8Unknown1;         // 
    uint32_t kf8StartOffset;      // KF8 ZIP容器起始偏移
    uint32_t kf8EndOffset;        // KF8 ZIP容器结束偏移
};



struct EXTHHeader {
    uint32_t identifier;       // 固定值 0x45585448 ("EXTH")
    uint32_t length;           // EXTH头总长度（含记录）
    uint32_t recordCount;      // 记录项数量

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