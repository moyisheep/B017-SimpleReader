#pragma once

#define WIN32_LEAN_AND_MEAN   
#include <Windows.h>
#include <string>

class IBaseWindow
{
public:
	IBaseWindow(HWND parent, HINSTANCE hinst, LPCWSTR window_class);
	//HWND Create(HWND parent, HINSTANCE hinst, LPCWSTR window_class);
	virtual LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
	~IBaseWindow();
	HWND GetHandle();
private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	HWND m_hwnd = nullptr;

};