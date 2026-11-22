#!/usr/bin/env bash
# b2.sh - Crowman resources, manifest and main.cpp wiring. Pairs with b1.sh.
# Usage:  bash b2.sh            (apply)
#         bash b2.sh --revert   (undo everything this script did)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
[ -n "${CROWMAN_ROOT:-}" ] && ROOT="$CROWMAN_ROOT"
cd "$ROOT"
STAMP="$(date +%Y%m%d-%H%M%S)"

say() { printf '  %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

MODE=apply
if [ $# -gt 0 ] && [ "$1" = "--revert" ]; then MODE=revert; fi

# ---------------------------------------------------------------- revert
if [ "$MODE" = revert ]; then
  n=0
  while IFS= read -r b; do
    o="$(printf '%s' "$b" | sed 's/\.bak\.[0-9-]*$//')"
    mv -f "$b" "$o"
    say "restored $o"
    n=$((n + 1))
  done < <(find . -type f -name '*.bak.*' | sort)
  rm -f res/app.ico res/version.rc res/app.manifest res/v.tmp
  say "$n file(s) restored; res/ artefacts removed"
  exit 0
fi

# ---------------------------------------------------------------- preflight
[ -f CMakeLists.txt ] || die "no CMakeLists.txt here - run from the project root"
[ -f src/main.cpp ]   || die "src/main.cpp missing"
[ -f src/extras.h ]   || die "src/extras.h missing - run b1.sh first"
mkdir -p res

# ---------------------------------------------------------------- icon
if command -v python3 >/dev/null 2>&1; then
  python3 - <<'PYEOF'
import struct
w = h = 32
xor = bytearray()
for _y in range(h):
    for _x in range(w):
        xor += bytes((196, 118, 38, 255))
row = (w + 31) // 32 * 4
andm = bytearray()
for _y in range(h):
    andm += b'\x00' * row
data = bytes(xor) + bytes(andm)
dib = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, len(data), 0, 0, 0, 0)
hdr = struct.pack('<HHH', 0, 1, 1)
ent = struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(data), 22)
open('res/app.ico', 'wb').write(hdr + ent + dib + data)
PYEOF
  say "res/app.ico written (32x32)"
else
  say "python3 not found - app.ico skipped"
fi

# ---------------------------------------------------------------- version.rc
cat > res/version.rc <<'XEOF_RC'
// Crowman Study Assistant - disguised as an Intel igfxEM module
#include <winver.h>

#define IDI_APP 101
#define IDR_MANIFEST 1

IDI_APP ICON "app.ico"
IDR_MANIFEST RT_MANIFEST "app.manifest"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION 10,0,19041,1
 PRODUCTVERSION 10,0,19041,1
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_APP
 FILESUBTYPE VFT2_UNKNOWN
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName",      "Intel Corporation"
      VALUE "FileDescription",  "igfxEM Module"
      VALUE "FileVersion",      "10.0.19041.1"
      VALUE "InternalName",     "igfxEM"
      VALUE "LegalCopyright",   "Copyright (C) Intel Corporation"
      VALUE "OriginalFilename", "igfxEM.exe"
      VALUE "ProductName",      "Intel(R) Graphics Media Accelerator Driver"
      VALUE "ProductVersion",   "10.0.19041.1"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
XEOF_RC
say "res/version.rc written"

if [ ! -f res/app.ico ]; then
  sed '/IDI_APP ICON/d' res/version.rc > res/v.tmp
  mv res/v.tmp res/version.rc
  say "version.rc: ICON line removed (no app.ico)"
fi

# ---------------------------------------------------------------- manifest
cat > res/app.manifest <<'XEOF_MF'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="10.0.19041.1" processorArchitecture="amd64"
                    name="Intel.igfxEM" type="win32"/>
  <description>igfxEM Module</description>

  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0" processorArchitecture="amd64"
        publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>

  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>

  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
    </application>
  </compatibility>

  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">permonitorv2</dpiAwareness>
      <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
    </windowsSettings>
  </application>
</assembly>
XEOF_MF
say "res/app.manifest written (requireAdministrator)"

# ---------------------------------------------------------------- patcher
python3 - "$STAMP" <<'PYEOF'
import re, sys

stamp = sys.argv[1]
manual = []

def load(path):
    with open(path, 'rb') as f:
        raw = f.read()
    with open(path + '.bak.' + stamp, 'wb') as f:
        f.write(raw)
    return raw.decode('utf-8', 'replace')

def save(path, text):
    with open(path, 'wb') as f:
        f.write(text.encode('utf-8'))

# ---- main.cpp
p = 'src/main.cpp'
s = load(p)
before = s

if '#include "extras.h"' not in s:
    m = re.search(r'^#include\s+"app\.h"[^\n]*$', s, re.M)
    if m:
        s = s[:m.end()] + '\n#include "extras.h"' + s[m.end():]
    else:
        m = re.search(r'(?:^#include[^\n]*\n)+', s, re.M)
        if m:
            s = s[:m.end()] + '#include "extras.h"\n' + s[m.end():]
        else:
            manual.append('add #include "extras.h" to src/main.cpp')

if 'x::InstallCrashGuard' not in s:
    m = re.search(r'wWinMain[^\n]*\n[ \t]*\{', s)
    if m:
        s = (s[:m.end()]
             + '\n    x::InstallCrashGuard();\n    x::HideFromDebugger();'
             + s[m.end():])
    else:
        manual.append('call x::InstallCrashGuard(); x::HideFromDebugger(); at the top of wWinMain')

def hook(wm, call, note):
    global s
    if call.split('(')[0] in s:
        return
    m = re.search(r'case[ \t]+' + wm + r'[ \t]*:', s)
    if m:
        s = s[:m.end()] + '\n        ' + call + s[m.end():]
    else:
        manual.append(note)

hook('WM_CREATE', 'x::OnCreate(hwnd);',
     'call x::OnCreate(hwnd) from WM_CREATE')
hook('WM_HOTKEY', 'x::OnHotkey(hwnd, (int)wParam);',
     'call x::OnHotkey(hwnd, (int)wParam) from WM_HOTKEY')
hook('WM_SIZE', 'x::OnSize(hwnd, LOWORD(lParam), HIWORD(lParam));',
     'call x::OnSize(hwnd, LOWORD(lParam), HIWORD(lParam)) from WM_SIZE')

if s != before:
    save(p, s)
    print('  patched src/main.cpp')
else:
    print('  src/main.cpp unchanged')

# ---- CMakeLists.txt
p = 'CMakeLists.txt'
s = load(p)
if 'extras.cpp' not in s:
    m = re.search(r'add_executable[ \t]*\([ \t]*([A-Za-z0-9_.\-]+)', s)
    if m:
        tgt = m.group(1)
    else:
        tgt = 'crowman_study'
        manual.append('add src/extras.cpp and res/version.rc to the add_executable source list')
    s += ('\n# --- crowman extras (b2.sh) ---\n'
          'target_sources(' + tgt + ' PRIVATE src/extras.cpp res/version.rc)\n'
          'target_compile_options(' + tgt + ' PRIVATE /EHa)\n'
          'target_include_directories(' + tgt + ' PRIVATE src)\n')
    save(p, s)
    print('  patched CMakeLists.txt (target ' + tgt + ')')
else:
    print('  CMakeLists.txt already wired')

if manual:
    print('')
    print('  MANUAL STEPS STILL NEEDED:')
    for item in manual:
        print('   - ' + item)
PYEOF

# ---------------------------------------------------------------- verify
echo
echo "--- written ---"
ls -l res
echo
echo "--- wiring ---"
grep -n 'extras' src/main.cpp CMakeLists.txt || true
echo
echo "Next: re-run cmake configure, then build. Revert with: bash b2.sh --revert"
