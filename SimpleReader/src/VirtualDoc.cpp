#include "VirtualDoc.h"


// ---------- 点击目录跳转 ----------
void VirtualDoc::OnTreeSelChanged(std::wstring href)
{
    if (href.empty()) return;


    /* 1. 分离文件路径与锚点 */

    size_t pos = href.find(L'#');
    std::wstring file_path = (pos == std::wstring::npos) ? href : href.substr(0, pos);
    int spine_id = get_id_by_href(file_path);
    m_anchor_id = (pos == std::wstring::npos) ? "" :
        w2a(href.substr(pos + 1));

    if (spine_id != get_scroll_position().spine_id)
    {
        clear();
        insert_chapter(spine_id);

        m_isAnchor.store(m_anchor_id.empty() ? false : true);

    }
    else
    {
        if (!m_anchor_id.empty())
        {
            std::wstring cssSel = a2w(m_anchor_id);   // 转成宽字符
            // WM_APP + 3 约定为“跳转到锚点选择器”
            //PostMessageW(g_hView, WM_EPUB_ANCHOR,
            //    reinterpret_cast<WPARAM>(_wcsdup(cssSel.c_str())), 0);
        }
    }



}





VirtualDoc::VirtualDoc()
{
    m_worker = std::thread(&VirtualDoc::workerLoop, this);
}

VirtualDoc::~VirtualDoc()
{

    m_worker.detach();   // 或 join，取决于生命周期
    clear();

}

void VirtualDoc::load_book(std::shared_ptr<EPUBBook> book, 
    std::shared_ptr<SimpleContainer> container, 
    HWND hwnd)
{
    m_book = book;
    m_container = container;
    m_hwnd = hwnd;
    m_spine = m_book->ocf_pkg_.spine;
}


// ---------- 分页 ----------
std::wstring VirtualDoc::get_href_by_id(int id)
{

    if (id < m_spine.size() && id >= 0)
    {
        return m_spine[id].href;
    }
    return L"";
}

int VirtualDoc::get_id_by_href(std::wstring& href)
{
    for (int i = 0; i < m_spine.size(); i++)
    {
        if (m_spine[i].href == href) {
            return i;
        }
    }
    return -1;
}

//void VirtualDoc::merge_block(HtmlBlock& dst, HtmlBlock& src, bool isAddToBottom)
//{
//
//    dst.head = src.head;
//    // 2. 把新的 body_blocks 追加到尾部
//    if (isAddToBottom)
//    {
//
//        dst.body_blocks.insert(
//            dst.body_blocks.end(),
//            src.body_blocks.begin(),
//            src.body_blocks.end());
//
//    }
//    // 追加到顶部
//    else
//    {
//        dst.body_blocks.insert(
//            dst.body_blocks.begin(),
//            src.body_blocks.begin(),
//            src.body_blocks.end());
//    }
//}
//
//
//HtmlBlock VirtualDoc::get_html_block(std::string html, int spine_id)
//{
//    HtmlBlock block;
//    block.spine_id = spine_id;
//    block.head = get_head(html);
//    block.body_blocks = get_body_blocks(html, spine_id);
//
//
//
//    BodyBlock bi;
//    bi.spine_id = spine_id;
//    bi.height = 0;
//    bi.html = "<div style = \"height:" + std::to_string(300) + "px; \"></div>";
//    bi.block_id = block.body_blocks.back().block_id + 1;
//    block.body_blocks.push_back(std::move(bi));
//
//
//
//    return block;
//}


// ---------- 工具：节点序列化 ----------
bool VirtualDoc::gumbo_tag_is_void(GumboTag tag)
{
    switch (tag)
    {
    case GUMBO_TAG_AREA:
    case GUMBO_TAG_BASE:
    case GUMBO_TAG_BR:
    case GUMBO_TAG_COL:
    case GUMBO_TAG_EMBED:
    case GUMBO_TAG_HR:
    case GUMBO_TAG_IMG:
    case GUMBO_TAG_INPUT:
    case GUMBO_TAG_LINK:
    case GUMBO_TAG_META:
    case GUMBO_TAG_PARAM:
    case GUMBO_TAG_SOURCE:
    case GUMBO_TAG_TRACK:
    case GUMBO_TAG_WBR:
        return true;
    default:
        return false;
    }
}

void VirtualDoc::serialize_element(const GumboElement& el, std::ostream& out) {
    out << '<' << gumbo_normalized_tagname(el.tag);
    for (unsigned int i = 0; i < el.attributes.length; ++i) {
        auto* attr = static_cast<GumboAttribute*>(el.attributes.data[i]);
        out << ' ' << attr->name << "=\"" << attr->value << '"';
    }
    if (el.children.length == 0 && gumbo_tag_is_void(el.tag)) {
        out << " />";
        return;
    }
    out << '>';
    for (unsigned int i = 0; i < el.children.length; ++i)
        serialize_node(static_cast<GumboNode*>(el.children.data[i]), out);
    out << "</" << gumbo_normalized_tagname(el.tag) << '>';
}

void VirtualDoc::serialize_node(const GumboNode* node, std::ostream& out) {
    if (!node) return;
    switch (node->type) {
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_CDATA:
        out << node->v.text.text;
        break;
    case GUMBO_NODE_WHITESPACE:
        out << node->v.text.text;
        break;
    case GUMBO_NODE_ELEMENT:
        serialize_element(node->v.element, out);
        break;
    default: break;
    }
}

// ---------- 1. 提取 <head> ----------
std::string VirtualDoc::get_head(std::string& html) {
    GumboOutput* out = gumbo_parse(html.c_str());
    std::string result;
    if (out->root->type == GUMBO_NODE_ELEMENT) {
        for (unsigned int i = 0; i < out->root->v.element.children.length; ++i) {
            auto* node = static_cast<GumboNode*>(out->root->v.element.children.data[i]);
            if (node->type == GUMBO_NODE_ELEMENT &&
                node->v.element.tag == GUMBO_TAG_HEAD) {
                std::ostringstream oss;
                serialize_element(node->v.element, oss);
                result = oss.str();
                break;
            }
        }
    }
    gumbo_destroy_output(&kGumboDefaultOptions, out);
    return result;
}

std::vector<BodyBlock>
VirtualDoc::get_body_blocks(std::string& html,
    int spine_id,
    size_t max_chunk_bytes)
{
    // 1. 用 string_view 避免拷贝
    std::string_view sv(html);

    // 2. 找到 <body ...> 和 </body>
    static const std::string_view body_tag = "<body";
    static const std::string_view body_end = "</body>";

    size_t body_open = sv.find(body_tag);
    if (body_open == std::string_view::npos) return {};

    body_open = sv.find('>', body_open);          // 跳过属性
    if (body_open == std::string_view::npos) return {};
    ++body_open;                                  // 指向 '>' 之后

    size_t body_close = sv.find(body_end, body_open);
    if (body_close == std::string_view::npos) return {};

    // 3. 直接取子串（零拷贝）
    std::string_view body_content = sv.substr(body_open, body_close - body_open);

    // 4. 构造唯一块
    return { BodyBlock{0, 0, std::string(body_content)} };
}
// ---------- 2. 切 <body> ----------
//std::vector<BodyBlock> VirtualDoc::get_body_blocks(std::string& html,
//    int spine_id,
//    size_t max_chunk_bytes) {
//    std::vector<BodyBlock> blocks;
//    GumboOutput* out = gumbo_parse(html.c_str());
//    GumboNode* body = nullptr;
//
//    // 找到 body
//    if (out->root->type == GUMBO_NODE_ELEMENT) {
//        for (unsigned int i = 0; i < out->root->v.element.children.length; ++i) {
//            auto* node = static_cast<GumboNode*>(out->root->v.element.children.data[i]);
//            if (node->type == GUMBO_NODE_ELEMENT &&
//                node->v.element.tag == GUMBO_TAG_BODY) {
//                body = node;
//                break;
//            }
//        }
//    }
//    if (!body) { gumbo_destroy_output(&kGumboDefaultOptions, out); return blocks; }
//
//    // 收集 body 的直接子节点
//    std::vector<const GumboNode*> nodes;
//    auto& children = body->v.element.children;
//    for (unsigned int i = 0; i < children.length; ++i)
//        nodes.emplace_back(static_cast<GumboNode*>(children.data[i]));
//
//    // 分块
//    std::ostringstream current;
//    size_t current_bytes = 0;
//    int block_id = 0;
//
//    auto flush = [&]() {
//        if (current.str().empty()) return;
//        BodyBlock bb;
//        bb.spine_id = spine_id;
//        bb.block_id = block_id++;
//        bb.html = current.str();
//        blocks.emplace_back(std::move(bb));
//        current.str("");
//        current.clear();
//        current_bytes = 0;
//        };
//
//    for (const GumboNode* n : nodes) {
//        std::ostringstream tmp;
//        serialize_node(n, tmp);
//        std::string frag = tmp.str();
//        if (current_bytes + frag.size() > max_chunk_bytes && !current.str().empty())
//            flush();
//        current << frag;
//        current_bytes += frag.size();
//    }
//    flush(); // 最后一块
//    gumbo_destroy_output(&kGumboDefaultOptions, out);
//    return blocks;
//}

void VirtualDoc::load_html(std::wstring& href)
{

    auto id = get_id_by_href(href);
    if (id < 0)
    {
        OutputDebugStringW(href.c_str());
        OutputDebugStringW(L" 未找到\n");
        return;
    }

    insert_chapter(id);


}


void VirtualDoc::reload()
{
    if (m_workerBusy) return;
    //if (g_cMain) { g_cMain->clear_selection(); }
    if (m_blocks.empty())
    {
        insert_chapter(0);
    }
    else
    {
        // 1. 记录当前滚动百分比
        ScrollPosition old = get_scroll_position();
        double percent = 0.0;
        if (old.height > 0.0f)          // 旧文档高度
            m_percent = double(old.offset) / old.height;


        clear();
        insert_chapter(old.spine_id);

        m_isReloading.store(true);
    }


    // 3. 把百分比换算成新的像素值

}
bool VirtualDoc::load_by_id(int spine_id, bool isPushBack)
{
    try
    {

        std::wstring href = get_href_by_id(spine_id);
        if (href.empty()) return false;


        std::string html = m_book->load_html(href);
        if (html.empty()) return false;


        //PreprocessHTML(html);          // 可能抛异常


        //auto block = get_html_block(html, spine_id);
        HtmlBlock block{ spine_id, html };
        if (isPushBack)
            m_blocks.push_back(std::move(block));
        else
            m_blocks.push_front(std::move(block));

        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA(("load_by_id exception: " +
            std::string(e.what()) + "\n").c_str());
    }
    catch (...)
    {
        OutputDebugStringA("load_by_id unknown exception\n");
    }
    return false;
}
ScrollPosition VirtualDoc::get_scroll_position() 
{

    return m_sp.get_position();

}
void VirtualDoc::set_scroll_position(ScrollPosition sp)
{
    //float offset = 0.0f;
    //for (const auto& bk : m_blocks)
    //{
    //    if (bk.spine_id == bk.spine_id)break;
    //    offset += bk.height;
    //}
    //offset += sp.offset;
    //g_offsetY.store(offset, std::memory_order_relaxed);
    m_sp.set_position(sp);
}


//void VirtualDoc::update_doc(int client_h, float offsetY)
//{
//    if (!m_book || !m_container) { return; }
//
//    m_offsetY = offsetY;
//    //float offsetY = g_offsetY.load(std::memory_order_relaxed);
//
//    OutputDebugStringA("[before] ");
//    OutputDebugStringA(std::to_string(offsetY).c_str());
//    OutputDebugStringA("\n");
//
//
//    if (offsetY < 0)
//    {
//        insert_prev_chapter();
//
//    }
//
//    if (offsetY > m_height - static_cast<float>(client_h) * 3.0f)
//    {
//        insert_next_chapter();
//
//    }
//
//
//    //ScrollPosition p = get_scroll_position();
//
//    //g_toc->SetHighlight(p);
//
//    //std::wstring spine_info = L"总进度：" + std::to_wstring(p.spine_id + 1) + L" / " + std::to_wstring(m_spine.size());
//    //std::wstring offset_info = L"当前进度：" + std::to_wstring((int)p.offset) + L" / " + std::to_wstring((int)p.height);
//    //SetStatus(STATUSBAR_SPINE_INFO, spine_info.c_str());
//    //SetStatus(STATUSBAR_OFFSET_INFO, offset_info.c_str());
//
//    //auto time_string = seconds2string(g_recorder->getBookTotalTime());
//    //SetStatus(STATUSBAR_TOTAL_TIME, (L"阅读时长：" + time_string).c_str());
//    //SetStatus(STATUSBAR_FONT_NAME, (L"自定义字体：" + g_cfg.font_name).c_str());
//    //SetStatus(STATUSBAR_FONT_SIZE, (L"字体大小：" + std::to_wstring(g_cfg.font_size)).c_str());
//    //SetStatus(STATUSBAR_LINE_HEIGHT, (L"行间距：" + std::to_wstring(g_cfg.line_height)).c_str());
//    //SetStatus(STATUSBAR_DOC_WIDTH, (L"文档宽度：" + std::to_wstring(g_cfg.document_width)).c_str());
//    //SetStatus(STATUSBAR_DOC_ZOOM, (L"文档缩放倍数：" + std::to_wstring(g_cMain->m_zoom_factor)).c_str());
//
//    //g_scrollbar->SetPosition(p.spine_id, p.height, p.offset);
//
//}


float VirtualDoc::get_height()
{
    return m_height.load(std::memory_order_acquire);
}
bool VirtualDoc::insert_chapter(int spine_id, bool insertAtFront)
{

    int id = spine_id;
    if (id < 0 || id >= static_cast<int>(m_spine.size()) || exists(id)) return false;

    {
        std::lock_guard<std::mutex> lk(m_taskMtx);
        auto temp_queue = m_taskQueue;
        while (!temp_queue.empty())
        {
            if (temp_queue.front().chapterId == id) { return false; }
            temp_queue.pop();
        }
        m_taskQueue.push({ id, insertAtFront });
    }
    m_taskCv.notify_one();
    return false;
}

//bool VirtualDoc::insert_prev_chapter()
//{
//    if (m_workerBusy.load(std::memory_order_relaxed)) return false;
//    int id = m_blocks.empty() ? 0 : m_blocks.front().spine_id - 1;
//    if (id < 0 || exists(id)) return false;
//
//    {
//        std::lock_guard<std::mutex> lk(m_taskMtx);
//        if (!m_taskQueue.empty()) return false;
//        m_taskQueue.push({ id, true });
//    }
//    m_taskCv.notify_one();
//    return false;
//}
//
//bool VirtualDoc::insert_next_chapter()
//{
//
//    if (m_workerBusy.load(std::memory_order_relaxed)) return false;
//    int id = m_blocks.empty() ? 0 : m_blocks.back().spine_id + 1;
//    if (id >= static_cast<int>(m_spine.size()) || exists(id)) return false;
//
//
//    {
//        std::lock_guard<std::mutex> lk(m_taskMtx);
//        if (!m_taskQueue.empty()) return false;
//        m_taskQueue.push({ id, false });
//    }
//    m_taskCv.notify_one();
//    return false;
//}
void VirtualDoc::workerLoop()
{
    while (true)
    {

        Task task;
        {
            std::unique_lock<std::mutex> lk(m_taskMtx);
            m_taskCv.wait(lk, [this] {
                /* 等待时也要能响应取消 */
                return !m_taskQueue.empty();
                });

            if (m_taskQueue.empty())
                continue;                // 虚假唤醒
            task = std::move(m_taskQueue.front());
            m_taskQueue.pop();
        }
        BusyGuard bg(m_workerBusy);   // 从这里开始置忙，析构时自动清 0

        OutputDebugStringA("[VirtualDod thread] 开始更新\n");
        // 1. 耗时 IO
                /* ---------- 2. 耗时 IO ---------- */



        if (!load_by_id(task.chapterId, !task.insertAtFront))
        {
            continue;
        }



        // 2. 组装 HTML

        HtmlBlock target = m_blocks.get(task.chapterId);

        auto doc = litehtml::document::createFromString(
            { target.html.c_str(), litehtml::encoding::utf_8 }, m_container.get());
        doc->render(m_doc_width.load(std::memory_order_acquire));
        /* ---------- 5. 计算高度 ---------- */

        auto height = m_height.load(std::memory_order_acquire);
        height += doc->height();
        m_height.store(height, std::memory_order_release);



        if (task.insertAtFront) { m_docs.push_front(DocBlock{ target.spine_id, doc }); }
        else { m_docs.push_back(DocBlock{ target.spine_id, doc }); }
    

        InvalidateRect(m_hwnd, nullptr, FALSE);
        OutputDebugStringA("[VirtualDod thread] 更新结束\n");
    }
}

bool VirtualDoc::exists(int spine_id)
{
    return m_blocks.exists(spine_id);
}

void VirtualDoc::present(int width, int height)
{
    auto sp = get_scroll_position();
    float remain = height / m_container->m_zoom_factor;
    float clientH = height / m_container->m_zoom_factor;
    float clientW = m_doc_width * m_container->m_zoom_factor;
    int x = 0;
    int y = 0;
    std::string txt = std::to_string(sp.spine_id) + ", " + std::to_string(sp.offset) + ", " + std::to_string(sp.height) + "\n";
    OutputDebugStringA(txt.c_str());
    m_container->BeginDraw();
    while(remain > 0 && m_docs.size() > 0)
    {
        auto block = m_docs.get(sp);
        if (!block.doc) { break; }

        float h = std::min(block.doc->height() - sp.offset, remain);
        litehtml::position clip{0, -sp.offset, clientW, h };
        block.doc->draw(m_container->getContext(), x, -sp.offset, nullptr);
        remain -= h;
        y -= h;
        sp.offset = -h;
        sp.spine_id += 1;

    }
    m_container->EndDraw();

}

void VirtualDoc::convert_coordinate(int &x, int &y, ScrollPosition sp)
{
    y += sp.offset;

}
void VirtualDoc::on_lbutton_down(int x, int y)
{
    auto sp = get_scroll_position();
    convert_coordinate(x, y, sp);
    litehtml::position::vector redraw_boxes;
    auto doc = m_docs.get(sp).doc;
    if (!doc) { return; }
    doc->on_lbutton_down(x, y, 0, 0, redraw_boxes);
    if (!redraw_boxes.empty())
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

}
void VirtualDoc::on_lbutton_up(int x, int y)
{
    auto sp = get_scroll_position();
    convert_coordinate(x, y, sp);
    litehtml::position::vector redraw_boxes;
    auto doc = m_docs.get(sp).doc;
    if (!doc) { return; }
    doc->on_lbutton_up(x, y, 0, 0, redraw_boxes); 
    if (!redraw_boxes.empty())
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

}
void VirtualDoc::on_lbutton_dblclk(int x, int y)
{
    //auto sp = get_scroll_position();
    //convert_coordinate(x, y, sp);
    //m_container->on_lbutton_dblclk(x, y);

}
void VirtualDoc::on_mouse_move(int x, int y)
{
    auto sp = get_scroll_position();
    convert_coordinate(x, y, sp);
    litehtml::position::vector redraw_boxes;
    auto doc = m_docs.get(sp).doc;
    if (!doc) { return; }
    doc->on_mouse_over(x, y, 0, 0, redraw_boxes);
    if (!redraw_boxes.empty())
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

 
}

void VirtualDoc::on_mouse_leave(int x, int y)
{
    auto sp = get_scroll_position();
    convert_coordinate(x, y, sp);
    litehtml::position::vector redraw_boxes;
    auto doc = m_docs.get(sp).doc;
    if (!doc) { return; }
    doc->on_mouse_leave(redraw_boxes);
    if(!redraw_boxes.empty())
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

}
void VirtualDoc::set_document_width(int width)
{
    m_doc_width.store(width, std::memory_order_release);
}
void VirtualDoc::on_mouse_wheel(int delta)
{
    auto sp = get_scroll_position();


    sp.offset -= delta;
    if (sp.offset < 0)
    {
        sp.spine_id -= 1;
        bool isExists = m_docs.exists(sp.spine_id);
        if (isExists) { 
            sp.height = m_docs.height(sp.spine_id);
            sp.offset += sp.height;
            
        }
        else
        {
            insert_chapter(sp.spine_id);
            return;
        }

    }
    if(sp.offset > sp.height)
    {
        sp.spine_id += 1;
        bool isExists = m_docs.exists(sp.spine_id);
        if (isExists) {
            sp.offset = 0;
            sp.height = m_docs.height(sp.spine_id);
    

        }
        else
        {
            insert_chapter(sp.spine_id);
            return;
        }

    }
    set_scroll_position(sp);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}


void VirtualDoc::clear()
{


    m_blocks.clear();
    //g_offsetY.store(0.0f, std::memory_order_relaxed);
    //float v = g_offsetY.load(std::memory_order_relaxed);
    m_offsetY.store(0.0f, std::memory_order_release);
    m_height.store(0.0f, std::memory_order_release);


}

void HtmlBlockColl::push_back(const HtmlBlock& block)
{

    std::lock_guard<std::mutex> lock(m_mutex);
    m_blocks.push_back(block);

}

void HtmlBlockColl::push_front(const HtmlBlock& block)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blocks.emplace(m_blocks.begin(), std::move(block));
}

HtmlBlock HtmlBlockColl::get(int spine_id) 
{
    std::lock_guard<std::mutex> lock(m_mutex);
    HtmlBlock block{};
    for (auto& b : m_blocks)
    {
        if (b.spine_id == spine_id)
        {
            block = b;
        }
    }
    return block;
}

size_t HtmlBlockColl::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.size();
}

HtmlBlock& HtmlBlockColl::front()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.front();
}

HtmlBlock& HtmlBlockColl::back()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.back();
}

void HtmlBlockColl::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blocks.clear();
}

bool HtmlBlockColl::exists(int spine_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_blocks.empty()) return false;
    for (const auto& b : m_blocks)
    {
        if (b.spine_id == spine_id) { return true; }
    }
    return false;
}
bool HtmlBlockColl::empty()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_blocks.empty();
}

void DocBlockColl::push_back(DocBlock block)
{

    std::lock_guard<std::mutex> lock(m_mutex);
    m_blocks.push_back(block);

}

void DocBlockColl::push_front( DocBlock block)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blocks.emplace(m_blocks.begin(), std::move(block));
}

size_t DocBlockColl::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.size();
}

DocBlock& DocBlockColl::front()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.front();
}

DocBlock& DocBlockColl::back()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks.back();
}

void DocBlockColl::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blocks.clear();
}

bool DocBlockColl::exists(int spine_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_blocks.empty()) return false;
    for (const auto& b : m_blocks)
    {
        if (b.spine_id == spine_id) { return true; }
    }
    return false;
}

float DocBlockColl::height(int spine_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_blocks.empty()) return 0.0f;
    float height = 0.0f;
    for (const auto& b : m_blocks)
    {
        if (b.spine_id == spine_id) { return height += b.doc->height(); }
    }
    return height;
}

DocBlock DocBlockColl::get(ScrollPosition sp)
{
    for (auto bk: m_blocks)
    {
        if(bk.spine_id == sp.spine_id)
        {
            return bk;
        }
    }
    return DocBlock{};
}

std::vector<DocBlock> DocBlockColl::get_snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_blocks;  // 返回副本
}

float DocBlockColl::height()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_blocks.empty()) return 0.0f;
    float height = 0.0f;
    for (const auto& b : m_blocks)
    {
        height += b.doc->height();
    }
    return height;
}



ScrollPosition SafeScrollPosition::get_position() 
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sp;
    // TODO: 在此处插入 return 语句
    
}


void SafeScrollPosition::set_position(ScrollPosition pos)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sp.height = pos.height;
    m_sp.offset = pos.offset;
    m_sp.spine_id = pos.spine_id;

}
