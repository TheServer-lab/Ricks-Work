#include <windows.h>
#include <windowsx.h>
#include <string>
#include "softgui_win.hpp"
#include "img_rnd.hpp"

using namespace SoftGUI;
using namespace ImgRnd;

struct ViewerState {
    ImageRenderer img;
    float zoom = 1.0f;
    int offsetX = 0, offsetY = 0;
    bool dragging = false;
    POINT lastMouse = {0, 0};
} g_viewer;

// Forward declarations
void LoadImageFile(HWND hwnd);
void DrawImageView(HDC hdc, RECT rc);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            LoadImageFile(hwnd);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            DrawImageView(hdc, rc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float factor = (delta > 0) ? 1.1f : 0.9f;
            g_viewer.zoom *= factor;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_LBUTTONDOWN:
            g_viewer.dragging = true;
            SetCapture(hwnd);
            g_viewer.lastMouse.x = GET_X_LPARAM(lParam);
            g_viewer.lastMouse.y = GET_Y_LPARAM(lParam);
            return 0;

        case WM_MOUSEMOVE:
            if (g_viewer.dragging) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                g_viewer.offsetX += (x - g_viewer.lastMouse.x);
                g_viewer.offsetY += (y - g_viewer.lastMouse.y);
                g_viewer.lastMouse.x = x;
                g_viewer.lastMouse.y = y;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;

        case WM_LBUTTONUP:
            g_viewer.dragging = false;
            ReleaseCapture();
            return 0;

        case WM_KEYDOWN:
            if (wParam == 'O') LoadImageFile(hwnd);
            else if (wParam == VK_ESCAPE) PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void LoadImageFile(HWND hwnd) {
    OPENFILENAMEW ofn{};
    wchar_t path[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        g_viewer.img.Load(path);
        g_viewer.zoom = 1.0f;
        g_viewer.offsetX = g_viewer.offsetY = 0;
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

void DrawImageView(HDC hdc, RECT rc) {
    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    if (!g_viewer.img.IsValid()) {
        DrawTextW(hdc, L"Press 'O' to open an image.", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    int imgW = g_viewer.img.GetWidth();
    int imgH = g_viewer.img.GetHeight();

    int scaledW = (int)(imgW * g_viewer.zoom);
    int scaledH = (int)(imgH * g_viewer.zoom);
    int x = (winW - scaledW) / 2 + g_viewer.offsetX;
    int y = (winH - scaledH) / 2 + g_viewer.offsetY;

    g_viewer.img.Draw(hdc, x, y, scaledW, scaledH, false);
}

// Correct entry point for GUI subsystem
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    const wchar_t CLASS_NAME[] = L"SoftImageViewer";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"SoftGUI Image Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        nullptr, nullptr, hInst, nullptr
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}