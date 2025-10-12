#pragma once
// main.cpp  ——  优化后完整单文件
#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "resource.h"
#include <windows.h>
#include <windowsx.h>   // 加这一行

#include <mmsystem.h>

#include <commctrl.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#include <cmath>
#include <memory>
#include <string>
#include <future>
#include <unordered_map>
#include <algorithm>







#include <objidl.h>
#include <filesystem>
#include <gdiplus.h>

using namespace Gdiplus;

#include <shlwapi.h>
#include <regex>


#include <wininet.h>



#include <unordered_set>
#include <chrono>
#include <thread>
#include <set>


#include <codecvt>
#include <locale>



#include <wincodec.h>


#include <cctype>

#include <queue>
#include <robuffer.h>   // IBufferByteAccess
#include <new>

#include <iostream>


#ifndef HR
#define HR(hr)  do { HRESULT _hr_ = (hr); if(FAILED(_hr_)) return 0; } while(0)
#endif


#include <atomic>


#include <condition_variable>
#include <array>


#include <cstdint>
#include <cwctype>



#include <cstring>
#include <stack>
#include <blake3.h>
#include <boost/algorithm/string.hpp>


#include <Shlobj.h>      // SHGetKnownFolderPath
#include <KnownFolders.h>
#include <numeric>
#include <commdlg.h>   // OPENFILENAMEW, GetOpenFileNameW
#include <shobjidl.h> // 包含任务对话框头文件
#include <mutex>
#define STB_IMAGE_IMPLEMENTATION

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "windowscodecs.lib")


#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

#include <gumbo.h>
#include <tinyxml2.h>

#include "MemFile.h"
#include "EPUBBook.h"
#include "SimpleContainer.h"
#include "MML2SVG.h"
#include "a2w_w2a.h"
#include "ReadingRecord.h"
#include "VirtualDoc.h"
#include "ScrollbarWindow.h"
#include "TocWindow.h"
#include "AppBootstrap.h"
namespace fs = std::filesystem;


















struct GetDocParam
{
    int        client_h;
    int        scrollY;
    int        offsetY;
    HWND       notify_hwnd;   // 通知窗口
};


class AccelManager {
public:
    explicit AccelManager(HWND h) : m_hwnd(h) {}

    // 添加/更新一条快捷键
    void set(WORD cmd, BYTE fVirt, WORD key) {
        // 先删除同命令的旧项
        erase(cmd);
        m_entries.push_back({ fVirt, key, cmd });
        rebuild();
    }
    void add(WORD cmd, BYTE fVirt, WORD key) {
        m_entries.push_back({ fVirt, key, cmd });
        rebuild();
    }
    // 删除某命令
    void erase(WORD cmd) {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [=](const ACCEL& a) { return a.cmd == cmd; }),
            m_entries.end());
        rebuild();
    }

    // 在消息循环里调用
    bool translate(MSG* m) {
        return m_hAccel && TranslateAccelerator(m_hwnd, m_hAccel, m);
    }

private:
    void rebuild() {
        if (m_hAccel) DestroyAcceleratorTable(m_hAccel);
        m_hAccel = m_entries.empty()
            ? nullptr
            : CreateAcceleratorTable(m_entries.data(),
                static_cast<int>(m_entries.size()));
    }

    std::vector<ACCEL> m_entries;
    HACCEL m_hAccel = nullptr;
    HWND m_hwnd;
};



struct BmpHeader {
    uint16_t bfType = 0x4D42;          // 'BM'
    uint32_t bfSize = 0;
    uint16_t bfReserved1 = 0;
    uint16_t bfReserved2 = 0;
    uint32_t bfOffBits = 54;           // 54 = sizeof(BmpHeader) + sizeof(BmpInfo)
};
struct BmpInfo {
    uint32_t biSize = 40;
    int32_t  biWidth = 0;
    int32_t  biHeight = 0;
    uint16_t biPlanes = 1;
    uint16_t biBitCount = 32;
    uint32_t biCompression = 0;      // BI_RGB
    uint32_t biSizeImage = 0;
    int32_t  biXPelsPerMeter = 0;
    int32_t  biYPelsPerMeter = 0;
    uint32_t biClrUsed = 0;
    uint32_t biClrImportant = 0;
};
//
//namespace mathml2tex {
//
//    /* 唯一对外接口 */
//    std::string convert(const std::string& mathml);
//
//} // namespace mathml2tex

//enum PuncClass {
//    PC_NORMAL,   // 普通字符
//    PC_LEFT,   // 左引号、左括号、书名号前半
//    PC_RIGHT,  // 右引号、右括号、句末标点
//    PC_MIDDLE, // 破折号、省略号（不可拆）
//};
//
//static PuncClass classify(UChar32 cp) {
//    switch (cp) {
//        /* ---------- 左半部分 ---------- */
//    case 0x3008: case 0x300A: case 0x300C: case 0x300E: // 〈 《 「 『
//    case 0xFF08:                                        // （
//    case 0x3010: case 0x3014: case 0x3016: case 0x3018: // 【 〔 〖 〘
//    case 0xFF3B: case 0xFF5B: case 0xFF5F:              // ［ ｛ ｟
//    case 0x2018: case 0x201C:                           // ‘ “
//        return PC_LEFT;
//
//        /* ---------- 右半部分 ---------- */
//    case 0x3001: case 0x3002:                           // 、 。
//    case 0xFF01: case 0xFF1F:                           // ！ ？
//    case 0xFF0C: case 0xFF1B: case 0xFF1A:              // ， ； ：
//    case 0x3009: case 0x300B: case 0x300D: case 0x300F: // 〉 》 」 』
//    case 0xFF09:                                        // ）
//    case 0x3011: case 0x3015: case 0x3017: case 0x3019: // 】 〕 〗 〙
//    case 0xFF3D: case 0xFF5D: case 0xFF60:              // ］ ｝ ｠
//    case 0x2019: case 0x201D:                           // ’ ”
//        return PC_RIGHT;
//
//        /* ---------- 中间整体 ---------- */
//    case 0x2014:                                        // —  破折号
//    case 0x2026:                                        // …  省略号
//    case 0x2025:                                        // ‥  二点省略
//        return PC_MIDDLE;
//
//    default:
//        return PC_NORMAL;
//    }
//}





struct FontItem
{
    std::wstring familyName;   // 字体原名（en-us）
    std::wstring displayName;  // 中文名（zh-cn），没有就用 familyName
};