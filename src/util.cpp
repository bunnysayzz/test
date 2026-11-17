#include "util.h"

#include <bcrypt.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>

#pragma comment(lib, "bcrypt.lib")

std::wstring FromUtf8(const std::string& text) {
    if (text.empty()) return {};
    const int size =
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out((size_t)size, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), size);
    return out;
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out((size_t)size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), size,
                          nullptr, nullptr);
    return out;
}

static const char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);

    size_t i = 0;
    while (i + 2 < data.size()) {
        const uint32_t block = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(kAlphabet[(block >> 18) & 63]);
        out.push_back(kAlphabet[(block >> 12) & 63]);
        out.push_back(kAlphabet[(block >> 6) & 63]);
        out.push_back(kAlphabet[block & 63]);
        i += 3;
    }

    const size_t left = data.size() - i;
    if (left == 1) {
        const uint32_t block = data[i] << 16;
        out.push_back(kAlphabet[(block >> 18) & 63]);
        out.push_back(kAlphabet[(block >> 12) & 63]);
        out += "==";
    } else if (left == 2) {
        const uint32_t block = (data[i] << 16) | (data[i + 1] << 8);
        out.push_back(kAlphabet[(block >> 18) & 63]);
        out.push_back(kAlphabet[(block >> 12) & 63]);
        out.push_back(kAlphabet[(block >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::string Base64Encode(const std::string& data) {
    return Base64Encode(std::vector<uint8_t>(data.begin(), data.end()));
}

std::string Base64Decode(const std::string& text) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    std::string out;
    out.reserve(text.size() / 4 * 3);
    uint32_t acc = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        const int v = value(c);
        if (v < 0) return {};
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((char)((acc >> bits) & 0xFF));
        }
    }
    return out;
}

std::string HexUpper(const std::string& raw) {
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size() * 2);
    for (unsigned char c : raw) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}

std::string Sha256Hex(const std::string& data) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return {};

    DWORD object_size = 0, hash_size = 0, produced = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_size,
                          sizeof(object_size), &produced, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_size,
                          sizeof(hash_size), &produced, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::vector<uint8_t> object(object_size);
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::string out;
    if (BCryptHashData(hash, (PUCHAR)const_cast<char*>(data.data()), (ULONG)data.size(),
                       0) >= 0) {
        std::vector<uint8_t> digest(hash_size);
        if (BCryptFinishHash(hash, digest.data(), hash_size, 0) >= 0)
            out = HexUpper(std::string((char*)digest.data(), digest.size()));
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return out;
}

std::string RandomHex(size_t bytes) {
    std::vector<uint8_t> buffer(bytes);
    if (BCryptGenRandom(nullptr, buffer.data(), (ULONG)buffer.size(),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return {};
    return HexUpper(std::string((char*)buffer.data(), buffer.size()));
}

bool ConstantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

std::string TrimA(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::wstring TrimW(const std::wstring& text) {
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const size_t last = text.find_last_not_of(L" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::wstring LowerW(const std::wstring& text) {
    std::wstring out = text;
    for (wchar_t& c : out) c = (wchar_t)::towlower(c);
    return out;
}

bool ReadWholeFile(const std::wstring& path, std::string& out, size_t cap) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        (size_t)size.QuadPart > cap) {
        ::CloseHandle(file);
        return false;
    }

    out.assign((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    const bool ok = size.QuadPart == 0 ||
                    (::ReadFile(file, out.data(), (DWORD)out.size(), &read, nullptr) &&
                     read == out.size());
    ::CloseHandle(file);
    return ok;
}

bool WriteWholeFile(const std::wstring& path, const std::string& data) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    const bool ok = data.empty() ||
                    (::WriteFile(file, data.data(), (DWORD)data.size(), &written,
                                 nullptr) &&
                     written == data.size());
    ::CloseHandle(file);
    return ok;
}

bool ParseRect(const std::wstring& text, RECT& out) {
    long values[4] = {};
    int found = 0;
    size_t at = 0;

    while (found < 4 && at < text.size()) {
        while (at < text.size() && (text[at] < L'0' || text[at] > L'9') &&
               text[at] != L'-')
            ++at;
        if (at >= text.size()) break;

        wchar_t* end = nullptr;
        values[found] = std::wcstol(text.c_str() + at, &end, 10);
        if (!end || end == text.c_str() + at) break;
        at = (size_t)(end - text.c_str());
        ++found;
    }

    if (found != 4) return false;
    out.left = values[0];
    out.top = values[1];
    out.right = values[2];
    out.bottom = values[3];
    return out.right > out.left && out.bottom > out.top;
}

std::wstring FormatRect(const RECT& rect) {
    return std::to_wstring(rect.left) + L"," + std::to_wstring(rect.top) + L"," +
           std::to_wstring(rect.right) + L"," + std::to_wstring(rect.bottom);
}
