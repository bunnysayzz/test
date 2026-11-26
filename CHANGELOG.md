# Changelog

## 1.2.0

- Parity solver modes: Alt+M (multi-MCQ), Alt+8 (DSA), Alt+9 (full-stack /
  SQL / web / debug), Alt+0 (game & puzzles), each sending the reference
  build's exam-evaluator persona
- Auto Type (Alt+F, plus the Alt+T alias its own prompt instructs) pastes the
  answer line by line into the exam window; Ghost Type (Alt+G) types it
  keystroke by keystroke with humanised delays and typo correction
- Alt+R pauses or resumes, Alt+E stops; progress and completion states on the
  status line
- Answer overlay drawn on top of the exam window, click-through and topmost,
  following the target as it moves
- Alt+B adds a capture to the buffer (`Added to Buffer (N ss)`), Alt+N dims to
  9%, Alt+D hides the UI, Alt+I shows a 10-second shortcut guide,
  Alt+Shift+X exits immediately
- Batch up to five captures into one request
- Secrets moved to DPAPI-sealed secrets.bin, credentials redacted from logs
- Per-window picker shows process names, crop rectangle persists per session
- Explicit proxy, 8 MB response cap, offline grace now hard-stops
- Escape cancels, elapsed timer, Ctrl+C and context menu in the answer box
- Searchable history, hotkey conflict validation, Diagnostics menu
- Single-instance guard, activation dialog with HWID copy
- Unhandled exceptions write a log line, a summary and a minidump, and say so
- Installer offers removal of AppData data, portable ZIP build
