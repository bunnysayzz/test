#include "app.h"
#include "util.h"

#include <algorithm>
#include <vector>

namespace {

constexpr wchar_t kPickClass[] = L"CrowmanPickWindow";
constexpr wchar_t kRegionClass[] = L"CrowmanRegionOverlay";
constexpr int ID_LIST = 2001, ID_OK = 2002, ID_CANCEL = 2003;

struct PickState {
    HWND list = nullptr;
    std::vector<HWND> windows;
    HWND chosen = nullptr;
};

struct RegionState {
    bool dragging = false;
    POINT origin{}, current{};
    RECT virtual_rect{}, result{};
    HBITMAP canvas = nullptr;
    void* bits = nullptr;
    int w = 0, h = 0;
};

std::wstring ProcessName(HWND window) {
    DWORD pid = 0;
    ::GetWindowThreadProcessId(window, &pid);
    if (!pid) return L"?";
    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return L"?";
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::wstring name = L"?";
    if (::QueryFullProcessImageNameW(process, 0, path, &size)) {
        const wchar_t* slash = wcsrchr(path, L'\\');
        name = slash ? slash + 1 : path;
    }
    ::CloseHandle(process);
    return name;
}

BOOL CALLBACK EnumProc(HWND window, LPARAM param) {
    auto* state = (PickState*)param;
    if (!::IsWindowVisible(window)) return TRUE;
    if (::GetWindow(window, GW_OWNER)) return TRUE;
    if (::GetWindowLongW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;

    wchar_t title[256] = {};
    if (::GetWindowTextW(window, title, 256) <= 0) return TRUE;

    RECT bounds{};
    if (!::GetWindowRect(window, &bounds)) return TRUE;
    if (bounds.right - bounds.left < 40 || bounds.bottom - bounds.top < 30) return TRUE;

    state->windows.push_back(window);
    std::wstring label = title;
    label += L" [";
    label += ProcessName(window);
    label += L"]";
    ::SendMessageW(state->list, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    return TRUE;
}

void Paint(RegionState* state, HWND hwnd) {
    if (!state->bits) return;
    auto* pixels = (uint32_t*)state->bits;
    const size_t count = (size_t)state->w * state->h;
    for (size_t i = 0; i < count; ++i) pixels[i] = 0x6E000000;

    if (state->dragging) {
        RECT sel{};
        sel.left = std::max<LONG>(
            std::min(state->origin.x, state->current.x) - state->virtual_rect.left, 0);
        sel.top = std::max<LONG>(
            std::min(state->origin.y, state->current.y) - state->virtual_rect.top, 0);
        sel.right = std::min<LONG>(
            std::max(state->origin.x, state->current.x) - state->virtual_rect.left,
            state->w);
        sel.bottom = std::min<LONG>(
            std::max(state->origin.y, state->current.y) - state->virtual_rect.top,
            state->h);

        for (LONG y = sel.top; y < sel.bottom; ++y) {
            uint32_t* row = pixels + (size_t)y * state->w;
            for (LONG x = sel.left; x < sel.right; ++x) row[x] = 0;
        }

        const uint32_t edge = 0xFFFFFFFF;
        for (LONG x = sel.left; x < sel.right; ++x) {
            pixels[(size_t)sel.top * state->w + x] = edge;
            pixels[(size_t)(sel.bottom - 1) * state->w + x] = edge;
        }
        for (LONG y = sel.top; y < sel.bottom; ++y) {
            pixels[(size_t)y * state->w + sel.left] = edge;
            pixels[(size_t)y * state->w + sel.right - 1] = edge;
        }
    }

    HDC screen = ::GetDC(nullptr);
    HDC mem = ::CreateCompatibleDC(screen);
    HGDIOBJ previous = ::SelectObject(mem, state->canvas);
    POINT destination{state->virtual_rect.left, state->virtual_rect.top};
    POINT source{0, 0};
    SIZE size{state->w, state->h};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    ::UpdateLayeredWindow(hwnd, screen, &destination, &size, mem, &source, 0, &blend,
                          ULW_ALPHA);
    ::SelectObject(mem, previous);
    ::DeleteDC(mem);
    ::ReleaseDC(nullptr, screen);
}

LRESULT CALLBACK PickProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = (PickState*)::GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_CREATE: {
            auto* created = (PickState*)((CREATESTRUCTW*)lparam)->lpCreateParams;
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)created);
            created->list = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                              WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                  LBS_NOTIFY,
                                              0, 0, 10, 10, hwnd, (HMENU)ID_LIST,
                                              ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(0, L"BUTTON", L"Capture",
                              WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10,
                              hwnd, (HMENU)ID_OK, ::GetModuleHandleW(nullptr), nullptr);
            ::CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 0, 0, 10,
                              10, hwnd, (HMENU)ID_CANCEL, ::GetModuleHandleW(nullptr),
                              nullptr);
            ::EnumWindows(EnumProc, (LPARAM)created);
            if (!created->windows.empty()) ::SendMessageW(created->list, LB_SETCURSEL, 0, 0);
            return 0;
        }

        case WM_SIZE: {
            if (!state) return 0;
            RECT rc{};
            ::GetClientRect(hwnd, &rc);
            const int pad = 12, by = rc.bottom - pad - 32;
            ::MoveWindow(state->list, pad, pad, rc.right - pad * 2, by - pad * 2, TRUE);
            ::MoveWindow(::GetDlgItem(hwnd, ID_OK), rc.right - pad - 220, by, 100, 32,
                         TRUE);
            ::MoveWindow(::GetDlgItem(hwnd, ID_CANCEL), rc.right - pad - 110, by, 100, 32,
                         TRUE);
            return 0;
        }

        case WM_COMMAND: {
            if (!state) break;
            const int id = LOWORD(wparam);
            if (id == ID_OK || (id == ID_LIST && HIWORD(wparam) == LBN_DBLCLK)) {
                const LRESULT index = ::SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                if (index >= 0 && (size_t)index < state->windows.size())
                    state->chosen = state->windows[(size_t)index];
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

        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK RegionProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = (RegionState*)::GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (message) {
        case WM_CREATE: {
            auto* created = (RegionState*)((CREATESTRUCTW*)lparam)->lpCreateParams;
            created->virtual_rect = VirtualScreenRect();
            created->w = created->virtual_rect.right - created->virtual_rect.left;
            created->h = created->virtual_rect.bottom - created->virtual_rect.top;

            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = created->w;
            info.bmiHeader.biHeight = -created->h;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            HDC screen = ::GetDC(nullptr);
            created->canvas = ::CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                                                 &created->bits, nullptr, 0);
            ::ReleaseDC(nullptr, screen);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)created);
            Paint(created, hwnd);
            return 0;
        }

        case WM_SETCURSOR:
            ::SetCursor(::LoadCursorW(nullptr, IDC_CROSS));
            return TRUE;

        case WM_LBUTTONDOWN:
            if (state) {
                state->dragging = true;
                ::GetCursorPos(&state->origin);
                state->current = state->origin;
                ::SetCapture(hwnd);
                Paint(state, hwnd);
            }
            return 0;

        case WM_MOUSEMOVE:
            if (state && state->dragging) {
                ::GetCursorPos(&state->current);
                Paint(state, hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (state && state->dragging) {
                state->dragging = false;
                ::ReleaseCapture();
                ::GetCursorPos(&state->current);
                RECT r{};
                r.left = std::min(state->origin.x, state->current.x);
                r.top = std::min(state->origin.y, state->current.y);
                r.right = std::max(state->origin.x, state->current.x);
                r.bottom = std::max(state->origin.y, state->current.y);
                if (r.right - r.left >= 8 && r.bottom - r.top >= 8) state->result = r;
                ::DestroyWindow(hwnd);
            }
            return 0;

        case WM_RBUTTONUP:
        case WM_KEYDOWN:
            if (message == WM_KEYDOWN && wparam != VK_ESCAPE) break;
            ::DestroyWindow(hwnd);
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void EnsureClasses() {
    static bool done = false;
    if (done) return;
    done = true;

    WNDCLASSEXW pick{};
    pick.cbSize = sizeof(pick);
    pick.style = CS_HREDRAW | CS_VREDRAW;
    pick.lpfnWndProc = PickProc;
    pick.hInstance = ::GetModuleHandleW(nullptr);
    pick.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    pick.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    pick.lpszClassName = kPickClass;
    ::RegisterClassExW(&pick);

    WNDCLASSEXW region{};
    region.cbSize = sizeof(region);
    region.style = CS_HREDRAW | CS_VREDRAW;
    region.lpfnWndProc = RegionProc;
    region.hInstance = ::GetModuleHandleW(nullptr);
    region.hCursor = ::LoadCursorW(nullptr, IDC_CROSS);
    region.lpszClassName = kRegionClass;
    ::RegisterClassExW(&region);
}

void RunModal(HWND window, HWND owner) {
    ::EnableWindow(owner, FALSE);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

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

}  // namespace

HWND PickWindowModal(HWND owner) {
    EnsureClasses();
    RECT bounds{};
    ::GetWindowRect(owner, &bounds);
    // The state has to outlive the window: GWLP_USERDATA cannot be read back
    // once the picker window has destroyed itself.
    auto* state = new PickState();
    HWND window = ::CreateWindowExW(WS_EX_DLGMODALFRAME, kPickClass, S(Str::TitlePick),
                                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                    bounds.left + 60, bounds.top + 60, 640, 460, owner,
                                    nullptr, ::GetModuleHandleW(nullptr), state);
    if (!window) {
        delete state;
        return nullptr;
    }

    RunModal(window, owner);
    HWND chosen = state->chosen;
    delete state;
    return chosen;
}

bool PickRegionModal(HWND owner, RECT& out) {
    EnsureClasses();
    const RECT screen = VirtualScreenRect();
    auto* state = new RegionState();
    HWND window = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                                    kRegionClass, L"", WS_POPUP | WS_VISIBLE, screen.left,
                                    screen.top, screen.right - screen.left,
                                    screen.bottom - screen.top, owner, nullptr,
                                    ::GetModuleHandleW(nullptr), state);
    if (!window) {
        delete state;
        return false;
    }

    RunModal(window, owner);
    bool ok = false;
    if (state->result.right > state->result.left) {
        out = state->result;
        ok = true;
    }
    if (state->canvas) ::DeleteObject(state->canvas);
    delete state;
    return ok;
}
