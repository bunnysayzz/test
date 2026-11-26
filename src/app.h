// app.h - shared types and module interfaces.
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "hotkey_shim.h"
#include "secrets.h"
#include "strings.h"

#define CROWMAN_VERSION L"1.2.0"
#define CROWMAN_APP_NAME L"Crowman Study Assistant"
#define CROWMAN_BATCH_MAX 5
#define CROWMAN_BATCH_DELAY_MS 1200

#define WM_JOB_CAPTURED (WM_APP + 1)
#define WM_JOB_DELTA (WM_APP + 2)
#define WM_APP_TRAY (WM_APP + 3)
#define WM_APP_RELOAD (WM_APP + 4)
#define WM_JOB_DONE (WM_APP + 5)
#define WM_APP_CROP (WM_APP + 6)
#define WM_APP_GHOST_PROGRESS (WM_APP + 7)
#define WM_APP_AUTOTYPE (WM_APP + 8)
// Status-line text posted by the parity extras (Dispatch / typing threads run
// off the UI thread, so SetStatus must not be called from them). The payload is
// an owned std::wstring*.
#define WM_APP_XSTATUS (WM_APP + 9)

enum class CaptureMode { Desktop = 0, ActiveMonitor = 1, ForegroundWindow = 2, PickWindow = 3, Region = 4 };

const wchar_t* CaptureModeName(CaptureMode mode);
bool CaptureModeFromName(const std::wstring& name, CaptureMode& out);

struct Config {
    std::wstring mode = L"openai";
    std::wstring endpoint;
    std::wstring model = L"gpt-4o-mini";
    std::wstring license_url;
    std::wstring proxy_host;
    int proxy_port = 0;
    std::wstring proxy_user;
    std::wstring ca_file;
    std::wstring cert_pin_sha256;
    int max_width_px = 1600;
    int max_height_px = 0;
    int timeout_ms = 60000;
    int max_retries = 2;
    int max_response_bytes = 8388608;
    int history_limit = 20;
    int offline_grace_hours = 72;
    bool stream = true;
    bool capture_cursor = false;
    bool hide_while_capturing = true;
    bool tray_icon = true;
    bool minimize_to_tray = false;
    bool start_hidden = false;
    bool reuse_crop = true;
    std::wstring crop_rect;
    CaptureMode capture_mode = CaptureMode::ActiveMonitor;
    HotkeySpec hotkey_capture{MOD_CONTROL | MOD_ALT, 'S', true};
    HotkeySpec hotkey_copy{MOD_CONTROL | MOD_ALT, 'C', true};
    HotkeySpec hotkey_toggle{MOD_CONTROL | MOD_ALT, 'H', true};
    std::wstring language = L"en";
};

std::wstring ConfigDir();
std::wstring ConfigPath();
std::wstring LicensePath();
std::wstring HistoryPath();
std::wstring LogDir();
std::wstring LogFilePath();
Config LoadConfig();
void SaveConfig(const Config& cfg);
void WriteDefaultConfig(const Config& cfg);
bool OpenConfigInEditor();
bool IsConfigured(const Config& cfg);

std::string DpapiSeal(const std::string& plain);
bool DpapiUnseal(const std::string& sealed, std::string& out);

void LogInit();
void LogClose();
void LogLine(const char* fmt, ...);
void InstallCrashHandler();
void RevealLogFolder();

struct CaptureSpec {
    CaptureMode mode = CaptureMode::ActiveMonitor;
    HWND window = nullptr;
    RECT region{};
    int max_width_px = 1600;
    int max_height_px = 0;
    bool include_cursor = false;
};

struct CaptureResult {
    bool ok = false;
    bool cancelled = false;
    bool restore = false;
    std::vector<uint8_t> png;
    int width = 0;
    int height = 0;
    std::string error;
};

CaptureResult CaptureScreenPng(const CaptureSpec& spec);
RECT VirtualScreenRect();
HWND PickWindowModal(HWND owner);
bool PickRegionModal(HWND owner, RECT& out);

struct CancelToken {
    std::atomic<bool> cancelled{false};
    std::atomic<HINTERNET> handle{nullptr};
    void Cancel();
    bool IsCancelled() const { return cancelled.load(); }
};

struct HttpRequest {
    std::wstring url;
    std::string body;
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    int timeout_ms = 60000;
    int max_retries = 2;
    size_t max_response_bytes = 8u * 1024 * 1024;
    bool stream = false;
    std::function<void(const std::string&)> on_text_delta;
    CancelToken* cancel = nullptr;
    std::wstring proxy_host;
    int proxy_port = 0;
    std::wstring proxy_user;
    std::wstring proxy_pass;
    std::wstring cert_pin_sha256;
};

struct HttpResponse {
    bool ok = false;
    bool cancelled = false;
    bool truncated = false;
    int attempts = 0;
    DWORD status = 0;
    std::string body;
    std::string error;
};

HttpResponse HttpPerform(const HttpRequest& req);
bool InstallCaBundle(const std::wstring& path);

struct AiAnswer {
    bool ok = false;
    bool cancelled = false;
    std::string text;
    std::string error;
};

using DeltaSink = std::function<void(const std::string&)>;
AiAnswer AskStudyAssistant(const Config& cfg, const std::string& hwid,
                           const std::vector<std::vector<uint8_t>>& pngs,
                           CancelToken* cancel = nullptr,
                           const DeltaSink& on_delta = {});

struct LicenseState {
    bool ok = false;
    bool transient = false;
    bool hard_stop = false;
    std::string token;
    std::string message;
};

std::string ComputeHwid();
LicenseState CheckLicense(const Config& cfg, const std::string& hwid,
                          bool force_refresh = false);
void InvalidateLicenseCache();

struct HistoryEntry {
    long long ts = 0;
    std::string text;
};

void HistoryLoad(std::vector<HistoryEntry>& out);
void HistoryAppend(const std::string& text, int limit);
void HistoryClear();

struct JobOutcome {
    bool ok = false;
    bool cancelled = false;
    std::wstring text;
    std::wstring status;
};

void ShowSettingsWindow(HWND owner, const Config& current);
void ShowHistoryWindow(HWND owner, int limit);
bool HotkeyAvailable(const HotkeySpec& spec);
