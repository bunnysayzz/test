#include "app.h"
#include "log.h"
#include "util.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <objbase.h>
#include <objidl.h>

#include <algorithm>
#include <memory>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

namespace {

struct GdiSession {
    ULONG_PTR token = 0;
    GdiSession() {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) token = 0;
    }
    ~GdiSession() {
        if (token) Gdiplus::GdiplusShutdown(token);
    }
};

GdiSession& Gdi() {
    static GdiSession session;
    return session;
}

bool FindPngEncoder(CLSID& clsid) {
    UINT count = 0, bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || !bytes)
        return false;
    std::vector<BYTE> storage(bytes);
    auto* codecs = (Gdiplus::ImageCodecInfo*)storage.data();
    if (Gdiplus::GetImageEncoders(count, bytes, codecs) != Gdiplus::Ok) return false;
    for (UINT i = 0; i < count; ++i) {
        if (codecs[i].MimeType && wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

struct ScreenDc {
    HDC dc = nullptr;
    ~ScreenDc() {
        if (dc) ::ReleaseDC(nullptr, dc);
    }
};

struct MemDc {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    ~MemDc() {
        if (dc && previous) ::SelectObject(dc, previous);
        if (bitmap) ::DeleteObject(bitmap);
        if (dc) ::DeleteDC(dc);
    }
};

RECT Clamp(const RECT& input) {
    const RECT screen = VirtualScreenRect();
    RECT out{};
    out.left = std::max(input.left, screen.left);
    out.top = std::max(input.top, screen.top);
    out.right = std::min(input.right, screen.right);
    out.bottom = std::min(input.bottom, screen.bottom);
    if (out.right <= out.left) out.right = out.left + 1;
    if (out.bottom <= out.top) out.bottom = out.top + 1;
    return out;
}

bool WindowBounds(HWND window, RECT& out) {
    if (!window || !::IsWindow(window)) return false;
    if (SUCCEEDED(::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &out,
                                          sizeof(out))))
        return out.right > out.left && out.bottom > out.top;
    if (!::GetWindowRect(window, &out)) return false;
    return out.right > out.left && out.bottom > out.top;
}

bool MonitorBounds(HWND reference, RECT& out) {
    POINT cursor{};
    ::GetCursorPos(&cursor);
    HMONITOR monitor = reference ? ::MonitorFromWindow(reference, MONITOR_DEFAULTTONEAREST)
                                 : ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    if (!monitor) return false;
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!::GetMonitorInfoW(monitor, &info)) return false;
    out = info.rcMonitor;
    return true;
}

HWND LastForeign() {
    HWND candidate = ::GetForegroundWindow();
    for (int i = 0; i < 32 && candidate; ++i) {
        wchar_t title[8] = {};
        if (::GetWindowTextW(candidate, title, 8) > 0 && ::IsWindowVisible(candidate) &&
            !(::GetWindowLongW(candidate, GWL_EXSTYLE) & WS_EX_TOOLWINDOW))
            return candidate;
        candidate = ::GetWindow(candidate, GW_HWNDNEXT);
    }
    return nullptr;
}

}  // namespace

RECT VirtualScreenRect() {
    RECT out{};
    out.left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    out.top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    out.right = out.left + ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    out.bottom = out.top + ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return out;
}

CaptureResult CaptureScreenPng(const CaptureSpec& spec) {
    CaptureResult result;
    RECT area{};

    switch (spec.mode) {
        case CaptureMode::Desktop:
            area = VirtualScreenRect();
            break;
        case CaptureMode::ActiveMonitor:
            if (!MonitorBounds(LastForeign(), area)) {
                result.error = "no monitor";
                return result;
            }
            break;
        case CaptureMode::ForegroundWindow:
        case CaptureMode::PickWindow:
            if (!WindowBounds(spec.window, area)) {
                result.error = "no window bounds";
                return result;
            }
            break;
        case CaptureMode::Region:
            area = spec.region;
            break;
    }

    area = Clamp(area);
    const int width = area.right - area.left;
    const int height = area.bottom - area.top;
    if (width <= 0 || height <= 0) {
        result.error = "empty area";
        return result;
    }

    Gdi();

    ScreenDc screen;
    screen.dc = ::GetDC(nullptr);
    if (!screen.dc) {
        result.error = "GetDC failed";
        return result;
    }

    MemDc mem;
    mem.dc = ::CreateCompatibleDC(screen.dc);
    if (!mem.dc) {
        result.error = "CreateCompatibleDC failed";
        return result;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    mem.bitmap = ::CreateDIBSection(mem.dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!mem.bitmap || !bits) {
        result.error = "CreateDIBSection failed";
        return result;
    }
    mem.previous = ::SelectObject(mem.dc, mem.bitmap);

    if (!::BitBlt(mem.dc, 0, 0, width, height, screen.dc, area.left, area.top,
                  SRCCOPY | CAPTUREBLT)) {
        result.error = "BitBlt failed";
        return result;
    }

    if (spec.include_cursor) {
        CURSORINFO cursor{};
        cursor.cbSize = sizeof(cursor);
        if (::GetCursorInfo(&cursor) && (cursor.flags & CURSOR_SHOWING) && cursor.hCursor)
            ::DrawIconEx(mem.dc, cursor.ptScreenPos.x - area.left,
                         cursor.ptScreenPos.y - area.top, cursor.hCursor, 0, 0, 0,
                         nullptr, DI_NORMAL);
    }

    auto* pixels = (uint8_t*)bits;
    const size_t count = (size_t)width * height;
    for (size_t i = 0; i < count; ++i) pixels[i * 4 + 3] = 0xFF;

    Gdiplus::Bitmap source(width, height, width * 4, PixelFormat32bppARGB, pixels);
    if (source.GetLastStatus() != Gdiplus::Ok) {
        result.error = "bitmap failed";
        return result;
    }

    std::unique_ptr<Gdiplus::Bitmap> scaled;
    Gdiplus::Bitmap* output = &source;
    int target_w = width, target_h = height;

    if (spec.max_width_px > 0 && target_w > spec.max_width_px) {
        target_h = (int)(((long long)target_h * spec.max_width_px + width / 2) / width);
        target_w = spec.max_width_px;
    }
    if (spec.max_height_px > 0 && target_h > spec.max_height_px) {
        target_w = (int)(((long long)target_w * spec.max_height_px + target_h / 2) / target_h);
        target_h = spec.max_height_px;
    }
    target_w = std::max(target_w, 1);
    target_h = std::max(target_h, 1);

    if (target_w != width || target_h != height) {
        scaled = std::make_unique<Gdiplus::Bitmap>(target_w, target_h,
                                                   PixelFormat32bppARGB);
        if (scaled->GetLastStatus() != Gdiplus::Ok) {
            result.error = "scale alloc failed";
            return result;
        }
        Gdiplus::Graphics graphics(scaled.get());
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        if (graphics.DrawImage(&source, 0, 0, target_w, target_h) != Gdiplus::Ok) {
            result.error = "downscale failed";
            return result;
        }
        output = scaled.get();
    }

    CLSID png{};
    if (!FindPngEncoder(png)) {
        result.error = "no PNG encoder";
        return result;
    }

    IStream* stream = nullptr;
    if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &stream)) || !stream) {
        result.error = "stream failed";
        return result;
    }
    if (output->Save(stream, &png, nullptr) != Gdiplus::Ok) {
        stream->Release();
        result.error = "PNG encode failed";
        return result;
    }

    STATSTG stat{};
    HGLOBAL handle = nullptr;
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) ||
        FAILED(::GetHGlobalFromStream(stream, &handle))) {
        stream->Release();
        result.error = "read failed";
        return result;
    }

    const size_t size = (size_t)stat.cbSize.QuadPart;
    void* data = size ? ::GlobalLock(handle) : nullptr;
    if (data) {
        result.png.assign((uint8_t*)data, (uint8_t*)data + size);
        ::GlobalUnlock(handle);
    }
    stream->Release();

    if (result.png.empty()) {
        result.error = "empty PNG";
        return result;
    }

    result.width = target_w;
    result.height = target_h;
    result.ok = true;
    return result;
}
