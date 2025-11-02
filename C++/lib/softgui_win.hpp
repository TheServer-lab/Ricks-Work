// softgui_win.hpp — SoftGUI expanded ANSI header-only implementation (double-buffered)
// Overwrite your existing softgui_win.hpp with this file.

#ifndef SOFTGUI_WIN_HPP
#define SOFTGUI_WIN_HPP

#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace SoftGUI {

// ---------- Basic types ----------
struct Color {
    uint8_t r,g,b;
    Color(): r(0),g(0),b(0) {}
    Color(int rr,int gg,int bb): r((uint8_t)rr), g((uint8_t)gg), b((uint8_t)bb) {}
    COLORREF toCOLORREF() const { return RGB(r,g,b); }
};

struct Geometry { int x=0,y=0,w=100,h=24; };

// ---------- Forward ----------
struct Widget;
class Window;
using WidgetPtr = std::shared_ptr<Widget>;

// ---------- Pack options (very small subset of Tk pack) ----------
enum class Side { TOP, LEFT };
enum class Fill { NONE, X, Y, BOTH };

struct PackOptions {
    Side side = Side::TOP;
    Fill fill = Fill::NONE;
    int padx = 0, pady = 0;
    PackOptions() {}
};

// ---------- Widget base ----------
struct Widget {
    Geometry geom;
    bool visible = true;
    std::string text;
    bool dirty = true;
    // parent container
    Widget* parent = nullptr;
    // style / font options
    std::string font_name = "Segoe UI";
    int font_size = 12;
    // callbacks
    std::function<void(Widget*)> on_click;
    std::function<void(Widget*, char)> on_key;
    std::function<void(Widget*)> on_focus;
    std::function<void(Widget*)> on_change;
    // layout helpers
    bool packed = false;
    PackOptions pack_opts;

    virtual ~Widget() {}
    virtual void measure() {}
    virtual void draw(HDC hdc) {}
    virtual void on_click_internal(int x,int y) { if (on_click) on_click(this); }
    virtual void on_key_internal(char ch) { if (on_key) on_key(this, ch); }
    virtual void mark_dirty() { dirty = true; if (parent) parent->mark_dirty(); }
    // helpers for font creation
    HFONT make_font(HDC hdc) {
        // Create a simple font based on font_name,font_size
        return CreateFontA(
            -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font_name.c_str());
    }
};

// ---------- Frame (container) ----------
struct Frame : Widget {
    std::vector<WidgetPtr> children;
    Frame() {}
    void add_child(WidgetPtr w) { 
        w->parent = this; 
        children.push_back(w); 
    }
    void draw(HDC hdc) override {
        // frames draw nothing by default (transparent); draw children
        for (auto &c : children) {
            if (c->visible) c->draw(hdc);
        }
    }
};

// ---------- Label ----------
struct Label : Widget {
    Label(const std::string& txt="") { text = txt; }
    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        // Fill background to avoid ghosting
        HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &r, bg);
        DeleteObject(bg);

        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, text.c_str(), (int)text.size(), &r, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, hOld);
    }
};

// ---------- Entry (single-line) ----------
struct Entry : Widget {
    bool focused = false;
    size_t caret = 0;
    std::chrono::steady_clock::time_point last_blink;
    bool caret_visible = true;
    Entry(const std::string &txt="") { text = txt; last_blink = std::chrono::steady_clock::now(); }

    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        // background (explicit)
        HBRUSH hbr = CreateSolidBrush(RGB(255,255,255));
        FillRect(hdc, &r, hbr);
        DeleteObject(hbr);
        // border
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);

        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        SetBkMode(hdc, TRANSPARENT);
        // text area
        RECT tr = r;
        tr.left += 4;
        DrawTextA(hdc, text.c_str(), (int)text.size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        // caret blinking (toggle based on time)
        if (focused) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_blink).count();
            if (ms > 500) { caret_visible = !caret_visible; last_blink = now; }
            if (caret_visible) {
                int cx = tr.left + TextWidth(hdc, text.c_str(), (int)caret);
                MoveToEx(hdc, cx, tr.top+4, nullptr);
                LineTo(hdc, cx, tr.bottom-4);
            }
        }

        SelectObject(hdc, hOld);
    }

    virtual void on_click_internal(int x,int y) override {
        focused = true;
        caret = std::min<size_t>(text.size(), (size_t)TextIndexFromPos(x));
        if (on_focus) on_focus(this);
        mark_dirty();
    }

    void on_key_internal(char ch) override {
        if (ch == '\b') {
            if (!text.empty() && caret>0) {
                text.erase(caret-1,1);
                caret = std::max<size_t>(0, caret-1);
                if (on_change) on_change(this);
            }
        } else if (ch == '\r') {
            // enter -> unfocus by default
            focused = false;
            if (on_change) on_change(this);
        } else if (ch >= 32) {
            text.insert(caret, 1, ch);
            caret++;
            if (on_change) on_change(this);
        }
        mark_dirty();
    }

private:
    static int TextWidth(HDC hdc, const char* s, int upto = -1) {
        if (!s) return 0;
        int len = (upto < 0) ? (int)strlen(s) : upto;
        SIZE sz{0,0};
        GetTextExtentPoint32A(hdc, s, len, &sz);
        return sz.cx;
    }
    // approximate position -> index (naive)
    int TextIndexFromPos(int px) {
        HDC hdc = GetDC(NULL);
        HFONT h = make_font(hdc);
        HFONT hold = (HFONT)SelectObject(hdc, h);
        int offset = px - (geom.x + 4);
        if (offset <= 0) { SelectObject(hdc, hold); DeleteObject(h); ReleaseDC(NULL, hdc); return 0; }
        int idx = 0;
        for (size_t i=1;i<=text.size();++i) {
            SIZE sz{0,0};
            GetTextExtentPoint32A(hdc, text.c_str(), (int)i, &sz);
            if (sz.cx >= offset) { idx = (int)i; break; }
            idx = (int)i;
        }
        SelectObject(hdc, hold);
        DeleteObject(h);
        ReleaseDC(NULL, hdc);
        return idx;
    }
};

// ---------- Button ----------
struct Button : Widget {
    std::function<void()> onclick0;
    Button(const std::string &txt="") { text = txt; }
    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        // draw background for consistent look
        HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(hdc, &r, bg);
        DeleteObject(bg);

        DrawFrameControl(hdc, &r, DFC_BUTTON, DFCS_BUTTONPUSH);
        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, text.c_str(), (int)text.size(), &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        SelectObject(hdc, hOld);
    }
    void on_click_internal(int x,int y) override {
        if (onclick0) onclick0();
        if (on_click) on_click(this);
    }
};

// ---------- Canvas ----------
struct Canvas : Widget {
    std::vector<uint8_t> buffer; // BGR per pixel
    int buf_w=0, buf_h=0;
    Canvas(int w=100,int h=100){ buf_w=w; buf_h=h; geom.w=w; geom.h=h; buffer.resize(w*h*3); }
    void put_pixel(int x,int y,const Color &c){
        if (x<0||y<0||x>=buf_w||y>=buf_h) return;
        int idx = (y*buf_w + x)*3;
        buffer[idx+0] = c.b;
        buffer[idx+1] = c.g;
        buffer[idx+2] = c.r;
        mark_dirty();
    }
    void clear(const Color &c=Color(255,255,255)) {
        for (int y=0;y<buf_h;y++) for (int x=0;x<buf_w;x++){
            int idx=(y*buf_w+x)*3;
            buffer[idx+0]=c.b; buffer[idx+1]=c.g; buffer[idx+2]=c.r;
        }
        mark_dirty();
    }
    void draw(HDC hdc) override {
        if (buf_w<=0||buf_h<=0) return;
        BITMAPINFO bmi;
        ZeroMemory(&bmi,sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = buf_w;
        bmi.bmiHeader.biHeight = -buf_h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(hdc, geom.x, geom.y, geom.w, geom.h,
                      0,0,buf_w,buf_h, buffer.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    }
};

// ---------- Window (host) ----------
class Window {
public:
    Window(int width, int height, const char* title) :
        width_(width), height_(height), title_(title)
    {
        register_class();
        create_window();
    }
    ~Window() {
        DestroyWindow(hwnd_);
        UnregisterClassA(wc_.lpszClassName, wc_.hInstance);
    }

    // Factories (return shared_ptr that you can store)
    std::shared_ptr<Label> make_label(const std::string &txt){ auto p = std::make_shared<Label>(txt); register_widget(p); return p; }
    std::shared_ptr<Entry> make_entry(const std::string &txt){ auto p = std::make_shared<Entry>(txt); register_widget(p); return p; }
    std::shared_ptr<Button> make_button(const std::string &txt){ auto p = std::make_shared<Button>(txt); register_widget(p); return p; }
    std::shared_ptr<Canvas> make_canvas(int w,int h){ auto p = std::make_shared<Canvas>(w,h); register_widget(p); return p; }
    std::shared_ptr<Frame> make_frame(){ auto p = std::make_shared<Frame>(); register_widget(p); return p; }

    // top-level add for widgets (also sets parent to window)
    void add_child(WidgetPtr w) {
        register_widget(w);
    }

    // pack/place helpers (like Tk)
    void pack(WidgetPtr w, const PackOptions &opts = PackOptions()) {
        if (!w) return;
        w->pack_opts = opts;
        w->packed = true;
        w->parent = nullptr; // top-level pack to window
        recompute_layout();
        InvalidateRect(hwnd_, NULL, TRUE);
    }
    void place(WidgetPtr w, int x, int y) {
        if (!w) return;
        w->geom.x = x; w->geom.y = y;
        w->parent = nullptr;
        w->packed = false;
        InvalidateRect(hwnd_, NULL, TRUE);
    }

    void measure() {
        // placeholder
    }

    void arrange() { recompute_layout(); }

    void mainloop(int fps = 60) {
        MSG msg;
        // No continuous timer redraw; painting done on-demand
        while (GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    HWND hwnd() const { return hwnd_; }

    void set_title(const char* t) { title_ = t; SetWindowTextA(hwnd_, t); }
    void resize(int w,int h) { width_=w; height_=h; SetWindowPos(hwnd_, NULL,0,0,w,h, SWP_NOMOVE|SWP_NOZORDER); recompute_layout(); InvalidateRect(hwnd_, NULL, TRUE); }

private:
    int width_, height_;
    std::string title_;
    HWND hwnd_ = NULL;
    WNDCLASSEXA wc_{};
    HINSTANCE hInst_ = GetModuleHandleA(NULL);

    // top-level widget storage (we own shared_ptr)
    std::vector<WidgetPtr> widgets_;
    // for pack-based layout we consider top-level ordered list
    std::vector<WidgetPtr> pack_order_;

    // focused entry handling
    std::shared_ptr<Entry> focused_entry_;

    // register widget
    void register_widget(WidgetPtr w) {
        if (!w) return;
        widgets_.push_back(w);
        pack_order_.push_back(w);
    }

    void register_class() {
        ZeroMemory(&wc_, sizeof(wc_));
        wc_.cbSize = sizeof(WNDCLASSEXA);
        wc_.style = CS_HREDRAW | CS_VREDRAW;
        wc_.lpfnWndProc = &Window::s_wndproc;
        wc_.cbClsExtra = 0;
        wc_.cbWndExtra = sizeof(void*);
        wc_.hInstance = hInst_;
        wc_.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc_.lpszClassName = "SoftGUIWindowClass";
        wc_.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExA(&wc_);
    }

    void create_window() {
        hwnd_ = CreateWindowExA(0, wc_.lpszClassName, title_.c_str(),
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                width_, height_,
                                NULL, NULL, hInst_, this);
        assert(hwnd_);
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }

    // layout recompute (very simple pack)
    void recompute_layout() {
        int cur_top = 10, cur_left = 10;
        for (auto &w : pack_order_) {
            if (!w->packed) continue;
            PackOptions &o = w->pack_opts;
            if (o.side == Side::TOP) {
                // width handling
                if (o.fill == Fill::X || o.fill == Fill::BOTH) {
                    w->geom.x = 10 + o.padx;
                    w->geom.w = (width_ - 20) - 2*o.padx;
                } else {
                    if (w->geom.w == 0) w->geom.w = 100;
                    w->geom.x = 10 + o.padx;
                }
                w->geom.y = cur_top + o.pady;
                cur_top += w->geom.h + o.pady + 8;
            } else { // LEFT
                if (o.fill == Fill::Y || o.fill == Fill::BOTH) {
                    w->geom.y = 10 + o.pady;
                    w->geom.h = (height_ - 20) - 2*o.pady;
                } else {
                    if (w->geom.h == 0) w->geom.h = 24;
                    w->geom.y = 10 + o.pady;
                }
                w->geom.x = cur_left + o.padx;
                cur_left += w->geom.w + o.padx + 8;
            }
        }
    }

    // WNDPROC glue
    static LRESULT CALLBACK s_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Window* p = nullptr;
        if (msg == WM_NCCREATE) {
            CREATESTRUCTA *cs = (CREATESTRUCTA*)lParam;
            p = (Window*)cs->lpCreateParams;
            SetWindowLongPtrA(hwnd, 0, (LONG_PTR)p);
            p->hwnd_ = hwnd;
        } else {
            p = (Window*)GetWindowLongPtrA(hwnd, 0);
        }
        if (p) return p->wndproc(hwnd, msg, wParam, lParam);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    // main instance wndproc with double-buffered painting
    LRESULT wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT rc;
                GetClientRect(hwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                if (w <= 0 || h <= 0) { EndPaint(hwnd, &ps); return 0; }

                // Create compatible memory DC + bitmap for double buffering
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBM = CreateCompatibleBitmap(hdc, w, h);
                HGDIOBJ oldBM = SelectObject(memDC, memBM);

                // Fill background once
                HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
                FillRect(memDC, &rc, bg);
                DeleteObject(bg);

                // Draw all widgets into memory DC
                for (auto &widget : widgets_) {
                    if (!widget->visible) continue;
                    widget->draw(memDC);
                    widget->dirty = false;
                }

                // Blit memory DC to screen
                BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

                // cleanup
                SelectObject(memDC, oldBM);
                DeleteObject(memBM);
                DeleteDC(memDC);

                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                // dispatch in reverse order (topmost first)
                for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
                    auto &w = *it;
                    if (!w->visible) continue;
                    if (x >= w->geom.x && x < w->geom.x + w->geom.w &&
                        y >= w->geom.y && y < w->geom.y + w->geom.h) {
                        w->on_click_internal(x - w->geom.x, y - w->geom.y);
                        // if Entry, manage focus
                        if (auto e = std::dynamic_pointer_cast<Entry>(w)) {
                            if (focused_entry_ && focused_entry_.get() != e.get()) {
                                focused_entry_->focused = false;
                                focused_entry_->mark_dirty();
                            }
                            focused_entry_ = e;
                            e->focused = true;
                        } else {
                            if (focused_entry_) { focused_entry_->focused = false; focused_entry_.reset(); }
                        }
                        InvalidateRect(hwnd, NULL, TRUE);
                        break;
                    }
                }
                return 0;
            }
            case WM_CHAR: {
                if (focused_entry_) {
                    char ch = (char)wParam;
                    focused_entry_->on_key_internal(ch);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                return 0;
            }
            case WM_KEYDOWN: {
                if (focused_entry_) {
                    int vk = (int)wParam;
                    if (vk == VK_LEFT) {
                        if (focused_entry_->caret > 0) focused_entry_->caret--;
                        focused_entry_->mark_dirty();
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else if (vk == VK_RIGHT) {
                        if (focused_entry_->caret < focused_entry_->text.size()) focused_entry_->caret++;
                        focused_entry_->mark_dirty();
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
                return 0;
            }
            case WM_SIZE:
                width_ = LOWORD(lParam);
                height_ = HIWORD(lParam);
                recompute_layout();
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
};

// Convenience creation functions (match prior API)
inline std::shared_ptr<Label> make_label(const std::string &txt) { return std::make_shared<Label>(txt); }
inline std::shared_ptr<Entry> make_entry(const std::string &txt) { return std::make_shared<Entry>(txt); }
inline std::shared_ptr<Button> make_button(const std::string &txt) { return std::make_shared<Button>(txt); }
inline std::shared_ptr<Canvas> make_canvas(int w, int h) { return std::make_shared<Canvas>(w,h); }
inline std::shared_ptr<Frame> make_frame(){ return std::make_shared<Frame>(); }

} // namespace SoftGUI

#endif // SOFTGUI_WIN_HPP
