#pragma once

#include "WindowBase.h"

LRESULT ViewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void register_view_class();
