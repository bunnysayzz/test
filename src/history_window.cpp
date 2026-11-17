#include "app.h"
#include "util.h"

#include <commctrl.h>

#include <ctime>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kClass[] = L"CrowmanHistoryWindow";
constexpr int ID_SEARCH = 4000, ID_LIST = 4001, ID_TEXT = 4002, ID_USE = 4003,
              ID_CLEAR = 4004;

struct State {
    std::vector<HistoryEntry> entries;
    HWND owner = nullptr;
    int limit = 20;
};

std::wstring Stamp(long long ts) {
    std::time_t when = (std::time_t)ts;
    std::tm local{};
    localtime_s(&local, &when);
    wchar_t buffer[32] = {};
    wcsftime(buffer, 32, L"%Y-%m-%d %H:%M", &local);
    return buffer;
}

std::wstring Text(HWND control) {
    const int length = ::GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring buffer((size_t)length + 1, L'\0');
    ::GetWindowTextW(control, buffer.data(), length + 1);
    buffer.resize((size_t)length);
    return buffer;
}

void Refresh(State* state, HWND hwnd) {
    HWND list = ::GetDlgItem(hwnd, ID_LIST);
    ::SendMessageW(list, LB_RESETCONTENT, 0, 0);
    state->entries.clear();
    HistoryLoad(state->entries);

    const std::wstring needle = LowerW(Text(::GetDlgItem(hwnd, ID_SEARCH)));
    for (const HistoryEntry& entry : state->entries) {
        const std::wstring body = FromUtf8(entry.text);
        if (!needle.empty() &&
            LowerW(body).find(needle) == std::wstring::npos &&
            LowerW(Stamp(entry.ts)).find(needle) == std::wstring::npos)
            continue;

        std::wstring preview = body;
        for (wchar_t& c : preview)
            if (c == L'\r' || c == L'\n') c = L' ';
        if (preview.size() > 90) preview = preview.substr(0, 90) + L"...";

        const std::wstring line = Stamp(entry.ts) + L" " + preview;
        ::SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
    ::SetWindowTextW(::GetDlgItem(hwnd, ID_TEXT), L"");
}

LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = (State*)::GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_CREATE: {
            auto* created = new State();
            created->owner = ::GetParent(hwnd);
            created->limit = *(int*)((CREATESTRUCTW*)lparam)->lpCreateParams;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)created);

            ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, hwnd,
                              (HMENU)ID_SEARCH, ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 0, 0, 10,
                              10, hwnd, (HMENU)ID_LIST, ::GetModuleHandleW(nullptr),
                              nullptr);
            ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                  ES_READONLY,
                              0, 0, 10, 10, hwnd, (HMENU)ID_TEXT,
                              ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnLoadIntoAnswer),
                              WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd, (HMENU)ID_USE,
                              ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(0, L"BUTTON", S(Str::BtnClearHistory),
                              WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd, (HMENU)ID_CLEAR,
                              ::GetModuleHandleW(nullptr), nullptr);

            Refresh(created, hwnd);
            return 0;
        }

        case WM_SIZE: {
            if (!state) return 0;
            RECT rc{};
            ::GetClientRect(hwnd, &rc);
            const int pad = 12, by = rc.bottom - pad - 32;
            const int list_h = (by - pad * 3 - 28) / 2;
            ::MoveWindow(::GetDlgItem(hwnd, ID_SEARCH), pad, pad, rc.right - pad * 2, 24,
                         TRUE);
            ::MoveWindow(::GetDlgItem(hwnd, ID_LIST), pad, pad + 32, rc.right - pad * 2,
                         list_h, TRUE);
            ::MoveWindow(::GetDlgItem(hwnd, ID_TEXT), pad, pad * 2 + 32 + list_h,
                         rc.right - pad * 2, list_h, TRUE);
            ::MoveWindow(::GetDlgItem(hwnd, ID_USE), rc.right - pad - 320, by, 160, 32,
                         TRUE);
            ::MoveWindow(::GetDlgItem(hwnd, ID_CLEAR), rc.right - pad - 150, by, 150, 32,
                         TRUE);
            return 0;
        }

        case WM_COMMAND: {
            if (!state) break;
            const int id = LOWORD(wparam);

            if (id == ID_SEARCH && HIWORD(wparam) == EN_CHANGE) {
                Refresh(state, hwnd);
                return 0;
            }

            if (id == ID_LIST && HIWORD(wparam) == LBN_SELCHANGE) {
                const LRESULT index =
                    ::SendMessageW(::GetDlgItem(hwnd, ID_LIST), LB_GETCURSEL, 0, 0);
                if (index >= 0 && (size_t)index < state->entries.size()) {
                    const std::wstring text =
                        FromUtf8(state->entries[(size_t)index].text);
                    ::SetWindowTextW(::GetDlgItem(hwnd, ID_TEXT), text.c_str());
                }
                return 0;
            }

            if (id == ID_USE || (id == ID_LIST && HIWORD(wparam) == LBN_DBLCLK)) {
                const LRESULT index =
                    ::SendMessageW(::GetDlgItem(hwnd, ID_LIST), LB_GETCURSEL, 0, 0);
                if (index >= 0 && (size_t)index < state->entries.size()) {
                    auto* outcome = new JobOutcome();
                    outcome->ok = true;
                    outcome->text = FromUtf8(state->entries[(size_t)index].text);
                    outcome->status = S(Str::StatusLoadedFromHistory);
                    ::PostMessageW(state->owner, WM_JOB_DONE, 0, (LPARAM)outcome);
                    ::DestroyWindow(hwnd);
                }
                return 0;
            }

            if (id == ID_CLEAR) {
                HistoryClear();
                Refresh(state, hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            ::DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (state) {
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete state;
            }
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

void ShowHistoryWindow(HWND owner, int limit) {
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

    int copy = limit;
    RECT bounds{};
    ::GetWindowRect(owner, &bounds);
    HWND window = ::CreateWindowExW(WS_EX_DLGMODALFRAME, kClass, S(Str::TitleHistory),
                                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                    bounds.left + 60, bounds.top + 60, 720, 640, owner,
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
