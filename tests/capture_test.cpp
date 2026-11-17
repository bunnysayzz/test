#include "../src/app.h"

#include <cstdio>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

int main() {
    CaptureSpec desktop;
    desktop.mode = CaptureMode::Desktop;
    desktop.max_width_px = 0;
    const CaptureResult full = CaptureScreenPng(desktop);
    Check(full.ok && full.png.size() > 100, "desktop capture returns bytes");
    Check(full.png.size() > 8 && full.png[0] == 0x89 && full.png[1] == 'P',
          "output is a PNG");

    const RECT screen = VirtualScreenRect();
    Check(full.width == screen.right - screen.left, "width matches the desktop");

    CaptureSpec scaled = desktop;
    scaled.max_width_px = 640;
    const CaptureResult small = CaptureScreenPng(scaled);
    Check(small.ok && small.width == 640, "width limit applied");
    Check(small.height < full.height, "height scales proportionally");

    CaptureSpec capped = desktop;
    capped.max_height_px = 480;
    const CaptureResult short_side = CaptureScreenPng(capped);
    Check(short_side.ok && short_side.height == 480, "height limit applied");

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
