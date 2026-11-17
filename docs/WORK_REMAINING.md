# WORK REMAINING

Read this first. Non-goals N1-N10 still apply.

Rules: C++17, Win32 + GDI+ + WinHTTP + BCrypt + DPAPI only. No new deps.
MSVC /W4 /permissive- and MinGW -Wall -Wextra clean, no suppressions.
All user-facing strings via src/strings.h. Every handle/GDI object released on
all paths. No unbounded buffers.

## 0. Decisions

| ID | Question | Default |
|----|----------|---------|
| D1 | N1 forbids layered dimming; windowpick.cpp uses UpdateLayeredWindow. | Keep, document deviation. |
| D2 | N1 forbids hiding UI on a hotkey; Ctrl+Alt+H exists. | Keep. |
| D3 | A12 bans imports; F4 adds OpenProcess. | Fine, A12 bans memory functions only. |

## 1. P0 - verify these first

The tree was reconciled but never compiled. Start with a build.

- P0-1 `src/json.cpp` and `src/util.cpp` are freshly written and have never been
  through a compiler. Expect sign/type fixes.
- P0-2 `res/crowman.ico` must be generated before configure:
  `python tools/make_icon.py`.
- P0-3 `src/settings.cpp` holds a Secrets draft; confirm the key field seeds
  from `CurrentSecrets()` and that Save calls `SaveSecrets()`.
- P0-4 `src/license.cpp` grace ordering: expiry-inside-grace must succeed,
  expiry-outside must set `hard_stop`.
- P0-5 `tests/license_test.cpp` sub-case order is deliberate; keep it.
- P0-6 `res/app.rc` must have exactly one `IDI_APP ICON` line.

## 2. P1 - spec gaps

- P1-1 F3: verify 100/150/200 % and mixed-DPI. No code change expected.
- P1-2 F20: history search box, EN_CHANGE filter, kHardCap 200.
- P1-3 F21: reject duplicate hotkeys in Collect(), then HotkeyAvailable().
- P1-4 F19: minimize_to_tray defaults 0, WM_CLOSE exits unless set.
- P1-5 F30/F32: Diagnostics button wired to RevealLogFolder().
- P1-6 F13/F16: HTTP 403 clears the licence cache, no grace fallback.
- P1-7 F12: proxy fields in settings; password to Secrets::proxy_pass.
- P1-8 Escape during capture: add g_capture_cancel, check before
  CaptureScreenPng, set restore=true on every CaptureThread exit path.
- P1-9 F39: finish README config table and troubleshooting; write
  docs/PORTABLE.txt.
- P1-10 CHANGELOG 1.2.0 entry.

## 3. P2 - quality

- P2-1 F33: first /W4 pass; expect signed/unsigned in capture.cpp.
- P2-2 F34: tests/leak_test.cpp, 100 captures, GetGuiResources delta flat.
- P2-3 F37: config cases for reuse_crop, crop_rect malformed, language
  fallback.
- P2-4 .gitignore: build/, build-tests/, dist/, .venv/, res/crowman.ico.
- P2-5 single-instance mutex in wWinMain.
- P2-6 A12 import gate in CI.
- P2-7 Inno [Code]: WizardIsTaskSelected is gone in 6.3+, use IsTaskSelected.
- P2-8 docs/DEV.md, docs/PORTABLE.txt.

## 4. Hardware / certificate

- H1 F27 needs a real certificate; record PFX or Trusted Signing in DEV.md.
- H2 F3 needs 4K at 200 % plus a second monitor.
- H3 A1 needs a clean Win11 VM, no VC++ redist.
- H4 A10 on the VM, no debugger attached.

## 5. Done when

Both toolchains clean; ctest green; import gate green; A1-A14 pass on the VM;
F1-F39 done; signed installer and portable zip from one commit; docs complete.
