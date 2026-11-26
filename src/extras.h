// extras.h - parity features. See extras.cpp.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <VersionHelpers.h>
#include <shlwapi.h>
#include <string.h>
#include <string>
#include <vector>

namespace x {

struct EnvironmentReport {
    bool safe_exam_browser, vmware, virtualbox, taskbar, program_manager;
    bool secure_desktop, rdp, debugger, debug_port, debug_object, elevated;
    // Parity probes the reference build reports by name (HWBreakpoint,
    // PEBDebugFlag, KernelDebug, AnalysisTool, VM_Detected).
    bool hw_breakpoint, peb_debug_flag, kernel_debug, analysis_tool, vm_detected;
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
bool AdoptInputDesktop();
void ProbeDebugPorts(EnvironmentReport* r);
size_t WalkStack(std::vector<FrameInfo>* out, size_t max);
void SnapshotThread(HANDLE thread, CONTEXT* out);
bool GhostType(HWND target, const std::wstring& text, const TypeOptions& opt,
               void(*progress)(size_t, size_t, void*) = nullptr, void* user = nullptr);
bool AutoType(HWND target, const std::wstring& text);
void SetTypingTarget(HWND target);
void SetGhostTyping(bool on);
bool GhostTyping();
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
HWND OverlayWindow();
void OverlayFollowTarget(HWND target);
void OnCreate(HWND hwnd);
void OnHotkey(HWND hwnd, int id);
void OnSize(UINT w, UINT h);
void OverlayShow(bool on);
void OverlayText(const std::wstring& text);
void RememberAnswer(const std::wstring& text);
bool Dispatch(int id);
int Mode();
int RegisterHotkeys(HWND hwnd);
void UnregisterHotkeys(HWND hwnd);
static constexpr int X_HK_BASE = 0x5000;

}
