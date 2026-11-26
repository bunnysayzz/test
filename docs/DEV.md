# Development

```
python -m venv .venv
.venv\Scripts\activate
pip install -r tools/requirements.txt
python tools/make_icon.py
```

MSVC: `cmake -S . -B build -A x64 && cmake --build build --config Release`

MinGW: see `.github/workflows/build.yml`

Code signing: choose PFX or Azure Trusted Signing and record it here.

## Parity layer

`src/extras.cpp` (namespace `x`) holds the features ported from the reference
binary; `src/prompts.h` holds its four exam personas verbatim. Build notes:

- **`/EHa` is mandatory.** `src/extras.cpp` uses `__try`/`__except` in the
  cross-process memory helpers. CMake adds `/EHa` for `crowman_study` only -
  anything else that compiles `extras.cpp` must add it too (this is the most
  likely failure on the MinGW CI leg, along with the `_s` CRT functions).
- `x::PostStatus` posts `WM_APP_XSTATUS` with an **owned `std::wstring*`**.
  The handler in `main.cpp` must take ownership and delete it.
- `kHk[]` in `extras.cpp` must stay in sync with `HK_COUNT`
  (`static_assert`ed), and `Str` must stay in sync with `kTable` in
  `strings.cpp` (also `static_assert`ed).
- Hotkey id spaces are disjoint: `main.cpp` uses 1-4, `x` uses
  `x::X_HK_BASE + 0..`. `x::OnHotkey` early-returns for ids below the base.

| Hotkey | Action |
|--------|--------|
| Alt+M / Alt+8 / Alt+9 / Alt+0 | Solver mode MCQ / DSA / Universal / Game |
| Alt+F (and Alt+T, prompt parity) | Auto type: line-by-line paste into the exam window |
| Alt+G | Ghost type: keystroke-by-keystroke with jitter and typos |
| Alt+R / Alt+E | Pause-resume / stop the running typing job |
| Alt+B | Add the current capture to the buffer |
| Alt+N / Alt+D | 9% stealth dim / show-hide |
| Alt+I | 10-second shortcut guide |
| Alt+Shift+X | Emergency exit |

Solver mode defaults to MCQ (`g_mode = 1`) and the exam persona is built for
every request; the study prompt rides along as `BuildPersonaPrompt`'s
`question` argument. Transport mode `parity` uses the embedded endpoint,
route and `TokenGeneral()` instead of the configured endpoint/API key.
