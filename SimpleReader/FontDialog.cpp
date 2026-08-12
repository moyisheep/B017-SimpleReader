#include "FontDialog.h"

#include <iostream>

#include "Tools.h"
#include "AppSettings.h"


std::vector<std::wstring> GetSystemFonts() {
    std::vector<std::wstring> fonts;
    HDC hdc = GetDC(NULL);
    LOGFONT lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET;

    EnumFontFamiliesExW(
        hdc, &lf,
        [](const LOGFONT* lpelfe, const TEXTMETRIC*, DWORD, LPARAM lParam) {
            auto& fonts = *reinterpret_cast<std::vector<std::wstring>*>(lParam);
            fonts.push_back(lpelfe->lfFaceName);
            return 1;
        },
        reinterpret_cast<LPARAM>(&fonts), 0
            );

    ReleaseDC(NULL, hdc);
    return fonts;
}

// 显示自定义字体选择对话框
std::wstring ShowSimpleFontDialog(HWND hParent) {
    std::vector<std::wstring> fonts = GetSystemFonts();

    // 创建 ComboBox 窗口
    HWND hCombo = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        10, 10, 300, 200, hParent, NULL, NULL, NULL
    );

    // 填充字体列表
    for (const auto& font : fonts) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font.c_str()));
    }

    // 显示模态对话框
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 获取用户选择的字体
    WCHAR selectedFont[LF_FACESIZE] = { 0 };
    SendMessageW(hCombo, CB_GETLBTEXT, SendMessageW(hCombo, CB_GETCURSEL, 0, 0), reinterpret_cast<LPARAM>(selectedFont));

    DestroyWindow(hCombo);
    return selectedFont;
}
void ChooseFontWithDialog(HWND hwnd)
{
    // 1. 准备一个 LOGFONTW 结构体
    LOGFONTW lf = { 0 };
    wcscpy_s(lf.lfFaceName, a2w(g_cfg.font_name).c_str());   // 默认字体
    lf.lfHeight = -g_cfg.font_size;                      // 16 像素高

    // 2. 填充 CHOOSEFONT
    CHOOSEFONTW cf = { 0 };
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hwnd;               // 没有父窗口
    cf.lpLogFont = &lf;                   // 输入/输出字体
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    // 3. 弹出对话框
    if (ChooseFontW(&cf))
    {

        HFONT hFont = CreateFontIndirectW(&lf);
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);  // 选入设备上下文
        TCHAR fontName[LF_FACESIZE];
        GetTextFace(hdc, LF_FACESIZE, fontName);
        SelectObject(hdc, oldFont);  // 恢复旧字体
        ReleaseDC(NULL, hdc);
        g_cfg.font_name = w2a(fontName);

    }
    else
    {
        DWORD err = CommDlgExtendedError();
        if (err)
            std::wcout << L"ChooseFont 失败，错误码: " << err << L"\n";
        else
            std::wcout << L"用户取消\n";
    }
    return;
}



