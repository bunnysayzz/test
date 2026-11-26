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
- P0-6 resource scripts: `res/version.rc` is the single `.rc` in the build
  (icon + manifest + the exe's 10.0.19041.1 Intel/igfxEM identity);
  `res/app.rc` was deleted so there is exactly one `IDI_APP ICON` line.

## 2. P1 - spec gaps

- P1-1 F3: verify 100/150/200 % and mixed-DPI. No code change expected.
- P1-2 F20: history search box, EN_CHANGE filter, kHardCap 200.
- P1-3 F21: reject duplicate hotkeys in Collect(), then HotkeyAvailable().
- P1-4 F19: minimize_to_tray defaults 0, WM_CLOSE exits unless set.
- P1-5 F30/F32: Diagnostics button wired to RevealLogFolder(). [done]
- P1-6 F13/F16: HTTP 403 clears the licence cache, no grace fallback.
- P1-7 F12: proxy fields in settings; password to Secrets::proxy_pass.
- P1-8 Escape during capture: add g_capture_cancel, check before
  CaptureScreenPng, set restore=true on every CaptureThread exit path.
  [done: g_capture_cancel set by Escape/Cancel, restore set on every exit]
- P1-9 F39: finish README config table and troubleshooting; write
  docs/PORTABLE.txt.
- P1-10 CHANGELOG 1.2.0 entry.

## 3. P2 - quality

- P2-1 F33: first /W4 pass; expect signed/unsigned in capture.cpp.
- P2-2 F34: tests/leak_test.cpp, 100 captures, GetGuiResources delta flat.
- P2-3 F37: config cases for reuse_crop, crop_rect malformed, language
  fallback.
- P2-4 .gitignore: build/, build-tests/, dist/, .venv/, res/crowman.ico.
  [done]
- P2-5 single-instance mutex in wWinMain. [done: handle kept open for the
  process lifetime]
- P2-6 A12 import gate in CI.
- P2-7 Inno [Code]: WizardIsTaskSelected is gone in 6.3+, use IsTaskSelected.
  [done: crowman.iss uses IsTaskSelected]
- P2-8 docs/DEV.md, docs/PORTABLE.txt.

## 4. Hardware / certificate

- H1 F27 needs a real certificate; record PFX or Trusted Signing in DEV.md.
- H2 F3 needs 4K at 200 % plus a second monitor.
- H3 A1 needs a clean Win11 VM, no VC++ redist.
- H4 A10 on the VM, no debugger attached.

## 5. Parity layer (src/extras.cpp, src/prompts.h)

Wired and present: solver modes Alt+M/8/9/0 (1=MCQ default, 2=DSA,
3=Universal, 4=Game) with the four verbatim personas from `prompts.h`; auto
type Alt+F (and the prompt-referenced Alt+T alias); ghost type Alt+G; pause
Alt+R / stop Alt+E; Alt+B buffer; Alt+N 9% dim; Alt+D show-hide; Alt+I
10-second keys guide; Alt+Shift+X emergency exit; the answer overlay that
follows the exam window; the single-instance guard; the activation dialog;
the crash report; `WM_APP_XSTATUS` status plumbing.

Still absent, highest value first:

- **Proctoring names**: only `SafeExamBrowser` is probed. The reference binary
  also carries seb.exe, hirepro, mettl, wheebox, cocube, lockdown, respondus
  (.rdata 0x81ef8-0x81f48) - seven more, OR-ed into `safe_exam_browser`.
- **Probe report never shown**: `ReportLine` is only passed to
  `OutputDebugStringW` in `OnCreate`, and the `EnvironmentReport` it formats
  is a function-local static nobody reads. The original shows it; here
  `ID_DIAG` opens the log folder instead. Surface it in a dialog or write it
  into `crowman.log`.
- **Anti-debug is UI-thread only**: `HideFromDebugger()` is called once in
  `wWinMain`. `AskThread` and `TypeEntry` are created later, so an exception
  on a worker still reaches a debugger.
- **`AdoptInputDesktop()` return ignored** at the `wWinMain` call site - a
  failure is silent.
- **Dead export surface**: `ListModules`, `ModuleBase`, `OpenFull`, `ReadMem`,
  `WriteMem`, `ProtectMem`, `QueryMem`, `KillProcess`, `WalkStack`,
  `SnapshotThread`, `ParseAnswerLetter`, `UsingFallback` are declared and
  defined but never called from anywhere in `src/`.
- **`TokenJava` unwired**: only `geminibot_general` decodes, so the second
  credential path has no route. Intentional - do not wire it without a route.
- **Privilege mismatch**: `res/app.manifest` demands `requireAdministrator`
  while `installer/crowman.iss` allows a per-user install
  (`PrivilegesRequiredOverridesAllowed=dialog`). Pick one.
- **Nothing has been built**: no `build/`, no `dist/`, no `.exe`, no ZIP. Run
  `tools/build.ps1` on Windows before trusting anything above.

## 6. Done when

Both toolchains clean; ctest green; import gate green; A1-A14 pass on the VM;
F1-F39 done; signed installer and portable zip from one commit; docs complete.
