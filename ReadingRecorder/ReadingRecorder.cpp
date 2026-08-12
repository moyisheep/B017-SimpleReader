#include "ReadingRecorder.h"

static int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}


/* ---------- 构造/析构 ---------- */
ReadingRecorder::ReadingRecorder(std::filesystem::path dbPath)
{
    m_book_record = {};
    m_time_frag = {};
    m_setting_record = {};
    initDB(dbPath);

}
ReadingRecorder::~ReadingRecorder()
{
    if (m_dbBook) sqlite3_close(m_dbBook);
    if (m_dbTime) sqlite3_close(m_dbTime);
    if (m_dbSetting) sqlite3_close(m_dbSetting);
}

/* ---------- 初始化数据库 ---------- */
void ReadingRecorder::initDB(std::filesystem::path dbPath) {
    namespace fs = std::filesystem;
  
    fs::create_directories(dbPath);
    fs::path db_book_path = dbPath / "Books.db";
    fs::path db_time_path = dbPath / "Time.db";
    fs::path db_setting_path = dbPath / "Settings.db";

    /* ---------- Books.db ---------- */
    if (sqlite3_open(db_book_path.generic_string().c_str(), &m_dbBook) != SQLITE_OK) 
    { 
        LogPrint("Books.db sqlite open failed\n");
        return; 
    }
       

    sqlite3_exec(m_dbBook, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS books(
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            path             TEXT UNIQUE,
            title            TEXT,
            author           TEXT,
            open_count       INTEGER DEFAULT 0,
            total_words      INTEGER DEFAULT 0,
            last_spine_id    INTEGER DEFAULT 0,
            last_offset      INTEGER DEFAULT 0,
            font_size        INTEGER DEFAULT 0,
            line_height_mul  REAL    DEFAULT 0.0,
            doc_width        INTEGER DEFAULT 0,
            total_time_s     INTEGER DEFAULT 0,
            first_open_us    INTEGER DEFAULT 0,
            last_open_us     INTEGER DEFAULT 0,
            enable_epub_css        INTEGER DEFAULT 1,
            enable_global_css      INTEGER DEFAULT 0,
            enable_custom_font     INTEGER DEFAULT 0,
            custom_font_name       TEXT
        );
    )";
    sqlite3_exec(m_dbBook, sql, nullptr, nullptr, nullptr);

    /* ---------- Time.db ---------- */
    if (sqlite3_open(db_time_path.generic_string().c_str(), &m_dbTime) != SQLITE_OK)
    {
        LogPrint("Time.db sqlite open failed\n");
        return;
    }
        
    sqlite3_exec(m_dbTime, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    const char* sqlTime = R"(
        CREATE TABLE IF NOT EXISTS reading_time(
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            path     TEXT,
            title         TEXT,
            authors       TEXT,
            spine_id      INTEGER,
            current_chapter TEXT,
            start_time    REAL,
            end_time      REAL,
            duration      INTEGER
        );
    )";
    sqlite3_exec(m_dbTime, sqlTime, nullptr, nullptr, nullptr);

    /* ---------- Settingss.db ---------- */
    if (sqlite3_open(db_setting_path.generic_string().c_str(), &m_dbSetting) != SQLITE_OK)
    {
        LogPrint("Settings.db sqlite open failed\n");
    }
       

    sqlite3_exec(m_dbSetting, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    const char* sqlSetting = R"(
        CREATE TABLE IF NOT EXISTS settings(
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            enable_load_epub_fonts        INTEGER DEFAULT 1,
            enable_scroll_animation       INTEGER DEFAULT 0,
            enable_hover_preview          INTEGER DEFAULT 1,
            enable_click_preview          INTEGER DEFAULT 1,
            enable_font_realtime_preview INTEGER DEFAULT 1,
            diaplay_toc                  INTEGER DEFAULT 1,
            display_status_bar          INTEGER DEFAULT 1,
            display_scroll_bar            INTEGER DEFAULT 1,
            display_frame_rate            INTEGER DEFAULT 1
        );
    )";
    sqlite3_exec(m_dbSetting, sqlSetting, nullptr, nullptr, nullptr);
    loadSettings();
}

bool ReadingRecorder::loadSettings() {
    if (!m_dbSetting) {
        LogPrint("Settings database not initialized\n");
        return false;
    }

    const char* sql = "SELECT * FROM settings LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_dbSetting, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogPrint("Failed to prepare SQL statement for settings\n");
        return false;
    }

    // Initialize with default values in case the query returns no rows
    m_setting_record = SettingRecord();

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Column indices (adjust if your table structure changes)
        m_setting_record.enableLoadEPUBFonts = sqlite3_column_int(stmt, 1) != 0;
        m_setting_record.enableScrollAnimation = sqlite3_column_int(stmt, 2) != 0;
        m_setting_record.enableHoverPreview = sqlite3_column_int(stmt, 3) != 0;
        m_setting_record.enableClickPreview = sqlite3_column_int(stmt, 4) != 0;
        m_setting_record.enableFontRealtimePreview = sqlite3_column_int(stmt, 5) != 0;
        m_setting_record.displayTOC = sqlite3_column_int(stmt, 6) != 0;
        m_setting_record.displayStatusBar = sqlite3_column_int(stmt, 7) != 0;
        m_setting_record.displayScrollBar = sqlite3_column_int(stmt, 8) != 0;
        m_setting_record.displayFrameRate = sqlite3_column_int(stmt, 9) != 0;
    }

    sqlite3_finalize(stmt);
    return true;
}
/* ---------- 打开书 ---------- */
void ReadingRecorder::openBook(const std::string absolutePath) {
    m_book_record = {};
    m_time_frag = {};
    BookRecord rec;
    rec.path = absolutePath;

    const char* select = "SELECT * FROM books WHERE path=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_dbBook, select, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, absolutePath.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // 已存在
        rec.id = sqlite3_column_int64(stmt, 0);
        auto colText = [](sqlite3_stmt* s, int idx) -> std::string {
            const char* p = reinterpret_cast<const char*>(sqlite3_column_text(s, idx));
            return p ? std::string(p, sqlite3_column_bytes(s, idx)) : "";
        };
        rec.title = colText(stmt, 2);
        rec.author = colText(stmt, 3);
        rec.openCount = sqlite3_column_int(stmt, 4);
        rec.totalWords = sqlite3_column_int(stmt, 5);
        rec.lastSpineId = sqlite3_column_int(stmt, 6);
        rec.lastOffset = sqlite3_column_int(stmt, 7);
        rec.fontSize = sqlite3_column_int(stmt, 8);
        rec.lineHeightMul = static_cast<float>(sqlite3_column_double(stmt, 9));
        rec.docWidth = sqlite3_column_int(stmt, 10);
        rec.totalTime = sqlite3_column_int(stmt, 11);
        rec.lastOpenTimestamp = sqlite3_column_int64(stmt, 13);
        rec.enableCSS = sqlite3_column_int(stmt, 14);
        rec.enableGlobalCSS = sqlite3_column_int(stmt, 15);
        rec.enableCustomFont = sqlite3_column_int(stmt, 16);
        rec.fontName = colText(stmt, 17);
    }
    else {
        // 新书：用当前 g_book 状态插入
        const char* insert = R"(
            INSERT INTO books(path, first_open_us, last_open_us)
            VALUES(?, ?, ?);
        )";
        sqlite3_prepare_v2(m_dbBook, insert, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, absolutePath.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, nowUs());
        sqlite3_bind_int64(stmt, 3, nowUs());
        sqlite3_step(stmt);

        rec.id = sqlite3_last_insert_rowid(m_dbBook);

    }
    sqlite3_finalize(stmt);

    // 更新打开次数 & 最后打开时间
    const char* update = "UPDATE books SET open_count=open_count+1, last_open_us=? WHERE id=?;";
    sqlite3_prepare_v2(m_dbBook, update, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, nowUs());
    sqlite3_bind_int64(stmt, 2, rec.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    m_book_record = std::move(rec);
}
int64_t ReadingRecorder::getTotalTime()
{
    const char* sql = "SELECT COALESCE(SUM(duration),0) FROM reading_time;";
    sqlite3_stmt* stmt = nullptr;
    int64_t totalUs = 0;

    if (sqlite3_prepare_v2(m_dbTime, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            totalUs = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return totalUs / 1'000'000;   // 返回秒
}
int64_t ReadingRecorder::getBookTotalTime() const
{

    return m_book_record.totalTime;
}
/* ---------- 写入 ---------- */
void ReadingRecorder::flush() {
    if (m_book_record.id < 0) return;   // 无效记录

    flushBookRecord();
    flushTimeRecord();
    flushSettingRecord();
}

void ReadingRecorder::flushSettingRecord() {
    if (!m_dbSetting) {
        LogPrint("Settings database not initialized\n");
        return;
    }

    // First, check if there's any existing record
    const char* checkSql = "SELECT COUNT(*) FROM settings;";
    sqlite3_stmt* checkStmt = nullptr;
    int count = 0;

    if (sqlite3_prepare_v2(m_dbSetting, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            count = sqlite3_column_int(checkStmt, 0);
        }
        sqlite3_finalize(checkStmt);
    }

    // Prepare the appropriate SQL statement (INSERT or UPDATE)
    const char* sql;
    if (count == 0) {
        sql = R"(
            INSERT INTO settings (
                enable_load_epub_fonts,
                enable_scroll_animation,
                enable_hover_preview,
                enable_click_preview,
                enable_font_realtime_preview,
                diaplay_toc,
                display_status_bar,
                display_scroll_bar,
                display_frame_rate
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    }
    else {
        sql = R"(
            UPDATE settings SET
                enable_load_epub_fonts = ?,
                enable_scroll_animation = ?,
                enable_hover_preview = ?,
                enable_click_preview = ?,
                enable_font_realtime_preview = ?,
                diaplay_toc = ?,
                display_status_bar = ?,
                display_scroll_bar = ?,
                display_frame_rate = ?
            WHERE id = 1;
        )";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_dbSetting, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogPrint("Failed to prepare SQL statement for settings update\n");
        return;
    }

    // Bind parameters
    sqlite3_bind_int(stmt, 1, m_setting_record.enableLoadEPUBFonts ? 1 : 0);
    sqlite3_bind_int(stmt, 2, m_setting_record.enableScrollAnimation ? 1 : 0);
    sqlite3_bind_int(stmt, 3, m_setting_record.enableHoverPreview ? 1 : 0);
    sqlite3_bind_int(stmt, 4, m_setting_record.enableClickPreview ? 1 : 0);
    sqlite3_bind_int(stmt, 5, m_setting_record.enableFontRealtimePreview ? 1 : 0);
    sqlite3_bind_int(stmt, 6, m_setting_record.displayTOC ? 1 : 0);
    sqlite3_bind_int(stmt, 7, m_setting_record.displayStatusBar ? 1 : 0);
    sqlite3_bind_int(stmt, 8, m_setting_record.displayScrollBar ? 1 : 0);
    sqlite3_bind_int(stmt, 9, m_setting_record.displayFrameRate ? 1 : 0);

    // Execute the statement
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LogPrint("Failed to execute settings update\n");
    }

    sqlite3_finalize(stmt);
}
void ReadingRecorder::flushBookRecord()
{
    auto& rec = m_book_record;
    const char* sql = R"(
        UPDATE books SET
            title           = ?,
            author          = ?,
            total_words     = ?,
            last_spine_id   = ?,
            last_offset     = ?,
            font_size       = ?,
            line_height_mul = ?,
            doc_width       = ?, 
            total_time_s    = ?,
            enable_epub_css       = ?,
            enable_global_css      = ?,
            enable_custom_font   = ?,
            custom_font_name      = ?
        WHERE id = ?;
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_dbBook, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, rec.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, rec.author.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, rec.totalWords);
    sqlite3_bind_int(stmt, 4, rec.lastSpineId);
    sqlite3_bind_int(stmt, 5, rec.lastOffset);
    sqlite3_bind_int(stmt, 6, rec.fontSize);
    sqlite3_bind_double(stmt, 7, rec.lineHeightMul);
    sqlite3_bind_double(stmt, 8, rec.docWidth);
    sqlite3_bind_int(stmt, 9, rec.totalTime);
    sqlite3_bind_int(stmt, 10, rec.enableCSS);
    sqlite3_bind_int(stmt, 11, rec.enableGlobalCSS);
    sqlite3_bind_int(stmt, 12, rec.enableCustomFont);
    sqlite3_bind_text(stmt, 13, rec.fontName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 14, rec.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ReadingRecorder::flushTimeRecord()
{
    if (m_time_frag.empty()) return;

    /* 0. 先把缓存拿出来，防止 flush 期间又被写入 */
    std::vector<timeFragment> batch = std::move(m_time_frag);
    m_time_frag.clear();                 // 立即清空原缓存

    /* 1. 按时间升序 */
    std::sort(batch.begin(), batch.end(),
        [](const timeFragment& a, const timeFragment& b)
        { return a.timestamp < b.timestamp; });

    /* 2. 事务开始 */
    char* err = nullptr;
    if (sqlite3_exec(m_dbTime, "BEGIN;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        LogPrint(("BEGIN failed: " + std::string(err) + "\n").c_str());
        sqlite3_free(err);
        return;
    }

    constexpr int64_t MERGE_THRESHOLD_US = 2'000'000;

    for (const timeFragment& frag : batch)
    {
        /* 3. 查询最近一条 */
        const char* sqlSel = R"(
            SELECT id, end_time, duration
            FROM reading_time
            WHERE path = ? AND current_chapter = ? AND spine_id = ?
            ORDER BY end_time DESC
            LIMIT 1;
        )";
        sqlite3_stmt* sel = nullptr;
        if (sqlite3_prepare_v2(m_dbTime, sqlSel, -1, &sel, nullptr) != SQLITE_OK)
        {
            LogPrint(("prepare SELECT failed\n"));
            continue;
        }
        sqlite3_bind_text(sel, 1, frag.path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(sel, 2, frag.chapter.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(sel, 3, frag.spine_id);

        bool merged = false;
        if (sqlite3_step(sel) == SQLITE_ROW)
        {
            int     oldId = sqlite3_column_int(sel, 0);
            int64_t oldEnd = sqlite3_column_int64(sel, 1);
            if (frag.timestamp - oldEnd <= MERGE_THRESHOLD_US)
            {
                const char* sqlUpd = R"(
                    UPDATE reading_time
                    SET end_time = ?,
                        duration = duration + (? - end_time)
                    WHERE id = ?;
                )";
                sqlite3_stmt* upd = nullptr;
                if (sqlite3_prepare_v2(m_dbTime, sqlUpd, -1, &upd, nullptr) == SQLITE_OK)
                {
                    sqlite3_bind_int64(upd, 1, frag.timestamp);
                    sqlite3_bind_int64(upd, 2, frag.timestamp);
                    sqlite3_bind_int(upd, 3, oldId);
                    if (sqlite3_step(upd) != SQLITE_DONE)
                    {
                        LogPrint(("UPDATE step failed\n"));
                    }
                       
                    sqlite3_finalize(upd);
                }
                else
                {
                    LogPrint(("prepare UPDATE failed\n"));
                }
                merged = true;
            }
        }
        sqlite3_finalize(sel);

        if (!merged)
        {
            const char* sqlIns = R"(
                INSERT INTO reading_time
                (path, title, authors, spine_id, current_chapter,
                 start_time, end_time, duration)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?);
            )";
            sqlite3_stmt* ins = nullptr;
            if (sqlite3_prepare_v2(m_dbTime, sqlIns, -1, &ins, nullptr) == SQLITE_OK)
            {
                sqlite3_bind_text(ins, 1, frag.path.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(ins, 2, frag.title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(ins, 3, frag.author.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(ins, 4, frag.spine_id);
                sqlite3_bind_text(ins, 5, frag.chapter.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int64(ins, 6, frag.timestamp);
                sqlite3_bind_int64(ins, 7, frag.timestamp);
                sqlite3_bind_int64(ins, 8, 0);

                if (sqlite3_step(ins) != SQLITE_DONE)
                {
                    LogPrint(("INSERT step failed\n"));
                }
                    
                sqlite3_finalize(ins);
            }
            else
            {
                LogPrint(("prepare INSERT failed\n"));
            }
        }
    }

    /* 4. 提交事务 */
    if (sqlite3_exec(m_dbTime, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK)
    {
        LogPrint(("COMMIT failed: " + std::string(err) + "\n").c_str());
        sqlite3_free(err);
    }
}

