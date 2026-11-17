#include "app.h"
#include "json.h"
#include "redact.h"
#include "util.h"

#include <shellapi.h>
#include <shlobj.h>

#include <iterator>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace {

constexpr wchar_t kSection[] = L"study";

std::wstring ReadIni(const std::wstring& path, const wchar_t* key,
                     const wchar_t* fallback) {
    wchar_t buffer[4096] = {};
    ::GetPrivateProfileStringW(kSection, key, fallback, buffer,
                               (DWORD)std::size(buffer), path.c_str());
    return TrimW(buffer);
}

int ToInt(const std::wstring& value, int fallback) {
    if (value.empty()) return fallback;
    const int parsed = _wtoi(value.c_str());
    return parsed >= 0 ? parsed : fallback;
}

bool ToBool(const std::wstring& value, bool fallback) {
    if (value.empty()) return fallback;
    return value == L"1" || value == L"true" || value == L"yes" || value == L"on";
}

std::wstring FromBool(bool value) { return value ? L"1" : L"0"; }

}  // namespace

const wchar_t* CaptureModeName(CaptureMode mode) {
    switch (mode) {
        case CaptureMode::Desktop: return L"desktop";
        case CaptureMode::ActiveMonitor: return L"monitor";
        case CaptureMode::ForegroundWindow: return L"window";
        case CaptureMode::PickWindow: return L"pick";
        case CaptureMode::Region: return L"region";
    }
    return L"monitor";
}

bool CaptureModeFromName(const std::wstring& name, CaptureMode& out) {
    if (name == L"desktop") out = CaptureMode::Desktop;
    else if (name == L"monitor") out = CaptureMode::ActiveMonitor;
    else if (name == L"window") out = CaptureMode::ForegroundWindow;
    else if (name == L"pick") out = CaptureMode::PickWindow;
    else if (name == L"region") out = CaptureMode::Region;
    else return false;
    return true;
}

std::wstring ConfigDir() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(::SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0,
                                  path)))
        return L".";
    const std::wstring dir = std::wstring(path) + L"\\CrowmanStudy";
    ::CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring ConfigPath() { return ConfigDir() + L"\\config.ini"; }
std::wstring LicensePath() { return ConfigDir() + L"\\license.json"; }
std::wstring HistoryPath() { return ConfigDir() + L"\\history.json"; }
std::wstring SecretsPath() { return ConfigDir() + L"\\secrets.bin"; }

Config LoadConfig() {
    Config cfg;
    const std::wstring path = ConfigPath();

    cfg.mode = ReadIni(path, L"mode", L"openai");
    cfg.endpoint = ReadIni(path, L"endpoint", L"");
    cfg.model = ReadIni(path, L"model", L"gpt-4o-mini");
    cfg.license_url = ReadIni(path, L"license_url", L"");
    cfg.proxy_host = ReadIni(path, L"proxy_host", L"");
    cfg.proxy_user = ReadIni(path, L"proxy_user", L"");
    cfg.ca_file = ReadIni(path, L"ca_file", L"");
    cfg.cert_pin_sha256 = ReadIni(path, L"cert_pin_sha256", L"");
    cfg.crop_rect = ReadIni(path, L"crop_rect", L"");
    cfg.language = ReadIni(path, L"language", L"en");

    cfg.proxy_port = ToInt(ReadIni(path, L"proxy_port", L"0"), 0);
    cfg.max_width_px = ToInt(ReadIni(path, L"max_width_px", L"1600"), 1600);
    cfg.max_height_px = ToInt(ReadIni(path, L"max_height_px", L"0"), 0);
    cfg.timeout_ms = ToInt(ReadIni(path, L"timeout_ms", L"60000"), 60000);
    cfg.max_retries = ToInt(ReadIni(path, L"max_retries", L"2"), 2);
    cfg.max_response_bytes = ToInt(ReadIni(path, L"max_response_bytes", L"8388608"), 8388608);
    cfg.history_limit = ToInt(ReadIni(path, L"history_limit", L"20"), 20);
    cfg.offline_grace_hours = ToInt(ReadIni(path, L"offline_grace_hours", L"72"), 72);

    cfg.stream = ToBool(ReadIni(path, L"stream", L"1"), true);
    cfg.capture_cursor = ToBool(ReadIni(path, L"capture_cursor", L"0"), false);
    cfg.hide_while_capturing = ToBool(ReadIni(path, L"hide_while_capturing", L"1"), true);
    cfg.tray_icon = ToBool(ReadIni(path, L"tray_icon", L"1"), true);
    cfg.minimize_to_tray = ToBool(ReadIni(path, L"minimize_to_tray", L"0"), false);
    cfg.start_hidden = ToBool(ReadIni(path, L"start_hidden", L"0"), false);
    cfg.reuse_crop = ToBool(ReadIni(path, L"reuse_crop", L"1"), true);

    CaptureMode mode = CaptureMode::ActiveMonitor;
    if (CaptureModeFromName(ReadIni(path, L"capture_mode", L"monitor"), mode))
        cfg.capture_mode = mode;

    ParseHotkey(ReadIni(path, L"hotkey_capture", L"ctrl+alt+s"), cfg.hotkey_capture);
    ParseHotkey(ReadIni(path, L"hotkey_copy", L"ctrl+alt+c"), cfg.hotkey_copy);
    ParseHotkey(ReadIni(path, L"hotkey_toggle", L"ctrl+alt+h"), cfg.hotkey_toggle);

    if (cfg.mode != L"broker") cfg.mode = L"openai";
    if (cfg.max_width_px != 0 && cfg.max_width_px < 320) cfg.max_width_px = 320;
    if (cfg.max_height_px != 0 && cfg.max_height_px < 200) cfg.max_height_px = 200;
    if (cfg.timeout_ms < 5000) cfg.timeout_ms = 5000;
    if (cfg.max_retries > 5) cfg.max_retries = 5;
    if (cfg.max_response_bytes < 65536) cfg.max_response_bytes = 65536;
    if (cfg.max_response_bytes > 67108864) cfg.max_response_bytes = 67108864;
    if (cfg.offline_grace_hours > 720) cfg.offline_grace_hours = 720;
    if (cfg.history_limit > 200) cfg.history_limit = 200;

    const Secrets secrets = LoadSecrets();
    SecretsSetCurrent(secrets);
    RedactClear();
    RedactRegister(secrets.api_key);
    RedactRegister(secrets.license_key);
    RedactRegister(secrets.proxy_pass);
    return cfg;
}

void SaveConfig(const Config& cfg) {
    std::string ini = "[study]\n";
    ini += "; Secrets live in secrets.bin (DPAPI), not here.\n";
    ini += "mode=" + ToUtf8(cfg.mode) + "\n";
    ini += "endpoint=" + ToUtf8(cfg.endpoint) + "\n";
    ini += "model=" + ToUtf8(cfg.model) + "\n";
    ini += "license_url=" + ToUtf8(cfg.license_url) + "\n";
    ini += "proxy_host=" + ToUtf8(cfg.proxy_host) + "\n";
    ini += "proxy_port=" + std::to_string(cfg.proxy_port) + "\n";
    ini += "proxy_user=" + ToUtf8(cfg.proxy_user) + "\n";
    ini += "ca_file=" + ToUtf8(cfg.ca_file) + "\n";
    ini += "cert_pin_sha256=" + ToUtf8(cfg.cert_pin_sha256) + "\n";
    ini += "crop_rect=" + ToUtf8(cfg.crop_rect) + "\n";
    ini += "reuse_crop=" + ToUtf8(FromBool(cfg.reuse_crop)) + "\n";
    ini += "max_width_px=" + std::to_string(cfg.max_width_px) + "\n";
    ini += "max_height_px=" + std::to_string(cfg.max_height_px) + "\n";
    ini += "timeout_ms=" + std::to_string(cfg.timeout_ms) + "\n";
    ini += "max_retries=" + std::to_string(cfg.max_retries) + "\n";
    ini += "max_response_bytes=" + std::to_string(cfg.max_response_bytes) + "\n";
    ini += "history_limit=" + std::to_string(cfg.history_limit) + "\n";
    ini += "offline_grace_hours=" + std::to_string(cfg.offline_grace_hours) + "\n";
    ini += "stream=" + ToUtf8(FromBool(cfg.stream)) + "\n";
    ini += "capture_cursor=" + ToUtf8(FromBool(cfg.capture_cursor)) + "\n";
    ini += "hide_while_capturing=" + ToUtf8(FromBool(cfg.hide_while_capturing)) + "\n";
    ini += "tray_icon=" + ToUtf8(FromBool(cfg.tray_icon)) + "\n";
    ini += "minimize_to_tray=" + ToUtf8(FromBool(cfg.minimize_to_tray)) + "\n";
    ini += "start_hidden=" + ToUtf8(FromBool(cfg.start_hidden)) + "\n";
    ini += "capture_mode=" + ToUtf8(CaptureModeName(cfg.capture_mode)) + "\n";
    ini += "hotkey_capture=" + ToUtf8(FormatHotkey(cfg.hotkey_capture)) + "\n";
    ini += "hotkey_copy=" + ToUtf8(FormatHotkey(cfg.hotkey_copy)) + "\n";
    ini += "hotkey_toggle=" + ToUtf8(FormatHotkey(cfg.hotkey_toggle)) + "\n";
    ini += "language=" + ToUtf8(cfg.language) + "\n";
    WriteWholeFile(ConfigPath(), ini);
}

void WriteDefaultConfig(const Config& cfg) { SaveConfig(cfg); }

bool OpenConfigInEditor() {
    if (::GetFileAttributesW(ConfigPath().c_str()) == INVALID_FILE_ATTRIBUTES)
        WriteDefaultConfig(Config());
    const HINSTANCE launched = ::ShellExecuteW(nullptr, L"open", ConfigPath().c_str(),
                                               nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)launched > 32;
}

bool IsConfigured(const Config& cfg) {
    const Secrets& secrets = CurrentSecrets();
    return !cfg.endpoint.empty() &&
           !(cfg.mode == L"broker" ? secrets.license_key : secrets.api_key).empty();
}
