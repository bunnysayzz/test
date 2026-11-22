#include "app.h"
#include "util.h"

#include <commctrl.h>
#include <cstdlib>
#include <string>

namespace {

constexpr wchar_t kClass[] = L"CrowmanSettingsWindow";

enum : int {
    ID_MODE = 3001,
    ID_ENDPOINT,
    ID_KEY,
    ID_MODEL,
    ID_CAPMODE,
    ID_MAXW,
    ID_MAXH,
    ID_TIMEOUT,
    ID_RETRIES,
    ID_GRACE,
    ID_STREAM,
    ID_CURSOR,
    ID_HIDE,
    ID_TRAY,
    ID_MINIMIZE,
    ID_REUSECROP,
    ID_PROXY,
    ID_PROXYPORT,
    ID_HK_CAPTURE,
    ID_HK_CAPTURE_SET,
    ID_HK_COPY,
    ID_HK_COPY_SET,
    ID_HK_TOGGLE,
    ID_HK_TOGGLE_SET,
    ID_LANG,
    ID_SAVE,
    ID_CANCEL,
    ID_OPENINI,
};

struct Draft {
    Config config;
    Secrets secrets;
    HWND owner = nullptr;
    HotkeySpec* arming = nullptr;
};

std::wstring Text(HWND control) {
    const int length = ::GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring buffer((size_t)length + 1, L'\0');
    ::GetWindowTextW(control, buffer.data(), length + 1);
    buffer.resize((size_t)length);
    return buffer;
}

void SetText(HWND control, const std::wstring& text) {
    ::SetWindowTextW(control, text.c_str());
}

HWND Label(HWND parent, const wchar_t* text, int x, int y, int w) {
    return ::CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, 18,
                             parent, nullptr, ::GetModuleHandleW(nullptr), nullptr);
}

HWND Edit(HWND parent, int id, int x, int y, int w, DWORD style) {
    return ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | style, x,
                             y, w, 24, parent, (HMENU)id, ::GetModuleHandleW(nullptr),
                             nullptr);
}

HWND Check(HWND parent, int id, const wchar_t* text, int x, int y, int w) {
    return ::CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                             x, y, w, 22, parent, (HMENU)id, ::GetModuleHandleW(nullptr),
                             nullptr);
}

void SyncHotkeys(Draft* draft, HWND parent) {
    SetText(::GetDlgItem(parent, ID_HK_CAPTURE), FormatHotkey(draft->config.hotkey_capture));
    SetText(::GetDlgItem(parent, ID_HK_COPY), FormatHotkey(draft->config.hotkey_copy));
    SetText(::GetDlgItem(parent, ID_HK_TOGGLE), FormatHotkey(draft->config.hotkey_toggle));
}

void Fill(Draft* draft, HWND parent) {
    const Config& cfg = draft->config;
    SetText(::GetDlgItem(parent, ID_ENDPOINT), cfg.endpoint);
    SetText(::GetDlgItem(parent, ID_KEY),
            cfg.mode == L"broker" ? draft->secrets.license_key : draft->secrets.api_key);
    SetText(::GetDlgItem(parent, ID_MODEL), cfg.model);
    SetText(::GetDlgItem(parent, ID_MAXW), std::to_wstring(cfg.max_width_px));
    SetText(::GetDlgItem(parent, ID_MAXH), std::to_wstring(cfg.max_height_px));
    SetText(::GetDlgItem(parent, ID_TIMEOUT), std::to_wstring(cfg.timeout_ms));
    SetText(::GetDlgItem(parent, ID_RETRIES), std::to_wstring(cfg.max_retries));
    SetText(::GetDlgItem(parent, ID_GRACE), std::to_wstring(cfg.offline_grace_hours));
    SetText(::GetDlgItem(parent, ID_PROXY), cfg.proxy_host);
    SetText(::GetDlgItem(parent, ID_PROXYPORT), std::to_wstring(cfg.proxy_port));

    HWND mode = ::GetDlgItem(parent, ID_MODE);
    ::SendMessageW(mode, CB_ADDSTRING, 0, (LPARAM)L"openai");
    ::SendMessageW(mode, CB_ADDSTRING, 0, (LPARAM)L"broker");
    ::SendMessageW(mode, CB_ADDSTRING, 0, (LPARAM)L"parity");
    ::SendMessageW(mode, CB_SETCURSEL,
                   cfg.mode == L"broker" ? 1 : cfg.mode == L"parity" ? 2 : 0, 0);

    HWND capmode = ::GetDlgItem(parent, ID_CAPMODE);
    const Str labels[] = {Str::ModeDesktop, Str::ModeMonitor, Str::ModeWindow, Str::ModePick,
                          Str::ModeRegion};
    for (Str label : labels) ::SendMessageW(capmode, CB_ADDSTRING, 0, (LPARAM)S(label));
    ::SendMessageW(capmode, CB_SETCURSEL, (WPARAM)cfg.capture_mode, 0);

    HWND lang = ::GetDlgItem(parent, ID_LANG);
    const wchar_t* codes[] = {L"en", L"de", L"es"};
    for (const wchar_t* code : codes)
        ::SendMessageW(lang, CB_ADDSTRING, 0, (LPARAM)code);
    const int selected = cfg.language == L"de" ? 1 : cfg.language == L"es" ? 2 : 0;
    ::SendMessageW(lang, CB_SETCURSEL, (WPARAM)selected, 0);

    ::SendMessageW(::GetDlgItem(parent, ID_STREAM), BM_SETCHECK, cfg.stream ? 1 : 0, 0);
    ::SendMessageW(::GetDlgItem(parent, ID_CURSOR), BM_SETCHECK, cfg.capture_cursor ? 1 : 0,
                   0);
    ::SendMessageW(::GetDlgItem(parent, ID_HIDE), BM_SETCHECK,
                   cfg.hide_while_capturing ? 1 : 0, 0);
    ::SendMessageW(::GetDlgItem(parent, ID_TRAY), BM_SETCHECK, cfg.tray_icon ? 1 : 0, 0);
    ::SendMessageW(::GetDlgItem(parent, ID_MINIMIZE), BM_SETCHECK,
                   cfg.minimize_to_tray ? 1 : 0, 0);
    ::SendMessageW(::GetDlgItem(parent, ID_REUSECROP), BM_SETCHECK,
                   cfg.reuse_crop ? 1 : 0, 0);

    SyncHotkeys(draft, parent);
}

bool Collect(Draft* draft, HWND parent, std::wstring& error) {
    Config& cfg = draft->config;

    const LRESULT mode_sel =
        ::SendMessageW(::GetDlgItem(parent, ID_MODE), CB_GETCURSEL, 0, 0);
    cfg.mode = mode_sel == 1 ? L"broker" : mode_sel == 2 ? L"parity" : L"openai";
    cfg.endpoint = TrimW(Text(::GetDlgItem(parent, ID_ENDPOINT)));
    cfg.model = TrimW(Text(::GetDlgItem(parent, ID_MODEL)));
    cfg.proxy_host = TrimW(Text(::GetDlgItem(parent, ID_PROXY)));
    cfg.proxy_port = _wtoi(Text(::GetDlgItem(parent, ID_PROXYPORT)).c_str());
    cfg.max_width_px = _wtoi(Text(::GetDlgItem(parent, ID_MAXW)).c_str());
    cfg.max_height_px = _wtoi(Text(::GetDlgItem(parent, ID_MAXH)).c_str());
    cfg.timeout_ms = _wtoi(Text(::GetDlgItem(parent, ID_TIMEOUT)).c_str());
    cfg.max_retries = _wtoi(Text(::GetDlgItem(parent, ID_RETRIES)).c_str());
    cfg.offline_grace_hours = _wtoi(Text(::GetDlgItem(parent, ID_GRACE)).c_str());

    const LRESULT capmode =
        ::SendMessageW(::GetDlgItem(parent, ID_CAPMODE), CB_GETCURSEL, 0, 0);
    if (capmode >= 0 && capmode <= 4) cfg.capture_mode = (CaptureMode)capmode;

    const LRESULT lang = ::SendMessageW(::GetDlgItem(parent, ID_LANG), CB_GETCURSEL, 0, 0);
    cfg.language = lang == 1 ? L"de" : lang == 2 ? L"es" : L"en";

    cfg.stream = ::SendMessageW(::GetDlgItem(parent, ID_STREAM), BM_GETCHECK, 0, 0) == 1;
    cfg.capture_cursor =
        ::SendMessageW(::GetDlgItem(parent, ID_CURSOR), BM_GETCHECK, 0, 0) == 1;
    cfg.hide_while_capturing =
        ::SendMessageW(::GetDlgItem(parent, ID_HIDE), BM_GETCHECK, 0, 0) == 1;
    cfg.tray_icon = ::SendMessageW(::GetDlgItem(parent, ID_TRAY), BM_GETCHECK, 0, 0) == 1;
    cfg.minimize_to_tray =
        ::SendMessageW(::GetDlgItem(parent, ID_MINIMIZE), BM_GETCHECK, 0, 0) == 1;
    cfg.reuse_crop =
        ::SendMessageW(::GetDlgItem(parent, ID_REUSECROP), BM_GETCHECK, 0, 0) == 1;

    const std::wstring key = TrimW(Text(::GetDlgItem(parent, ID_KEY)));
    if (cfg.mode == L"broker") draft->secrets.license_key = key;
    else draft->secrets.api_key = key;

    // Parity mode supplies its own endpoint + token, so none is required here.
    if (cfg.mode != L"parity") {
        if (cfg.endpoint.empty()) {
            error = S(Str::ErrEndpointRequired);
            return false;
        }
        if (cfg.endpoint.compare(0, 8, L"https://") != 0 &&
            cfg.endpoint.compare(0, 7, L"http://") != 0) {
            error = S(Str::ErrEndpointScheme);
            return false;
        }
    }
    if (cfg.timeout_ms < 5000) cfg.timeout_ms = 5000;
    if (cfg.max_retries < 0) cfg.max_retries = 0;
    if (cfg.max_retries > 5) cfg.max_retries = 5;
    if (FormatHotkey(cfg.hotkey_capture) == FormatHotkey(cfg.hotkey_copy) ||
        FormatHotkey(cfg.hotkey_capture) == FormatHotkey(cfg.hotkey_toggle) ||
        FormatHotkey(cfg.hotkey_copy) == FormatHotkey(cfg.hotkey_toggle)) {
        error = S(Str::ErrHotkeyTaken);
        return false;
    }
    for (const HotkeySpec* spec :
         {&cfg.hotkey_capture, &cfg.hotkey_copy, &cfg.hotkey_toggle}) {
        if (!HotkeyAvailable(*spec)) {
            error = S(Str::ErrHotkeyTaken);
            return false;
        }
    }
    return true;
}

LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* draft = (Draft*)::GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_CREATE: {
            auto* created = new Draft();
            created->config = *(Config*)((CREATESTRUCTW*)lparam)->lpCreateParams;
            created->secrets = CurrentSecrets();
            created->owner = ::GetParent(hwnd);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)created);

            const int left = 16, field = 150, width = 420;
            int y = 14;
            Label(hwnd, S(Str::LblTransport), left, y, field);
            ::CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                              left + field, y - 3, 150, 200, hwnd, (HMENU)ID_MODE,
                              ::GetModuleHandleW(nullptr), nullptr);
            y += 30;
            Label(hwnd, S(Str::LblEndpoint), left, y, field);
            Edit(hwnd, ID_ENDPOINT, left + field, y - 3, width, 0);
            y += 30;
            Label(hwnd, S(Str::LblApiKey), left, y, field);
            Edit(hwnd, ID_KEY, left + field, y - 3, width, ES_PASSWORD);
            y += 30;
            Label(hwnd, S(Str::LblModel), left, y, field);
            Edit(hwnd, ID_MODEL, left + field, y - 3, width, 0);
            y += 30;
            Label(hwnd, S(Str::LblCaptureMode), left, y, field);
            ::CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                              left + field, y - 3, 200, 200, hwnd, (HMENU)ID_CAPMODE,
                              ::GetModuleHandleW(nullptr), nullptr);
            y += 32;
            Label(hwnd, S(Str::LblMaxSize), left, y, field);
            Edit(hwnd, ID_MAXW, left + field, y - 3, 70, 0);
            Edit(hwnd, ID_MAXH, left + field + 78, y - 3, 70, 0);
            y += 30;
            Label(hwnd, S(Str::LblTimeout), left, y, field);
            Edit(hwnd, ID_TIMEOUT, left + field, y - 3, 90, 0);
            Label(hwnd, S(Str::LblRetries), left + field + 100, y, 50);
            Edit(hwnd, ID_RETRIES, left + field + 155, y - 3, 50, 0);
            y += 30;
            Label(hwnd, S(Str::LblGrace), left, y, field);
            Edit(hwnd, ID_GRACE, left + field, y - 3, 70, 0);
            y += 30;
            Label(hwnd, S(Str::LblProxy), left, y, field);
            Edit(hwnd, ID_PROXY, left + field, y - 3, 200, 0);
            Label(hwnd, S(Str::LblProxyPort), left + field + 208, y, 40);
            Edit(hwnd, ID_PROXYPORT, left + field + 250, y - 3, 60, 0);
            y += 32;
            Check(hwnd, ID_STREAM, S(Str::ChkStream), left, y, 200);
            Check(hwnd, ID_HIDE, S(Str::ChkHide), left + 210, y, 240);
            y += 26;
            Check(hwnd, ID_CURSOR, S(Str::ChkCursor), left, y, 200);
            Check(hwnd, ID_TRAY, S(Str::ChkTray), left + 210, y, 200);
            y += 26;
            Check(hwnd, ID_MINIMIZE, S(Str::ChkMinimize), left, y, 200);
            Check(hwnd, ID_REUSECROP, S(Str::ChkReuseCrop), left + 210, y, 240);
            y += 30;
            Label(hwnd, S(Str::LblHotkeys), left, y, field);
            Edit(hwnd, ID_HK_CAPTURE, left + field, y - 3, 110, ES_READONLY);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnSet), WS_CHILD | WS_VISIBLE,
                              left + field + 116, y - 3, 40, 24, hwnd,
                              (HMENU)ID_HK_CAPTURE_SET, ::GetModuleHandleW(nullptr),
                              nullptr);
            Edit(hwnd, ID_HK_COPY, left + field + 164, y - 3, 110, ES_READONLY);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnSet), WS_CHILD | WS_VISIBLE,
                              left + field + 280, y - 3, 40, 24, hwnd,
                              (HMENU)ID_HK_COPY_SET, ::GetModuleHandleW(nullptr), nullptr);
            y += 30;
            Edit(hwnd, ID_HK_TOGGLE, left + field, y - 3, 110, ES_READONLY);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnSet), WS_CHILD | WS_VISIBLE,
                              left + field + 116, y - 3, 40, 24, hwnd,
                              (HMENU)ID_HK_TOGGLE_SET, ::GetModuleHandleW(nullptr),
                              nullptr);
            Label(hwnd, S(Str::LblShowHide), left + field + 164, y, 100);
            y += 34;
            ::CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                              left, y, 80, 120, hwnd, (HMENU)ID_LANG,
                              ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnOpenIni), WS_CHILD | WS_VISIBLE,
                              left + 90, y, 130, 30, hwnd, (HMENU)ID_OPENINI,
                              ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnSave),
                              WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, left + width - 90,
                              y, 110, 30, hwnd, (HMENU)ID_SAVE, ::GetModuleHandleW(nullptr),
                              nullptr);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnCancel), WS_CHILD | WS_VISIBLE,
                              left + width + 10, y, 90, 30, hwnd, (HMENU)ID_CANCEL,
                              ::GetModuleHandleW(nullptr), nullptr);

            Fill(created, hwnd);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (!draft || !draft->arming) break;
            const UINT vk = (UINT)wparam;
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN ||
                vk == VK_RWIN)
                return 0;
            if (vk == VK_ESCAPE) {
                draft->arming = nullptr;
                SyncHotkeys(draft, hwnd);
                return 0;
            }
            HotkeySpec spec;
            spec.vk = vk;
            spec.mods = 0;
            if (::GetKeyState(VK_CONTROL) & 0x8000) spec.mods |= MOD_CONTROL;
            if (::GetKeyState(VK_MENU) & 0x8000) spec.mods |= MOD_ALT;
            if (::GetKeyState(VK_SHIFT) & 0x8000) spec.mods |= MOD_SHIFT;
            if ((::GetKeyState(VK_LWIN) | ::GetKeyState(VK_RWIN)) & 0x8000)
                spec.mods |= MOD_WIN;
            if (!spec.mods) spec.mods = MOD_CONTROL | MOD_ALT;
            *draft->arming = spec;
            draft->arming = nullptr;
            SyncHotkeys(draft, hwnd);
            return 0;
        }

        case WM_COMMAND: {
            if (!draft) break;
            const int id = LOWORD(wparam);
            if (id == ID_HK_CAPTURE_SET || id == ID_HK_COPY_SET || id == ID_HK_TOGGLE_SET) {
                draft->arming = id == ID_HK_CAPTURE_SET
                                    ? &draft->config.hotkey_capture
                                    : id == ID_HK_COPY_SET ? &draft->config.hotkey_copy
                                                           : &draft->config.hotkey_toggle;
                // Keys go to the focused control, so pull focus onto the dialog
                // itself or the next keypress never reaches WM_KEYDOWN below.
                ::SetFocus(hwnd);
                return 0;
            }
            if (id == ID_OPENINI) {
                OpenConfigInEditor();
                return 0;
            }
            if (id == ID_SAVE) {
                std::wstring error;
                if (!Collect(draft, hwnd, error)) {
                    ::MessageBoxW(hwnd, error.c_str(), S(Str::TitleSettings),
                                  MB_OK | MB_ICONWARNING);
                    return 0;
                }
                SaveSecrets(draft->secrets);
                SaveConfig(draft->config);
                ::PostMessageW(draft->owner, WM_APP_RELOAD, 0, 0);
                ::DestroyWindow(hwnd);
                return 0;
            }
            if (id == ID_CANCEL) {
                ::DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (draft) {
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete draft;
            }
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

void ShowSettingsWindow(HWND owner, const Config& current) {
    static bool registered = false;
    if (!registered) {
        registered = true;
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = Proc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = kClass;
        ::RegisterClassExW(&wc);
    }

    Config copy = current;
    RECT bounds{};
    ::GetWindowRect(owner, &bounds);
    HWND window = ::CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, S(Str::TitleSettings),
                                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                    bounds.left + 40, bounds.top + 40, 660, 620, owner,
                                    nullptr, ::GetModuleHandleW(nullptr), &copy);
    if (!window) return;

    ::EnableWindow(owner, FALSE);
    MSG message{};
    while (::IsWindow(window)) {
        if (::GetMessageW(&message, nullptr, 0, 0) <= 0) break;
        if (!::IsDialogMessageW(window, &message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }
    ::EnableWindow(owner, TRUE);
    ::SetForegroundWindow(owner);
}
