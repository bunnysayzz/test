#include "../src/hotkey_shim.h"

#include <cstdio>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

int main() {
    HotkeySpec spec;
    Check(ParseHotkey(L"ctrl+alt+s", spec), "parses ctrl+alt+s");
    Check(spec.vk == 'S' && (spec.mods & MOD_CONTROL) && (spec.mods & MOD_ALT),
          "modifiers correct");
    Check(FormatHotkey(spec) == L"ctrl+alt+s", "round trips");
    Check(ParseHotkey(L"none", spec) && !spec.enabled, "none disables");
    Check(ParseHotkey(L"ctrl+shift+f9", spec) && spec.vk == VK_F9, "parses function key");
    Check(!ParseHotkey(L"ctrl+", spec), "rejects trailing plus");
    Check(!ParseHotkey(L"ctrl+bogus", spec), "rejects unknown key");
    Check(ParseHotkey(L"alt+`", spec) && spec.vk == VK_OEM_3, "parses OEM key");

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
