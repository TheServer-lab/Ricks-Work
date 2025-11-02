// softgui_win.hpp — SoftGUI expanded ANSI header-only implementation (full Core Widgets, smooth)
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
#include <cmath>

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

// ---------- Pack options ----------
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
    Widget* parent = nullptr;            // container parent (Frame*) or nullptr when top-level
    std::string font_name = "Segoe UI";
    int font_size = 12;
    std::function<void(Widget*)> on_click;
    std::function<void(Widget*, char)> on_key;
    std::function<void(Widget*)> on_focus;
    std::function<void(Widget*)> on_change;
    bool packed = false;
    PackOptions pack_opts;

    virtual ~Widget() {}
    virtual void measure() {}
    virtual void draw(HDC hdc) {}
    virtual void on_click_internal(int x,int y) { if (on_click) on_click(this); }
    virtual void on_key_internal(char ch) { if (on_key) on_key(this, ch); }
    virtual void mark_dirty() { dirty = true; if (parent) parent->mark_dirty(); }

    HFONT make_font(HDC hdc) {
        return CreateFontA(
            -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font_name.c_str());
    }
};

// ---------- Frame ----------
struct Frame : Widget {
    std::vector<WidgetPtr> children;
    Frame() {}
    void add_child(WidgetPtr w) { w->parent = this; children.push_back(w); }
    void draw(HDC hdc) override {
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
        HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(hdc, &r, bg); DeleteObject(bg);

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
        HBRUSH hbr = CreateSolidBrush(RGB(255,255,255));
        FillRect(hdc, &r, hbr); DeleteObject(hbr);

        // border
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);

        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        SetBkMode(hdc, TRANSPARENT);

        RECT tr = r;
        tr.left += 4;
        DrawTextA(hdc, text.c_str(), (int)text.size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        // caret blinking (timer-driven redraw will animate caret)
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
                caret = (caret==0?0:caret-1);
                if (on_change) on_change(this);
            }
        } else if (ch == '\r') {
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
        HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        FillRect(hdc, &r, bg); DeleteObject(bg);

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

// ---------- Checkbox ----------
struct Checkbox : Widget {
    bool checked = false;
    Checkbox(const std::string &txt="") { text = txt; }
    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        // checkbox square
        Rectangle(hdc, r.left, r.top, r.left + 16, r.top + 16);
        if (checked) {
            MoveToEx(hdc, r.left+3, r.top+8, nullptr);
            LineTo(hdc, r.left+7, r.top+12);
            LineTo(hdc, r.left+13, r.top+4);
        }
        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        SetBkMode(hdc, TRANSPARENT);
        RECT tr = r; tr.left += 20;
        DrawTextA(hdc, text.c_str(), (int)text.size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, hOld);
    }
    void on_click_internal(int x,int y) override {
        checked = !checked;
        if (on_change) on_change(this);
        mark_dirty();
    }
};

// ---------- RadioButton ----------
struct RadioButton : Widget {
    bool selected = false;
    int group_id = 0;
    RadioButton(const std::string &txt="", int gid=0) : group_id(gid) { text = txt; }
    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        Ellipse(hdc, r.left, r.top, r.left + 16, r.top + 16);
        if (selected) {
            Ellipse(hdc, r.left+4, r.top+4, r.left+12, r.top+12);
        }
        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        SetBkMode(hdc, TRANSPARENT);
        RECT tr = r; tr.left += 20;
        DrawTextA(hdc, text.c_str(), (int)text.size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, hOld);
    }
    void on_click_internal(int x,int y) override {
        // if inside a Frame, clear group siblings with same group_id
        if (parent) {
            if (Frame* f = dynamic_cast<Frame*>(parent)) {
                for (auto &wptr : f->children) {
                    if (auto rb = std::dynamic_pointer_cast<RadioButton>(wptr)) {
                        if (rb->group_id == group_id) { rb->selected = false; rb->mark_dirty(); }
                    }
                }
            }
        }
        selected = true;
        if (on_change) on_change(this);
        mark_dirty();
    }
};

// ---------- Horizontal Slider (smoothed) ----------
struct HSlider : Widget {
    int min=0, max=100;
    int value=50;                // exposed integer value (rounded)
    float fvalue = 50.0f;        // internal float used for smoothing
    float target = 50.0f;        // animated target
    bool dragging = false;
    HSlider(int mn=0,int mx=100,int val=50) : min(mn), max(mx), value(val), fvalue((float)val), target((float)val) {}
    void draw(HDC hdc) override {
        // draw track
        RECT r{geom.x, geom.y + geom.h/2 - 4, geom.x + geom.w, geom.y + geom.h/2 + 4};
        HBRUSH bg = CreateSolidBrush(RGB(200,200,200));
        FillRect(hdc, &r, bg); DeleteObject(bg);

        int range = std::max(1, max - min);
        int pos = geom.x + (int)((fvalue - min) * (geom.w - 16) / (float)range);
        // draw thumb
        RoundRect(hdc, pos, geom.y + 2, pos + 16, geom.y + geom.h - 2, 4, 4);
    }
    void on_click_internal(int x,int y) override {
        // set immediate target from click and start dragging
        int newval = min + x * (max - min) / std::max(1, geom.w - 16);
        newval = std::clamp(newval, min, max);
        target = (float)newval;
        dragging = true;
        // don't call on_change here; Window's timer will call on_change when integer changes
        mark_dirty();
    }
};

// ---------- Vertical Slider (smoothed) ----------
struct VSlider : Widget {
    int min=0, max=100;
    int value=50;
    float fvalue = 50.0f;
    float target = 50.0f;
    bool dragging = false;
    VSlider(int mn=0,int mx=100,int val=50) : min(mn), max(mx), value(val), fvalue((float)val), target((float)val) {}
    void draw(HDC hdc) override {
        RECT bar{geom.x + geom.w/2 - 4, geom.y, geom.x + geom.w/2 + 4, geom.y + geom.h};
        HBRUSH bg = CreateSolidBrush(RGB(200,200,200));
        FillRect(hdc, &bar, bg); DeleteObject(bg);
        int range = std::max(1, max - min);
        int pos = geom.y + (geom.h - 16) - (int)((fvalue - min) * (geom.h - 16) / (float)range);
        RoundRect(hdc, geom.x + 2, pos, geom.x + geom.w - 2, pos + 16, 4, 4);
    }
    void on_click_internal(int x,int y) override {
        int rel = y;
        int newval = min + (geom.h - rel - 16) * (max - min) / std::max(1, geom.h - 16);
        newval = std::clamp(newval, min, max);
        target = (float)newval;
        dragging = true;
        mark_dirty();
    }
};

// ---------- ListBox (single-select) ----------
struct ListBox : Widget {
    std::vector<std::string> items;
    int selected = -1;
    int item_height = 20;
    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        HBRUSH bg = CreateSolidBrush(RGB(255,255,255));
        FillRect(hdc, &r, bg); DeleteObject(bg);
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        int yoff = r.top;
        for (size_t i=0;i<items.size();++i) {
            RECT tr = { r.left + 2, yoff, r.right, yoff + item_height };
            if ((int)i == selected) {
                HBRUSH sel = CreateSolidBrush(RGB(180,200,240));
                FillRect(hdc, &tr, sel);
                DeleteObject(sel);
            }
            DrawTextA(hdc, items[i].c_str(), (int)items[i].size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            yoff += item_height;
            if (yoff > r.bottom) break;
        }
        SelectObject(hdc, hOld);
    }
    void on_click_internal(int x,int y) override {
        int idx = y / item_height;
        if (idx >= 0 && idx < (int)items.size()) {
            selected = idx;
            if (on_change) on_change(this);
            mark_dirty();
        }
    }
};

// ---------- Multi-select ListBox ----------
struct MultiListBox : Widget {
    std::vector<std::string> items;
    std::vector<int> selected_indices; // multi-selection
    int item_height = 20;
    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        HBRUSH bg = CreateSolidBrush(RGB(255,255,255));
        FillRect(hdc, &r, bg); DeleteObject(bg);
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        int yoff = r.top;
        for (size_t i=0;i<items.size();++i) {
            RECT tr = { r.left + 2, yoff, r.right, yoff + item_height };
            if (std::find(selected_indices.begin(), selected_indices.end(), (int)i) != selected_indices.end()) {
                HBRUSH sel = CreateSolidBrush(RGB(180,200,240));
                FillRect(hdc, &tr, sel);
                DeleteObject(sel);
            }
            DrawTextA(hdc, items[i].c_str(), (int)items[i].size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            yoff += item_height;
            if (yoff > r.bottom) break;
        }
        SelectObject(hdc, hOld);
    }
    void on_click_internal(int x,int y) override {
        int idx = y / item_height;
        if (idx >= 0 && idx < (int)items.size()) {
            auto it = std::find(selected_indices.begin(), selected_indices.end(), idx);
            if (it != selected_indices.end()) selected_indices.erase(it);
            else selected_indices.push_back(idx);
            if (on_change) on_change(this);
            mark_dirty();
        }
    }
};

// ---------- ComboBox (basic dropdown) ----------
struct ComboBox : Widget {
    std::vector<std::string> options;
    int selected = -1;
    bool expanded = false;
    int option_height = 20;

    void draw(HDC hdc) override {
        RECT r{geom.x, geom.y, geom.x + geom.w, geom.y + geom.h};
        HBRUSH bg = CreateSolidBrush(RGB(255,255,255));
        FillRect(hdc, &r, bg); DeleteObject(bg);
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);

        HFONT hOld = (HFONT)SelectObject(hdc, make_font(hdc));
        if (selected >= 0 && selected < (int)options.size()) {
            RECT tr = r; tr.left += 4;
            DrawTextA(hdc, options[selected].c_str(), (int)options[selected].size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        }
        SelectObject(hdc, hOld);

        // arrow
        POINT pts[3] = { {r.right-14, r.top + (r.bottom - r.top)/2 - 4}, {r.right-6, r.top + (r.bottom - r.top)/2 - 4}, {r.right-10, r.top + (r.bottom - r.top)/2 + 2} };
        Polygon(hdc, pts, 3);

        if (expanded) {
            RECT ext = {r.left, r.bottom, r.right, r.bottom + (int)options.size()*option_height};
            HBRUSH bg2 = CreateSolidBrush(RGB(240,240,240));
            FillRect(hdc, &ext, bg2); DeleteObject(bg2);
            HFONT hOld2 = (HFONT)SelectObject(hdc, make_font(hdc));
            int yoff = r.bottom;
            for (size_t i=0;i<options.size();++i) {
                RECT tr = { r.left + 4, yoff, r.right, yoff + option_height };
                DrawTextA(hdc, options[i].c_str(), (int)options[i].size(), &tr, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
                yoff += option_height;
            }
            SelectObject(hdc, hOld2);
        }
    }

    void on_click_internal(int x,int y) override {
        if (!expanded) {
            expanded = true;
        } else {
            int idx = (y - geom.h) / option_height;
            if (idx >= 0 && idx < (int)options.size()) selected = idx;
            expanded = false;
            if (on_change) on_change(this);
        }
        mark_dirty();
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
        // start periodic timer for smooth redraw and animations (~60Hz)
        if (hwnd_) {
            SetTimer(hwnd_, kRefreshTimerId, kRefreshPeriodMs, NULL);
        }
    }
    ~Window() {
        if (hwnd_) {
            KillTimer(hwnd_, kRefreshTimerId);
            DestroyWindow(hwnd_);
        }
        UnregisterClassA(wc_.lpszClassName, wc_.hInstance);
    }

    // Factories
    std::shared_ptr<Label> make_label(const std::string &txt){ auto p = std::make_shared<Label>(txt); register_widget(p); return p; }
    std::shared_ptr<Entry> make_entry(const std::string &txt){ auto p = std::make_shared<Entry>(txt); register_widget(p); return p; }
    std::shared_ptr<Button> make_button(const std::string &txt){ auto p = std::make_shared<Button>(txt); register_widget(p); return p; }
    std::shared_ptr<Canvas> make_canvas(int w,int h){ auto p = std::make_shared<Canvas>(w,h); register_widget(p); return p; }
    std::shared_ptr<Frame> make_frame(){ auto p = std::make_shared<Frame>(); register_widget(p); return p; }
    std::shared_ptr<Checkbox> make_checkbox(const std::string &txt=""){ auto p = std::make_shared<Checkbox>(txt); register_widget(p); return p; }
    std::shared_ptr<RadioButton> make_radiobutton(const std::string &txt="", int gid=0){ auto p = std::make_shared<RadioButton>(txt,gid); register_widget(p); return p; }
    std::shared_ptr<HSlider> make_hslider(int mn=0,int mx=100,int val=50){ auto p = std::make_shared<HSlider>(mn,mx,val); register_widget(p); return p; }
    std::shared_ptr<VSlider> make_vslider(int mn=0,int mx=100,int val=50){ auto p = std::make_shared<VSlider>(mn,mx,val); register_widget(p); return p; }
    std::shared_ptr<ListBox> make_listbox(){ auto p = std::make_shared<ListBox>(); register_widget(p); return p; }
    std::shared_ptr<MultiListBox> make_multilistbox(){ auto p = std::make_shared<MultiListBox>(); register_widget(p); return p; }
    std::shared_ptr<ComboBox> make_combobox(){ auto p = std::make_shared<ComboBox>(); register_widget(p); return p; }

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
        InvalidateRect(hwnd_, NULL, FALSE);
    }
    void place(WidgetPtr w, int x, int y) {
        if (!w) return;
        w->geom.x = x; w->geom.y = y;
        w->parent = nullptr;
        w->packed = false;
        InvalidateRect(hwnd_, NULL, FALSE);
    }

    void measure() {
        // placeholder
    }

    void arrange() { recompute_layout(); }

    void mainloop(int fps = 60) {
        MSG msg;
        while (GetMessageA(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    HWND hwnd() const { return hwnd_; }

    void set_title(const char* t) { title_ = t; SetWindowTextA(hwnd_, t); }
    void resize(int w,int h) { width_=w; height_=h; SetWindowPos(hwnd_, NULL,0,0,w,h, SWP_NOMOVE|SWP_NOZORDER); recompute_layout(); InvalidateRect(hwnd_, NULL, FALSE); }

private:
    static constexpr UINT_PTR kRefreshTimerId = 0x1001;
    static constexpr int kRefreshPeriodMs = 16; // ~60Hz

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

    // capture dragging widget (raw pointer into a widget owned in widgets_)
    Widget* capture_widget_ = nullptr;

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

    // helper: update animations (sliders) each tick
    void tick_animate() {
        bool need_invalidate = false;
        const float smoothing = 0.20f; // interpolation factor (0..1) larger -> faster
        for (auto &wptr : widgets_) {
            if (!wptr) continue;
            // HSlider
            if (auto hs = dynamic_cast<HSlider*>(wptr.get())) {
                float prev = hs->fvalue;
                float diff = hs->target - hs->fvalue;
                if (std::fabs(diff) > 0.01f) {
                    hs->fvalue += diff * smoothing;
                    // clamp
                    if ((hs->target - hs->fvalue) * diff < 0) hs->fvalue = hs->target;
                    int newv = (int)std::lround(hs->fvalue);
                    if (newv != hs->value) {
                        hs->value = newv;
                        if (hs->on_change) hs->on_change(hs);
                    }
                    hs->mark_dirty();
                    need_invalidate = true;
                }
            }
            // VSlider
            if (auto vs = dynamic_cast<VSlider*>(wptr.get())) {
                float prev = vs->fvalue;
                float diff = vs->target - vs->fvalue;
                if (std::fabs(diff) > 0.01f) {
                    vs->fvalue += diff * smoothing;
                    if ((vs->target - vs->fvalue) * diff < 0) vs->fvalue = vs->target;
                    int newv = (int)std::lround(vs->fvalue);
                    if (newv != vs->value) {
                        vs->value = newv;
                        if (vs->on_change) vs->on_change(vs);
                    }
                    vs->mark_dirty();
                    need_invalidate = true;
                }
            }
            // caret blink & other widget-level periodic updates -> mark dirty to keep blink working
            if (auto en = dynamic_cast<Entry*>(wptr.get())) {
                // rely on timer-driven blink in Entry::draw (it checks time) but we must invalidate to show it
                need_invalidate = true;
            }
        }

        if (need_invalidate && hwnd_) {
            InvalidateRect(hwnd_, NULL, FALSE);
        }
    }

    // main instance wndproc with double-buffered painting, mouse capture, and timer-driven animation
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
                        // widget-local coords
                        int lx = x - w->geom.x;
                        int ly = y - w->geom.y;
                        w->on_click_internal(lx, ly);

                        // begin capture for dragging (sliders handled while captured)
                        capture_widget_ = w.get();
                        SetCapture(hwnd);

                        // if the clicked widget is an Entry -> focus management
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

                        InvalidateRect(hwnd, NULL, FALSE);
                        break;
                    }
                }
                return 0;
            }

            case WM_MOUSEMOVE: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                if (capture_widget_) {
                    Widget* w = capture_widget_;
                    // compute local coords
                    int lx = x - w->geom.x;
                    int ly = y - w->geom.y;
                    // HSlider dragging
                    if (auto hs = dynamic_cast<HSlider*>(w)) {
                        int newval = hs->min + lx * (hs->max - hs->min) / std::max(1, w->geom.w - 16);
                        newval = std::clamp(newval, hs->min, hs->max);
                        hs->target = (float)newval;
                        hs->dragging = true;
                        // mark and don't call on_change here; animation will call on_change when int actually changes
                        hs->mark_dirty();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    // VSlider dragging
                    else if (auto vs = dynamic_cast<VSlider*>(w)) {
                        int rel = ly;
                        int newval = vs->min + (w->geom.h - rel - 16) * (vs->max - vs->min) / std::max(1, w->geom.h - 16);
                        newval = std::clamp(newval, vs->min, vs->max);
                        vs->target = (float)newval;
                        vs->dragging = true;
                        vs->mark_dirty();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    // ComboBox: if expanded and captured, map to option selection quickly
                    else if (auto cb = dynamic_cast<ComboBox*>(w)) {
                        if (cb->expanded) {
                            int idx = (y - w->geom.y - w->geom.h) / cb->option_height;
                            // show hover selection visually by temporarily setting selected (but do not commit until mouse up)
                            if (idx >= 0 && idx < (int)cb->options.size()) {
                                // temporary visual selection (no persistent state change yet)
                                cb->selected = idx;
                                cb->mark_dirty();
                                InvalidateRect(hwnd, NULL, FALSE);
                            }
                        }
                    }
                }
                return 0;
            }

            case WM_LBUTTONUP: {
                // release capture and end dragging
                if (capture_widget_) {
                    Widget* w = capture_widget_;
                    if (auto hs = dynamic_cast<HSlider*>(w)) {
                        hs->dragging = false;
                        // ensure final integer value is set to target immediately
                        hs->fvalue = hs->target;
                        int newv = (int)std::lround(hs->fvalue);
                        if (newv != hs->value) {
                            hs->value = newv;
                            if (hs->on_change) hs->on_change(hs);
                        }
                    } else if (auto vs = dynamic_cast<VSlider*>(w)) {
                        vs->dragging = false;
                        vs->fvalue = vs->target;
                        int newv = (int)std::lround(vs->fvalue);
                        if (newv != vs->value) {
                            vs->value = newv;
                            if (vs->on_change) vs->on_change(vs);
                        }
                    } else if (auto cb = dynamic_cast<ComboBox*>(w)) {
                        // ComboBox commit selection on mouse up (if expanded)
                        if (cb->expanded) {
                            int idx = (GET_Y_LPARAM(lParam) - w->geom.y - w->geom.h) / cb->option_height;
                            if (idx >= 0 && idx < (int)cb->options.size()) {
                                cb->selected = idx;
                                if (cb->on_change) cb->on_change(cb);
                            }
                            cb->expanded = false;
                        }
                    }
                    ReleaseCapture();
                    capture_widget_ = nullptr;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            case WM_TIMER: {
                if (wParam == kRefreshTimerId) {
                    // animate sliders and other widgets
                    tick_animate();
                }
                return 0;
            }

            case WM_CHAR: {
                if (focused_entry_) {
                    char ch = (char)wParam;
                    focused_entry_->on_key_internal(ch);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            case WM_KEYDOWN: {
                if (focused_entry_) {
                    int vk = (int)wParam;
                    if (vk == VK_LEFT) {
                        if (focused_entry_->caret > 0) focused_entry_->caret--;
                        focused_entry_->mark_dirty();
                        InvalidateRect(hwnd, NULL, FALSE);
                    } else if (vk == VK_RIGHT) {
                        if (focused_entry_->caret < focused_entry_->text.size()) focused_entry_->caret++;
                        focused_entry_->mark_dirty();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
                return 0;
            }

            case WM_SIZE:
                width_ = LOWORD(lParam);
                height_ = HIWORD(lParam);
                recompute_layout();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;

            case WM_DESTROY:
                KillTimer(hwnd_, kRefreshTimerId);
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
};

} // namespace SoftGUI

#endif // SOFTGUI_WIN_HPP
