#pragma once

#include <vector>
#include <string>

#define NOMINMAX
#include <Windows.h>

std::vector<std::wstring> GetSystemFonts();

std::wstring ShowSimpleFontDialog(HWND hParent);

void ChooseFontWithDialog(HWND hwnd);
