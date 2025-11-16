#include "FontWindow.h"

INT_PTR CALLBACK FontDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        HWND hList = GetDlgItem(hDlg, IDC_LIST_FONT);
        for (size_t i = 0; i < g_fontList.size(); ++i)
        {
            const FontItem& fi = g_fontList[i];
            int pos = (int)SendMessage(hList, LB_ADDSTRING, 0,
                (LPARAM)fi.displayName.c_str());
            // 把索引 i 存进去
            SendMessage(hList, LB_SETITEMDATA, pos, (LPARAM)i);
        }
        SendMessage(hList, LB_SETCURSEL, 0, 0);

        // 设置"启用实时预览"复选框的初始状态
        HWND hRealtimePreviewCheck = GetDlgItem(hDlg, IDM_TOGGLE_FONT_REALTIME_PREVIEW);
        SendMessage(hRealtimePreviewCheck, BM_SETCHECK,
            g_cfg.enableFontRealtimePreview ? BST_CHECKED : BST_UNCHECKED, 0);
        return TRUE;
    }
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hDlg, &rc);

        // 用白色填充整个客户区
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        return TRUE; // 表示我们已经处理了背景擦除
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wp;
        HWND hwndStatic = (HWND)lp;
        UINT ctrlId = GetDlgCtrlID(hwndStatic);

        // 处理复选框背景
        if (ctrlId == IDM_TOGGLE_CUSTOM_FONT ||
            ctrlId == IDM_TOGGLE_FONT_REALTIME_PREVIEW)
        {
            SetBkMode(hdcStatic, TRANSPARENT);
            SetTextColor(hdcStatic, RGB(0, 0, 0)); // 黑色文本

            // 使用淡蓝色背景 (RGB: 240, 248, 255 - AliceBlue)
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
            return (INT_PTR)hBrush; // 注意: Windows会自动删除这个画刷
        }
        return FALSE;
    }


    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_LIST_FONT:   // 来自列表框的消息
            switch (HIWORD(wp))
            {
            case LBN_SELCHANGE:   // 单击改变选择
            case LBN_DBLCLK:      // 双击
            {
                if (!g_cfg.enableFontRealtimePreview) { break; }
                HWND hList = (HWND)lp;
                int pos = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (pos != LB_ERR)
                {
                    size_t idx = (size_t)SendMessage(hList, LB_GETITEMDATA, pos, 0);
                    g_cfg.font_name = g_fontList[idx].familyName;   // 立即保存
                    if (g_vd) { g_vd->reload(); }
                }
                return TRUE;
            }
            }
            break;

        case IDM_TOGGLE_FONT_REALTIME_PREVIEW:
            g_cfg.enableFontRealtimePreview = !g_cfg.enableFontRealtimePreview;          // 切换状态
            break;
        case IDOK:
        {
            HWND hList = GetDlgItem(hDlg, IDC_LIST_FONT);
            int pos = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (pos != LB_ERR)
            {
                size_t idx = (size_t)SendMessage(hList, LB_GETITEMDATA, pos, 0);

                if (idx < g_fontList.size())
                {
                    g_cfg.font_name = g_fontList[idx].familyName;   // 立即保存
                    if (g_vd) { g_vd->reload(); }
                    EndDialog(hDlg, static_cast<INT_PTR>(idx + 1)); // 任意非 0
                    return TRUE;
                }


            }

            EndDialog(hDlg, 0);


            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

