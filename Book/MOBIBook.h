#ifndef MOBI_BOOK_H
#define MOBI_BOOK_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include "Book.h"

namespace mobi {

    // ==================== 数据结构定义 ====================

#pragma pack(push, 1)

// PDB头
// 根据规范修正的 PDB 头结构
    struct PDBHeader {
        char name[32];                  // 数据库名称，以0终止
        uint16_t attributes;            // 属性位域
        uint16_t version;               // 文件版本
        uint32_t creation_time;         // 创建时间：自1904年1月1日以来的秒数
        uint32_t modification_time;     // 修改时间
        uint32_t backup_time;          // 最后备份时间
        uint32_t modification_number;   // 修改编号
        uint32_t app_info_offset;      // Application Info 偏移（如果存在）
        uint32_t sort_info_offset;     // Sort Info 偏移（如果存在）
        char type[4];                   // 类型（例如 'BOOK', 'TEXt', 'MOBI'）
        char creator[4];                // 创建者
        uint32_t unique_id_seed;       // 唯一ID种子
        uint32_t next_record_list_id;  // 内存中使用，文件中设为0
        uint16_t num_records;          // 记录数量 N
    };

    // 记录信息结构（每个记录占8字节）
    struct RecordInfo {
        uint32_t data_offset;          // 记录数据偏移（相对于PDB开头）
        uint8_t attributes;            // 记录属性
        uint8_t unique_id[3];          // 唯一ID（3字节）
    };





    // PalmDOC 头（位于第一个记录的开头）
    struct PalmDocHeader {
        uint16_t compression;           // 压缩类型：1=无压缩，2=PalmDOC压缩，17480=HUFF/CDIC
        uint16_t unused;               // 总是0
        uint32_t text_length;          // 未压缩的文本总长度
        uint16_t record_count;         // 用于文本的PDB记录数量
        uint16_t record_size;          // 每个文本记录的最大大小，总是4096
        uint16_t encryption_type;      // 加密类型：0=无加密，1=旧加密，2=当前加密
        uint16_t unknown;              // 通常为0
    };

    // MOBI 头标识
    const char MOBI_IDENTIFIER[5] = "MOBI";

    // MOBI 头结构
    struct MobiHeader {
        char identifier[4];            // "MOBI" 标识
        uint32_t header_length;        // MOBI头长度（包括前4字节）
        uint32_t mobi_type;            // Mobipocket文件类型
        uint32_t text_encoding;        // 文本编码：1252=CP1252，65001=UTF-8
        uint32_t unique_id;            // 唯一ID
        uint32_t file_version;         // Mobipocket格式版本
        uint32_t orthographic_index;   // 正字法索引位置
        uint32_t inflection_index;     // 变形索引位置
        uint32_t index_names;          // 索引名称位置
        uint32_t index_keys;           // 索引键位置
        uint32_t extra_index0;         // 额外索引0位置
        uint32_t extra_index1;         // 额外索引1位置
        uint32_t extra_index2;         // 额外索引2位置
        uint32_t extra_index3;         // 额外索引3位置
        uint32_t extra_index4;         // 额外索引4位置
        uint32_t extra_index5;         // 额外索引5位置
        uint32_t first_non_book_index; // 第一个非文本记录编号
        uint32_t full_name_offset;     // 完整名称偏移（相对于记录0）
        uint32_t full_name_length;     // 完整名称长度
        uint32_t locale;               // 区域设置代码
        uint32_t input_language;       // 输入语言（用于字典）
        uint32_t output_language;      // 输出语言（用于字典）
        uint32_t min_version;          // 支持的最小版本
        uint32_t first_image_index;    // 第一个图像记录编号
        uint32_t huffman_record_offset; // Huffman压缩记录偏移
        uint32_t huffman_record_count; // Huffman压缩记录数量
        uint32_t huffman_table_offset; // Huffman表偏移
        uint32_t huffman_table_length; // Huffman表长度
        uint32_t exth_flags;           // EXTH标志位
        uint8_t unknown32[32];         // 32字节未知数据
        uint32_t unknown_a4;           // 未知，通常为0xFFFFFFFF
        uint32_t drm_offset;           // DRM密钥信息偏移
        uint32_t drm_count;            // DRM条目数量
        uint32_t drm_size;             // DRM信息字节数
        uint32_t drm_flags;            // DRM标志位

        // 以下字段只有当header_length >= 228时存在
        uint64_t unknown_b8;           // 未知的8字节

        // 以下字段只有当header_length >= 244时存在
        uint16_t first_content_record; // 第一个内容记录编号
        uint16_t last_content_record;  // 最后一个内容记录编号
        uint32_t unknown_c4;           // 未知，通常为0x00000001
        uint32_t fcis_record;          // FCIS记录编号
        uint32_t fcis_count;           // FCIS记录数量
        uint32_t flis_record;          // FLIS记录编号
        uint32_t flis_count;           // FLIS记录数量
        uint64_t unknown_d8;           // 未知的8字节
        uint32_t unknown_e0;           // 未知，通常为0xFFFFFFFF
        uint32_t first_compilation_section; // 第一个编译数据部分计数
        uint32_t num_compilation_sections;  // 编译数据部分数量
        uint32_t unknown_ec;           // 未知，通常为0xFFFFFFFF
        uint32_t extra_record_data_flags; // 额外记录数据标志

        // 以下字段只有当header_length >= 248时存在
        uint32_t indx_record_offset;   // INDX记录偏移
        uint32_t unknown_f8;           // 未知
        uint32_t unknown_fc;           // 未知

        // 以下字段只有当header_length >= 256时存在
        uint32_t unknown_100;          // 未知
        uint32_t unknown_104;          // 未知
        uint32_t unknown_108;          // 未知
        uint32_t unknown_10b;          // 未知
    };

    // EXTH 头标识
    const char EXTH_IDENTIFIER[5] = "EXTH";

    // EXTH 头结构
    struct ExthHeader {
        char identifier[4];            // "EXTH" 标识
        uint32_t header_length;        // EXTH头长度
        uint32_t record_count;         // EXTH记录数量
    };

    // EXTH 记录结构
    struct ExthRecord {
        uint32_t type;                 // 记录类型
        uint32_t length;               // 记录长度
        // 数据部分紧随其后
    };

    // FLIS 记录结构
    struct FlisRecord {
        char identifier[4];            // "FLIS"
        uint32_t unknown_4;            // 固定值: 8
        uint16_t unknown_8;            // 固定值: 65
        uint16_t unknown_a;            // 固定值: 0
        uint32_t unknown_c;            // 固定值: 0
        uint32_t unknown_10;           // 固定值: -1 (0xFFFFFFFF)
        uint16_t unknown_14;           // 固定值: 1
        uint16_t unknown_16;           // 固定值: 3
        uint32_t unknown_18;           // 固定值: 3
        uint32_t unknown_1c;           // 固定值: 1
        uint32_t unknown_20;           // 固定值: -1 (0xFFFFFFFF)
    };

    // FCIS 记录结构
    struct FcisRecord {
        char identifier[4];            // "FCIS"
        uint32_t unknown_4;            // 固定值: 20
        uint32_t unknown_8;            // 固定值: 16
        uint32_t unknown_c;            // 固定值: 1
        uint32_t unknown_10;           // 固定值: 0
        uint32_t text_length;          // 文本长度（与PalmDocHeader相同）
        uint32_t unknown_18;           // 固定值: 0
        uint32_t unknown_1c;           // 固定值: 32
        uint32_t unknown_20;           // 固定值: 8
        uint16_t unknown_24;           // 固定值: 1
        uint16_t unknown_26;           // 固定值: 1
        uint32_t unknown_28;           // 固定值: 0
    };

    // EOF 记录结构
    struct EofRecord {
        uint8_t byte0;                 // 固定值: 233 (0xe9)
        uint8_t byte1;                 // 固定值: 142 (0x8e)
        uint8_t byte2;                 // 固定值: 13 (0x0d) - CR
        uint8_t byte3;                 // 固定值: 10 (0x0a) - LF
    };

    // A SRCS record is a record whose content is a zip archive of all source files (i.e., .opf, .ncx, .htm, .jpg, ...) given to the command and puts it in the generated MOBI file. 
    struct SrcsRecord
    {
        char identifier[4];            // "SRCS"
        uint32_t unknown1;             // fixed value(?): 0x00000010
        uint32_t unknown2;             // fixed value(?): 0x0000002f
        uint32_t unknown3;             // fixed value(?): 0x00000001
        // zip	The zip archive continues to the end of this record
    };

    // A CMET record is a record whose content is the output of the compilation operation, and perhaps extra info. 
    struct CmetRecord
    {
        char identifier[4];            // "CMET"
        uint32_t unknown1;             // 	fixed value(?): 0x0000000C
        uint32_t text_length;          // 	(big endian)
        // 	variable	text	compilation output text, line endings are CRLF
        // 	variable	?	unknown data to the end of the record
    };

    struct AudiRecord
    {
        char identifier[4];            // "AUDI"
        uint32_t unknown1;             // 	unkown value
        uint32_t unknown2;             // 	unkown value
        // media	The media data continues to the end of this record
    };

    struct VideRecord
    {
        char identifier[4];            // "VIDE"
        uint32_t unknown1;             // 	unkown value
        uint32_t unknown2;             // 	unkown value
        // media	The media data continues to the end of this record
    };

    // INDX 头结构（索引元数据）
    struct IndxHeader {
        char identifier[4];            // "INDX"
        uint32_t header_length;        // INDX头长度
        uint32_t index_type;           // 索引类型：0=普通索引，2=变形索引
        uint32_t unknown_c;            // 未知
        uint32_t unknown_10;           // 未知
        uint32_t idxt_offset;          // IDXT部分偏移
        uint32_t index_record_count;   // 索引记录数量
        uint32_t index_encoding;       // 索引编码：1252=CP1252，65001=UTF-8
        uint32_t index_language;       // 索引语言代码
        uint32_t total_index_count;    // 索引条目总数
        uint32_t ordt_offset;          // ORDT部分偏移
        uint32_t ligt_offset;          // LIGT部分偏移
        uint32_t unknown_30;           // 未知
        uint32_t unknown_34;           // 未知
    };

    // TAGX 头结构
    struct TagxHeader {
        char identifier[4];            // "TAGX"
        uint32_t header_length;        // TAGX头长度
        uint32_t control_byte_count;   // 控制字节数量
        // 标签表紧随其后
    };

    // 如果解析图像记录时有索引结构
    struct ImageRecordIndex {
        uint32_t offset;
        uint32_t length;
        uint16_t width;
        uint16_t height;
        uint8_t type;  // 1=JPEG, 2=GIF, 3=PNG
    };
#pragma pack(pop)

    enum CompressionType {
        COMPRESSION_NONE = 1,
        COMPRESSION_PALMDOC = 2,
        COMPRESSION_HUFFCDIC = 17480
    };

    enum EncryptionType {
        ENCRYPTION_NONE = 0,
        ENCRYPTION_OLD_MOBI = 1,
        ENCRYPTION_MOBI = 2
    };

    enum MobiType {
        MOBI_TYPE_BOOK = 2,            // Mobipocket Book
        MOBI_TYPE_PALMDOC = 3,         // PalmDoc Book
        MOBI_TYPE_AUDIO = 4,           // Audio
        MOBI_TYPE_KINDLEGEN1 = 232,    // KindleGen 1.2 生成
        MOBI_TYPE_KF8 = 248,           // KF8: KindleGen 2 生成
        MOBI_TYPE_NEWS = 257,          // News
        MOBI_TYPE_NEWS_FEED = 258,     // News_Feed
        MOBI_TYPE_NEWS_MAGAZINE = 259, // News_Magazine
        MOBI_TYPE_PICS = 513,          // PICS
        MOBI_TYPE_WORD = 514,          // WORD
        MOBI_TYPE_XLS = 515,           // XLS
        MOBI_TYPE_PPT = 516,           // PPT
        MOBI_TYPE_TEXT = 517,          // TEXT
        MOBI_TYPE_HTML = 518           // HTML
    };

    // ==================== TextEncoding 枚举 ====================
    enum TextEncoding {
        // 标准编码
        ENCODING_LATIN1 = 1,        // ISO-8859-1 (Latin-1)
        ENCODING_UCS2 = 2,        // UCS-2 (UCS-2 Big Endian)
        ENCODING_UTF16 = 3,        // UTF-16 (UTF-16 Big Endian)

        // Windows Code Pages
        ENCODING_CP1250 = 4,        // Windows-1250 (Central European)
        ENCODING_CP1251 = 5,        // Windows-1251 (Cyrillic)
        ENCODING_CP1252 = 1252,     // Windows-1252 (Western European)
        ENCODING_CP1253 = 6,        // Windows-1253 (Greek)
        ENCODING_CP1254 = 7,        // Windows-1254 (Turkish)
        ENCODING_CP1255 = 8,        // Windows-1255 (Hebrew)
        ENCODING_CP1256 = 9,        // Windows-1256 (Arabic)
        ENCODING_CP1257 = 10,       // Windows-1257 (Baltic)
        ENCODING_CP1258 = 11,       // Windows-1258 (Vietnamese)

        // 亚洲编码
        ENCODING_JIS = 12,       // JIS (Japanese Industrial Standard)
        ENCODING_SJIS = 13,       // Shift-JIS (Japanese)
        ENCODING_GB2312 = 14,       // GB2312 (Simplified Chinese)
        ENCODING_BIG5 = 15,       // Big5 (Traditional Chinese)
        ENCODING_EUC_KR = 16,       // EUC-KR (Korean)

        // UTF 编码
        ENCODING_UTF16BE = 17,       // UTF-16 Big Endian (正式名称)
        ENCODING_UTF8 = 65001,    // UTF-8

        // 其他编码
        ENCODING_MAC_ROMAN = 10000,    // Mac Roman
        ENCODING_MAC_JAPANESE = 10001,    // Mac Japanese
        ENCODING_MAC_TRADCHINESE = 10002, // Mac Traditional Chinese
        ENCODING_MAC_KOREAN = 10003,    // Mac Korean
        ENCODING_MAC_ARABIC = 10004,    // Mac Arabic
        ENCODING_MAC_HEBREW = 10005,    // Mac Hebrew
        ENCODING_MAC_GREEK = 10006,    // Mac Greek
        ENCODING_MAC_CYRILLIC = 10007,    // Mac Cyrillic
        ENCODING_MAC_DEVANAGARI = 10008,  // Mac Devanagari
        ENCODING_MAC_GURMUKHI = 10009,    // Mac Gurmukhi

        // 少见的编码
        ENCODING_ISO_8859_2 = 18,       // ISO-8859-2 (Latin-2)
        ENCODING_ISO_8859_3 = 19,       // ISO-8859-3 (Latin-3)
        ENCODING_ISO_8859_4 = 20,       // ISO-8859-4 (Latin-4)
        ENCODING_ISO_8859_5 = 21,       // ISO-8859-5 (Cyrillic)
        ENCODING_ISO_8859_6 = 22,       // ISO-8859-6 (Arabic)
        ENCODING_ISO_8859_7 = 23,       // ISO-8859-7 (Greek)
        ENCODING_ISO_8859_8 = 24,       // ISO-8859-8 (Hebrew)
        ENCODING_ISO_8859_9 = 25,       // ISO-8859-9 (Latin-5)
        ENCODING_ISO_8859_10 = 26,       // ISO-8859-10 (Latin-6)
        ENCODING_ISO_8859_11 = 27,       // ISO-8859-11 (Thai)
        ENCODING_ISO_8859_13 = 28,       // ISO-8859-13 (Latin-7)
        ENCODING_ISO_8859_14 = 29,       // ISO-8859-14 (Latin-8)
        ENCODING_ISO_8859_15 = 30,       // ISO-8859-15 (Latin-9)
        ENCODING_ISO_8859_16 = 31        // ISO-8859-16 (Latin-10)
    };

    // ==================== Locale 枚举 ====================
    enum Locale {
        // 英语
        LOCALE_EN_US = 0x0409,          // English (United States)
        LOCALE_EN_GB = 0x0809,          // English (United Kingdom)
        LOCALE_EN_CA = 0x1009,          // English (Canada)
        LOCALE_EN_AU = 0x0C09,          // English (Australia)
        LOCALE_EN_NZ = 0x1409,          // English (New Zealand)
        LOCALE_EN_IE = 0x1809,          // English (Ireland)
        LOCALE_EN_IN = 0x4009,          // English (India)
        LOCALE_EN_SG = 0x4809,          // English (Singapore)

        // 简体中文
        LOCALE_ZH_CN = 0x0804,          // Chinese (Simplified, China)
        LOCALE_ZH_SG = 0x1004,          // Chinese (Simplified, Singapore)

        // 繁体中文
        LOCALE_ZH_TW = 0x0404,          // Chinese (Traditional, Taiwan)
        LOCALE_ZH_HK = 0x0C04,          // Chinese (Traditional, Hong Kong)
        LOCALE_ZH_MO = 0x1404,          // Chinese (Traditional, Macau)

        // 日语
        LOCALE_JA_JP = 0x0411,          // Japanese (Japan)

        // 韩语
        LOCALE_KO_KR = 0x0412,          // Korean (Korea)

        // 法语
        LOCALE_FR_FR = 0x040C,          // French (France)
        LOCALE_FR_CA = 0x0C0C,          // French (Canada)
        LOCALE_FR_CH = 0x100C,          // French (Switzerland)
        LOCALE_FR_BE = 0x140C,          // French (Belgium)

        // 德语
        LOCALE_DE_DE = 0x0407,          // German (Germany)
        LOCALE_DE_AT = 0x0C07,          // German (Austria)
        LOCALE_DE_CH = 0x0807,          // German (Switzerland)

        // 西班牙语
        LOCALE_ES_ES = 0x0C0A,          // Spanish (Spain)
        LOCALE_ES_MX = 0x080A,          // Spanish (Mexico)
        LOCALE_ES_AR = 0x2C0A,          // Spanish (Argentina)

        // 意大利语
        LOCALE_IT_IT = 0x0410,          // Italian (Italy)
        LOCALE_IT_CH = 0x0810,          // Italian (Switzerland)

        // 葡萄牙语
        LOCALE_PT_BR = 0x0416,          // Portuguese (Brazil)
        LOCALE_PT_PT = 0x0816,          // Portuguese (Portugal)

        // 俄语
        LOCALE_RU_RU = 0x0419,          // Russian (Russia)

        // 阿拉伯语
        LOCALE_AR_SA = 0x0401,          // Arabic (Saudi Arabia)
        LOCALE_AR_EG = 0x0C01,          // Arabic (Egypt)

        // 印地语
        LOCALE_HI_IN = 0x0439,          // Hindi (India)

        // 其他亚洲语言
        LOCALE_TH_TH = 0x041E,          // Thai (Thailand)
        LOCALE_VI_VN = 0x042A,          // Vietnamese (Vietnam)

        // 北欧语言
        LOCALE_SV_SE = 0x041D,          // Swedish (Sweden)
        LOCALE_NB_NO = 0x0414,          // Norwegian Bokmål (Norway)
        LOCALE_DA_DK = 0x0406,          // Danish (Denmark)
        LOCALE_FI_FI = 0x040B,          // Finnish (Finland)

        // 中东欧语言
        LOCALE_PL_PL = 0x0415,          // Polish (Poland)
        LOCALE_CS_CZ = 0x0405,          // Czech (Czech Republic)
        LOCALE_HU_HU = 0x040E,          // Hungarian (Hungary)
        LOCALE_RO_RO = 0x0418,          // Romanian (Romania)
        LOCALE_EL_GR = 0x0408,          // Greek (Greece)
        LOCALE_TR_TR = 0x041F,          // Turkish (Turkey)

        // 希伯来语
        LOCALE_HE_IL = 0x040D,          // Hebrew (Israel)

        // 荷兰语
        LOCALE_NL_NL = 0x0413,          // Dutch (Netherlands)
        LOCALE_NL_BE = 0x0813,          // Dutch (Belgium)


    };

    enum ExthRecordType {
        EXTH_AUTHOR = 100,
        EXTH_PUBLISHER = 101,
        EXTH_IMPRINT = 102,
        EXTH_DESCRIPTION = 103,
        EXTH_ISBN = 104,
        EXTH_SUBJECT = 105,
        EXTH_PUBLISHING_DATE = 106,
        EXTH_REVIEW = 107,
        EXTH_CONTRIBUTOR = 108,
        EXTH_RIGHTS = 109,
        EXTH_SUBJECT_CODE = 110,
        EXTH_TYPE = 111,
        EXTH_SOURCE = 112,
        EXTH_ASIN = 113,
        EXTH_VERSION_NUMBER = 114,
        EXTH_SAMPLE = 115,
        EXTH_START_READING = 116,
        EXTH_ADULT = 117,
        EXTH_RETAIL_PRICE = 118,
        EXTH_RETAIL_PRICE_CURRENCY = 119,
        EXTH_KF8_BOUNDARY_OFFSET = 121,
        EXTH_FIXED_LAYOUT = 122,
        EXTH_BOOK_TYPE = 123,
        EXTH_ORIENTATION_LOCK = 124,
        EXTH_COUNT_OF_RESOURCES = 125,
        EXTH_ORIGINAL_RESOLUTION = 126,
        EXTH_ZERO_GUTTER = 127,
        EXTH_ZERO_MARGIN = 128,
        EXTH_METADATA_RESOURCE_URI = 129,
        EXTH_UNKNOWN_131 = 131,
        EXTH_UNKNOWN_132 = 132,
        EXTH_DICTIONARY_SHORT_NAME = 200,
        EXTH_COVER_OFFSET = 201,
        EXTH_THUMB_OFFSET = 202,
        EXTH_HAS_FAKE_COVER = 203,
        EXTH_CREATOR_SOFTWARE = 204,
        EXTH_CREATOR_MAJOR_VERSION = 205,
        EXTH_CREATOR_MINOR_VERSION = 206,
        EXTH_CREATOR_BUILD_NUMBER = 207,
        EXTH_WATERMARK = 208,
        EXTH_TAMPER_PROOF_KEYS = 209,
        EXTH_FONT_SIGNATURE = 300,
        EXTH_CLIPPING_LIMIT = 401,
        EXTH_PUBLISHER_LIMIT = 402,
        EXTH_TTS_FLAG = 404,
        EXTH_RENT_BORROW_FLAG = 405,
        EXTH_RENT_BORROW_EXPIRATION = 406,
        EXTH_UNKNOWN_407 = 407,
        EXTH_CDE_TYPE = 501,
        EXTH_LAST_UPDATE_TIME = 502,
        EXTH_UPDATED_TITLE = 503,
        EXTH_ASIN2 = 504,
        EXTH_LANGUAGE = 524,
        EXTH_WRITING_MODE = 525,
        EXTH_CREATOR_BUILD_NUMBER2 = 535,
        EXTH_UNKNOWN_536 = 536,
        EXTH_UNIX_TIMESTAMP = 542,
        EXTH_IN_MEMORY = 547
    };

    enum ExtraRecordDataFlags {
        EXTRA_MULTIBYTE_BYTES = 0x0001,
        EXTRA_TBS_INDEXING = 0x0002,
        EXTRA_UNCROSSABLE_BREAKS = 0x0004
    };

    static const uint8_t VAR_INT_CONTINUE_MASK_FORWARD = 0x80;  // 前向编码：MSB=1表示继续
    static const uint8_t VAR_INT_CONTINUE_MASK_BACKWARD = 0x01; // 后向编码：LSB=1表示继续
    // ==================== MobiBook类 ====================

    class MobiBook : public Book {
    private:
        // 内部状态
        std::string book_path_;
        std::string current_dir_;
        bool is_loaded_ = false;

        // 文件数据
        std::vector<uint8_t> m_file_data;





        // EXTH记录
        PDBHeader m_pdb_header;
        MobiHeader m_mobi_header;
        PalmDocHeader m_palm_doc_header;
        ExthHeader m_exth_header;


        std::string m_full_name;
        std::vector<IndxHeader> m_indx_list;
        std::vector<TagxHeader> m_tagx_list;
        FlisRecord m_flis_record;
        FcisRecord m_fcis_record;
        EofRecord m_eof_record;
        SrcsRecord m_srcs_record;


        // 内容记录
        std::vector<RecordInfo> m_record_info_list;
        std::map<uint32_t, std::string> m_exth_records;
        std::vector<std::vector<uint8_t>> content_records_;
        std::vector<std::vector<uint8_t>> image_records_;

        // OCF包信息
        OCFPackage m_ocf_package;


        uint32_t m_palm_doc_header_offset = 0;
        uint32_t m_mobi_header_offset = 0;
        uint32_t m_exth_header_offset = 0;







        uint32_t readForwardVarWidthInt(const uint8_t* data, uint32_t data_size, uint32_t& offset);


        std::vector<uint8_t> readVariableWidthInteger(const uint8_t* data, uint32_t& offset, bool forward_encoded) const;
        uint32_t decodeVariableWidthInteger(const std::vector<uint8_t>& bytes) const;





        // 字节序转换函数声明
        static IndxHeader swapIndxHeader(const IndxHeader& header);
        static TagxHeader swapTagxHeader(const TagxHeader& header);


        // 辅助函数
        std::string getIndexTypeName(uint32_t type) const;
        std::string getEncodingNameFromCode(uint32_t encoding) const;

        // 私有方法
        bool parsePDBHeader();
        bool parseRecordIndices();

        bool parseContentRecords();
        bool parseRecord0();
        bool parseExthRecords(uint32_t start_offset, uint32_t data_length, uint32_t record_count);
        bool parseSpecialRecords();  // 解析FLIS/FCIS/EOF等特殊记录

        // 文本处理
        std::string decodeText(const uint8_t* data, uint32_t length);
        std::vector<uint8_t> decompressPalmDoc(const uint8_t* data, uint32_t length);


        std::string getFullName() const;

        std::string getExthRecord(ExthRecordType);

        // OCF包设置方法
        void setupOCFPackage();

        // 字节序转换函数
        static uint16_t swapUint16(uint16_t value);
        static uint32_t swapUint32(uint32_t value);
        static int32_t swapInt32(int32_t value);
        static uint64_t swapUint64(uint64_t value);

        // 结构体转换函数
        static PalmDocHeader swapPalmDocHeader(const PalmDocHeader& header);
        static MobiHeader swapMobiHeader(const MobiHeader& header);
        PDBHeader swapPDBHeader(const PDBHeader& header);

        RecordInfo swapRecordInfo(const RecordInfo& index);
        static ExthHeader swapExthHeader(const ExthHeader& header);
        static ExthRecord swapExthRecord(const ExthRecord& record);

        FlisRecord swapFlisRecord(const FlisRecord& record);

        FcisRecord swapFcisRecord(const FcisRecord& record);

        ImageRecordIndex swapImageRecordIndex(const ImageRecordIndex& index);

        std::string convertPalmTime(uint32_t palm_time) const;



        void printPDBHeader() const;

        void printRecordInfoStat() const;


        void printRecordInfoList() const;

        void printPalmDocHeader() const;

        void printMobiHeader() const;

        void printExthHeader() const;

        void printExthRecords() const;

        void printRecord0() const;

        std::string getTypeDescription(const std::string& type) const;

        std::string getCreatorDescription(const std::string& creator) const;

        void printField(const std::string& label, const std::string& value, int label_width) const;

        void printHexDecField(const std::string& label, uint32_t value, int label_width) const;

        std::string formatFileSize(size_t bytes) const;

        std::string getCompressionName(uint16_t compression) const;

        std::string getEncryptionName(uint16_t  encryption) const;

        std::string getMobiTypeName(uint32_t  mobi_type) const;

        std::string getEncodingName(uint32_t  encoding) const;

        std::string getLocaleName(uint32_t locale) const;

        std::string getExthTypeName(uint32_t  type) const;

        bool validatePalmDocHeader() const;

        bool validateMobiHeader() const;

        std::string toHexString(uint32_t value) const;

        std::string formatExthValue(uint32_t type, const std::string& data) const;

        uint32_t getRecordLength(uint32_t record_index) const;

        uint32_t readBackwardVarWidthInt(const uint8_t* data, uint32_t data_size, uint32_t& offset);


    public:
        MobiBook() = default;
        virtual ~MobiBook() { clear(); }

        // Book接口实现
        bool load(const std::string& mobi_path) override;
        std::vector<uint8_t> get_binary(std::string base_url, std::string url) override;
        std::string get_string(const std::string& path) override;
        std::string get_book_path() override { return book_path_; }
        std::string get_current_dir() override { return current_dir_; }
        std::string get_chapter_name_by_id(int spine_id) override;
        std::string get_title() override { return getExthRecord(EXTH_UPDATED_TITLE); }
        std::string get_author() override { return getExthRecord(EXTH_AUTHOR); }
        std::string get_version() override { return m_ocf_package.meta["version"]; }
        std::vector<OCFRef>& get_spine() override { return m_ocf_package.spine; }
        OCFPackage& get_ocf_package() override { return m_ocf_package; }
        bool has_script() override { return false; }
        bool has_font() override { return false; }
        bool has_css() override { return false; }
        void clear() override;
        std::string resolve_path(std::string base_url, std::string href) override;

        // Mobi特有方法
        bool isLoaded() const { return is_loaded_; }
        size_t getFileSize() const { return m_file_data.size(); }
        std::string getPublisher() { return getExthRecord(EXTH_PUBLISHER); }
        std::string getISBN() { return getExthRecord(EXTH_ISBN); }

    private:
        bool readFile(const std::string& path);
        std::string normalizePath(const std::string& path);
    };



} // namespace mobi

#endif // MOBI_BOOK_H