#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <Shlobj.h>      // SHGetKnownFolderPath
#include <mutex>

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
struct TimeFragment
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
    ReadingRecorder(const std::string saveDir);
    ~ReadingRecorder();

    void openBook(const std::string absolutePath); // 返回记录（读或建）
    void flush();
    void flushSettingRecord();
    // 一次性写回
    void flushBookRecord();
    void flushTimeRecord();
    BookRecord& getBookRecord();
    SettingRecord& getSettingRecord();
    void pushTimeFrag(TimeFragment frag);
    void updateRecord();
    int64_t getTotalTime();
    int64_t getBookTotalTime() const;

    BookRecord m_book_record;
    SettingRecord m_setting_record;
private:
    std::string m_base_dir = "";
    void initDB();

    bool loadSettings();



    sqlite3* m_dbBook = nullptr;
    sqlite3* m_dbTime = nullptr;
    sqlite3* m_dbSetting = nullptr;

    std::vector<TimeFragment> m_time_frag;
    mutable std::mutex m_mutex;


};
