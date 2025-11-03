// img_rnd.hpp — Simple image renderer for SoftGUI
// Requires linking with: -lgdiplus

#ifndef IMG_RND_HPP
#define IMG_RND_HPP

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <memory>

#pragma comment (lib, "gdiplus.lib")

namespace ImgRnd {

using namespace Gdiplus;

class GDIPlusInit {
public:
    GDIPlusInit() {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&token, &gdiplusStartupInput, nullptr);
    }
    ~GDIPlusInit() {
        GdiplusShutdown(token);
    }
private:
    ULONG_PTR token;
};

// Singleton instance — initialized once
inline GDIPlusInit gdiInit;

// -------------------- Image class --------------------
class ImageRenderer {
private:
    std::unique_ptr<Gdiplus::Image> image;
    std::wstring path;
    bool valid = false;
    int width = 0, height = 0;

public:
    ImageRenderer() = default;

    bool Load(const std::wstring& filePath) {
        path = filePath;
        image.reset(new Gdiplus::Image(filePath.c_str()));
        if (image && image->GetLastStatus() == Gdiplus::Ok) {
            width = image->GetWidth();
            height = image->GetHeight();
            valid = true;
        } else {
            valid = false;
        }
        return valid;
    }

    bool IsValid() const { return valid; }

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    void Draw(HDC hdc, int x, int y, int w = -1, int h = -1, bool keepAspect = true) {
        if (!valid) return;

        Graphics g(hdc);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(SmoothingModeHighQuality);

        if (w <= 0) w = width;
        if (h <= 0) h = height;

        if (keepAspect) {
            float ratio = std::min((float)w / width, (float)h / height);
            int nw = (int)(width * ratio);
            int nh = (int)(height * ratio);
            int cx = x + (w - nw) / 2;
            int cy = y + (h - nh) / 2;
            g.DrawImage(image.get(), cx, cy, nw, nh);
        } else {
            g.DrawImage(image.get(), x, y, w, h);
        }
    }
};

// -------------------- Utility Function --------------------
inline void DrawCenteredImage(HDC hdc, RECT rc, ImageRenderer& img) {
    if (!img.IsValid()) return;

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int imgW = img.GetWidth();
    int imgH = img.GetHeight();

    float scale = std::min((float)w / imgW, (float)h / imgH);
    int newW = (int)(imgW * scale);
    int newH = (int)(imgH * scale);
    int x = rc.left + (w - newW) / 2;
    int y = rc.top + (h - newH) / 2;

    img.Draw(hdc, x, y, newW, newH, false);
}

} // namespace ImgRnd

#endif
