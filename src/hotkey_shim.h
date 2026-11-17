#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#define MOD_ALT 0x0001
#define MOD_CONTROL 0x0002
#define MOD_SHIFT 0x0004
#define MOD_WIN 0x0008
#define VK_SPACE 0x20
#define VK_RETURN 0x0D
#define VK_TAB 0x09
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HOME 0x24
#define VK_END 0x23
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_LEFT 0x25
#define VK_RIGHT 0x27
#define VK_UP 0x26
#define VK_DOWN 0x28
#define VK_SNAPSHOT 0x2C
#define VK_PAUSE 0x13
#define VK_OEM_3 0xC0
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PLUS 0xBB
#define VK_OEM_4 0xDB
#define VK_OEM_6 0xDD
#define VK_OEM_5 0xDC
#define VK_OEM_1 0xBA
#define VK_OEM_7 0xDE
#define VK_OEM_COMMA 0xBC
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#endif

struct HotkeySpec {
    unsigned mods = MOD_CONTROL | MOD_ALT;
    unsigned vk = 'S';
    bool enabled = true;
};

std::wstring FormatHotkey(const HotkeySpec& spec);
bool ParseHotkey(const std::wstring& text, HotkeySpec& out);
