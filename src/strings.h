#pragma once

#include <string>

enum class Str {
    AppTitle,
    BtnCapture,
    BtnWorking,
    BtnCancel,
    BtnCopy,
    BtnHistory,
    BtnSettings,
    BtnSave,
    BtnSet,
    BtnSend,
    BtnDiagnostics,
    BtnLoadIntoAnswer,
    BtnClearHistory,
    BtnOpenIni,
    StatusReady,
    StatusCreatedConfig,
    StatusCapturing,
    StatusStreaming,
    StatusCancelling,
    StatusCopied,
    StatusNothingToCopy,
    StatusClipboardFailed,
    StatusReloaded,
    StatusConfigNeeded,
    StatusCaptureFailed,
    StatusRequestFailed,
    StatusAnswerReceived,
    StatusNotActivated,
    StatusNoWindowPicked,
    StatusRegionCancelled,
    StatusCancelled,
    StatusTrayRunning,
    StatusWorkerFailed,
    StatusBatch,
    StatusBatchFull,
    StatusLoadedFromHistory,
    Intro,
    NoConfig,
    PrefixCaptureFailed,
    PrefixRequestFailed,
    PrefixActivation,
    TitleSettings,
    TitleHistory,
    TitlePick,
    LblTransport,
    LblEndpoint,
    LblApiKey,
    LblModel,
    LblCaptureMode,
    LblMaxSize,
    LblTimeout,
    LblRetries,
    LblGrace,
    LblHotkeys,
    LblShowHide,
    LblProxy,
    LblProxyPort,
    ChkStream,
    ChkHide,
    ChkCursor,
    ChkTray,
    ChkMinimize,
    ChkReuseCrop,
    ModeDesktop,
    ModeMonitor,
    ModeWindow,
    ModePick,
    ModeRegion,
    ErrEndpointRequired,
    ErrEndpointScheme,
    ErrHotkeyTaken,
    TrayCapture,
    TrayCopy,
    TraySettings,
    TrayHistory,
    TrayDiag,
    TrayQuit,
    CtxSelectAll,
    CtxClear,
    HotkeyCapture,
    HotkeyCopy,
    HotkeyToggle,
    TitleActivation,
    LblActivation,
    LblUniqueId,
    BtnActivate,
    StatusKeySaved,
    GhostProgress,
    AutoProgress,
    // --- parity status lines (exe .rdata 00080160-000802d0 / 000845f0-00084810) ---
    StatusBatchAdded,      // Added to Buffer (N ss)
    StatusStealthDim,      // Stealth Dim: 9% [Alt+N to Restore]
    StatusTyping,          // Typing... [Alt+R: Pause | Alt+E: Stop]
    StatusPaused,          // PAUSED [Alt+R: Resume | Alt+E: Stop]
    StatusTypingPaused,    // Typing Paused [Alt+R to resume]
    StatusTypingResumed,   // Typing Resumed
    StatusTypingStopped,   // Typing Stopped
    StatusTypingComplete,  // Typing Complete!
    StatusGhostComplete,   // Ghost Complete!
    StatusGhostStopped,    // GhostType Stopped
    StatusGhostActive,     // Ghost Active (N chars) [Alt+G: Stop]
    StatusGhostWaiting,    // Ghost Active [Type naturally | Alt+G: Stop]
    StatusAutoTyping,      // AutoTyping in progress... [Alt+R: Pause]
    StatusNoCode,          // No Code! Press Alt+8 or Alt+9 first
    StatusReadyMcq,        // MCQ Ready
    StatusReadyCoding,     // Coding Ready
    StatusReadyUniversal,  // Universal Ready
    StatusReadyGame,       // Game Ready
    StatusTypingHint,      // Alt+F: Auto Typing | Alt+G: Ghost | Alt+R: Pause | Alt+E: Stop
    StatusSolveError,      // Solve Error: Check Log
    StatusBlackScreen,     // Error: Black Screen
    Count
};

void SetLanguage(const std::wstring& code);
const wchar_t* S(Str id);
std::wstring SFormat(Str id, size_t value);
// Progress line for the typing features. The entry text uses {} for the two
// counters plus {P1} (one decimal) or {P0} (whole percent) for the percentage.
std::wstring SProgress(Str id, size_t done, size_t total);
