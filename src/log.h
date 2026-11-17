// log.h - rotating file log, crash handler and log folder helper.
#pragma once

#include <string>

void LogInit();
void LogClose();
void LogLine(const char* format, ...);
void InstallCrashHandler();
void RevealLogFolder();
std::wstring LogDir();
std::wstring LogFilePath();
