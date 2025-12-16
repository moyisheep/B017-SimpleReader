#include "MML2SVG.h"


static std::string w2a(const std::wstring& s)
{
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len - 1, 0);                 // 去掉末尾 '\0'
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], len, nullptr, nullptr);
    return out;
}

static std::wstring a2w(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len - 1, 0);                // 去掉末尾 '\0'
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], len);
    return out;
}


using namespace tinyxml2;


/* ---------- 内部实现 ---------- */
class MathML2SVG::Impl {
public:
    /* 样式结构体 —— 只在 Impl 内部可见 */


    /* 策略表 */
    using RenderFn = std::function<std::string(const tinyxml2::XMLElement*, const Style&)>;
    using AttrFn = void(*)(const class tinyxml2::XMLAttribute*, class Style&);

    std::unordered_map<std::string, RenderFn> tagRender;
    std::unordered_map<std::string, AttrFn>   attrApply;
    std::mutex                                mtx;
    bool m_usePath = true;   // 外部可改
    Impl() { registerAll(); }

    /* 线程安全注册 */
    void registerTag(const std::string& tag, RenderFn fn) {
        std::lock_guard<std::mutex> lock(mtx);
        tagRender[tag] = std::move(fn);
    }
    void registerAttr(const std::string& attr, AttrFn fn) {
        std::lock_guard<std::mutex> lock(mtx);
        attrApply[attr] = fn;
    }

    std::string cleanup(std::string svg) {
        std::regex re(R"(\s*data-w="[^"]*")");
        return std::regex_replace(svg, re, "");
    }

    /* 主转换 */
    std::string convert(const std::string& mathml) {
        tinyxml2::XMLDocument doc;
        if (doc.Parse(mathml.c_str()) != XML_SUCCESS)
            return "<!-- parse error -->";

        XMLElement* root = doc.RootElement();
        if (!root) return "<!-- empty -->";

        Style st;
        std::string inner = renderElement(root, st);
        double ascent = extractAscent(inner);
        double descent = extractDescent(inner);
        double height = ascent - descent;
        double width = extractWidth(inner);
        double em = std::stod(st.fontSize);
        double margin = 0.25 * em;
        double x = margin;
        double y = margin + ascent;

        std::ostringstream svg;
        svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width + margin
            << "\" height=\"" << height + margin << "\">"
            << "<g transform = \"translate(" << x << "," << y << ")\">" << inner << "</g>"
            << "</svg>";
        return (svg.str());
    }

private:

    static std::string xmlEscape(std::string_view raw)
    {
        std::string out;
        out.reserve(raw.size() + 16);          // 小优化
        for (char c : raw)
        {
            switch (c)
            {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
            }
        }
        return out;
    }
    static std::wstring extractPlainText(const tinyxml2::XMLElement* e) {
        std::wstring out;
        if (!e) return out;

        // 深度优先收集所有文本节点
        if (const char* txt = e->GetText())
            out += a2w(txt);

        for (const tinyxml2::XMLElement* c = e->FirstChildElement();
            c; c = c->NextSiblingElement()) {
            out += extractPlainText(c);
        }
        return out;
    }

    /* ---------- 工具函数 ---------- */
    static std::string textRender(const tinyxml2::XMLElement* e, const Style& st)
    {
        std::string txt = e->GetText() ? e->GetText() : "";
        for (size_t pos = 0;
            (pos = txt.find("&nbsp;", pos)) != std::string::npos; )
        {
            txt.replace(pos, 6, " ");   // 普通空格 U+0020
            ++pos;                      // 继续向后找
        }

        std::wstring wtxt = a2w(txt);

        // 1. 精确测量
        auto si = FreeTypeTextMeasurer::instance().measure(
            wtxt, st.fontFamily, std::stof(st.fontSize));

        // 2. 生成裸 <text>（相对于基线原点）
        std::ostringstream os;
        os << "<text x=\"0\" y=\"" << 0 << "\""
            << " font-size=\"" << st.fontSize
            << "\" font-family=\"" << w2a(st.fontFamily)
            << "\" fill=\"" << st.fill << "\">"
            << xmlEscape(txt)
            << "</text>";

        // 3. 用 <g> 包一层，把尺寸放在 g 的 data-* 上
        std::ostringstream finalOSS;
        finalOSS << "<g data-w=\"" << si.width
            << "\" data-asc=\"" << si.ascent
            << "\" data-desc=\"" << si.descent << "\">"
            << os.str()
            << "</g>";
        return finalOSS.str();
    }
    static double getDimAttr(const tinyxml2::XMLElement* e,
        const char* name,
        double defVal /*em*/)
    {
        const char* v = e->Attribute(name);
        if (!v) return defVal;
        std::string s = v;
        // 去掉单位，只保留数字
        if (s.back() == 'e' || s.back() == 'm') s.pop_back();
        if (s.empty()) return defVal;
        return std::stod(s);
    }
    static double extractWidth(const std::string& svg) {
        const char* tag = "data-w=\"";
        size_t pos = svg.find(tag);
        if (pos == std::string::npos) return 0;
        pos += strlen(tag);
        size_t end = svg.find('"', pos);
        return std::stod(svg.substr(pos, end - pos));
    }

    static double extractAscent(const std::string& svg) {
        const char* tag = "data-asc=\"";
        size_t pos = svg.find(tag);
        if (pos == std::string::npos) return 0;
        pos += strlen(tag);
        size_t end = svg.find('"', pos);
        return std::stod(svg.substr(pos, end - pos));
    }
    static double extractDescent(const std::string& svg) {
        const char* tag = "data-desc=\"";
        size_t pos = svg.find(tag);
        if (pos == std::string::npos) return 0;
        pos += strlen(tag);
        size_t end = svg.find('"', pos);
        return std::stod(svg.substr(pos, end - pos));
    }

    static std::string hbox(const std::vector<std::string>& parts,
        double dx = 2.0, std::string tag_name = "hbox")
    {
        if (parts.empty())
            return R"(<g data-w="0"  data-asc="0" data-desc="0" />)";

        double totalW = 0.0;
        double asc = 0.0, des = 0.0;


        for (const auto& p : parts)
        {
            totalW += extractWidth(p);
            asc = std::max(asc, extractAscent(p));
            des = std::min(des, extractDescent(p));
        }
        totalW += dx * (parts.size() - 1);

        /* 拼 SVG：所有子元素 y=0 对齐 */
        std::ostringstream os;
        os << "<g class=\"" << tag_name << "\" data-w=\"" << totalW
            << "\" data-asc=\"" << asc
            << "\" data-desc=\"" << des << "\" >";

        double x = 0.0;
        for (const auto& p : parts)
        {
            os << "<g transform=\"translate(" << x << ",0)\">" << p << "</g>";
            x += extractWidth(p) + dx;
        }
        os << "</g>";
        return os.str();
    }
    static std::string vbox(const std::vector<std::string>& parts,
        double dy = 2.0, std::string tag_name = "vbox")
    {
        if (parts.empty())
            return R"(<g data-w="0"  data-asc="0" data-desc="0" />)";

        double width = 0.0;
        double asc = 0.0, des = 0.0;
        double totalH = 0.0;

        for (const auto& p : parts)
        {
            width = std::max(width, extractWidth(p));
            totalH += (extractAscent(p) - extractDescent(p));
        }
        totalH += dy * (parts.size() - 1);
        asc = totalH * 0.5;
        des = -(totalH - asc);
        /* 拼 SVG：所有子元素 y=0 对齐 */
        std::ostringstream os;
        os << "<g class=\"" << tag_name << "\" data-w=\"" << width
            << "\" data-asc=\"" << asc
            << "\" data-desc=\"" << des << "\" >";

        double y = -asc;
        for (const auto& p : parts)
        {
            os << "<g transform=\"translate(0, " << y + extractAscent(p) << ")\">" << p << "</g>";
            y += (extractAscent(p) - extractDescent(p)) + dy;

        }
        os << "</g>";
        return os.str();
    }


    /* ---------- 递归渲染 ---------- */
    std::string renderElement(const XMLElement* e, Style st) {
        //DbgPrint("[renderElement] <%s>\n", e->Name());
        /* 1. 处理属性 */
        for (const XMLAttribute* a = e->FirstAttribute(); a; a = a->Next()) {
            auto it = attrApply.find(a->Name());
            if (it != attrApply.end()) it->second(a, st);
        }

        /* 2. 如果是文本类节点，直接渲染，不再递归子元素 */
        const char* tag = e->Name();
        if (!strcmp(tag, "mtext") || !strcmp(tag, "mo") || !strcmp(tag, "mi") || !strcmp(tag, "mn")) {
            return textRender(e, st);
        }

        /* 3. 容器节点：递归子元素 */
        std::vector<std::string> children;
        for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
            children.push_back(renderElement(c, st));
        }

        /* 4. 查找是否有注册的渲染器（如 mfrac、msup 等） */
        auto it = tagRender.find(tag);
        if (it != tagRender.end()) {
            return it->second(e, st);
        }

        /* 5. 默认水平排列子节点 */
        return hbox(children, 2.0, "math");
    }
    /* ---------- 注册表 ---------- */
    void registerAll() {
        /* 记号类 */
        registerTag("mi", textRender);
        registerTag("mn", textRender);
        registerTag("mo", textRender);
        registerTag("ms", textRender);
        registerTag("mtext", textRender);

        /* 布局类 */
        registerTag("mrow",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> boxes;
                /* 1. 渲染所有子元素并收集尺寸 */
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                {
                    boxes.push_back(renderElement(c, st));
                }
                return hbox(boxes, 2.0, "mrow");
            });
        registerTag("mfrac",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 子元素 ---------- */
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- mfrac needs 2 children -->";

                /* ---------- 2. 解析属性 ---------- */
                double thickness = 1.0;
                const char* lt = e->Attribute("linethickness");
                if (lt) {
                    std::string val = lt;
                    if (val == "thin")        thickness = 0.5;
                    else if (val == "medium") thickness = 1.0;
                    else if (val == "thick")  thickness = 2.0;
                    else if (val.back() == 'x' || val.back() == 'X')
                        thickness = std::stod(val.substr(0, val.size() - 2));
                    else if (val.back() == 't' || val.back() == 'T')
                        thickness = std::stod(val.substr(0, val.size() - 2)) * 1.33;
                    else if (val.back() == 'm' || val.back() == 'M')
                        thickness = std::stod(val.substr(0, val.size() - 2)) * std::stof(st.fontSize);
                    else
                        thickness = std::stod(val);
                }
                std::string numAlign = e->Attribute("numalign") ? e->Attribute("numalign") : "center";
                std::string denAlign = e->Attribute("denomalign") ? e->Attribute("denomalign") : "center";

                /* ---------- 3. 精确盒尺寸 ---------- */
                double numW = extractWidth(kids[0]);
                double numAsc = extractAscent(kids[0]);
                double numDesc = extractDescent(kids[0]);

                double denW = extractWidth(kids[1]);
                double denAsc = extractAscent(kids[1]);
                double denDesc = extractDescent(kids[1]);

                /* ---------- 4. TeX 参数 ---------- */
                double em = std::stof(st.fontSize);
                double y_shift = 0.3 * em;
                double rule = thickness;
                double gap = 0.3 * em;

                /* ---------- 5. 垂直距离（分数线 y = 0） ---------- */


                double ascent = (numAsc - numDesc) + gap + rule / 2.0 + y_shift;   // 分子最上沿到分数线
                double descent = -((denAsc - denDesc) + gap + rule / 2.0) + y_shift;   // 分数线到分母最下沿


                /* ---------- 6. 水平对齐 ---------- */
                double ruleW = std::max(numW, denW);
                auto offset = [](double w, double ruleW, const std::string& align)
                {
                    if (align == "left")  return 0.0;
                    if (align == "right") return ruleW - w;
                    return (ruleW - w) * 0.5;
                };
                double numX = offset(numW, ruleW, numAlign);
                double denX = offset(denW, ruleW, denAlign);

                /* ---------- 7. 分子、分母相对于分数线 y = 0 的 y ---------- */
                double lineY = -y_shift;
                double numY = -(-numDesc + rule * 0.5 + gap + y_shift);   // 分子基线
                double denY = (rule * 0.5 + denAsc + gap - y_shift);  // 分母基线

                /* ---------- 8. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"mfrac\" data-w=\"" << ruleW
                    << "\" data-asc=\"" << ascent
                    << "\" data-desc=\"" << descent << "\">";
                oss << "<g transform=\"translate(" << numX << "," << numY << ")\">" << kids[0] << "</g>";
                oss << "<line x1=\"0\" y1=\"" << lineY << "\" x2=\"" << ruleW << "\" y2=\"" << lineY << "\" "
                    << "stroke=\"black\" stroke-width=\"" << rule << "\"/>";
                oss << "<g transform=\"translate(" << denX << "," << denY << ")\">" << kids[1] << "</g>";
                oss << "</g>";
                return oss.str();
            });
        registerTag("msup",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- msup needs 2 children -->";

                /* ---------- 1. 主体尺寸 ---------- */
                double baseW = extractWidth(kids[0]);
                double baseAsc = extractAscent(kids[0]);
                double baseDes = extractDescent(kids[0]);


                /* ---------- 2. 上标原始尺寸 ---------- */
                double supW0 = extractWidth(kids[1]);
                double supAsc0 = extractAscent(kids[1]);
                double supDes0 = extractDescent(kids[1]);


                double em = std::stod(st.fontSize);
                double y_shift = 0.5 * em;
                /* ---------- 3. 缩放后尺寸 ---------- */
                const double scale = 0.7;
                double supW = supW0 * scale;
                double supAsc = supAsc0 * scale;
                double supDes = supDes0 * scale;

                /* ---------- 4. 上标位置 ---------- */

                double supX = baseW;                               // 右上角
                /* 补偿缩放导致的基线偏移：先平移 -supBase0*scale，再整体放到 gap 上方 */
                double supY = -(baseAsc - supDes);

                /* ---------- 5. 整体盒 ---------- */
                double totalW = baseW + supW;
                double totalAsc = baseAsc + (supAsc - supDes);   // 最上沿
                double totalDes = baseDes;                           // 最下沿

                /* ---------- 6. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"msup\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\">"
                    << kids[0]                                         // 主体：已位于 baseBaseline
                    << "<g transform=\"translate(" << supX << "," << supY << ") scale(" << scale << ")\">"
                    << kids[1]                                         // 上标：相对位移
                    << "</g>"
                    << "</g>";
                return oss.str();
            });
        registerTag("msub",
            [this](const XMLElement* e, const Style& st) -> std::string {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() < 2) return kids.empty() ? "" : kids[0];

                /* ---------- 1. 主体尺寸 ---------- */
                double baseW = extractWidth(kids[0]);
                double baseAsc = extractAscent(kids[0]);
                double baseDes = extractDescent(kids[0]);


                /* ---------- 2. 下标原始尺寸 ---------- */
                double subW0 = extractWidth(kids[1]);
                double subAsc0 = extractAscent(kids[1]);
                double subDes0 = extractDescent(kids[1]);


                /* ---------- 3. 缩放后尺寸 ---------- */
                const double scale = 0.7;

                //const double drop = 0.25 * baseDes;         // 下标基线相对主体基线的下移量

                double subW = subW0 * scale;
                double subAsc = subAsc0 * scale;
                double subDes = subDes0 * scale;

                /* ---------- 4. 下标位置 ---------- */
                double subX = baseW;                     // 右下角
                /* 补偿缩放导致的基线偏移：先平移 -subBaseline0*scale，再整体下移 drop */
                double subY = subAsc - baseDes;

                /* ---------- 5. 整体盒尺寸 ---------- */
                double totalW = subX + subW;
                double totalAsc = baseAsc;                     // 最上沿
                double totalDes = baseDes - (subAsc - subDes); // 最下沿

                /* ---------- 6. 组装 ---------- */
                std::ostringstream os;
                os << "<g class=\"msub\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\">"
                    << kids[0]                                  // 主体：基线 y = 0
                    << "<g transform=\"translate(" << subX << "," << subY << ") scale(" << scale << ")\">"
                    << kids[1]                                  // 下标：已补偿基线
                    << "</g>"
                    << "</g>";
                return os.str();
            });
        registerTag("msubsup",
            [this](const tinyxml2::XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() < 3) return kids.empty() ? "" : kids[0];

                /* ---------- 1. 主体尺寸 ---------- */
                double baseW = extractWidth(kids[0]);
                double baseAsc = extractAscent(kids[0]);
                double baseDes = extractDescent(kids[0]);


                /* ---------- 2. 上标原始尺寸 ---------- */
                double supW0 = extractWidth(kids[1]);
                double supAsc0 = extractAscent(kids[1]);
                double supDes0 = extractDescent(kids[1]);

                /* ---------- 3. 下标原始尺寸 ---------- */
                double subW0 = extractWidth(kids[2]);
                double subAsc0 = extractAscent(kids[2]);
                double subDes0 = extractDescent(kids[2]);


                /* ---------- 4. 缩放后尺寸 ---------- */
                const double scale = 0.7;


                double supW = supW0 * scale;
                double supAsc = supAsc0 * scale;
                double supDes = supDes0 * scale;

                double subW = subW0 * scale;
                double subAsc = subAsc0 * scale;
                double subDes = subDes0 * scale;

                /* ---------- 5. 上标位置 ---------- */
                double supX = baseW;
                /* 补偿缩放导致的基线偏移：先平移 -supBase0*scale，再整体放到 shiftUp 上方 */
                double supY = baseAsc - supDes;

                /* ---------- 6. 下标位置 ---------- */
                double subX = baseW;
                /* 补偿缩放导致的基线偏移：先平移 -subBase0*scale，再整体放到 shiftDown 下方 */
                double subY = -(supAsc - baseDes);

                /* ---------- 7. 整体盒尺寸 ---------- */
                double totalW = std::max(baseW, supX + std::max(supW, subW));
                double totalAsc = baseAsc + (supAsc - supDes);   // 最上沿
                double totalDes = baseDes - (supAsc - supDes); // 最下沿

                /* ---------- 8. 组装 ---------- */
                std::ostringstream os;
                os << "<g class=\"msubsup\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\">"
                    << kids[0]                                   // 主体：基线 y = 0
                    << "<g transform=\"translate(" << supX << "," << supY << ") scale(" << scale << ")\">"
                    << kids[1]                                   // 上标：已补偿基线
                    << "</g>"
                    << "<g transform=\"translate(" << subX << "," << subY << ") scale(" << scale << ")\">"
                    << kids[2]                                   // 下标：已补偿基线
                    << "</g>"
                    << "</g>";
                return os.str();
            });
        registerTag("msqrt",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 收集子元素 ---------- */
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.empty()) return "<!-- msqrt needs at least 1 child -->";

                /* ---------- 2. 水平拼接子元素 ---------- */
                std::string inner = hbox(kids);

                /* ---------- 3. 内容尺寸 ---------- */
                double em = std::stof(st.fontSize);
                double rule = 1.0;                       // 线厚
                double gap = 0.2 * em;                // 根号与内容间隙
                double hook = 0.4 * em;                // 左侧小勾宽度

                double contW = extractWidth(inner);
                double contAsc = extractAscent(inner);
                double contDes = extractDescent(inner);

                /* ---------- 4. 根号盒高（以内容基线为 0） ---------- */
                double boxAsc = contAsc + gap + rule;           // 内容顶部到基线
                double boxDes = contDes;           // 基线到内容底部


                /* ---------- 5. 根号路径（相对于内容基线） ---------- */
                double left = hook;                     // 根号左侧勾起点
                double right = left + contW + gap;       // 根号横线终点
                double barY = -(contAsc + gap);                  // 横线 y 坐标（负值，在基线上方）

                std::ostringstream path;
                path << "M" << 0 << "," << -contAsc * 0.5
                    << "L" << left << "," << 0
                    << "L" << (left) << "," << barY
                    << "L" << right << "," << barY;

                /* ---------- 6. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"msqrt\" data-w=\"" << (right + rule)
                    << "\" data-asc=\"" << boxAsc
                    << "\" data-desc=\"" << boxDes << "\">";
                oss << "<path d=\"" << path.str()
                    << "\" stroke=\"black\" fill=\"none\" stroke-width=\"" << rule << "\"/>";
                oss << "<g transform=\"translate(" << left + gap << ",0)\">"   // 内容基线 y = 0
                    << inner << "</g>";
                oss << "</g>";
                return oss.str();
            });
        registerTag("mroot",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 收集子元素 ---------- */
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- mroot needs exactly 2 children -->";

                /* ---------- 2. 被开方内容尺寸（基线 = 0） ---------- */
                double innerW = extractWidth(kids[0]);
                double innerAsc = extractAscent(kids[0]);
                double innerDes = extractDescent(kids[0]);

                /* ---------- 3. 指数原始尺寸 ---------- */
                double idxW0 = extractWidth(kids[1]);
                double idxAsc0 = extractAscent(kids[1]);
                double idxDes0 = extractDescent(kids[1]);
                double idxBase0 = 0;

                /* ---------- 4. 缩放后指数尺寸 ---------- */
                const double scale = 0.7;
                double idxW = idxW0 * scale;
                double idxAsc = idxAsc0 * scale;
                double idxDes = idxDes0 * scale;

                /* ---------- 5. 根号几何参数 ---------- */
                const double pad = 2.0;   // 内边距
                const double bar = 1.2;   // 横线超出系数
                const double vgap = 2.0;   // 横线与内容顶部间距
                const double tick = 6.0;   // 勾线水平段
                const double idxGap = 1.0;  // 指数与勾线空隙

                /* ---------- 6. 整体盒尺寸（以被开方内容基线为 0） ---------- */
                double bodyAsc = innerAsc + vgap + pad;   // 被开方内容顶部到基线
                double bodyDes = innerDes + pad;          // 被开方内容底部到基线
                double totalH = bodyAsc + bodyDes;
                double totalW = tick + innerW * bar + pad * 2;

                /* ---------- 7. 指数位置（基线对齐整体基线） ---------- */
                double idxX = -idxW - idxGap;   // 指数左上角 x
                /* 补偿缩放导致的基线偏移：先平移 -idxBase0*scale，再整体放到勾线左侧 */
                double idxY = -idxAsc - idxBase0 * scale;

                /* ---------- 8. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"mroot\" data-w=\"" << totalW
                    << "\" data-h=\"" << totalH
                    << "\" data-asc=\"" << bodyAsc
                    << "\" data-desc=\"" << bodyDes
                    << "\" data-baseline=\"0\">"
                    /* 指数：基线对齐整体基线（y = 0） */
                    << "<g transform=\"translate(" << idxX << "," << idxY << ") scale(" << scale << ")\">"
                    << kids[1] << "</g>"
                    /* 根号勾线 + 横线（相对于基线） */
                    << "<path d=\"M0," << bodyAsc
                    << " L" << tick << "," << -vgap
                    << " L" << tick + innerW * bar << "," << -vgap
                    << "\" stroke=\"black\" fill=\"none\" stroke-width=\"1\"/>"
                    /* 被开方内容：左上角对齐勾线右侧，基线保持 y = 0 */
                    << "<g transform=\"translate(" << tick << ",0)\">"
                    << kids[0] << "</g>"
                    << "</g>";
                return oss.str();
            });
        /* 属性 */
        registerAttr("mathcolor", [](const XMLAttribute* a, Style& st) {
            st.fill = a->Value();
            });
        registerAttr("mathsize", [](const XMLAttribute* a, Style& st) {
            st.fontSize = a->Value();
            });
        registerAttr("mathvariant", [](const XMLAttribute* a, Style& st) {
            const char* v = a->Value();
            if (!strcmp(v, "italic")) st.fontStyle = "italic";
            else if (!strcmp(v, "bold")) st.fontWeight = "bold";
            });

        /* ---------- mtd ---------- */
        registerTag("mtd",
            [this](const XMLElement* e, const Style& st) -> std::string {
                std::ostringstream inner;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    inner << renderElement(c, st);

                std::string s = inner.str();
                std::ostringstream os;
                os << "<g data-w=\"" << extractWidth(s)
                    << "\" data-asc=\"" << extractAscent(s)
                    << "\" data-desc=\"" << extractDescent(s) << "\">"
                    << s << "</g>";
                return os.str();
            });

        /* ---------- mtr ---------- */
        registerTag("mtr",
            [this](const XMLElement* e, const Style& st) -> std::string {
                std::ostringstream oss;
                for (const XMLElement* c = e->FirstChildElement("mtd"); c; c = c->NextSiblingElement("mtd"))
                    oss << renderElement(c, st);   // 每个 <mtd> 已自带 metrics
                return oss.str();
            });
        registerTag("mtable",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 解析属性 ---------- */
                const double defaultGap = 8.0;
                std::vector<double> colGap = { defaultGap };
                std::vector<double> rowGap = { defaultGap };

                auto parseList = [](const char* s, std::vector<double>& v, double def)
                {
                    if (!s) return;
                    std::stringstream ss(s);
                    v.clear();
                    for (std::string t; std::getline(ss, t, ' '); )
                        v.push_back(std::stod(t));
                    if (v.empty()) v.push_back(def);
                };
                parseList(e->Attribute("columnspacing"), colGap, defaultGap);
                parseList(e->Attribute("rowspacing"), rowGap, defaultGap);

                /* ---------- 2. 收集所有单元格 ---------- */
                std::vector<std::vector<std::string>> grid;   // 每行每列的 SVG 片段
                std::vector<std::vector<double>> wGrid, hGrid;
                size_t rows = 0, cols = 0;

                for (const XMLElement* r = e->FirstChildElement("mtr"); r; r = r->NextSiblingElement("mtr")) {
                    grid.emplace_back();
                    wGrid.emplace_back();
                    hGrid.emplace_back();
                    auto& rowCells = grid.back();
                    auto& rowW = wGrid.back();
                    auto& rowH = hGrid.back();

                    for (const XMLElement* c = r->FirstChildElement("mtd"); c; c = c->NextSiblingElement("mtd")) {
                        std::string cell = renderElement(c, st);   // 已带 data-w / data-asc / data-desc
                        rowCells.push_back(cell);
                        rowW.push_back(extractWidth(cell));
                        rowH.push_back(extractAscent(cell) - extractDescent(cell));
                    }
                    rows++;
                    cols = std::max(cols, rowCells.size());
                }
                if (rows == 0) return "<!-- empty mtable -->";

                /* ---------- 3. 统一列宽、行高 ---------- */
                std::vector<double> colW(cols, 0.0);
                std::vector<double> rowH(rows, 0.0);

                for (size_t r = 0; r < rows; ++r)
                    for (size_t c = 0; c < grid[r].size(); ++c)
                        colW[c] = std::max(colW[c], wGrid[r][c]);

                for (size_t r = 0; r < rows; ++r) {
                    double h = 0.0;
                    for (size_t c = 0; c < grid[r].size(); ++c)
                        h = std::max(h, hGrid[r][c]);
                    rowH[r] = h;
                }

                /* ---------- 4. 整体尺寸 ---------- */
                double totalW = std::accumulate(colW.begin(), colW.end(), 0.0)
                    + (cols ? (cols - 1) * colGap[0] : 0.0);
                double totalH = std::accumulate(rowH.begin(), rowH.end(), 0.0)
                    + (rows ? (rows - 1) * rowGap[0] : 0.0);

                double asc = totalH * 0.5;
                double des = -(totalH - asc);

                /* ---------- 5. 绝对坐标摆放 ---------- */
                std::ostringstream os;
                os << "<g class=\"mtable\" data-w=\"" << totalW
                    << "\" data-asc=\"" << asc
                    << "\" data-desc=\"" << des << "\">";

                double y = -asc;
                for (size_t r = 0; r < rows; ++r) {
                    double x = 0.0;
                    for (size_t c = 0; c < grid[r].size(); ++c) {
                        double dy = (rowH[r] - hGrid[r][c]) * 0.5;   // 垂直居中
                        os << "<g transform=\"translate(" << x << "," << (y + dy + extractAscent(grid[r][c])) << ")\">"
                            << grid[r][c]
                            << "</g>";
                        x += colW[c] + (c + 1 < cols ? colGap[0] : 0.0);
                    }
                    y += rowH[r] + (r + 1 < rows ? rowGap[0] : 0.0);
                }
                os << "</g>";
                return os.str();
            });

        registerTag("mlabeledtr",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> cells;
                for (const XMLElement* c = e->FirstChildElement("mtd");
                    c; c = c->NextSiblingElement("mtd"))
                    cells.push_back(renderElement(c, st));

                if (cells.empty()) return "<!-- empty mlabeledtr -->";

                /* 列间距 */
                double colGap = 4.0;
                if (const char* gap = e->Attribute("columnspacing"))
                    colGap = std::stod(gap);

                /* 用 hbox 横向排布，但手动计算基线对齐 */
                double totalW = 0;
                double maxAsc = 0;
                double maxDes = 0;
                for (const auto& cell : cells) {
                    totalW += extractWidth(cell);
                    maxAsc = std::max(maxAsc, extractAscent(cell));
                    maxDes = std::max(maxDes, extractDescent(cell));
                }
                totalW += colGap * (cells.size() - 1);

                std::ostringstream oss;
                oss << "<g class=\"mlabeledtr\" data-w=\"" << totalW
                    << "\" data-h=\"" << (maxAsc + maxDes)
                    << "\" data-asc=\"" << maxAsc
                    << "\" data-desc=\"" << maxDes
                    << "\" data-baseline=\"0\">";

                double x = 0;
                for (const auto& cell : cells) {
                    double dx = x;
                    /* 垂直按基线对齐：单元格内部基线 0 → 行基线 0 */
                    double dy = maxAsc - extractAscent(cell);
                    oss << "<g transform=\"translate(" << dx << "," << dy << ")\">"
                        << cell << "</g>";
                    x += extractWidth(cell) + colGap;
                }
                oss << "</g>";
                return oss.str();
            });
        registerTag("mmultiscripts",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                /* ---------- 1. 收集节点 ---------- */
                std::vector<const XMLElement*> nodes;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    nodes.push_back(c);
                if (nodes.empty()) return "";

                /* ---------- 2. 分区 ---------- */
                size_t split = nodes.size();
                for (size_t i = 0; i < nodes.size(); ++i)
                    if (std::string(nodes[i]->Name()) == "mprescripts") { split = i; break; }

                const XMLElement* baseNode = nodes[0];
                std::vector<const XMLElement*> postNodes(nodes.begin() + 1, nodes.begin() + split);   // post: sub1 sup1 sub2 sup2 ...
                std::vector<const XMLElement*> preNodes(nodes.begin() + split + 1, nodes.end());      // pre : sup1 sub1 sup2 sub2 ...

                /* ---------- 3. 主字符 ---------- */
                std::string baseSVG = renderElement(baseNode, st);
                double baseW = extractWidth(baseSVG);
                double baseAsc = extractAscent(baseSVG);
                double baseDes = extractDescent(baseSVG);

                /* ---------- 4. 常量 ---------- */
                const double scale = 0.7;
                const double kern = 1.0;
                const double supGap = 1.5;
                const double subGap = 1.0;
                const double pairGap = 1.0;

                /* ---------- 5. 工具：一次性渲染并缓存尺寸 ---------- */
                struct ScriptRec {
                    std::string svg;
                    double w, asc, des;   // asc>0, des<0
                };
                auto makeRec = [&](const XMLElement* el) -> ScriptRec
                {
                    if (!el || std::string(el->Name()) == "none")
                        return { "", 0, 0, 0 };

                    std::string raw = renderElement(el, st);
                    double w = extractWidth(raw) * scale;
                    double asc = extractAscent(raw) * scale;
                    double des = extractDescent(raw) * scale;   // 负值

                    std::ostringstream os;
                    os << "<g data-w=\"" << w
                        << "\" data-asc=\"" << asc
                        << "\" data-desc=\"" << des << "\">"
                        << "<g transform=\"scale(" << scale << ")\">"
                        << raw << "</g></g>";

                    return { os.str(), w, asc, des };
                };

                /* ---------- 6. 收集列 ---------- */
                std::vector<ScriptRec> preSub, preSup, postSub, postSup;
                double preW = 0;
                double postW = 0;

                /* pre 区段：sup1 sub1 sup2 sub2 ... */
                for (size_t i = 0; i + 1 < preNodes.size(); i += 2)
                {
                    ScriptRec sup = makeRec(preNodes[i]);
                    ScriptRec sub = makeRec(preNodes[i + 1]);
                    preSup.push_back(sup);
                    preSub.push_back(sub);
                    preW += std::max(sup.w, sub.w) + pairGap;
                }
                if (!preSup.empty()) preW -= pairGap;

                /* post 区段：sub1 sup1 sub2 sup2 ... */
                for (size_t i = 0; i + 1 < postNodes.size(); i += 2)
                {
                    ScriptRec sub = makeRec(postNodes[i]);
                    ScriptRec sup = makeRec(postNodes[i + 1]);
                    postSub.push_back(sub);
                    postSup.push_back(sup);
                    postW += std::max(sub.w, sup.w) + pairGap;
                }
                if (!postSub.empty()) postW -= pairGap;

                /* ---------- 7. 整体盒尺寸 ---------- */
                double totalW = preW + kern + baseW + kern + postW;

                double maxSup = baseAsc;
                double minSub = baseDes;
                auto update = [&](const ScriptRec& r)
                {
                    maxSup = std::max(maxSup, baseAsc + r.asc);
                    minSub = std::min(minSub, baseDes + r.des);
                };
                for (const auto& r : preSup) update(r);
                for (const auto& r : preSub) update(r);
                for (const auto& r : postSub) update(r);
                for (const auto& r : postSup) update(r);

                /* ---------- 8. 组装 ---------- */
                std::ostringstream oss;
                oss << "<g class=\"mmultiscripts\" data-w=\"" << totalW
                    << "\" data-asc=\"" << maxSup
                    << "\" data-desc=\"" << minSub << "\">";

                double x = 0;

                /* pre 列（从右向左排） */
                for (int i = (int)preSup.size() - 1; i >= 0; --i)
                {
                    const ScriptRec& sup = preSup[i];
                    const ScriptRec& sub = preSub[i];
                    double w = std::max(sup.w, sub.w);

                    double ySup = (supGap - sup.des);
                    double ySub = -(subGap + sub.asc);   // sub.des 负值
                    oss << "<g transform=\"translate(" << x << ",0)\">"
                        << "<g transform=\"translate(0," << ySup << ")\">" << sup.svg << "</g>"
                        << "<g transform=\"translate(0," << ySub << ")\">" << sub.svg << "</g>"
                        << "</g>";
                    x += w + pairGap;
                }

                /* 主字符 */
                oss << "<g transform=\"translate(" << x << ",0)\">" << baseSVG << "</g>";
                x += baseW + kern;

                /* post 列（从左向右排） */
                for (size_t i = 0; i < postSub.size(); ++i)
                {
                    const ScriptRec& sub = postSub[i];
                    const ScriptRec& sup = postSup[i];
                    double w = std::max(sub.w, sup.w);

                    double ySup = -(supGap + sup.asc);
                    double ySub = (subGap - sub.des);
                    oss << "<g transform=\"translate(" << x << ",0)\">"
                        << "<g transform=\"translate(0," << ySub << ")\">" << sub.svg << "</g>"
                        << "<g transform=\"translate(0," << ySup << ")\">" << sup.svg << "</g>"
                        << "</g>";
                    x += w + pairGap;
                }

                oss << "</g>";
                return oss.str();
            });
        registerTag("munder",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- munder needs 2 children -->";

                const double em = std::stof(st.fontSize);
                const double gap = 0.2 * em;
                const double scale = 0.7;

                const std::string& base = kids[0];
                const std::string& sub = kids[1];

                double baseW = extractWidth(base);
                double baseAsc = extractAscent(base);
                double baseDes = extractDescent(base);

                double subW = extractWidth(sub) * scale;
                double subAsc = extractAscent(sub) * scale;
                double subDes = extractDescent(sub) * scale;


                /* ---------- 整体盒尺寸（以主字符基线为 0） ---------- */
                double totalW = std::max(baseW, subW);
                double baseX = (totalW - baseW) / 2.0;
                double subX = (totalW - subW) / 2.0;

                /* 下标位置：主字符底部 + 间隙，再补偿下标自身基线 */
                double subY = (subAsc + gap - baseDes);

                double totalAsc = baseAsc;                       // 最上沿
                double totalDes = baseDes - gap - (subAsc - subDes); // 最下沿


                std::ostringstream oss;
                oss << "<g class=\"munder\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\" >";
                oss << "<g transform=\"translate(" << baseX << ",0)\">" << base << "</g>";
                oss << "<g transform=\"translate(" << subX << "," << subY << ")" << " scale(" << scale << ")\">" << sub << "</g>";
                oss << "</g>";
                return oss.str();
            });
        registerTag("munderover",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 3) return "<!-- munderover needs 3 children -->";

                const double em = std::stof(st.fontSize);
                const double gap = 0.2 * em;
                const double scale = 0.7;

                const std::string& base = kids[0];
                const std::string& sub = kids[1];
                const std::string& sup = kids[2];

                /* 主字符 */
                double baseW = extractWidth(base);
                double baseAsc = extractAscent(base);
                double baseDes = extractDescent(base);

                /* 下标 */
                double subW = extractWidth(sub) * scale;
                double subAsc = extractAscent(sub) * scale;
                double subDes = extractDescent(sub) * scale;


                /* 上标 */
                double supW = extractWidth(sup) * scale;
                double supAsc = extractAscent(sup) * scale;
                double supDes = extractDescent(sup) * scale;


                /* ---------- 整体盒尺寸（以主字符基线为 0） ---------- */
                double totalW = std::max({ baseW, subW, supW });
                double baseX = (totalW - baseW) / 2.0;
                double subX = (totalW - subW) / 2.0;
                double supX = (totalW - supW) / 2.0;

                /* 上标位置：主字符顶部上方 gap，再补偿上标自身基线 */
                double supY = -(-supDes + baseAsc + gap);

                /* 下标位置：主字符底部下方 gap，再补偿下标自身基线 */
                double subY = (subAsc + gap - baseDes);

                double totalAsc = baseAsc + gap + (supAsc - supDes); // 最上沿
                double totalDes = baseDes - gap - (subAsc - subDes); // 最下沿
                double totalH = totalAsc + totalDes;

                std::ostringstream oss;
                oss << "<g class=\"munderover\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes << "\">";
                oss << "<g transform=\"translate(" << supX << "," << supY << ") scale(" << scale << ")\">" << sup << "</g>";
                oss << "<g transform=\"translate(" << baseX << ",0)\">" << base << "</g>";
                oss << "<g transform=\"translate(" << subX << "," << subY << ") scale(" << scale << ")\">" << sub << "</g>";
                oss << "</g>";
                return oss.str();
            });
        // ------------------------------------------------------------------
// 1. mover  (上划线 / 下划线 / 上下箭头)
// 语法: <mover> base overscript </mover>
// ------------------------------------------------------------------
        registerTag("mover",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                std::vector<std::string> kids;
                for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())
                    kids.push_back(renderElement(c, st));
                if (kids.size() != 2) return "<!-- mover needs 2 children -->";

                const double scale = 0.7;
                const double gap = 0.2 * std::stof(st.fontSize); // 0.2 em

                const std::string& base = kids[0];
                const std::string& over = kids[1];

                /* 主字符 */
                double baseW = extractWidth(base);
                double baseAsc = extractAscent(base);
                double baseDes = extractDescent(base);

                /* 上标原始尺寸 */
                double overW0 = extractWidth(over);
                double overAsc0 = extractAscent(over);
                double overDes0 = extractDescent(over);


                /* 缩放后尺寸 */
                double overW = overW0 * scale;
                double overAsc = overAsc0 * scale;
                double overDes = overDes0 * scale;

                /* ---------- 整体盒尺寸（以主字符基线为 0） ---------- */
                double totalW = std::max(baseW, overW);
                double baseX = 0;                      // 主字符左上角 x
                double overX = (totalW - overW) / 2.0;   // 上标居中

                /* 上标位置：主字符顶部上方 gap，再补偿上标自身基线 */
                double overY = -(-overDes + baseAsc + gap);

                double totalAsc = baseAsc + gap + (overAsc - overDes); // 最上沿
                double totalDes = baseDes;                           // 最下沿
                double totalH = totalAsc + totalDes;

                std::ostringstream oss;
                oss << "<g class=\"mover\" data-w=\"" << totalW
                    << "\" data-asc=\"" << totalAsc
                    << "\" data-desc=\"" << totalDes
                    << "\">";
                /* 主字符：基线 y=0 */
                oss << "<g transform=\"translate(" << baseX << ",0)\">" << base << "</g>";
                /* 上标：基线补偿后摆放 */
                oss << "<g transform=\"translate(" << overX << "," << overY << ") scale(" << scale << ")\">"
                    << over << "</g>";
                oss << "</g>";
                return oss.str();
            });
        // ------------------------------------------------------------------
// 2. mpadded  (人工设置宽度/高度/深度)
// 语法: <mpadded width="..." height="..." depth="..."> child </mpadded>
// 单位：em，可省略符号
// ------------------------------------------------------------------
        registerTag("mpadded",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                const XMLElement* child = e->FirstChildElement();
                if (!child) return "";

                std::string inner = renderElement(child, st);
                double em = std::stof(st.fontSize);

                /* 解析属性，缺省用子元素自身尺寸 */
                double w = getDimAttr(e, "width", extractWidth(inner) / em) * em;
                double asc = getDimAttr(e, "height", extractAscent(inner) / em) * em;
                double des = getDimAttr(e, "depth", extractDescent(inner) / em) * em;

                /* 子元素基线到盒上下沿的距离 */
                double innerAsc = extractAscent(inner);
                double innerDes = extractDescent(inner);

                /* 居中放置：水平居中，垂直按基线对齐 */
                double dx = (w - extractWidth(inner)) * 0.5;
                double dy = asc - innerAsc;   // 使子元素基线落在 y=0

                std::ostringstream oss;
                oss << "<g class=\"mpadded\" data-w=\"" << w
                    << "\" data-asc=\"" << asc
                    << "\" data-desc=\"" << des
                    << "0\">"
                    << "<g transform=\"translate(" << dx << "," << dy << ")\">"
                    << inner
                    << "</g>"
                    << "</g>";
                return oss.str();
            });

        // ------------------------------------------------------------------
        // 3. mphantom  (占位但不显示)
        // 语法: <mphantom> child </mphantom>
        // 与 mpadded 类似，但把内容设为透明
        // ------------------------------------------------------------------
        registerTag("mphantom",
            [this](const XMLElement* e, const Style& st) -> std::string
            {
                const XMLElement* child = e->FirstChildElement();
                if (!child) return "";

                std::string inner = renderElement(child, st);

                std::ostringstream oss;
                oss << "<g class=\"mphantom\" data-w=\"" << extractWidth(inner)
                    << "\" data-asc=\"" << extractAscent(inner)
                    << "\" data-desc=\"" << extractDescent(inner) << "\">"
                    << "<g fill=\"transparent\">"
                    << inner
                    << "</g>"
                    << "</g>";
                return oss.str();
            });

        // ----------------------------------------------------------
// 1. <none>  —— 空占位，宽 0，高/深 0
// ----------------------------------------------------------
        registerTag("none",
            [](const XMLElement*, const Style&) -> std::string
            {
                return R"(<g class="none" data-w="0"  data-asc="0" data-desc="0" />)";
            });

        // ----------------------------------------------------------
        // mprescripts —— 占位节点，尺寸 0，基线 0
        // ----------------------------------------------------------
        registerTag("mprescripts",
            [](const XMLElement*, const Style&) -> std::string
            {
                return R"(<g class="mprescripts" data-w="0"  data-asc="0" data-desc="0" />)";
            });

        // ----------------------------------------------------------
        // mspace —— 只产生水平间距
        // width 支持 2.5、2.5em、2.5ex 等写法
        // ----------------------------------------------------------
        registerTag("mspace",
            [](const XMLElement* e, const Style& st) -> std::string
            {
                const char* w = e->Attribute("width");
                double width = 0.0;

                if (w) {
                    std::string s = w;
                    if (s.find("em") != std::string::npos) {
                        width = std::stod(s) * std::stof(st.fontSize);
                    }
                    else if (s.find("ex") != std::string::npos) {
                        width = std::stod(s) * std::stof(st.fontSize) * 0.5; // 1ex ≈ 0.5em
                    }
                    else {
                        width = std::stod(s);          // 纯数字，默认单位 em
                    }
                }

                std::ostringstream oss;
                oss << "<g class=\"mspace\" data-w=\"" << width
                    << "\"  data-asc=\"0\" data-desc=\"0\" />";
                return oss.str();
            });
    }
};

FreeTypeTextMeasurer& FreeTypeTextMeasurer::instance() {
    static FreeTypeTextMeasurer inst;
    return inst;
}

FreeTypeTextMeasurer::FreeTypeTextMeasurer() {
    if (FT_Init_FreeType(&ft_)) throw std::runtime_error("FT_Init_FreeType failed");
}

FreeTypeTextMeasurer::~FreeTypeTextMeasurer() {
    for (auto& kv : cache_) FT_Done_Face(kv.second.face);
    FT_Done_FreeType(ft_);
}

static std::wstring makeKey(const std::wstring & name, int style) {
    return name + L'|' + std::to_wstring(style);
}


FT_Face FreeTypeTextMeasurer::loadFace(const std::wstring & fontName, int style) {
    // 简单映射：Windows 字体目录
    wchar_t winFontPath[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, 0, winFontPath);

    std::wstring file = winFontPath;
    file += L"\\";
    if (fontName == L"Times New Roman") file += (style & 1) ? L"timesbd.ttf" : L"times.ttf";
    else if (fontName == L"Arial")      file += (style & 1) ? L"arialbd.ttf" : L"arial.ttf";
    else                                file += fontName + L".ttf";

    FT_Face face;
    if (FT_New_Face(instance().ft_, w2a(file).c_str(), 0, &face)) return nullptr;
    return face;
}

FreeTypeTextMeasurer::CachedFace& FreeTypeTextMeasurer::getFace(const std::wstring & fontName, int style) {
    std::wstring key = makeKey(fontName, style);
    std::lock_guard<std::mutex> lock(mtx_);
    auto& slot = cache_[key];
    if (!slot.face) {
        slot.face = loadFace(fontName, style);
        if (!slot.face) slot.face = loadFace(L"Times New Roman", 0); // fallback
        slot.emSize = slot.face->units_per_EM;
    }
    return slot;
}

FreeTypeTextMeasurer::Size
FreeTypeTextMeasurer::measure(const std::wstring & text,
    const std::wstring & fontName,
    float               fontSizePx,
    int                 style) {
    auto& slot = getFace(fontName, style);
    FT_Face face = slot.face;
    float scale = fontSizePx / slot.emSize;

    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fontSizePx);

    float width = 0.f;
    float maxY = -FLT_MAX, minY = FLT_MAX;

    for (wchar_t ch : text) {
        FT_UInt idx = FT_Get_Char_Index(face, ch);
        if (FT_Load_Glyph(face, idx, FT_LOAD_DEFAULT)) continue;
        FT_GlyphSlot g = face->glyph;

        width += (g->advance.x >> 6);   // 26.6 固定小数 → 像素
        if (g->metrics.horiBearingY > maxY) maxY = g->metrics.horiBearingY >> 6;
        if (g->metrics.horiBearingY - g->metrics.height < minY)
            minY = (g->metrics.horiBearingY - g->metrics.height) >> 6;
    }

    Size sz;
    sz.width = width;
    sz.height = (maxY - minY);
    sz.ascent = maxY;
    sz.descent = minY;         // baseline → bottom（minY 为负值）
    return sz;
}

/* ---------- 单例 ---------- */
MathML2SVG& MathML2SVG::instance() {
    static MathML2SVG inst;
    return inst;
}
MathML2SVG::MathML2SVG() : pImpl(std::make_unique<Impl>()) {}
MathML2SVG::~MathML2SVG() = default;

/* 暴露接口 */
std::string MathML2SVG::convert(const std::string & mathml) {
    return pImpl->convert(mathml);
}
void MathML2SVG::registerTag(const std::string & tag, RenderFn fn) {
    pImpl->registerTag(tag, fn);
}
void MathML2SVG::registerAttr(const std::string & attr, AttrFn fn) {
    pImpl->registerAttr(attr, fn);
}
