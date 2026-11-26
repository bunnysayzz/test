#include "hotkey_shim.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

namespace {

struct NamedKey {
    const wchar_t* name;
    unsigned vk;
};

const NamedKey kNames[] = {
    {L"f1", VK_F1},   {L"f2", VK_F2},   {L"f3", VK_F3},     {L"f4", VK_F4},
    {L"f5", VK_F5},   {L"f6", VK_F6},   {L"f7", VK_F7},     {L"f8", VK_F8},
    {L"f9", VK_F9},   {L"f10", VK_F10}, {L"f11", VK_F11},   {L"f12", VK_F12},
    {L"space", VK_SPACE}, {L"enter", VK_RETURN}, {L"tab", VK_TAB},
    {L"insert", VK_INSERT}, {L"delete", VK_DELETE}, {L"home", VK_HOME},
    {L"end", VK_END}, {L"pageup", VK_PRIOR}, {L"pagedown", VK_NEXT},
    {L"left", VK_LEFT}, {L"right", VK_RIGHT}, {L"up", VK_UP}, {L"down", VK_DOWN},
    {L"printscreen", VK_SNAPSHOT}, {L"pause", VK_PAUSE},
    {L"`", VK_OEM_3}, {L"-", VK_OEM_MINUS}, {L"=", VK_OEM_PLUS},
    {L"[", VK_OEM_4}, {L"]", VK_OEM_6}, {L"\\", VK_OEM_5}, {L";", VK_OEM_1},
    {L"'", VK_OEM_7}, {L",", VK_OEM_COMMA}, {L".", VK_OEM_PERIOD}, {L"/", VK_OEM_2},
};

std::wstring KeyName(unsigned vk) {
    for (const auto& entry : kNames)
        if (entry.vk == vk) return entry.name;
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)(vk - 'A' + 'a'));
    return L"vk" + std::to_wstring(vk);
}

}  // namespace

std::wstring FormatHotkey(const HotkeySpec& spec) {
    if (!spec.enabled) return L"none";
    std::wstring out;
    if (spec.mods & MOD_CONTROL) out += L"ctrl+";
    if (spec.mods & MOD_ALT) out += L"alt+";
    if (spec.mods & MOD_SHIFT) out += L"shift+";
    if (spec.mods & MOD_WIN) out += L"win+";
    out += KeyName(spec.vk);
    return out;
}

bool ParseHotkey(const std::wstring& text, HotkeySpec& out) {
    std::wstring lowered;
    for (wchar_t c : text)
        if (c != L' ') lowered.push_back((wchar_t)::towlower(c));

    if (lowered.empty() || lowered == L"none" || lowered == L"off") {
        out.enabled = false;
        return true;
    }

    HotkeySpec spec;
    spec.mods = 0;
    spec.vk = 0;
    size_t start = 0;
    for (;;) {
        const size_t plus = lowered.find(L'+', start);
        const std::wstring token = lowered.substr(
            start, plus == std::wstring::npos ? std::wstring::npos : plus - start);

        if (token == L"ctrl" || token == L"control") spec.mods |= MOD_CONTROL;
        else if (token == L"alt") spec.mods |= MOD_ALT;
        else if (token == L"shift") spec.mods |= MOD_SHIFT;
        else if (token == L"win") spec.mods |= MOD_WIN;
        else if (token.size() == 1 && token[0] >= L'a' && token[0] <= L'z')
            spec.vk = (unsigned)(token[0] - L'a' + 'A');
        else if (token.size() == 1 && token[0] >= L'0' && token[0] <= L'9')
            spec.vk = (unsigned)token[0];
        else {
            bool found = false;
            for (const auto& entry : kNames)
                if (token == entry.name) {
                    spec.vk = entry.vk;
                    found = true;
                    break;
                }
            if (!found) {
                if (token.compare(0, 2, L"vk") == 0)
                    spec.vk = (unsigned)std::wcstol(token.c_str() + 2, nullptr, 10);
                if (!spec.vk) return false;
            }
        }

        if (plus == std::wstring::npos) break;
        start = plus + 1;
    }

    if (!spec.vk) return false;
    if (!spec.mods) spec.mods = MOD_CONTROL | MOD_ALT;
    out = spec;
    return true;
}
