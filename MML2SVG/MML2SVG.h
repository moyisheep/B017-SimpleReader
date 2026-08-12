#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <regex>
#include <sstream>
#include <numeric>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Shlobj.h> 


#include <tinyxml2.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_GLYPH_H
#pragma comment(lib, "freetype.lib")

class MathML2SVG {
public:
    static MathML2SVG& instance();

    // 删除拷贝/移动
    MathML2SVG(const MathML2SVG&) = delete;
    MathML2SVG& operator=(const MathML2SVG&) = delete;
    MathML2SVG(MathML2SVG&&) = delete;
    MathML2SVG& operator=(MathML2SVG&&) = delete;

    /* 业务接口 */
    std::string convert(const std::string& mathml);

    struct Style {
        std::string fontSize = "20";
        std::wstring fontFamily = L"Cambria Math";
        std::string fill = "#000000";
        std::string fontStyle;
        std::string fontWeight;
    };
    /* 扩展点 */
    using AttrMap = std::unordered_map<std::string, std::string>;
    using RenderFn = std::function<std::string(const tinyxml2::XMLElement*, const Style&)>;
    using AttrFn = void(*)(const class tinyxml2::XMLAttribute*, class Style&);

    void registerTag(const std::string& tag, RenderFn  fn);
    void registerAttr(const std::string& attr, AttrFn fn);

private:
    MathML2SVG();
    ~MathML2SVG();

    class Impl;
    std::unique_ptr<Impl> pImpl;

};




class FreeTypeTextMeasurer {
public:
    static FreeTypeTextMeasurer& instance();   // Meyers 单例
    struct Size {
        float width = 0.f;
        float height = 0.f;
        float ascent = 0.f;   // baseline → top
        float descent = 0.f;  // baseline → bottom
    };
    Size measure(const std::wstring& text,
        const std::wstring& fontName,
        float               fontSizePx,
        int                 style = 0);   // 0=Regular, 1=Bold, 2=Italic

    std::string outlineToSVG(const std::wstring& text,
        const std::wstring& fontName,
        float               fontSizePx,
        const std::string& fill = "black");
private:
    struct CachedFace {
        FT_Face face = nullptr;
        float   emSize = 0.f;      // units_per_EM
    };
    FreeTypeTextMeasurer();
    ~FreeTypeTextMeasurer();
    static FT_Face loadFace(const std::wstring& fontName, int style);
    CachedFace& getFace(const std::wstring& fontName, int style);
    FreeTypeTextMeasurer(const FreeTypeTextMeasurer&) = delete;
    FreeTypeTextMeasurer& operator=(const FreeTypeTextMeasurer&) = delete;


    std::unordered_map<std::wstring, CachedFace> cache_;
    std::mutex                                   mtx_;

    FT_Library ft_ = nullptr;
};


