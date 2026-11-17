#include "redact.h"

#include "util.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

std::mutex g_lock;
std::vector<std::string> g_secrets;

bool TokenChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~' || c == '+' || c == '/' ||
           c == '=';
}

void ScrubPrefix(std::string& text, const std::string& prefix) {
    size_t at = 0;
    while ((at = text.find(prefix, at)) != std::string::npos) {
        size_t end = at + prefix.size();
        while (end < text.size() && TokenChar(text[end])) ++end;
        if (end > at + prefix.size()) {
            text.replace(at + prefix.size(), end - (at + prefix.size()), "***");
            at += prefix.size() + 3;
        } else {
            at += prefix.size();
        }
    }
}

std::string Apply(std::string text) {
    std::vector<std::string> secrets;
    {
        std::lock_guard<std::mutex> guard(g_lock);
        secrets = g_secrets;
    }
    std::sort(secrets.begin(), secrets.end(),
              [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    for (const std::string& secret : secrets) {
        if (secret.size() < 6) continue;
        size_t at = 0;
        while ((at = text.find(secret, at)) != std::string::npos) {
            text.replace(at, secret.size(), "***");
            at += 3;
        }
    }
    ScrubPrefix(text, "Bearer ");
    ScrubPrefix(text, "sk-");
    return text;
}

}  // namespace

void RedactRegister(const std::wstring& secret) {
    const std::string value = ToUtf8(secret);
    if (value.size() < 6) return;
    std::lock_guard<std::mutex> guard(g_lock);
    if (std::find(g_secrets.begin(), g_secrets.end(), value) == g_secrets.end())
        g_secrets.push_back(value);
}

void RedactClear() {
    std::lock_guard<std::mutex> guard(g_lock);
    g_secrets.clear();
}

std::string Redact(const std::string& text) { return Apply(text); }

std::wstring RedactW(const std::wstring& text) { return FromUtf8(Apply(ToUtf8(text))); }
