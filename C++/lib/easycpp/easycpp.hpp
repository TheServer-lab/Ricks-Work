// ============================================================================
// easycpp.hpp — Unified C++ Development Header
// Combines SoftGUI + ImgRnd + Alias utility system
// Author: S.D. | Purpose: Make C++ GUI and logic development easier
// ============================================================================

#pragma once
#ifndef EASYC_HPP
#define EASYC_HPP

// ---------------------------------------------------------------------------
// STANDARD + PLATFORM HEADERS
// ---------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <random>
#include <cassert>

#ifdef _WIN32
    #include <windows.h>
    #include <commdlg.h>
    #include <gdiplus.h>
    #pragma comment (lib, "gdiplus.lib")
#endif

// ---------------------------------------------------------------------------
// INCLUDE CORE MODULES
// ---------------------------------------------------------------------------
#include "alias.hpp"       // shorthand macros, helpers, and type aliases
#include "softgui_win.hpp" // complete GUI framework
#include "img_rnd.hpp"     // GDI+ image renderer

// ---------------------------------------------------------------------------
// USING DECLARATIONS
// ---------------------------------------------------------------------------
using namespace std;
using namespace SoftGUI;
using namespace ImgRnd;

// ---------------------------------------------------------------------------
// EASYCPP NAMESPACE — simple utilities and wrappers
// ---------------------------------------------------------------------------
namespace EasyCPP {

    // ---------- Logging ----------
    inline void print(const string& s) { cout << s << '\n'; }
    inline void log(const string& s)   { cout << "[Log] " << s << '\n'; }

    // ---------- Sleep ----------
    inline void sleep_ms(int ms) { this_thread::sleep_for(chrono::milliseconds(ms)); }
    inline void sleep_s(int s)   { this_thread::sleep_for(chrono::seconds(s)); }

    // ---------- Random ----------
    inline int randint(int min, int max) {
        static mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());
        uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    inline double randf(double min, double max) {
        static mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());
        uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }

    // ---------- File Helpers ----------
    inline bool file_exists(const string& path) {
        return filesystem::exists(path);
    }

    inline string read_file(const string& path) {
        ifstream f(path);
        if (!f.is_open()) return "";
        stringstream buf; buf << f.rdbuf();
        return buf.str();
    }

    inline void write_file(const string& path, const string& data) {
        ofstream f(path);
        f << data;
    }

    // ---------- Math Helpers ----------
    template<typename T>
    inline T clamp(T v, T lo, T hi) {
        return max(lo, min(v, hi));
    }

    inline double lerp(double a, double b, double t) {
        return a + (b - a) * t;
    }

    // ---------- Time Helpers ----------
    inline uint64_t timestamp_ms() {
        return chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    inline string current_time() {
        time_t t = time(nullptr);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
        return buf;
    }

    // ---------- Auto GDI+ Initialization ----------
    class GDIPlusAuto {
    public:
        GDIPlusAuto() {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&token, &input, nullptr);
        }
        ~GDIPlusAuto() {
            Gdiplus::GdiplusShutdown(token);
        }
    private:
        ULONG_PTR token;
    };

    inline GDIPlusAuto gdi_auto; // ensures auto startup/shutdown

} // namespace EasyCPP

#endif // EASYC_HPP
