#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <iostream>
#include <sstream>
#include <mutex>
#include <queue>

#include <gumbo.h>
#include <litehtml.h>

#include "a2w_w2a.h"
#include "EPUBBook.h"
#include "SimpleContainer.h"

struct ScrollPosition
{
    int spine_id = 0;
    float offset = 0.0f;
    float height = 0.0f;
};


struct BodyBlock {
    int spine_id = 0;
    int block_id = 0;
    std::string html;
    float height = 0.0f; // 未渲染前默认 -1

};

//struct HtmlBlock {
//    int spine_id;
//    float height = 0.0f;
//    std::string head;
//    std::vector<BodyBlock> body_blocks;
//};
struct HtmlBlock {
    int spine_id;
    std::string html;

};

struct DocBlock {
    int spine_id;
    litehtml::document::ptr doc;
};


class HtmlBlockColl {
public:
    void push_back(const HtmlBlock& block);
    void push_front(const HtmlBlock& block);
    HtmlBlock get(int spine_id) ;

    size_t size() const;

    bool empty();
    HtmlBlock& front();
    HtmlBlock& back();
    void clear();
    bool exists(int spine_id);
private:
    mutable std::mutex m_mutex;
    std::vector<HtmlBlock> m_blocks;
};

class DocBlockColl
{
public:
    void push_back( DocBlock block);
    void push_front( DocBlock block);
    size_t size() const;

    bool empty();
    DocBlock& front();
    DocBlock& back();
    void clear();
    bool exists(int spine_id);
    float height();
    float height(int spine_id);
    DocBlock get(ScrollPosition sp);

    // 迭代器支持
    std::vector<DocBlock> get_snapshot() const;
private:
    mutable std::mutex m_mutex;
    std::vector<DocBlock> m_blocks;
};

class SafeScrollPosition
{
public:
 
    ScrollPosition get_position() ;
    void set_position(ScrollPosition pos);
    void clear();
private:
    mutable std::mutex m_mutex;
    ScrollPosition m_sp{};
};
class VirtualDoc {
public:
    VirtualDoc();
    ~VirtualDoc();
    void load_book(std::shared_ptr<EPUBBook> book, std::shared_ptr<SimpleContainer> container, HWND hwnd);
    void OnTreeSelChanged(std::wstring href);
    //void update_doc(int client_h, float offsetY);
    void load_html(std::wstring& href);
    void clear();
    ScrollPosition get_scroll_position() ;
    void set_scroll_position(ScrollPosition sp);
    
    void reload();
    bool exists(int spine_id);
    void present(int width, int height);

    void convert_coordinate(int& x, int& y, ScrollPosition sp);

    void on_lbutton_down(int x, int y);
    void on_lbutton_up(int x, int y);
    void on_lbutton_dblclk(int x, int y);
    void on_mouse_move(int x, int y);
    void on_mouse_wheel(int delta);
    void on_mouse_leave(int x, int y);
    void set_document_width(int width);
private:
    //HtmlBlock get_html_block(std::string html, int spine_id);
    //void merge_block(HtmlBlock& dst, HtmlBlock& src, bool isAddToBottom = true);
    int get_id_by_href(std::wstring& href);
    std::wstring get_href_by_id(int spine_id);
    std::string get_head(std::string& html);
    std::vector<BodyBlock> get_body_blocks(std::string& html, int spine_id = 0, size_t max_chunk_bytes = 4 * 1024);
    void serialize_node(const GumboNode* node, std::ostream& out);
    bool gumbo_tag_is_void(GumboTag tag);
    void serialize_element(const GumboElement& el, std::ostream& out);




    //bool insert_next_chapter();

    void workerLoop();


    float get_height();
    bool insert_chapter(int spine_id, bool insertAtFront = false);
    //bool insert_prev_chapter();


    bool load_by_id(int spine_id, bool isPushBack);
    struct DocCache
    {
        litehtml::document::ptr doc;
        float height;
        int spine_id;
    };
    std::vector<OCFRef> m_spine;
    std::shared_ptr<EPUBBook> m_book;
    std::shared_ptr<SimpleContainer> m_container;
    HWND m_hwnd = nullptr;
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

    std::atomic<float> m_offsetY = 0.0f;
    std::atomic<float>  m_height{ 0.0f };
    DocBlockColl m_docs;
    std::atomic<bool>        m_isReloading{ false };
    std::atomic<bool>        m_isAnchor{ false };
    HtmlBlockColl m_blocks;
    SafeScrollPosition m_sp;
    float m_percent = 0.0;

    std::string m_anchor_id = "";
    std::atomic<bool>        m_workerBusy{ false }; // 是否正在干活
    std::atomic<int>      m_doc_width{ 600};
};




class BusyGuard {
public:
    explicit BusyGuard(std::atomic<bool>& flag) : m_flag(flag) {
        m_flag.store(true, std::memory_order_relaxed);
    }
    ~BusyGuard() {
        OutputDebugStringA("BusyGuard::~BusyGuard() - workerBusy = false\n");
        m_flag.store(false, std::memory_order_relaxed);
    }
    BusyGuard(const BusyGuard&) = delete;
    BusyGuard& operator=(const BusyGuard&) = delete;
private:
    std::atomic<bool>& m_flag;
};