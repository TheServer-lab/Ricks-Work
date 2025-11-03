// soft_text_editor.cpp
// Simple Windows text editor (single-file) using the Win32 API.
// A lightweight "soft GUI"-style text editor: New / Open / Save / Save As / Exit
// Compile with: g++ soft_text_editor.cpp -o soft_text_editor.exe -mwindows -lcomdlg32 -lgdi32

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string>
#include <fstream>

// Menu command IDs
#define IDM_FILE_NEW   1001
#define IDM_FILE_OPEN  1002
#define IDM_FILE_SAVE  1003
#define IDM_FILE_SAVEAS 1004
#define IDM_FILE_EXIT  1005

HWND hEdit = NULL;
HWND hMain = NULL;
std::string currentFile;
bool isModified = false;

void UpdateTitle()
{
    std::string title = "Soft Text Editor";
    if (!currentFile.empty()) {
        title += " - ";
        title += currentFile;
    }
    if (isModified) title = "*" + title;
    SetWindowTextA(hMain, title.c_str());
}

bool SaveToFile(const std::string &filename)
{
    int len = GetWindowTextLengthA(hEdit);
    std::string buf;
    buf.resize(len);
    GetWindowTextA(hEdit, &buf[0], len + 1);

    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) return false;
    ofs.write(buf.data(), buf.size());
    ofs.close();
    return true;
}

bool LoadFromFile(const std::string &filename)
{
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return false;
    std::string contents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    SetWindowTextA(hEdit, contents.c_str());
    return true;
}

bool DoFileSaveAs()
{
    char szFile[260] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "txt";
    if (GetSaveFileNameA(&ofn)) {
        if (SaveToFile(ofn.lpstrFile)) {
            currentFile = ofn.lpstrFile;
            isModified = false;
            UpdateTitle();
            return true;
        }
    }
    return false;
}

bool DoFileSave()
{
    if (currentFile.empty()) return DoFileSaveAs();
    if (SaveToFile(currentFile)) {
        isModified = false;
        UpdateTitle();
        return true;
    }
    return false;
}

bool DoFileOpen()
{
    if (isModified) {
        int r = MessageBoxA(hMain, "Current file has unsaved changes. Save?", "Unsaved Changes", MB_YESNOCANCEL | MB_ICONWARNING);
        if (r == IDCANCEL) return false;
        if (r == IDYES) {
            if (!DoFileSave()) return false;
        }
    }

    char szFile[260] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        if (LoadFromFile(ofn.lpstrFile)) {
            currentFile = ofn.lpstrFile;
            isModified = false;
            UpdateTitle();
            return true;
        } else {
            MessageBoxA(hMain, "Failed to open file.", "Error", MB_OK | MB_ICONERROR);
        }
    }
    return false;
}

void DoFileNew()
{
    if (isModified) {
        int r = MessageBoxA(hMain, "Current file has unsaved changes. Save?", "Unsaved Changes", MB_YESNOCANCEL | MB_ICONWARNING);
        if (r == IDCANCEL) return;
        if (r == IDYES) {
            if (!DoFileSave()) return;
        }
    }
    SetWindowTextA(hEdit, "");
    currentFile.clear();
    isModified = false;
    UpdateTitle();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
    {
        // Create menu
        HMENU hMenubar = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenuA(hFile, MF_STRING, IDM_FILE_NEW, "&New\tCtrl+N");
        AppendMenuA(hFile, MF_STRING, IDM_FILE_OPEN, "&Open...\tCtrl+O");
        AppendMenuA(hFile, MF_STRING, IDM_FILE_SAVE, "&Save\tCtrl+S");
        AppendMenuA(hFile, MF_STRING, IDM_FILE_SAVEAS, "Save &As...");
        AppendMenuA(hFile, MF_SEPARATOR, 0, NULL);
        AppendMenuA(hFile, MF_STRING, IDM_FILE_EXIT, "E&xit");
        AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hFile, "&File");
        SetMenu(hWnd, hMenubar);

        // Create multi-line edit control
        hEdit = CreateWindowExA(0, "EDIT", "",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_BORDER,
                                 0, 0, 100, 100, hWnd, (HMENU)1, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        // Set a monospaced font for the edit control
        HFONT hFont = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  FIXED_PITCH | FF_MODERN, "Consolas");
        SendMessageA(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        UpdateTitle();
    }
    return 0;

    case WM_SIZE:
        if (hEdit) SetWindowPos(hEdit, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER);
        return 0;

    case WM_SETFOCUS:
        if (hEdit) SetFocus(hEdit);
        return 0;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case IDM_FILE_NEW: DoFileNew(); break;
        case IDM_FILE_OPEN: DoFileOpen(); break;
        case IDM_FILE_SAVE: DoFileSave(); break;
        case IDM_FILE_SAVEAS: DoFileSaveAs(); break;
        case IDM_FILE_EXIT: PostMessageA(hWnd, WM_CLOSE, 0, 0); break;
        default:
            // Edit notifications
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hEdit) {
                isModified = true;
                UpdateTitle();
            }
            break;
        }
    }
    return 0;

    case WM_CLOSE:
        if (isModified) {
            int r = MessageBoxA(hWnd, "There are unsaved changes. Save before exiting?", "Unsaved Changes", MB_YESNOCANCEL | MB_ICONWARNING);
            if (r == IDCANCEL) return 0;
            if (r == IDYES) {
                if (!DoFileSave()) return 0;
            }
        }
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "SoftTextEditorClass";

    if (!RegisterClassA(&wc)) return -1;

    hMain = CreateWindowA(wc.lpszClassName, "Soft Text Editor", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          CW_USEDEFAULT, CW_USEDEFAULT, 900, 600, NULL, NULL, hInstance, NULL);

    if (!hMain) return -1;

    ShowWindow(hMain, nCmdShow);
    UpdateWindow(hMain);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
