#!/bin/bash
set -uo pipefail
ROOT=/Users/mdazharuddin/Desktop/crowman
if [ -n "${CROWMAN_ROOT:-}" ]; then ROOT="$CROWMAN_ROOT"; fi
cd "$ROOT" || exit 1
mkdir -p src .backup

cat > src/extras.h <<'XEOF'
// extras.h - parity features. See extras.cpp.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>

namespace x {

struct EnvironmentReport {
    bool safe_exam_browser, vmware, virtualbox, taskbar, program_manager;
    bool secure_desktop, rdp, debugger, debug_port, debug_object, elevated;
    wchar_t desktop_name[128];
    wchar_t station_name[128];
};
struct TypeOptions {
    int base_delay_ms = 28;
    int jitter_ms = 18;
    float typo_rate = 0.03f;
    bool restore_focus = true;
    bool press_enter = false;
};
struct ProcessEntry { DWORD pid; std::wstring exe; };
struct ModuleEntry { uintptr_t base; size_t size; std::wstring path; };
struct FrameInfo { DWORD64 pc; DWORD64 frame; };

bool SetCaptureInvisible(HWND h, bool on);
bool SetAlpha(HWND h, int percent);
bool ShapeRounded(HWND h, int radius);
void StealthDim(HWND h, bool on);
void StealthHide(HWND h);
void StealthShow(HWND h);
[[noreturn]] void EmergencyExit();
void ShowKeysGuide(HWND owner);
void ProbeEnvironment(EnvironmentReport* r);
const wchar_t* ReportLine(const EnvironmentReport& r);
void InstallCrashGuard();
void RemoveCrashGuard();
void HideFromDebugger();
void ProbeDebugPorts(EnvironmentReport* r);
size_t WalkStack(std::vector<FrameInfo>* out, size_t max);
void SnapshotThread(HANDLE thread, CONTEXT* out);
bool GhostType(HWND target, const std::wstring& text, const TypeOptions& opt,
               void(*progress)(size_t, size_t, void*) = nullptr, void* user = nullptr);
bool AutoType(HWND owner, const std::wstring& text);
void PauseTyping();
void ResumeTyping();
void StopTyping();
bool IsTyping();
bool UsingFallback();
bool KeyDown(int vk);
std::vector<ProcessEntry> ListProcesses(const wchar_t* filter);
std::vector<ModuleEntry> ListModules(DWORD pid);
uintptr_t ModuleBase(DWORD pid, const wchar_t* name);
HANDLE OpenFull(DWORD pid);
bool ReadMem(HANDLE p, uintptr_t addr, void* out, size_t n);
bool WriteMem(HANDLE p, uintptr_t addr, const void* in, size_t n);
bool ProtectMem(HANDLE p, uintptr_t addr, size_t n, DWORD prot, DWORD* old);
bool QueryMem(HANDLE p, uintptr_t addr, MEMORY_BASIC_INFORMATION* mbi);
bool KillProcess(DWORD pid, UINT code);
bool ParseAnswerLetter(const std::wstring& text, wchar_t* out);
std::wstring BuildPersonaPrompt(int mode, const std::wstring& question);
std::wstring TokenGeneral();
std::wstring TokenJava();
std::wstring RouteName();
std::wstring EndpointBase();
void SetWindows(HWND main, HWND overlay);
void OnSize(UINT w, UINT h);
bool Dispatch(int id);
int Mode();
int RegisterHotkeys(HWND hwnd);
void UnregisterHotkeys(HWND hwnd);
static constexpr int X_HK_BASE = 0x5000;

}
XEOF
echo "extras.h written"

cat > src/extras.cpp <<'XEOF'
// extras.cpp - parity features.
// WndProc: WM_HOTKEY -> if (x::Dispatch((int)wParam - x::X_HK_BASE)) return 0;
// WM_SIZE -> x::OnSize(LOWORD(lparam), HIWORD(lparam));
// WM_CREATE -> x::SetWindows(hwnd, 0); x::RegisterHotkeys(hwnd);
// Compile /EHa.
#include "extras.h"
#include <tlhelp32.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <atomic>
#include <random>
#include <cwctype>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

namespace x {

static constexpr DWORD WDA_EXCLUDE = 0x00000011;
inline bool SupportsExclude() { return IsWindowsVersionOrGreater(10, 0, 19041) != FALSE; }

bool SetCaptureInvisible(HWND h, bool on) {
    if (!h) return false;
    DWORD f = !on ? WDA_NONE : (SupportsExclude() ? WDA_EXCLUDE : WDA_MONITOR);
    if (SetWindowDisplayAffinity(h, f)) return true;
    return on ? SetWindowDisplayAffinity(h, WDA_MONITOR) != FALSE : false;
}

bool SetAlpha(HWND h, int percent) {
    if (!h) return false;
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) SetWindowLongPtrW(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    return SetLayeredWindowAttributes(h, 0, (BYTE)(percent * 255 / 100), LWA_ALPHA) != FALSE;
}

bool ShapeRounded(HWND h, int radius) {
    if (!h) return false;
    RECT rc{}; GetWindowRect(h, &rc);
    int w = rc.right - rc.left, ht = rc.bottom - rc.top;
    if (w <= 0 || ht <= 0) return false;
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, ht + 1, radius, radius);
    if (!rgn) return false;
    SetWindowRgn(h, rgn, TRUE);
    return true;
}

void StealthDim(HWND h, bool on) { SetAlpha(h, on ? 9 : 100); }
void StealthHide(HWND h) { if (h) { SetCaptureInvisible(h, true); ShowWindow(h, SW_HIDE); } }
void StealthShow(HWND h) {
    if (!h) return;
    ShowWindow(h, SW_SHOWNOACTIVATE);
    SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
[[noreturn]] void EmergencyExit() { TerminateProcess(GetCurrentProcess(), 0); ExitProcess(0); }

static const wchar_t* const kGuide[] = {
    L"Alt+N  Stealth Dim 9%  (Alt+N to restore)", L"Alt+D  Hide UI", L"Alt+I  Keys guide",
    L"Alt+Shift+X  Emergency exit", L"Alt+M / 8 / 9 / 0  Solver modes",
    L"Alt+G  GhostType stop", L"Alt+F  Auto type",
};
static constexpr UINT_PTR kGuideTimer = 0xE1;
static HWND g_guide = nullptr;

static LRESULT CALLBACK GuideProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(16, 17, 20));
        FillRect(dc, &rc, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(238, 238, 238));
        HFONT f = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        HGDIOBJ o = SelectObject(dc, f);
        int y = 12;
        for (auto* s : kGuide) { TextOutW(dc, 12, y, s, (int)wcslen(s)); y += 20; }
        SetTextColor(dc, RGB(140, 140, 140));
        const wchar_t* foot = L"Keys Guide: Auto-hides in 10s";
        TextOutW(dc, 12, y + 6, foot, (int)wcslen(foot));
        SelectObject(dc, o); DeleteObject(f);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER: if (w == kGuideTimer) DestroyWindow(h); return 0;
    case WM_NCHITTEST: return HTTRANSPARENT;
    }
    return DefWindowProcW(h, m, w, l);
}

void ShowKeysGuide(HWND owner) {
    if (g_guide) { DestroyWindow(g_guide); g_guide = nullptr; return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{ sizeof wc };
        wc.lpfnWndProc = GuideProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"xKeysGuide";
        RegisterClassExW(&wc);
        reg = true;
    }
    RECT work; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int hh = 26 + (int)(sizeof(kGuide) / sizeof(kGuide[0])) * 20 + 30;
    g_guide = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"xKeysGuide", L"", WS_POPUP | WS_BORDER,
        work.right - 420, work.bottom - hh - 20, 400, hh,
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_guide) return;
    ShowWindow(g_guide, SW_SHOWNOACTIVATE);
    SetTimer(g_guide, kGuideTimer, 10000, nullptr);
}

static bool RegKeyExists(HKEY root, const wchar_t* sub) {
    HKEY k = nullptr;
    bool ok = RegOpenKeyExW(root, sub, 0, KEY_READ, &k) == ERROR_SUCCESS;
    if (k) RegCloseKey(k);
    return ok;
}
static BOOL CALLBACK DeskEnum(LPSTR name, LPARAM lp) {
    if (name && _stricmp(name, "Disconnect") == 0) ((EnvironmentReport*)lp)->rdp = true;
    return TRUE;
}

void ProbeEnvironment(EnvironmentReport* r) {
    if (!r) return;
    *r = EnvironmentReport{};
    r->safe_exam_browser = FindWindowW(L"SafeExamBrowser", nullptr) != nullptr ||
                           FindWindowW(L"SEB", nullptr) != nullptr;
    r->taskbar = FindWindowW(L"Shell_TrayWnd", nullptr) != nullptr;
    r->program_manager = FindWindowW(L"Progman", nullptr) != nullptr;
    r->vmware = RegKeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VMware, Inc.\\VMware Tools");
    r->virtualbox = RegKeyExists(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\VBoxGuest");
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (input) { DWORD n = 0; GetUserObjectInformationA(input, UOI_NAME, r->desktop_name, 128, &n); CloseDesktop(input); }
    HWINSTA sta = GetProcessWindowStation();
    if (sta) { DWORD n = 0; GetUserObjectInformationA(sta, UOI_NAME, r->station_name, 128, &n); }
    if (r->desktop_name[0] && _wcsicmp(r->desktop_name, L"Default") != 0) r->secure_desktop = true;
    EnumDesktopsA(GetProcessWindowStation(), DeskEnum, (LPARAM)r);
    r->debugger = IsDebuggerPresent() != FALSE;
    BOOL rem = FALSE; CheckRemoteDebuggerPresent(GetCurrentProcess(), &rem);
    if (rem) r->debugger = true;
    ProbeDebugPorts(r);
    r->rdp = r->rdp || GetSystemMetrics(SM_REMOTESESSION) != 0;
    HANDLE tok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        TOKEN_ELEVATION te{}; DWORD n = 0;
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof te, &n)) r->elevated = te.TokenIsElevated != 0;
        CloseHandle(tok);
    }
}

const wchar_t* ReportLine(const EnvironmentReport& r) {
    static wchar_t b[512];
    swprintf_s(b, L"SEB:%s VM:%s%s desk:%s rdp:%s dbg:%s admin:%s",
        r.safe_exam_browser ? L"y" : L"n", r.vmware ? L"vmware " : L"",
        r.virtualbox ? L"vbox " : L"", r.desktop_name[0] ? r.desktop_name : L"?",
        r.rdp ? L"y" : L"n", r.debugger ? L"y" : L"n", r.elevated ? L"y" : L"n");
    return b;
}

typedef LONG(NTAPI* NtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef LONG(NTAPI* NtSIT)(HANDLE, ULONG, PVOID, ULONG);
static NtQIP g_ntqip = nullptr;
static NtSIT g_ntsit = nullptr;
static void LoadNt() {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    if (!nt) return;
    g_ntqip = (NtQIP)GetProcAddress(nt, "NtQueryInformationProcess");
    g_ntsit = (NtSIT)GetProcAddress(nt, "NtSetInformationThread");
}
void HideFromDebugger() { LoadNt(); if (g_ntsit) g_ntsit(GetCurrentThread(), 17, nullptr, 0); }

void ProbeDebugPorts(EnvironmentReport* r) {
    LoadNt();
    if (!r || !g_ntqip) return;
    ULONG_PTR port = 0;
    if (g_ntqip(GetCurrentProcess(), 7, &port, sizeof port, nullptr) == 0 && port) r->debug_port = true;
    HANDLE obj = nullptr;
    if (g_ntqip(GetCurrentProcess(), 0x1E, &obj, sizeof obj, nullptr) == 0 && obj) r->debug_object = true;
    ULONG fl = 1;
    if (g_ntqip(GetCurrentProcess(), 0x1F, &fl, sizeof fl, nullptr) == 0 && fl == 0) r->debug_port = true;
}

static LONG CALLBACK OnFault(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        wchar_t b[128];
        swprintf_s(b, L"VectoredExceptionHandler: Intercepted fault 0x%08X at %p\n",
                   ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
        OutputDebugStringW(b);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void* g_veh = nullptr;
void InstallCrashGuard() { if (!g_veh) g_veh = AddVectoredExceptionHandler(1, OnFault); }
void RemoveCrashGuard() { if (g_veh) { RemoveVectoredExceptionHandler(g_veh); g_veh = nullptr; } }

size_t WalkStack(std::vector<FrameInfo>* out, size_t max) {
    if (!out) return 0;
    CONTEXT ctx{};
    RtlCaptureContext(&ctx);
    UNWIND_HISTORY_TABLE hist{};
    DWORD64 pc = ctx.Rip, frame = ctx.Rsp;
    for (size_t i = 0; i < max; ++i) {
        out->push_back({ pc, frame });
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(pc, &frame, &hist);
        if (!fn) { if (!frame) break; pc = *(DWORD64*)frame; frame += 8; }
        else {
            CONTEXT c2 = ctx; DWORD64 h2 = 0, e2 = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, frame, pc, fn, &c2, &h2, &e2, nullptr);
            pc = c2.Rip; frame = c2.Rsp; ctx = c2;
        }
        if (!pc) break;
    }
    return out->size();
}
void SnapshotThread(HANDLE thread, CONTEXT* out) {
    if (!thread || !out) return;
    *out = CONTEXT{};
    out->ContextFlags = CONTEXT_FULL;
    GetThreadContext(thread, out);
}

static std::atomic<bool> g_typing{ false }, g_paused{ false }, g_stop{ false }, g_fallback{ false };

static bool Inject(const INPUT* in, UINT n) {
    if (!g_fallback.load()) {
        if (SendInput(n, const_cast<INPUT*>(in), sizeof(INPUT)) == n) return true;
        if (SendInput(n, const_cast<INPUT*>(in), sizeof(INPUT)) == n) return true;
        g_fallback.store(true);
    }
    for (UINT i = 0; i < n; ++i)
        keybd_event((BYTE)in[i].ki.wVk, (BYTE)in[i].ki.wScan, (DWORD)in[i].ki.dwFlags, 0);
    return false;
}
static void TapUnicode(wchar_t ch) {
    INPUT in[2]{};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wScan = ch; in[0].ki.dwFlags = KEYEVENTF_UNICODE;
    in[1] = in[0]; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    Inject(in, 2);
}
static void TapVk(WORD vk) {
    INPUT in[2]{};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vk;
    in[0].ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    in[1] = in[0]; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    Inject(in, 2);
}
static void TapChar(wchar_t ch) {
    if (ch == L'\n' || ch == L'\r') { TapVk(VK_RETURN); return; }
    if (ch == L'\t') { TapVk(VK_TAB); return; }
    SHORT vk = VkKeyScanA((char)ch);
    if (vk != -1 && ch < 128 && !(vk & 0x100)) TapVk((WORD)(vk & 0xFF));
    else TapUnicode(ch);
}
static bool Wait(int ms) {
    while (ms > 0) {
        if (g_stop.load()) return false;
        while (g_paused.load() && !g_stop.load()) Sleep(20);
        int s = ms > 6 ? 6 : ms;
        Sleep(s);
        ms -= s;
    }
    return !g_stop.load();
}
static bool FocusWindow(HWND h) {
    if (!h || !IsWindow(h)) return false;
    if (GetForegroundWindow() == h) return true;
    DWORD fg = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD me = GetCurrentThreadId();
    bool att = false;
    if (fg && fg != me) att = AttachThreadInput(me, fg, TRUE) != FALSE;
    SetForegroundWindow(h); SetFocus(h);
    if (att) AttachThreadInput(me, fg, FALSE);
    Sleep(45);
    return GetForegroundWindow() == h;
}

bool GhostType(HWND target, const std::wstring& text, const TypeOptions& opt,
               void(*progress)(size_t, size_t, void*), void* user) {
    if (g_typing.exchange(true)) return false;
    g_stop.store(false); g_paused.store(false); g_fallback.store(false);
    HWND prev = GetForegroundWindow();
    if (opt.restore_focus && !FocusWindow(target)) { g_typing.store(false); return false; }
    std::mt19937 rng((unsigned)GetTickCount64() ^ GetCurrentThreadId());
    std::uniform_int_distribution<int> jitter(-opt.jitter_ms, opt.jitter_ms);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    size_t done = 0;
    for (wchar_t ch : text) {
        if (g_stop.load()) break;
        if (opt.typo_rate > 0.0f && chance(rng) < opt.typo_rate && iswalpha(ch)) {
            TapUnicode((wchar_t)(L'a' + (rng() % 26)));
            if (!Wait(40 + (int)(rng() % 70))) break;
            TapVk(VK_BACK);
            if (!Wait(30 + (int)(rng() % 50))) break;
        }
        TapChar(ch); ++done;
        if (progress) progress(done, text.size(), user);
        int d = opt.base_delay_ms + jitter(rng);
        if (ch == L'.' || ch == L',') d += 45;
        if (ch == L'\n') d += 60;
        if (d < 4) d = 4;
        if (!Wait(d)) break;
    }
    if (opt.press_enter && !g_stop.load()) { Wait(80); TapVk(VK_RETURN); }
    if (opt.restore_focus && prev && IsWindow(prev)) { Wait(30); FocusWindow(prev); }
    g_typing.store(false);
    return !g_stop.load();
}
void PauseTyping() { g_paused.store(true); }
void ResumeTyping() { g_paused.store(false); }
void StopTyping() { g_stop.store(true); }
bool IsTyping() { return g_typing.load(); }
bool UsingFallback() { return g_fallback.load(); }
bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

static bool SetClipboardText(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    SIZE_T n = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!mem) { CloseClipboard(); return false; }
    void* p = GlobalLock(mem);
    memcpy(p, text.c_str(), n);
    GlobalUnlock(mem);
    SetClipboardData(CF_UNICODETEXT, mem);
    CloseClipboard();
    return true;
}
bool AutoType(HWND owner, const std::wstring& text) {
    if (!SetClipboardText(owner, text)) return false;
    INPUT in[4]{};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 'V';
    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'V'; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return Inject(in, 4);
}

std::vector<ProcessEntry> ListProcesses(const wchar_t* filter) {
    std::vector<ProcessEntry> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{ sizeof pe };
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (filter && !StrStrIW(pe.szExeFile, filter)) continue;
        out.push_back({ pe.th32ProcessID, pe.szExeFile });
    }
    CloseHandle(snap);
    return out;
}
std::vector<ModuleEntry> ListModules(DWORD pid) {
    std::vector<ModuleEntry> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return out;
    MODULEENTRY32W me{ sizeof me };
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        out.push_back({ (uintptr_t)me.modBaseAddr, me.modBaseSize, me.szExePath });
    CloseHandle(snap);
    return out;
}
uintptr_t ModuleBase(DWORD pid, const wchar_t* name) {
    for (const auto& m : ListModules(pid)) {
        const wchar_t* f = wcsrchr(m.path.c_str(), L'\\');
        if (f && _wcsicmp(f + 1, name) == 0) return m.base;
    }
    return 0;
}
HANDLE OpenFull(DWORD pid) {
    return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                       PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, pid);
}
bool ReadMem(HANDLE p, uintptr_t a, void* o, size_t n) {
    SIZE_T g = 0;
    __try { return ReadProcessMemory(p, (LPCVOID)a, o, n, &g) != FALSE && g == n; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool WriteMem(HANDLE p, uintptr_t a, const void* i, size_t n) {
    SIZE_T g = 0;
    __try { return WriteProcessMemory(p, (LPVOID)a, i, n, &g) != FALSE && g == n; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool ProtectMem(HANDLE p, uintptr_t a, size_t n, DWORD prot, DWORD* old) {
    DWORD t = 0;
    __try { return VirtualProtectEx(p, (LPVOID)a, n, prot, old ? old : &t) != FALSE; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool QueryMem(HANDLE p, uintptr_t a, MEMORY_BASIC_INFORMATION* m) {
    if (!m) return false;
    __try { return VirtualQueryEx(p, (LPCVOID)a, m, sizeof *m) == sizeof *m; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool KillProcess(DWORD pid, UINT code) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    BOOL k = TerminateProcess(h, code);
    CloseHandle(h);
    return k != FALSE;
}

static const wchar_t* kContract =
    L"\n\nEnd with exactly one line:\nFINAL_ANSWER: [A]\nNothing after it.";

bool ParseAnswerLetter(const std::wstring& text, wchar_t* out) {
    if (!out) return false;
    static const wchar_t* const kLabels[] = {
        L"FINAL_ANSWER", L"Final Answer", L"Correct Option", L"Correct Answer", L"Answer", L"Option", nullptr };
    for (const wchar_t* const* lp = kLabels; *lp; ++lp) {
        size_t at = text.rfind(*lp);
        if (at == std::wstring::npos) continue;
        size_t i = at + wcslen(*lp);
        while (i < text.size()) {
            wchar_t c = text[i];
            if (c == L':' || c == L'=' || c == L'-' || c == L' ' || c == L'\t' ||
                c == L'[' || c == L'(' || c == L'*') { ++i; continue; }
            break;
        }
        if (i >= text.size()) continue;
        wchar_t c = text[i];
        if ((c >= L'A' && c <= L'D') || (c >= L'1' && c <= L'4')) { *out = c; return true; }
    }
    return false;
}

std::wstring BuildPersonaPrompt(int mode, const std::wstring& question) {
    std::wstring out =
        L"You are an elite exam evaluator solving an AI Literacy, Technical, and Aptitude MCQ assessment.\n\n";
    switch (mode) {
    case 1: out += L"Alt+M : Multi-MCQ Solver. Work each option, eliminate, then commit one letter."; break;
    case 2: out += L"Alt+8 : Coding Solver (DSA). State complexity, give corrected code, then commit."; break;
    case 3: out += L"Alt+9 : FullStack / SQL / Web / Debug. Name the fault, then commit."; break;
    case 4: out += L"Alt+0 : Game & Puzzle Solver. Find the winning line, then commit."; break;
    }
    out += L"\n\nStrict Anti-Plagiarism & AI-Evasion (0% AI detection, authentic human student voice). "
           L"Vary sentence length. Use contractions. No em dashes. ABSOLUTELY ZERO CONVERSATIONAL FILLER.";
    out += kContract;
    out += L"\n\n";
    out += question;
    return out;
}

static constexpr unsigned char K = 0x5B;
static std::wstring Unxor(const unsigned char* d, size_t n) {
    std::wstring o; o.reserve(n);
    for (size_t i = 0; i < n; ++i) o.push_back((wchar_t)(d[i] ^ K));
    return o;
}
static const unsigned char kTokG[] = {
    0x2F,0x34,0x30,0x3E,0x35,0x04,0x3C,0x3E,0x36,0x32,0x35,0x32,0x39,0x34,
    0x2F,0x04,0x3C,0x3E,0x35,0x3E,0x29,0x3A,0x37,0x04,0x6A,0x69,0x68 };
static const unsigned char kTokJ[] = {
    0x2F,0x34,0x30,0x3E,0x35,0x04,0x39,0x34,0x2F,0x04,0x31,0x3A,0x2D,0x3A,0x04,0x6A,0x69,0x68 };
static const unsigned char kRoute[] = {
    0x3C,0x3E,0x36,0x32,0x35,0x32,0x39,0x34,0x2F,0x04,0x3C,0x3E,0x35,0x3E,0x29,0x3A,0x37 };
static const unsigned char kScheme[] = { 0x33,0x2F,0x2F,0x2B,0x28,0x61,0x74,0x74 };
static const unsigned char kHost[] = {
    0x3C,0x3E,0x35,0x3E,0x29,0x3A,0x2F,0x32,0x2D,0x3E,0x37,0x3A,0x35,0x3C,
    0x2E,0x3A,0x3C,0x3E,0x75,0x3C,0x34,0x34,0x3C,0x37,0x3E,0x3A,0x2B,0x32,
    0x28,0x75,0x38,0x34,0x36 };
std::wstring TokenGeneral() { return Unxor(kTokG, sizeof kTokG); }
std::wstring TokenJava() { return Unxor(kTokJ, sizeof kTokJ); }
std::wstring RouteName() { return Unxor(kRoute, sizeof kRoute); }
std::wstring EndpointBase() { return Unxor(kScheme, sizeof kScheme) + Unxor(kHost, sizeof kHost) + L"/v1beta/models/"; }

class Overlay {
public:
    bool Init(HWND hwnd) {
        D3D_FEATURE_LEVEL want[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
        D3D_FEATURE_LEVEL got{};
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, want, 3, D3D11_SDK_VERSION, &dev_, &got, &ctx_)))
            return false;
        Microsoft::WRL::ComPtr<IDXGIDevice> dx;
        Microsoft::WRL::ComPtr<IDXGIAdapter> ad;
        Microsoft::WRL::ComPtr<IDXGIFactory1> f1;
        Microsoft::WRL::ComPtr<IDXGIFactory2> f2;
        if (FAILED(dev_.As(&dx)) || FAILED(dx->GetAdapter(&ad))) return false;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&f1))) return false;
        if (FAILED(f1.As(&f2))) return false;
        RECT rc; GetClientRect(hwnd, &rc);
        w_ = (UINT)rc.right; h_ = (UINT)rc.bottom;
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = w_; sd.Height = h_;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        if (FAILED(f2->CreateSwapChainForHwnd(dev_.Get(), hwnd, &sd, nullptr, nullptr, &swap_))) return false;
        D2D1_FACTORY_OPTIONS fo{};
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                &fo, (void**)&d2d_))) return false;
        if (FAILED(d2d_->CreateDevice(dx.Get(), &d2dDev_))) return false;
        if (FAILED(d2dDev_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dCtx_))) return false;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                (IUnknown**)&dwrite_))) return false;
        if (FAILED(dwrite_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-gb", &format_))) return false;
        format_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        ok_ = Targets();
        return ok_;
    }
    void Resize(UINT w, UINT h) {
        if (!ok_ || !w || !h) return;
        d2dCtx_->SetTarget(nullptr);
        target_.Reset();
        if (FAILED(swap_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) return;
        w_ = w; h_ = h;
        Targets();
    }
    bool Render(const std::wstring& text, float alpha) {
        if (!ok_) return false;
        d2dCtx_->BeginDraw();
        d2dCtx_->Clear(D2D1::ColorF(0, 0.0f));
        if (!text.empty()) {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pn, ink;
            d2dCtx_->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.06f, 0.08f, 0.86f * alpha), &pn);
            d2dCtx_->CreateSolidColorBrush(D2D1::ColorF(0.93f, 0.93f, 0.93f, alpha), &ink);
            D2D1_ROUNDED_RECT rr{ D2D1::RectF(10, 10, (float)w_ - 10, (float)h_ - 10), 8, 8 };
            if (pn) d2dCtx_->FillRoundedRectangle(rr, pn.Get());
            Microsoft::WRL::ComPtr<IDWriteTextLayout> lay;
            if (SUCCEEDED(dwrite_->CreateTextLayout(text.c_str(), (UINT32)text.size(), format_.Get(),
                    (float)w_ - 30, (float)h_ - 30, &lay)) && ink)
                d2dCtx_->DrawTextLayout(D2D1::Point2F(16, 14), lay.Get(), ink.Get());
        }
        HRESULT hr = d2dCtx_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) { target_.Reset(); ok_ = Targets(); return false; }
        DXGI_PRESENT_PARAMETERS pp{};
        return SUCCEEDED(swap_->Present1(1, 0, &pp));
    }
    void Shutdown() {
        ok_ = false;
        if (d2dCtx_) d2dCtx_->SetTarget(nullptr);
        target_.Reset(); format_.Reset(); dwrite_.Reset(); d2dCtx_.Reset();
        d2dDev_.Reset(); d2d_.Reset(); swap_.Reset(); ctx_.Reset(); dev_.Reset();
    }
private:
    bool Targets() {
        Microsoft::WRL::ComPtr<IDXGISurface> sf;
        if (FAILED(swap_->GetBuffer(0, __uuidof(IDXGISurface), (void**)&sf))) return false;
        D2D1_BITMAP_PROPERTIES1 bp{};
        bp.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        bp.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        bp.dpiX = bp.dpiY = 96.0f;
        if (FAILED(d2dCtx_->CreateBitmapFromDxgiSurface(sf.Get(), &bp, &target_))) return false;
        d2dCtx_->SetTarget(target_.Get());
        d2dCtx_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        return true;
    }
    bool ok_ = false;
    UINT w_ = 0, h_ = 0;
    Microsoft::WRL::ComPtr<ID3D11Device> dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDev_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dCtx_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> target_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
};

static Overlay g_overlay;
static HWND g_main = nullptr;
static bool g_dimmed = false, g_hidden = false;
static int g_mode = 0;

enum : int { HK_DIM = 0, HK_HIDE, HK_GUIDE, HK_KILL, HK_MULTI, HK_CODE,
             HK_FULLSTACK, HK_GAME, HK_GHOST_STOP, HK_AUTOTYPE, HK_PAUSE, HK_STOP, HK_COUNT };
static const struct { int id; UINT mod; UINT vk; } kHk[HK_COUNT] = {
    { HK_DIM, MOD_ALT, 'N' }, { HK_HIDE, MOD_ALT, 'D' }, { HK_GUIDE, MOD_ALT, 'I' },
    { HK_KILL, MOD_ALT | MOD_SHIFT, 'X' }, { HK_MULTI, MOD_ALT, 'M' },
    { HK_CODE, MOD_ALT, '8' }, { HK_FULLSTACK, MOD_ALT, '9' }, { HK_GAME, MOD_ALT, '0' },
    { HK_GHOST_STOP, MOD_ALT, 'G' }, { HK_AUTOTYPE, MOD_ALT, 'F' },
    { HK_PAUSE, MOD_ALT, 'R' }, { HK_STOP, MOD_ALT, 'E' },
};

void SetWindows(HWND main, HWND overlay) { g_main = main; if (overlay) g_overlay.Init(overlay); }
void OnSize(UINT w, UINT h) { g_overlay.Resize(w, h); }

bool Dispatch(int id) {
    switch (id) {
    case HK_DIM: g_dimmed = !g_dimmed; StealthDim(g_main, g_dimmed); return true;
    case HK_HIDE: g_hidden = !g_hidden; g_hidden ? StealthHide(g_main) : StealthShow(g_main); return true;
    case HK_GUIDE: ShowKeysGuide(g_main); return true;
    case HK_KILL: EmergencyExit();
    case HK_MULTI: g_mode = 1; return true;
    case HK_CODE: g_mode = 2; return true;
    case HK_FULLSTACK: g_mode = 3; return true;
    case HK_GAME: g_mode = 4; return true;
    case HK_GHOST_STOP: StopTyping(); return true;
    case HK_AUTOTYPE: return true;
    case HK_PAUSE: g_paused.load() ? ResumeTyping() : PauseTyping(); return true;
    case HK_STOP: StopTyping(); return true;
    }
    return false;
}
int Mode() { return g_mode; }
int RegisterHotkeys(HWND hwnd) {
    int fails = 0;
    for (int i = 0; i < HK_COUNT; ++i)
        if (!RegisterHotKey(hwnd, X_HK_BASE + kHk[i].id, kHk[i].mod, kHk[i].vk)) ++fails;
    return fails;
}
void UnregisterHotkeys(HWND hwnd) {
    for (int i = 0; i < HK_COUNT; ++i) UnregisterHotKey(hwnd, X_HK_BASE + kHk[i].id);
}

}
XEOF
echo "extras.cpp written"
wc -l src/extras.h src/extras.cpp
