#pragma once

#include <string>

void RedactRegister(const std::wstring& secret);
void RedactClear();
std::string Redact(const std::string& text);
std::wstring RedactW(const std::wstring& text);
