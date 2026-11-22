#!/usr/bin/env bash
# c1.sh - fix the four compile blockers, the link libs, the resource strings,
#        the MinGW workflow, and wire the dead code paths.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
[ -n "${CROWMAN_ROOT:-}" ] && ROOT="$CROWMAN_ROOT"
cd "$ROOT"
STAMP="$(date +%Y%m%d-%H%M%S)"
say() { printf '  %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

if [ $# -gt 0 ] && [ "$1" = "--revert" ]; then
  n=0
  while IFS= read -r b; do
    o="$(printf '%s' "$b" | sed 's/\.bak\.[0-9-]*$//')"
    mv -f "$b" "$o"; say "restored $o"; n=$((n + 1))
  done < <(find . -type f -name '*.bak.*' | sort)
  say "$n file(s) restored"
  exit 0
fi

for f in src/main.cpp src/extras.h src/extras.cpp CMakeLists.txt res/version.rc .github/workflows/build.yml; do
  [ -f "$f" ] || die "missing $f"
done

python3 - "$STAMP" <<'PYEOF'
import re, sys

stamp = sys.argv[1]
manual = []

def load(p):
    raw = open(p, 'rb').read()
    open(p + '.bak.' + stamp, 'wb').write(raw)
    return raw.decode('utf-8', 'replace')

def save(p, t):
    open(p, 'wb').write(t.encode('utf-8'))

def sub1(p, old, new, note):
    t = load(p)
    if new in t:
        print('  ' + p + ': already applied')
        return
    if old not in t:
        manual.append(note)
        print('  ' + p + ': ANCHOR MISS - ' + note)
        return
    save(p, t.replace(old, new, 1))
    print('  ' + p + ': patched')

# ------------------------------------------------------------------ extras.h
p = 'src/extras.h'
t = load(p)
if '<VersionHelpers.h>' not in t:
    t = t.replace('#include <windows.h>\n',
                  '#include <windows.h>\n#include <VersionHelpers.h>\n'
                  '#include <shlwapi.h>\n#include <string.h>\n', 1)
if 'void OnCreate(HWND hwnd);' not in t:
    t = t.replace('void OnSize(UINT w, UINT h);',
                  'void OnCreate(HWND hwnd);\n'
                  'void OnHotkey(HWND hwnd, int id);\n'
                  'void OnSize(UINT w, UINT h);\n'
                  'void OverlayShow(bool on);\n'
                  'void OverlayText(const std::wstring& text);\n'
                  'void RememberAnswer(const std::wstring& text);', 1)
save(p, t)
print('  src/extras.h: patched')

# ------------------------------------------------------------------ extras.cpp
p = 'src/extras.cpp'
t = load(p)

BLOCK = '''static HWND g_overlayWnd = nullptr;
static std::wstring g_lastAnswer;
static bool g_overlayOn = false;

static LRESULT CALLBACK OverlayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: g_overlay.Resize(LOWORD(l), HIWORD(l)); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
void OverlayText(const std::wstring& text) {
    g_lastAnswer = text;
    if (g_overlayOn) g_overlay.Render(g_lastAnswer, 0.92f);
}
void OverlayShow(bool on) {
    g_overlayOn = on;
    if (g_overlayWnd) ShowWindow(g_overlayWnd, on ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (on) g_overlay.Render(g_lastAnswer, 0.92f);
}
void RememberAnswer(const std::wstring& text) { g_lastAnswer = text; }

void OnCreate(HWND hwnd) {
    g_main = hwnd;
    SetCaptureInvisible(hwnd, true);
    ShapeRounded(hwnd, 14);
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{ sizeof wc };
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = OverlayProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"xOverlay";
        RegisterClassExW(&wc);
        reg = true;
    }
    g_overlayWnd = CreateWindowExW(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE, L"xOverlay", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 100, 100,
        hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (g_overlayWnd) g_overlay.Init(g_overlayWnd);
    RegisterHotkeys(hwnd);
    static EnvironmentReport rep;
    ProbeEnvironment(&rep);
    OutputDebugStringW(ReportLine(rep));
}
void OnHotkey(HWND, int id) {
    int n = id - X_HK_BASE;
    if (n < 0 || n >= HK_COUNT) return;
    Dispatch(n);
}

void SetWindows(HWND main, HWND overlay)'''

if 'void OnCreate(HWND hwnd) {' not in t:
    t = t.replace('void SetWindows(HWND main, HWND overlay)', BLOCK, 1)
    print('  src/extras.cpp: overlay + OnCreate/OnHotkey inserted')
else:
    print('  src/extras.cpp: already applied')

t = t.replace('void OnSize(UINT w, UINT h) { g_overlay.Resize(w, h); }',
'''void OnSize(UINT w, UINT h) {
    if (g_overlayWnd && w && h)
        SetWindowPos(g_overlayWnd, nullptr, 0, 0, (int)w, (int)h,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    ShapeRounded(g_main, 14);
    g_overlay.Resize(w, h);
}''', 1)

t = t.replace('    case HK_AUTOTYPE: return true;',
'''    case HK_AUTOTYPE:
        if (g_lastAnswer.empty()) return false;
        return AutoType(g_main, g_lastAnswer);''', 1)

save(p, t)
print('  src/extras.cpp: OnSize + AutoType wired')

# ------------------------------------------------------------------ main.cpp
p = 'src/main.cpp'
t = load(p)
t = t.replace('(int)wParam', '(int)wparam')
t = t.replace('x::OnSize(hwnd, LOWORD(lParam), HIWORD(lParam));',
              'x::OnSize(LOWORD(lparam), HIWORD(lparam));')
if 'x::UnregisterHotkeys' not in t:
    t = t.replace('PostQuitMessage(0);',
                  'x::UnregisterHotkeys(hwnd);\n    PostQuitMessage(0);', 1)
save(p, t)
print('  src/main.cpp: wparam/lparam casing, OnSize arity, hotkey cleanup')

# ------------------------------------------------------------------ CMakeLists
p = 'CMakeLists.txt'
t = load(p)
if 'd3d11' not in t:
    t = t.replace('    winhttp gdiplus bcrypt crypt32 dbghelp shcore dwmapi',
                  '    winhttp gdiplus bcrypt crypt32 dbghelp shcore dwmapi\n'
                  '    d3d11 dxgi d2d1 dwrite shlwapi psapi', 1)
t = t.replace('target_compile_options(crowman_study PRIVATE /EHa)',
              'target_compile_options(crowman_study PRIVATE\n'
              '    $<$<CXX_COMPILER_ID:MSVC>:/EHa>)')
save(p, t)
print('  CMakeLists.txt: libs added, /EHa guarded to MSVC')

# ------------------------------------------------------------------ version.rc
p = 'res/version.rc'
t = load(p)
t = t.replace('"igfxEM Module"', '"Intel(R) HD Graphics Background Service"')
t = t.replace('"InternalName",     "igfxEM"', '"InternalName",     "igfxEM.exe"')
save(p, t)
print('  res/version.rc: FileDescription + InternalName corrected')

# ------------------------------------------------------------------ workflow
p = '.github/workflows/build.yml'
t = load(p)
t = t.replace('res/app.rc', 'res/version.rc')
t = t.replace('-lws2_32"', '-lws2_32 -ld3d11 -ldxgi -ld2d1 -ldwrite -lshlwapi"')
save(p, t)
print('  build.yml: version.rc instead of app.rc, D2D/D3D libs added')

# ------------------------------------------------------------------ gitignore
p = '.gitignore'
try:
    t = load(p)
except FileNotFoundError:
    t = ''
if '*.bak.*' not in t:
    t = t.rstrip('\n') + '\n*.bak.*\n'
    save(p, t)
    print('  .gitignore: *.bak.* ignored')

if manual:
    print('\n  MANUAL STEPS:')
    for m in manual:
        print('   - ' + m)
PYEOF

echo
echo "--- verify ---"
grep -n 'OnCreate\|OnHotkey' src/extras.h src/extras.cpp | head -6
grep -n 'x::OnSize\|x::OnHotkey' src/main.cpp
grep -n 'd3d11\|/EHa' CMakeLists.txt
grep -n 'FileDescription\|InternalName' res/version.rc
grep -n 'version.rc' .github/workflows/build.yml
echo
echo "Revert with: bash c1.sh --revert"