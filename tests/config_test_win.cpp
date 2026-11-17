#include "../src/app.h"
#include "../src/util.h"

#include <cstdio>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

static void WriteIni(const char* text) { WriteWholeFile(ConfigPath(), text); }

int main() {
    ::DeleteFileW(ConfigPath().c_str());

    const Config fresh = LoadConfig();
    Check(fresh.max_width_px == 1600, "default width");
    Check(fresh.timeout_ms == 60000, "default timeout");
    Check(fresh.max_response_bytes == 8388608, "default 8 MB cap");
    Check(fresh.history_limit == 20, "default history 20");
    Check(!fresh.minimize_to_tray, "tray minimise off by default");
    Check(fresh.reuse_crop, "reuse_crop on by default");

    WriteIni("[study]\nmax_width_px=10\ntimeout_ms=100\nmax_response_bytes=1\n"
             "mode=nonsense\nhistory_limit=9999\noffline_grace_hours=99999\n");
    const Config clamped = LoadConfig();
    Check(clamped.max_width_px == 320, "width floored");
    Check(clamped.timeout_ms == 5000, "timeout floored");
    Check(clamped.max_response_bytes == 65536, "cap floored");
    Check(clamped.mode == L"openai", "bad mode falls back");
    Check(clamped.history_limit == 200, "history capped");
    Check(clamped.offline_grace_hours == 720, "grace capped");

    WriteIni("not an ini\n===\n");
    Check(LoadConfig().max_width_px == 1600, "garbage yields defaults");

    WriteIni("[study]\nmax_width_px=0\n");
    Check(LoadConfig().max_width_px == 0, "0 disables the limit");

    WriteIni("[study]\ncrop_rect=10,20,300,400\nreuse_crop=0\n");
    RECT rect{};
    const Config cropped = LoadConfig();
    Check(ParseRect(cropped.crop_rect, rect) && rect.right == 300, "rect round trips");
    Check(!cropped.reuse_crop, "reuse_crop=0 read back");

    WriteIni("[study]\ncrop_rect=10,20\n");
    Check(!ParseRect(LoadConfig().crop_rect, rect), "short rect rejected");

    WriteIni("[study]\nlanguage=zz\nproxy_port=99999\n");
    Check(LoadConfig().language == L"zz", "unknown language preserved");
    Check(LoadConfig().proxy_port == 99999, "proxy port read");

    ::DeleteFileW(ConfigPath().c_str());

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
