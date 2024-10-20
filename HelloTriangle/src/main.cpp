#include "Win32Application.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int __stdcall wWinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPWSTR lpCmdLine,
  _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  Win32Application::WindowInitParams initParams{
    .width = 1280,
    .height = 720,
    .cmdShow = nCmdShow,
    .hInstance = hInstance,
  };
  return Win32Application::Run(initParams);
}
