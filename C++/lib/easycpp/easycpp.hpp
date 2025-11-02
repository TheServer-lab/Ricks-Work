// easycpp.hpp — unified SoftGUI + Alisa high-level wrapper
// C++ made easy with GUI + scripting + helper utilities
#pragma once
#ifndef EASYC_HPP
#define EASYC_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <thread>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <cmath>
#include "softgui_win.hpp"
#include "alias.hpp"

namespace easy {

// ===========================================================
// 🔹 BASIC UTILITIES
// ===========================================================
inline void print(const std::string &s) { std::cout << s << std::endl; }

inline std::string input(const std::string &prompt = "") {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

inline void sleep(double seconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(seconds * 1000)));
}

inline std::string read(const std::string &path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline bool write(const std::string &path, const std::string &content) {
    std::ofstream f(path, std::ios::out | std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}

inline bool exists(const std::string &path) {
    return std::filesystem::exists(path);
}

inline std::string join(const std::string &a, const std::string &b) {
    return (std::filesystem::path(a) / std::filesystem::path(b)).string();
}

// ===========================================================
// 🔹 JSON-like MAP
// ===========================================================
struct Map {
    std::map<std::string, std::string> data;
    std::string &operator[](const std::string &k) { return data[k]; }
    void set(const std::string &k, const std::string &v) { data[k] = v; }

    std::string get(const std::string &k, const std::string &def = "") const {
        auto it = data.find(k);
        return (it != data.end()) ? it->second : def;
    }

    std::string dump() const {
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (auto &[k, v] : data) {
            if (!first) ss << ", ";
            ss << "\"" << k << "\": \"" << v << "\"";
            first = false;
        }
        ss << "}";
        return ss.str();
    }

    std::string to_string() const { return dump(); }
};
using dict = Map;

// ===========================================================
// 🔹 TIMING HELPERS
// ===========================================================
inline double now() {
    using namespace std::chrono;
    auto tp = high_resolution_clock::now();
    auto dur = tp.time_since_epoch();
    return duration_cast<milliseconds>(dur).count() / 1000.0;
}

inline long long now_ms() {
    using namespace std::chrono;
    auto tp = high_resolution_clock::now();
    auto dur = tp.time_since_epoch();
    return duration_cast<milliseconds>(dur).count();
}

// ===========================================================
// 🔹 GUI WRAPPERS (SoftGUI simplified)
// ===========================================================
class App {
public:
    SoftGUI::Window win;
    SoftGUI::Window &window;

    App(const std::string &title = "EasyCPP App", int w = 640, int h = 480)
        : win(w, h, title.c_str()), window(win)
    {
        // Get HWND handle from SoftGUI getter
        HWND hwnd = win.hwnd();

        // Show and update properly
        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);

        // Force initial paint after small delay
        std::thread([hwnd]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL,
                         RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        }).detach();
    }

    int run() {
        win.mainloop();
        return 0;  // fixed: mainloop() returns void, so return success
    }

    template <typename F>
    void after(double delay_sec, F func) {
        std::thread([delay_sec, func]() {
            sleep(delay_sec);
            func();
        }).detach();
    }
};



// -----------------------------------------------------------
// Button Wrapper
// -----------------------------------------------------------
class Button {
public:
    std::shared_ptr<SoftGUI::Button> btn;
    std::function<void()> callback;

    Button(App &app, const std::string &text, int x, int y, int w, int h, std::function<void()> onClick = nullptr) {
        btn = app.win.make_button(text);
        btn->geom = {x, y, w, h};
        if (onClick) btn->onclick0 = onClick;
        app.win.add_child(btn);
    }

    void onClick(std::function<void()> fn) { btn->onclick0 = fn; }
    std::string text() const { return btn->text; }
    void set(const std::string &t) { btn->text = t; btn->mark_dirty(); }
};

// -----------------------------------------------------------
// Label Wrapper
// -----------------------------------------------------------
class Label {
public:
    std::shared_ptr<SoftGUI::Label> lbl;
    Label(App &app, const std::string &text, int x, int y, int w, int h) {
        lbl = app.win.make_label(text);
        lbl->geom = {x, y, w, h};
        app.win.add_child(lbl);
    }

    void set(const std::string &text) { lbl->text = text; lbl->mark_dirty(); }
    void setText(const std::string &text) { set(text); }
    std::string text() const { return lbl->text; }

    void setFont(const std::string &font, int size = 16) {
        lbl->font_name = font;
        lbl->font_size = size;
        lbl->mark_dirty();
    }

    void alignRight() {  }
};

// -----------------------------------------------------------
// Entry Wrapper
// -----------------------------------------------------------
class Entry {
public:
    std::shared_ptr<SoftGUI::Entry> ent;
    Entry(App &app, const std::string &text, int x, int y, int w, int h) {
        ent = app.win.make_entry(text);
        ent->geom = {x, y, w, h};
        app.win.add_child(ent);
    }
    std::string get() { return ent->text; }
    void set(const std::string &text) { ent->text = text; ent->mark_dirty(); }
};

// ===========================================================
// 🔹 MATH / SCRIPT-LIKE EVALUATION
// ===========================================================
// Simple arithmetic expression evaluator (supports + - * / and parentheses)
inline double eval(const std::string &expr) {
    std::string s;
    for (char c : expr)
        if (!isspace((unsigned char)c)) s += c;

    size_t i = 0;
    std::function<double()> parse_expr, parse_term, parse_factor;

    parse_factor = [&]() -> double {
        if (i < s.size() && s[i] == '(') {
            ++i;
            double v = parse_expr();
            if (i < s.size() && s[i] == ')') ++i;
            return v;
        }
        size_t j = i;
        while (j < s.size() && (isdigit(s[j]) || s[j] == '.')) j++;
        double val = std::stod(s.substr(i, j - i));
        i = j;
        return val;
    };

    parse_term = [&]() -> double {
        double v = parse_factor();
        while (i < s.size()) {
            if (s[i] == '*') { ++i; v *= parse_factor(); }
            else if (s[i] == '/') { ++i; v /= parse_factor(); }
            else break;
        }
        return v;
    };

    parse_expr = [&]() -> double {
        double v = parse_term();
        while (i < s.size()) {
            if (s[i] == '+') { ++i; v += parse_term(); }
            else if (s[i] == '-') { ++i; v -= parse_term(); }
            else break;
        }
        return v;
    };

    return parse_expr();
}

} // namespace easy
#endif // EASYC_HPP
