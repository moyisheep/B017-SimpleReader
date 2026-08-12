#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>


#include <sqlite3.h>

struct BookRecord {
    int64_t id = -1;                       // 数据库主键；-1 表示未找到
    std::string path;
    std::string title;
    std::string author;
    int         openCount = 0;
    int         totalWords = 0;
    int         lastSpineId = 0;
    int         lastOffset = 0;
    int         fontSize = 0;
    float       lineHeightMul = 0.0f;
    int         docWidth = 0;
    int         totalTime = 0;        // 累计阅读秒数
    int64_t     lastOpenTimestamp = 0;        // 微秒
    bool        enableCSS = true;
    bool        enableGlobalCSS = true;
    bool        enableCustomFont = false;
    std::string   fontName = "Verdana";
    float zoomFactor;

};
struct SettingRecord
{
    bool enableLoadEPUBFonts = true;
    bool enableScrollAnimation = false;
    bool enableHoverPreview = true;
    bool enableClickPreview = true;
    bool enableFontRealtimePreview = true;
    bool displayTOC = true;
    bool displayStatusBar = true;
    bool displayScrollBar = true;
    bool displayFrameRate = true;
};
struct timeFragment
{
    std::string path;
    std::string title;
    std::string author;
    int       spine_id;
    std::string chapter;
    int64_t timestamp;
};

class ReadingRecorder {
public:
    ReadingRecorder(std::filesystem::path dbPath);
    ~ReadingRecorder();

    void openBook(const std::string absolutePath); // 返回记录（读或建）
    void flush();
    void flushSettingRecord();
    // 一次性写回
    void flushBookRecord();
    void flushTimeRecord();
    int64_t getTotalTime();
    int64_t getBookTotalTime() const;

    BookRecord m_book_record;
    SettingRecord m_setting_record;
    std::vector<timeFragment> m_time_frag;
private:
    void initDB(std::filesystem::path dbPath);

    bool loadSettings();

    void LogPrint(std::string txt) { std::cout << "[ReadingRecord]"<< txt; }

    sqlite3* m_dbBook = nullptr;
    sqlite3* m_dbTime = nullptr;
    sqlite3* m_dbSetting = nullptr;





};
