// util.h - conversions, hashing, base64, file IO.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

std::wstring FromUtf8(const std::string& text);
std::string ToUtf8(const std::wstring& text);

std::string Base64Encode(const std::vector<uint8_t>& data);
std::string Base64Encode(const std::string& data);
std::string Base64Decode(const std::string& text);

std::string Sha256Hex(const std::string& data);
std::string HexUpper(const std::string& raw);
std::string RandomHex(size_t bytes);
bool ConstantTimeEquals(const std::string& a, const std::string& b);

std::string TrimA(const std::string& text);
std::wstring TrimW(const std::wstring& text);
std::wstring LowerW(const std::wstring& text);

bool ReadWholeFile(const std::wstring& path, std::string& out,
                   size_t cap = 8u * 1024 * 1024);
bool WriteWholeFile(const std::wstring& path, const std::string& data);

#ifdef _WIN32
bool ParseRect(const std::wstring& text, RECT& out);
std::wstring FormatRect(const RECT& rect);
#endif
