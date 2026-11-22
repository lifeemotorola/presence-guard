/* A separate, disposable input window for the real Windows hook smoke test. */
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  HWND window = CreateWindowExW(0, L"STATIC", L"TypeBloom Test Input", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                40, 40, 400, 170, nullptr, nullptr, instance, nullptr);
  if (!window) return 1;
  HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                              16, 20, 350, 32, window, reinterpret_cast<HMENU>(201), instance, nullptr);
  CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
                  16, 65, 350, 32, window, reinterpret_cast<HMENU>(202), instance, nullptr);
  SetForegroundWindow(window);
  SetFocus(edit);
  MSG msg;
  while (IsWindow(window) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}
