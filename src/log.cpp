#include "app.h"
#include "redact.h"
#include "util.h"

#include <dbghelp.h>
#include <shlobj.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shell32.lib")

namespace {

constexpr long long kMaxLogBytes = 1024 * 1024;
constexpr int kKeep = 5;

std::mutex g_lock;
FILE* g_file = nullptr;
std::wstring g_path;

std::string Stamp() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return buffer;
}

void Rotate() {
    if (!g_file) return;
    if (_ftelli64(g_file) < kMaxLogBytes) return;
    std::fclose(g_file);
    g_file = nullptr;

    ::DeleteFileW((LogDir() + L"\\crowman." + std::to_wstring(kKeep - 1) + L".log").c_str());
    for (int i = kKeep - 2; i >= 1; --i) {
        ::MoveFileExW((LogDir() + L"\\crowman." + std::to_wstring(i) + L".log").c_str(),
                      (LogDir() + L"\\crowman." + std::to_wstring(i + 1) + L".log").c_str(),
                      MOVEFILE_REPLACE_EXISTING);
    }
    ::MoveFileExW(g_path.c_str(), (LogDir() + L"\\crowman.1.log").c_str(),
                  MOVEFILE_REPLACE_EXISTING);
    g_file = _wfopen(g_path.c_str(), L"ab");
}

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info) {
    const DWORD code =
        info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    const void* address =
        info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;

    LogLine("CRASH: unhandled exception code=0x%08lX at %p", code, address);

    wchar_t stamp[64] = {};
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    wcsftime(stamp, 64, L"crash-%Y%m%d-%H%M%S", &local);

    const std::wstring summary = ConfigDir() + L"\\" + stamp + L".txt";
    if (FILE* text = _wfopen(summary.c_str(), L"wb")) {
        std::fprintf(text,
                     "Crowman Study Assistant %ls\ncode: 0x%08lX\naddress: %p\n"
                     "thread: %lu\nbuild: %s %s\nlog: %s\n",
                     CROWMAN_VERSION, code, address, ::GetCurrentThreadId(), __DATE__,
                     __TIME__, ToUtf8(LogFilePath()).c_str());
        std::fclose(text);
    }

    const std::wstring dump = ConfigDir() + L"\\" + stamp + L".dmp";
    HANDLE file = ::CreateFileW(dump.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = ::GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;
        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file,
                            MiniDumpWithIndirectlyReferencedMemory, &mei, nullptr, nullptr);
        ::CloseHandle(file);
    }

    // Exe parity: tell the user the crash was captured (log + summary + dump).
    wchar_t notice[160] = {};
    swprintf_s(notice, L"CRASH: Unhandled Exception 0x%08X at %p safely logged.",
               (unsigned)code, address);
    ::MessageBoxW(nullptr, notice, CROWMAN_APP_NAME,
                  MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND);

    LogClose();
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

std::wstring LogDir() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(::SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                                  nullptr, 0, path)))
        return ConfigDir();
    const std::wstring base = std::wstring(path) + L"\\CrowmanStudy";
    ::CreateDirectoryW(base.c_str(), nullptr);
    const std::wstring dir = base + L"\\logs";
    ::CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring LogFilePath() { return LogDir() + L"\\crowman.log"; }

void LogInit() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_file) return;
    g_path = LogFilePath();
    g_file = _wfopen(g_path.c_str(), L"ab");
    if (g_file) {
        std::fprintf(g_file, "\n=== %s start %s\n", Stamp().c_str(),
                     ToUtf8(CROWMAN_VERSION).c_str());
        std::fflush(g_file);
    }
}

void LogClose() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void LogLine(const char* fmt, ...) {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_file) return;
    Rotate();
    if (!g_file) return;

    char message[2048] = {};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    std::fprintf(g_file, "%s %s\n", Stamp().c_str(), Redact(message).c_str());
    std::fflush(g_file);
}

void InstallCrashHandler() { ::SetUnhandledExceptionFilter(CrashFilter); }

void RevealLogFolder() {
    ::ShellExecuteW(nullptr, L"open", LogDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
