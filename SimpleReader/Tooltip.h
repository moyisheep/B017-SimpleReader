#pragma once
#include "WindowBase.h"

void register_tooltip_class();

LRESULT TooltipProc(HWND hwnd, UINT m, WPARAM w, LPARAM l);
