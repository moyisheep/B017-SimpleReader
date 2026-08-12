#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <set>

#define NOMINMAX
#include <Windows.h>
#include <ShlObj.h>
#include <dwrite.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <litehtml.h>
#include <gumbo.h>

#include "Constants.h"
#include "resource.h"

namespace fs = std::filesystem;

// 自闭合标签集合
static const std::set<std::string> void_tags = {
    "area", "base", "br", "col", "embed", "hr", "img",
    "input", "keygen", "link", "meta", "param", "source",
    "track", "wbr"
};


struct HtmlFeatureFlags {
    bool has_svg = false;
    bool has_math = false;
    bool has_script = false;
    bool all() const { return has_svg && has_math && has_script; }
};

std::wstring seconds2string(int64_t sec);

 std::string get_global_css();

 std::string w2a(const std::wstring& s);

 std::wstring a2w(const std::string& s);

 fs::path exe_dir();

fs::path documents_dir();

 std::string read_file(const fs::path& p);

void CheckAllMenuItem();

void convert_coordinate(POINT& pt);

bool SaveHDCAsBmp(HDC hdc, int width, int height, const wchar_t* name);

void LogToFile(const std::string& message);

 std::string make_temp_dir();

wchar_t* DupPath(const wchar_t* src);

 int64_t nowUs();

void OnFrameRateTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

void OnScrollTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

void OnFlush(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

void Tick(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

void OnUpdateTimer(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

void DumpBookRecord();

void UpdateCache(void);

bool IsMouseOverWindow(HWND hWnd);

void SetStatus(int pane, const wchar_t* msg);

std::wstring OpenEpubWithDialog(HWND hwnd);

std::string blade16(std::string_view data);

 void save_image(const ImageFrame& img, const std::filesystem::path& bmpPath);

void replace_svg_with_img(std::string& html, const fs::path& tempDir);

void replace_math_with_svg(std::string& html);

HtmlFeatureFlags detect_html_features(const std::string& html) noexcept;

void PreprocessHTML(std::string& html);

void preprocess_js(std::string& html);

std::string base64_encode(const std::vector<uint8_t>& in);

 std::vector<unsigned char> base64_decode(const std::string& in);
void LogPtrint(std::string txt);
void DumpHex(const wchar_t* tag, const std::wstring& s);
std::string _escape_html(const std::string& s);
std::string to_lower(const std::string& str);
std::string generate_html(litehtml::element::ptr elem);
 void gumbo_serialize(const GumboNode* node, std::string& out);
std::string get_document_html(litehtml::document::ptr doc);
void save_document_html(litehtml::document::ptr doc);
 bool ends_with(const std::string& str, const std::string& suffix);
 bool is_image_url(const char* url);
 std::wstring GetMainFontNameFromTextLayout(ComPtr<IDWriteTextLayout> pTextLayout);
 std::string GetFontNameFromTextFormat(ComPtr<IDWriteTextFormat> textFormat);
 void inject_global_css(std::string& html);
 void inject_css(std::string& html);
 void EnableClearType();
 void DbgPrint(const char* fmt, ...);
  bool is_word_boundary(wchar_t ch);
static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";