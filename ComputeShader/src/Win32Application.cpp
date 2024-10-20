#include "App.h"
#include "Win32Application.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui/backends/imgui_impl_win32.h"

HWND Win32Application::m_hwnd;
HINSTANCE Win32Application::m_hInstance;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
  {
    return TRUE;
  }

  auto& theApp = GetApplication();
  switch (msg)
  {
  case WM_PAINT:
    if (theApp)
    {
      theApp->OnUpdate();
    }
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  default:
    break;
  }
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Win32Application::GetWindowSize(int& width, int& height)
{
  RECT rect = { 0 };
  GetClientRect(m_hwnd, &rect);
  width = rect.right - rect.left;
  height = rect.bottom - rect.top;
}

int Win32Application::Run(const WindowInitParams& initParams)
{
  auto& theApp = GetApplication();

  // ウィンドウの作成.
  WNDCLASSEX windowClass = { 0 };
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WndProc;
  windowClass.hInstance = initParams.hInstance;
  windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  windowClass.lpszClassName = L"D3D12Sample";
  RegisterClassEx(&windowClass);

  DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
  RECT windowRect = { 0, 0, static_cast<LONG>(1280), static_cast<LONG>(720) };
  AdjustWindowRect(&windowRect, dwStyle, FALSE);

  auto title = theApp->GetTitle();
  m_hwnd = CreateWindow(
    windowClass.lpszClassName,
    title.c_str(),
    dwStyle, CW_USEDEFAULT, CW_USEDEFAULT,
    windowRect.right - windowRect.left,
    windowRect.bottom - windowRect.top,
    nullptr, nullptr, windowClass.hInstance, nullptr);
  m_hInstance = initParams.hInstance;
  theApp->Initialize();

  ShowWindow(m_hwnd, SW_NORMAL);

  MSG msg = {};
  while (msg.message != WM_QUIT)
  {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  theApp->Shutdown();
  return static_cast<int>(msg.wParam);
}
