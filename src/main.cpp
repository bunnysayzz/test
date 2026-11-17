#include "app.h"
#include "dragdrop.h"
#include "log.h"
#include "util.h"

#include <commctrl.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#pragma comment(lib, "comctl32.lib")

namespace {

constexpr wchar_t kClass[] = L"CrowmanStudyWindow";

enum : int {
    ID_ANSWER = 1001,
    ID_STATUS,
    ID_CAPTURE,
    ID_COPY,
    ID_SETTINGS,
    ID_CANCEL,
    ID_HISTORY,
    ID_SEND,
    ID_DIAG,
    ID_QUIT,
    ID_CTX_COPY = 9001,
    ID_CTX_SELECT,
    ID_CTX_CLEAR,
};

constexpr int HK_CAPTURE = 1, HK_COPY = 2, HK_TOGGLE = 3;
constexpr UINT_PTR kElapsed = 1, kBatch = 2;

HWND g_answer, g_status, g_capture, g_copy, g_settings, g_cancel, g_history, g_send,
    g_diag;
HFONT g_font;
UINT g_dpi = 96;
Config g_config;
std::string g_hwid;
std::wstring g_last_answer;
std::vector<std::vector<uint8_t>> g_batch;
std::atomic<bool> g_capturing{false}, g_asking{false}, g_capture_cancel{false};
CancelToken g_cancel;
HWND g_last_foreign = nullptr;
HWINEVENTHOOK g_hook = nullptr;
NOTIFYICONDATAW g_tray{};
bool g_tray_visible = false;
int g_elapsed = 0;
bool g_streaming = false;

int Scale(int value) { return ::MulDiv(value, (int)g_dpi, 96); }

bool Busy() { return g_capturing.load() || g_asking.load(); }

void SetStatus(const std::wstring& text) {
    if (g_status) ::SetWindowTextW(g_status, text.c_str());
}

void AppendAnswer(const std::wstring& text) {
    if (!g_answer || text.empty()) return;
    const LRESULT length = ::SendMessageW(g_answer, WM_GETTEXTLENGTH, 0, 0);
    ::SendMessageW(g_answer, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    ::SendMessageW(g_answer, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    ::SendMessageW(g_answer, EM_SCROLLCARET, 0, 0);
}

void ReplaceAnswer(const std::wstring& text) {
    if (g_answer) ::SetWindowTextW(g_answer, text.c_str());
}

void UpdateBatchUi() {
    const size_t count = g_batch.size();
    ::ShowWindow(g_send, count ? SW_SHOW : SW_HIDE);
    ::EnableWindow(g_capture, count >= CROWMAN_BATCH_MAX ? FALSE : TRUE);
    if (count) SetStatus(SFormat(Str::StatusBatch, count));
}

void ClearBatch() {
    g_batch.clear();
    ::KillTimer(g_capture, kBatch);
    ::ShowWindow(g_send, SW_HIDE);
    ::EnableWindow(g_capture, TRUE);
}

void SetBusyUi(bool busy) {
    ::EnableWindow(g_capture, busy ? FALSE : TRUE);
    ::ShowWindow(g_cancel, busy ? SW_SHOW : SW_HIDE);
    if (busy) {
        g_elapsed = 0;
        ::SetTimer(g_capture, kElapsed, 1000, nullptr);
    } else {
        ::KillTimer(g_capture, kElapsed);
        ::SetWindowTextW(g_capture, S(Str::BtnCapture));
    }
}

void Layout(HWND hwnd) {
    RECT rc{};
    ::GetClientRect(hwnd, &rc);
    const int pad = Scale(14), bh = Scale(36), sh = Scale(22), gap = Scale(10);
    const int by = rc.bottom - pad - bh, sy = by - gap - sh;
    const int ah = sy - gap - pad;

    if (g_answer)
        ::MoveWindow(g_answer, pad, pad, rc.right - pad * 2,
                     ah > Scale(80) ? ah : Scale(80), TRUE);
    if (g_status) ::MoveWindow(g_status, pad, sy, rc.right - pad * 2, sh, TRUE);

    int x = pad;
    ::MoveWindow(g_capture, x, by, Scale(160), bh, TRUE);
    x += Scale(168);
    ::MoveWindow(g_send, x, by, Scale(90), bh, TRUE);
    x += Scale(98);
    ::MoveWindow(g_cancel, x, by, Scale(80), bh, TRUE);
    x += Scale(88);
    ::MoveWindow(g_copy, x, by, Scale(80), bh, TRUE);
    x += Scale(88);
    ::MoveWindow(g_history, x, by, Scale(80), bh, TRUE);
    x += Scale(88);
    ::MoveWindow(g_settings, x, by, Scale(90), bh, TRUE);
    x += Scale(98);
    ::MoveWindow(g_diag, x, by, Scale(100), bh, TRUE);
}

void CreateFonts() {
    if (g_font) ::DeleteObject(g_font);
    g_font = ::CreateFontW(-Scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void ApplyFonts() {
    for (HWND control : {g_answer, g_status, g_capture, g_copy, g_settings, g_cancel,
                         g_history, g_send, g_diag})
        if (control) ::SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
}

void CopyAnswer(HWND hwnd) {
    if (g_last_answer.empty()) {
        SetStatus(S(Str::StatusNothingToCopy));
        return;
    }
    if (!::OpenClipboard(hwnd)) {
        SetStatus(S(Str::StatusClipboardFailed));
        return;
    }
    ::EmptyClipboard();
    const size_t bytes = (g_last_answer.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* target = ::GlobalLock(memory)) {
            std::memcpy(target, g_last_answer.c_str(), bytes);
            ::GlobalUnlock(memory);
            if (::SetClipboardData(CF_UNICODETEXT, memory)) SetStatus(S(Str::StatusCopied));
            else ::GlobalFree(memory);
        } else {
            ::GlobalFree(memory);
        }
    }
    ::CloseClipboard();
}

void TrayAdd(HWND hwnd) {
    if (!g_config.tray_icon || g_tray_visible) return;
    g_tray = NOTIFYICONDATAW{};
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = hwnd;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WM_APP_TRAY;
    g_tray.hIcon = ::LoadIconW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    if (!g_tray.hIcon) g_tray.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_tray.szTip, S(Str::AppTitle));
    g_tray_visible = ::Shell_NotifyIconW(NIM_ADD, &g_tray) == TRUE;
}

void TrayRemove() {
    if (!g_tray_visible) return;
    ::Shell_NotifyIconW(NIM_DELETE, &g_tray);
    g_tray_visible = false;
}

void TrayMenu(HWND hwnd) {
    POINT cursor{};
    ::GetCursorPos(&cursor);
    HMENU menu = ::CreatePopupMenu();
    ::AppendMenuW(menu, MF_STRING, ID_CAPTURE, S(Str::TrayCapture));
    ::AppendMenuW(menu, MF_STRING, ID_COPY, S(Str::TrayCopy));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, ID_SETTINGS, S(Str::TraySettings));
    ::AppendMenuW(menu, MF_STRING, ID_HISTORY, S(Str::TrayHistory));
    ::AppendMenuW(menu, MF_STRING, ID_DIAG, S(Str::TrayDiag));
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, ID_QUIT, S(Str::TrayQuit));
    ::SetForegroundWindow(hwnd);
    ::TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd, nullptr);
    ::DestroyMenu(menu);
}

void Toggle(HWND hwnd) {
    if (::IsWindowVisible(hwnd)) {
        ::ShowWindow(hwnd, SW_HIDE);
    } else {
        ::ShowWindow(hwnd, SW_SHOW);
        ::SetForegroundWindow(hwnd);
    }
}

void UnregisterHotkeys(HWND hwnd) {
    ::UnregisterHotKey(hwnd, HK_CAPTURE);
    ::UnregisterHotKey(hwnd, HK_COPY);
    ::UnregisterHotKey(hwnd, HK_TOGGLE);
}

void RegisterHotkeys(HWND hwnd) {
    UnregisterHotkeys(hwnd);
    auto one = [&](int id, const HotkeySpec& spec) {
        if (!spec.enabled || !spec.vk) return;
        if (!::RegisterHotKey(hwnd, id, spec.mods | MOD_NOREPEAT, spec.vk))
            LogLine("RegisterHotKey %d failed for %ls", id, FormatHotkey(spec).c_str());
    };
    one(HK_CAPTURE, g_config.hotkey_capture);
    one(HK_COPY, g_config.hotkey_copy);
    one(HK_TOGGLE, g_config.hotkey_toggle);
}

struct CaptureJob {
    HWND hwnd;
    Config config;
    CaptureMode mode;
    bool force_pick;
};

struct AskJob {
    HWND hwnd;
    Config config;
    std::string hwid;
    std::vector<std::vector<uint8_t>> pngs;
    CancelToken* cancel;
};

void CALLBACK ForegroundEvent(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD,
                              DWORD) {
    if (window) g_last_foreign = window;
}

void StartAskJob(HWND hwnd);

DWORD WINAPI CaptureThread(LPVOID parameter) {
    std::unique_ptr<CaptureJob> job((CaptureJob*)parameter);
    auto result = std::make_unique<CaptureResult>();
    result->restore = true;
    const HWND target = job->hwnd;

    CaptureSpec spec;
    spec.mode = job->mode;
    spec.max_width_px = job->config.max_width_px;
    spec.max_height_px = job->config.max_height_px;
    spec.include_cursor = job->config.capture_cursor;

    bool picked = false;
    if (spec.mode == CaptureMode::PickWindow) {
        spec.window = PickWindowModal(target);
        if (!spec.window) {
            result->cancelled = true;
            result->error = "no window";
            ::PostMessageW(target, WM_JOB_CAPTURED, 0, (LPARAM)result.release());
            return 0;
        }
    } else if (spec.mode == CaptureMode::ForegroundWindow) {
        spec.window = g_last_foreign;
        if (!spec.window || spec.window == target) spec.mode = CaptureMode::ActiveMonitor;
    } else if (spec.mode == CaptureMode::Region) {
        RECT stored{};
        if (job->config.reuse_crop && !job->force_pick &&
            ParseRect(job->config.crop_rect, stored)) {
            spec.region = stored;
        } else if (PickRegionModal(target, spec.region)) {
            picked = true;
        } else {
            result->cancelled = true;
            result->error = "no region";
            ::PostMessageW(target, WM_JOB_CAPTURED, 0, (LPARAM)result.release());
            return 0;
        }
    }

    ::Sleep(90);
    if (g_capture_cancel.load()) {
        result->cancelled = true;
        result->error = "cancelled";
        ::PostMessageW(target, WM_JOB_CAPTURED, 0, (LPARAM)result.release());
        return 0;
    }

    *result = CaptureScreenPng(spec);
    result->restore = true;
    if (picked && result->ok)
        ::PostMessageW(target, WM_APP_CROP, 0, (LPARAM)new RECT(spec.region));
    ::PostMessageW(target, WM_JOB_CAPTURED, 0, (LPARAM)result.release());
    return 0;
}

DWORD WINAPI AskThread(LPVOID parameter) {
    std::unique_ptr<AskJob> job((AskJob*)parameter);
    auto outcome = std::make_unique<JobOutcome>();
    const HWND target = job->hwnd;

    const LicenseState licence = CheckLicense(job->config, job->hwid);
    if (!licence.ok) {
        outcome->text = S(Str::PrefixActivation) + FromUtf8(licence.message);
        outcome->status = licence.hard_stop ? FromUtf8(licence.message)
                                            : S(Str::StatusNotActivated);
        ::PostMessageW(target, WM_JOB_DONE, 0, (LPARAM)outcome.release());
        return 0;
    }

    const AiAnswer answer = AskStudyAssistant(
        job->config, job->hwid, job->pngs, job->cancel,
        [target](const std::string& delta) {
            ::PostMessageW(target, WM_JOB_DELTA, 0, (LPARAM)new std::wstring(FromUtf8(delta)));
        });

    if (answer.cancelled) {
        outcome->cancelled = true;
        outcome->status = S(Str::StatusCancelled);
    } else if (answer.ok) {
        outcome->ok = true;
        outcome->text = FromUtf8(answer.text);
        outcome->status = S(Str::StatusAnswerReceived);
    } else {
        outcome->text = S(Str::PrefixRequestFailed) + FromUtf8(answer.error);
        outcome->status = S(Str::StatusRequestFailed);
    }
    ::PostMessageW(target, WM_JOB_DONE, 0, (LPARAM)outcome.release());
    return 0;
}

void StartCaptureJob(HWND hwnd, CaptureMode mode, bool force_pick) {
    if (g_capturing.exchange(true)) return;
    if (g_batch.size() >= CROWMAN_BATCH_MAX) {
        g_capturing = false;
        SetStatus(S(Str::StatusBatchFull));
        return;
    }
    g_capture_cancel = false;
    if (g_config.hide_while_capturing && ::IsWindowVisible(hwnd))
        ::ShowWindow(hwnd, SW_HIDE);
    SetStatus(S(Str::StatusCapturing));

    HANDLE worker = ::CreateThread(nullptr, 0, CaptureThread,
                                   new CaptureJob{hwnd, g_config, mode, force_pick}, 0,
                                   nullptr);
    if (worker) {
        ::CloseHandle(worker);
    } else {
        g_capturing = false;
        ::ShowWindow(hwnd, SW_SHOW);
        SetStatus(S(Str::StatusWorkerFailed));
    }
}

void StartAskJob(HWND hwnd) {
    if (g_batch.empty() || g_asking.exchange(true)) return;
    ::KillTimer(g_capture, kBatch);
    ::ShowWindow(g_send, SW_HIDE);

    if (!IsConfigured(g_config)) {
        ReplaceAnswer(S(Str::NoConfig));
        SetStatus(S(Str::StatusConfigNeeded));
        g_asking = false;
        return;
    }

    g_cancel.cancelled = false;
    g_cancel.handle = nullptr;
    g_streaming = false;
    g_last_answer.clear();
    ReplaceAnswer(L"");
    SetBusyUi(true);
    SetStatus(S(Str::StatusStreaming));

    auto* job = new AskJob{hwnd, g_config, g_hwid, g_batch, &g_cancel};
    HANDLE worker = ::CreateThread(nullptr, 0, AskThread, job, 0, nullptr);
    if (worker) {
        ::CloseHandle(worker);
    } else {
        delete job;
        g_asking = false;
        SetBusyUi(false);
        SetStatus(S(Str::StatusWorkerFailed));
    }
}

void FinishJob(HWND hwnd, JobOutcome* raw) {
    std::unique_ptr<JobOutcome> outcome(raw);
    if (!outcome) return;

    if (!g_streaming) ReplaceAnswer(outcome->text);
    g_streaming = false;

    if (outcome->ok && !outcome->text.empty()) {
        g_last_answer = outcome->text;
        HistoryAppend(ToUtf8(outcome->text), g_config.history_limit);
    }
    if (!outcome->status.empty()) SetStatus(outcome->status);

    g_asking = false;
    SetBusyUi(false);
    ClearBatch();
    ::SetFocus(hwnd);
}

void ReloadConfig(HWND hwnd) {
    g_config = LoadConfig();
    SetLanguage(g_config.language);
    RegisterHotkeys(hwnd);
    TrayRemove();
    TrayAdd(hwnd);
    if (!g_config.ca_file.empty()) InstallCaBundle(g_config.ca_file);
    SetStatus(S(Str::StatusReloaded));
}

LRESULT CALLBACK AnswerSubclass(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                                UINT_PTR, DWORD_PTR) {
    static bool dragging = false;
    static POINT anchor{};

    switch (message) {
        case WM_LBUTTONDBLCLK:
            CopyAnswer(hwnd);
            return 0;
        case WM_LBUTTONDOWN:
            dragging = false;
            ::GetCursorPos(&anchor);
            break;
        case WM_MOUSEMOVE:
            if (dragging || !(wparam & MK_LBUTTON) || g_last_answer.empty()) break;
            {
                POINT now{};
                ::GetCursorPos(&now);
                if (abs(now.x - anchor.x) < ::GetSystemMetrics(SM_CXDRAG) &&
                    abs(now.y - anchor.y) < ::GetSystemMetrics(SM_CYDRAG))
                    break;
            }
            dragging = true;
            BeginTextDrag(hwnd, g_last_answer);
            dragging = false;
            return 0;
        case WM_KEYDOWN:
            if (wparam == 'C' && (::GetKeyState(VK_CONTROL) & 0x8000)) {
                CopyAnswer(hwnd);
                return 0;
            }
            if (wparam == VK_ESCAPE) {
                ::PostMessageW(::GetParent(hwnd), WM_KEYDOWN, VK_ESCAPE, 0);
                return 0;
            }
            break;
        case WM_CONTEXTMENU: {
            HMENU menu = ::CreatePopupMenu();
            ::AppendMenuW(menu, MF_STRING, ID_CTX_COPY, S(Str::BtnCopy));
            ::AppendMenuW(menu, MF_STRING, ID_CTX_SELECT, S(Str::CtxSelectAll));
            ::AppendMenuW(menu, MF_STRING, ID_CTX_CLEAR, S(Str::CtxClear));
            POINT at{};
            ::GetCursorPos(&at);
            const int choice = ::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                                at.x, at.y, 0, hwnd, nullptr);
            ::DestroyMenu(menu);
            if (choice == ID_CTX_COPY) CopyAnswer(hwnd);
            else if (choice == ID_CTX_SELECT) ::SendMessageW(hwnd, EM_SETSEL, 0, -1);
            else if (choice == ID_CTX_CLEAR) {
                g_last_answer.clear();
                ::SetWindowTextW(hwnd, L"");
            }
            return 0;
        }
        default:
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

void CreateControls(HWND hwnd, HINSTANCE instance) {
    auto make = [&](const wchar_t* klass, const wchar_t* text, DWORD style, int id) {
        return ::CreateWindowExW(0, klass, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                                 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, instance,
                                 nullptr);
    };

    g_answer = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                     ES_AUTOVSCROLL | ES_READONLY,
                                 0, 0, 10, 10, hwnd, (HMENU)ID_ANSWER, instance, nullptr);
    ::SetWindowSubclass(g_answer, AnswerSubclass, 1, 0);

    g_status = make(L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, ID_STATUS);
    g_capture = make(L"BUTTON", S(Str::BtnCapture), BS_PUSHBUTTON, ID_CAPTURE);
    g_send = make(L"BUTTON", S(Str::BtnSend), BS_PUSHBUTTON, ID_SEND);
    g_cancel = make(L"BUTTON", S(Str::BtnCancel), BS_PUSHBUTTON, ID_CANCEL);
    g_copy = make(L"BUTTON", S(Str::BtnCopy), BS_PUSHBUTTON, ID_COPY);
    g_history = make(L"BUTTON", S(Str::BtnHistory), BS_PUSHBUTTON, ID_HISTORY);
    g_settings = make(L"BUTTON", S(Str::BtnSettings), BS_PUSHBUTTON, ID_SETTINGS);
    g_diag = make(L"BUTTON", S(Str::BtnDiagnostics), BS_PUSHBUTTON, ID_DIAG);

    ::ShowWindow(g_cancel, SW_HIDE);
    ::ShowWindow(g_send, SW_HIDE);

    ApplyFonts();
    Layout(hwnd);
}

void ShowIntro() {
    std::wstring text = S(Str::Intro);
    text += L"\n\n" + std::wstring(S(Str::HotkeyCapture)) + L" " +
            FormatHotkey(g_config.hotkey_capture);
    text += L"\n" + std::wstring(S(Str::HotkeyCopy)) + L" " +
            FormatHotkey(g_config.hotkey_copy);
    text += L"\n" + std::wstring(S(Str::HotkeyToggle)) + L" " +
            FormatHotkey(g_config.hotkey_toggle);
    ReplaceAnswer(text);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_dpi = ::GetDpiForWindow(hwnd);
            if (!g_dpi) g_dpi = 96;
            CreateFonts();
            g_config = LoadConfig();
            SetLanguage(g_config.language);
            g_hwid = ComputeHwid();

            CreateControls(hwnd, (HINSTANCE)::GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            LogLine("window created, hwid %s", g_hwid.c_str());

            const bool fresh =
                ::GetFileAttributesW(ConfigPath().c_str()) == INVALID_FILE_ATTRIBUTES;
            if (fresh) WriteDefaultConfig(g_config);

            g_hook = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                       nullptr, ForegroundEvent, 0, 0,
                                       WINEVENT_OUTOFCONTEXT);
            RegisterHotkeys(hwnd);
            TrayAdd(hwnd);
            if (!g_config.ca_file.empty()) InstallCaBundle(g_config.ca_file);

            ShowIntro();
            SetStatus(fresh ? S(Str::StatusCreatedConfig) : S(Str::StatusReady));
            if (g_config.start_hidden) ::ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        case WM_DPICHANGED: {
            g_dpi = HIWORD(wparam);
            CreateFonts();
            ApplyFonts();
            const RECT* suggested = (const RECT*)lparam;
            ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                           suggested->right - suggested->left,
                           suggested->bottom - suggested->top,
                           SWP_NOZORDER | SWP_NOACTIVATE);
            Layout(hwnd);
            return 0;
        }

        case WM_SIZE:
            Layout(hwnd);
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = (MINMAXINFO*)lparam;
            info->ptMinTrackSize.x = Scale(640);
            info->ptMinTrackSize.y = Scale(480);
            return 0;
        }

        case WM_TIMER:
            if (wparam == kElapsed) {
                ++g_elapsed;
                wchar_t text[64] = {};
                swprintf_s(text, L"%ls %ds", S(Str::BtnWorking), g_elapsed);
                ::SetWindowTextW(g_capture, text);
            } else if (wparam == kBatch) {
                ::KillTimer(g_capture, kBatch);
                StartAskJob(hwnd);
            }
            return 0;

        case WM_APP_CROP: {
            std::unique_ptr<RECT> rect((RECT*)lparam);
            if (rect) {
                g_config.crop_rect = FormatRect(*rect);
                SaveConfig(g_config);
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE && Busy()) {
                g_capture_cancel = true;
                ClearBatch();
                g_cancel.Cancel();
                SetStatus(S(Str::StatusCancelling));
                return 0;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case ID_CAPTURE:
                    StartCaptureJob(hwnd, g_config.capture_mode,
                                    (::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                    return 0;
                case ID_SEND:
                    StartAskJob(hwnd);
                    return 0;
                case ID_CANCEL:
                    g_capture_cancel = true;
                    ClearBatch();
                    g_cancel.Cancel();
                    SetStatus(S(Str::StatusCancelling));
                    return 0;
                case ID_COPY:
                    CopyAnswer(hwnd);
                    return 0;
                case ID_HISTORY:
                    ShowHistoryWindow(hwnd, g_config.history_limit);
                    return 0;
                case ID_SETTINGS:
                    ShowSettingsWindow(hwnd, g_config);
                    return 0;
                case ID_DIAG:
                    RevealLogFolder();
                    return 0;
                case ID_QUIT:
                    ::DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;

        case WM_HOTKEY:
            if (wparam == HK_CAPTURE) {
                StartCaptureJob(hwnd, g_config.capture_mode,
                                (::GetKeyState(VK_SHIFT) & 0x8000) != 0);
                return 0;
            }
            if (wparam == HK_COPY) {
                CopyAnswer(hwnd);
                return 0;
            }
            if (wparam == HK_TOGGLE) {
                Toggle(hwnd);
                return 0;
            }
            break;

        case WM_APP_TRAY:
            if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU)
                TrayMenu(hwnd);
            else if (LOWORD(lparam) == WM_LBUTTONDBLCLK)
                Toggle(hwnd);
            return 0;

        case WM_APP_RELOAD:
            ReloadConfig(hwnd);
            return 0;

        case WM_JOB_CAPTURED: {
            std::unique_ptr<CaptureResult> result((CaptureResult*)lparam);
            g_capturing = false;
            if (result && result->restore) ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            if (!result) return 0;

            if (result->cancelled) {
                SetStatus(result->error == "no region"
                              ? S(Str::StatusRegionCancelled)
                              : result->error == "no window" ? S(Str::StatusNoWindowPicked)
                                                             : S(Str::StatusCancelled));
                return 0;
            }
            if (!result->ok) {
                ReplaceAnswer(S(Str::PrefixCaptureFailed) + FromUtf8(result->error));
                SetStatus(S(Str::StatusCaptureFailed));
                LogLine("capture failed: %s", result->error.c_str());
                return 0;
            }

            g_batch.push_back(std::move(result->png));
            LogLine("captured %dx%d, batch %zu", result->width, result->height,
                    g_batch.size());
            UpdateBatchUi();
            if (g_batch.size() >= CROWMAN_BATCH_MAX) StartAskJob(hwnd);
            else ::SetTimer(g_capture, kBatch, CROWMAN_BATCH_DELAY_MS, nullptr);
            return 0;
        }

        case WM_JOB_DELTA: {
            std::unique_ptr<std::wstring> delta((std::wstring*)lparam);
            if (delta) {
                g_streaming = true;
                AppendAnswer(*delta);
            }
            return 0;
        }

        case WM_JOB_DONE:
            FinishJob(hwnd, (JobOutcome*)lparam);
            return 0;

        case WM_CLOSE:
            if (g_config.minimize_to_tray && g_config.tray_icon && !Busy()) {
                ::ShowWindow(hwnd, SW_HIDE);
                SetStatus(S(Str::StatusTrayRunning));
                return 0;
            }
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            g_cancel.Cancel();
            UnregisterHotkeys(hwnd);
            TrayRemove();
            if (g_hook) ::UnhookWinEvent(g_hook);
            if (g_font) ::DeleteObject(g_font);
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

// Declared in app.h and called from settings.cpp, so it has to sit outside the
// anonymous namespace above.
bool HotkeyAvailable(const HotkeySpec& spec) {
    if (!spec.enabled || !spec.vk) return true;
    HWND probe = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                   nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (!probe) return true;
    const bool free = ::RegisterHotKey(probe, 1, spec.mods | MOD_NOREPEAT, spec.vk) != FALSE;
    if (free) ::UnregisterHotKey(probe, 1);
    ::DestroyWindow(probe);
    return free;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    if (!::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        ::SetProcessDPIAware();
    ::OleInitialize(nullptr);
    LogInit();
    InstallCrashHandler();
    srand((unsigned)::GetTickCount());

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    wc.hIconSm = wc.hIcon;
    if (!::RegisterClassExW(&wc)) return 1;

    HWND window = ::CreateWindowExW(0, kClass, CROWMAN_APP_NAME, WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 900, 680, nullptr,
                                    nullptr, instance, nullptr);
    if (!window) return 1;

    ::ShowWindow(window, show_command == 0 ? SW_SHOWNORMAL : show_command);
    ::UpdateWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    ::OleUninitialize();
    LogClose();
    return (int)message.wParam;
}
