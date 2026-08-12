#pragma once

#include <queue>
#include <atomic>
#include <mutex>
#include <vector>

#include <litehtml.h>
#include <gumbo.h>


#include "Constants.h"
#include "Book.h"

class BusyGuard {
public:
    explicit BusyGuard(std::atomic<bool>& flag) : m_flag(flag) {
        m_flag.store(true, std::memory_order_relaxed);
    }
    ~BusyGuard() {
        //OutputDebugStringA("BusyGuard::~BusyGuard() - workerBusy = false\n");
        m_flag.store(false, std::memory_order_relaxed);
    }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;
private:
    std::atomic<bool>& m_flag;
};


class VirtualDoc {
public:
    VirtualDoc();
    ~VirtualDoc();
    void load_book();
    void OnTreeSelChanged(std::string href);
    void update_doc(int client_h);
    void load_html(std::string& href);
    void clear();
    ScrollPosition get_scroll_position();
    void set_scroll_position(ScrollPosition sp);
    std::vector<HtmlBlock> m_blocks;
    float get_height_by_id(int spine_id);
    void reload();
    bool exists(int spine_id);
    //void draw(int x, int y, int w, int h, float offsetY);
    litehtml::document::ptr m_doc;
    std::atomic<bool>        m_isReloading{ false };
    std::atomic<bool>        m_isAnchor{ false };
    float m_percent = 0.0;
    float  m_height = 0.0f;
    std::string m_anchor_id = "";
    std::atomic<bool>        m_workerBusy{ false }; // 是否正在干活
private:
    HtmlBlock get_html_block(std::string html, int spine_id);
    void merge_block(HtmlBlock& dst, HtmlBlock& src, bool isAddToBottom = true);
    int get_id_by_href(std::string& href);
    std::string get_href_by_id(int spine_id);
    std::string get_head(std::string& html);
    std::vector<BodyBlock> get_body_blocks(std::string& html, int spine_id = 0, size_t max_chunk_bytes = 4 * 1024);
    void serialize_node(const GumboNode* node, std::ostream& out);
    bool gumbo_tag_is_void(GumboTag tag);
    void serialize_element(const GumboElement& el, std::ostream& out);




    bool insert_next_chapter();

    void workerLoop();


    float get_height();
    bool insert_chapter(int spine_id, bool isPushBack = true);
    //bool insert_chapter(int spine_id);
    bool insert_prev_chapter();


    bool load_by_id(int spine_id, bool isPushBack);
    struct DocCache
    {
        litehtml::document::ptr doc;
        float height;
        int spine_id;
    };
    std::vector<OCFRef> m_spine;
    std::shared_ptr<Book> m_book;
    std::shared_ptr<SimpleContainer> m_container;
    //std::vector<DocCache> m_doc_cache;

    // 放在 VirtualDoc 内，仅这 5 个
    std::thread              m_worker;          // 后台线程
    std::mutex               m_taskMtx;         // 任务队列锁
    std::condition_variable  m_taskCv;          // 任务通知
    struct Task {
        int  chapterId;
        bool insertAtFront;   // true=prev, false=next
    };
    std::queue<Task>         m_taskQueue;       // 待处理任务

    std::condition_variable m_cvFinish;
    std::atomic<bool> m_cancelFlag{ false };
};

