#pragma once
// main.cpp  ——  优化后完整单文件
#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "resource.h"

#include <fstream>
#include <sstream>
#include <vector>

#include <memory>
#include <string>
#include <future>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <set>
#include <codecvt>
#include <locale>
#include <cctype>
#include <queue>
#include <robuffer.h>   // IBufferByteAccess
#include <new>
#include <iostream>
#include <numeric>
#include <mutex>

#include <wrl/client.h>
#include <wrl.h>
#include <wrl/implements.h>   // 关键
#include <windows.h>
#include <windowsx.h>   // 加这一行
#include <mmsystem.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commdlg.h>   // OPENFILENAMEW, GetOpenFileNameW
#include <shobjidl.h> // 包含任务对话框头文件
#include <objidl.h>
#include <Shlobj.h>      // SHGetKnownFolderPath
#include <KnownFolders.h>
#include <gdiplus.h>
#include <wininet.h>
#include <wincodec.h>
#include <atomic>
#include <condition_variable>
#include <array>
#include <shared_mutex>
#include <cstdint>
#include <cwctype>
#include <functional>
#include <cstring>
#include <stack>

#include <dwrite_3.h>
#include <d2d1_3.h>        // ID2D1DeviceContext / ID2D1Bitmap1
#include <d2d1_1.h>       // D2D 1.1
#include <d3d11.h>        // D3D11
#include <dxgi1_2.h>  // DXGI 1.2
#include <d2d1.h>
#include <d2d1helper.h>   // 保险起见，再带一次


#include <miniz/miniz.h>
#include <tinyxml2.h>


#include <litehtml.h>
#include <gumbo.h>
#include <sqlite3.h>


#include <boost/algorithm/string.hpp>

#include "SimpleContainer.h"
#include "Tools.h"
#include "Constants.h"
#include "ScrollBar.h"
#include "TocPanel.h"
#include "VirtualDoc.h"
#include "ViewWindow.h"
#include "ImageView.h"
#include "HomePage.h"
#include "Tooltip.h"
#include "MainWindow.h"
#include "FontDialog.h"

using namespace Gdiplus;



#ifndef HR
#define HR(hr)  do { HRESULT _hr_ = (hr); if(FAILED(_hr_)) return 0; } while(0)
#endif


#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "windowscodecs.lib")

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")



#include "MML2SVG.h"
#include "ReadingRecorder.h"
#include "EPUBBook.h"
#include "MOBIBook.h"
#include "DJVUBook.h"
#include "Book.h"
#include "AppSettings.h"
#include "AppBootstrap.h"
#include "AppStates.h"


#include "Timer.h"













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



//
//namespace mathml2tex {
//
//    /* 唯一对外接口 */
//    std::string convert(const std::string& mathml);
//
//} // namespace mathml2tex











