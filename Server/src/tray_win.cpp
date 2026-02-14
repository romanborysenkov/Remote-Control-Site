#ifdef _WIN32
#include "tray_win.h"
#include "logger.h"
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

namespace {

const UINT WM_TRAYICON = WM_USER + 1;
const UINT ID_TRAY_EXIT = 1000;

std::atomic<bool>* g_quitFlag = nullptr;
HWND g_trayHwnd = nullptr;

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Вийти");
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            if (cmd == ID_TRAY_EXIT && g_quitFlag) {
                g_quitFlag->store(true);
                PostMessageW(hwnd, WM_QUIT, 0, 0);
            }
            return 0;
        }
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

namespace tray {

void run(std::atomic<bool>& quitFlag) {
    g_quitFlag = &quitFlag;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"RemoteControlTray";
    if (!RegisterClassExW(&wc)) return;

    g_trayHwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_trayHwnd) return;

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(1));
    if (!nid.hIcon) nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Remote Control Server");

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        Logger::warning("Не вдалося додати іконку в трей");
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Shell_NotifyIconW(NIM_DELETE, &nid);
    DestroyWindow(g_trayHwnd);
}

} // namespace tray
#endif
