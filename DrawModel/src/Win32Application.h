#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

class Win32Application
{
public:
  struct WindowInitParams
  {
    int width = 1280;
    int height = 720;
    int cmdShow = SW_NORMAL;
    HINSTANCE hInstance;
  };

  static int Run(const WindowInitParams& initParams);

  static void GetWindowSize(int& width, int& height);

  static HWND GetHwnd() { return m_hwnd; }
  static HINSTANCE GetHInstance() { return m_hInstance; }
private:
  static HWND m_hwnd;
  static HINSTANCE m_hInstance;
};

