#include "strings.h"

namespace {

struct Entry {
    Str id;
    const wchar_t* en;
    const wchar_t* de;
    const wchar_t* es;
};

const Entry kTable[] = {
    {Str::AppTitle, L"Crowman Study Assistant", L"Crowman Lernassistent", L"Asistente de estudio Crowman"},
    {Str::BtnCapture, L"Capture && Ask", L"Aufnehmen && Fragen", L"Capturar y preguntar"},
    {Str::BtnWorking, L"Working", L"Arbeitet", L"Trabajando"},
    {Str::BtnCancel, L"Cancel", L"Abbrechen", L"Cancelar"},
    {Str::BtnCopy, L"Copy", L"Kopieren", L"Copiar"},
    {Str::BtnHistory, L"History", L"Verlauf", L"Historial"},
    {Str::BtnSettings, L"Settings", L"Einstellungen", L"Ajustes"},
    {Str::BtnSave, L"Save", L"Speichern", L"Guardar"},
    {Str::BtnSet, L"Set", L"Setzen", L"Fijar"},
    {Str::BtnSend, L"Send now", L"Jetzt senden", L"Enviar ya"},
    {Str::BtnDiagnostics, L"Diagnostics", L"Diagnose", L"Diagnostico"},
    {Str::BtnLoadIntoAnswer, L"Load into answer box", L"In Antwortfeld laden", L"Cargar en el cuadro"},
    {Str::BtnClearHistory, L"Clear history", L"Verlauf leeren", L"Borrar historial"},
    {Str::BtnOpenIni, L"Open config.ini", L"config.ini offnen", L"Abrir config.ini"},
    {Str::StatusReady, L"Ready.", L"Bereit.", L"Listo."},
    {Str::StatusCreatedConfig, L"Created config.ini - click Settings.", L"config.ini erstellt - Einstellungen offnen.", L"config.ini creado: abre Ajustes."},
    {Str::StatusCapturing, L"Capturing...", L"Aufnahme...", L"Capturando..."},
    {Str::StatusStreaming, L"Streaming answer...", L"Antwort wird gestreamt...", L"Transmitiendo..."},
    {Str::StatusCancelling, L"Cancelling...", L"Wird abgebrochen...", L"Cancelando..."},
    {Str::StatusCopied, L"Answer copied to clipboard.", L"Antwort kopiert.", L"Respuesta copiada."},
    {Str::StatusNothingToCopy, L"Nothing to copy yet.", L"Noch nichts zu kopieren.", L"Aun nada que copiar."},
    {Str::StatusClipboardFailed, L"Could not open the clipboard.", L"Zwischenablage nicht verfugbar.", L"Portapapeles no disponible."},
    {Str::StatusReloaded, L"Settings reloaded.", L"Einstellungen neu geladen.", L"Ajustes recargados."},
    {Str::StatusConfigNeeded, L"Configuration needed - click Settings.", L"Konfiguration fehlt - Einstellungen offnen.", L"Falta configuracion: abre Ajustes."},
    {Str::StatusCaptureFailed, L"Capture failed.", L"Aufnahme fehlgeschlagen.", L"Fallo la captura."},
    {Str::StatusRequestFailed, L"Request failed.", L"Anfrage fehlgeschlagen.", L"Fallo la solicitud."},
    {Str::StatusAnswerReceived, L"Answer received.", L"Antwort erhalten.", L"Respuesta recibida."},
    {Str::StatusNotActivated, L"Not activated.", L"Nicht aktiviert.", L"Sin activar."},
    {Str::StatusNoWindowPicked, L"No window picked.", L"Kein Fenster gewahlt.", L"Ninguna ventana."},
    {Str::StatusRegionCancelled, L"Region selection cancelled.", L"Bereichsauswahl abgebrochen.", L"Seleccion cancelada."},
    {Str::StatusCancelled, L"Cancelled.", L"Abgebrochen.", L"Cancelado."},
    {Str::StatusTrayRunning, L"Still running in the tray.", L"Lauft weiter im Infobereich.", L"Sigue en la bandeja."},
    {Str::StatusWorkerFailed, L"Could not start the worker thread.", L"Worker-Thread fehlgeschlagen.", L"No se pudo iniciar el hilo."},
    {Str::StatusBatch, L"{} of 5 captured - sending shortly.", L"{} von 5 aufgenommen - sendet gleich.", L"{} de 5 capturadas - enviando pronto."},
    {Str::StatusBatchFull, L"Batch full - sending now.", L"Stapel voll - sendet jetzt.", L"Lote lleno - enviando."},
    {Str::StatusLoadedFromHistory, L"Loaded from history.", L"Aus Verlauf geladen.", L"Cargado del historial."},
    {Str::Intro, L"Capture a question, code snippet or diagram.\n\nThe assistant explains the concept and works through the answer. It never types into other windows.", L"Frage, Code oder Diagramm aufnehmen.\n\nDer Assistent erklart das Konzept und rechnet vor. Er tippt nie in andere Fenster.", L"Captura una pregunta, codigo o diagrama.\n\nEl asistente explica y resuelve. Nunca escribe en otras ventanas."},
    {Str::NoConfig, L"No AI endpoint configured yet.\n\n1. Click Settings.\n2. Set the endpoint and the API key.\n3. Save, then capture again.", L"Kein Endpunkt konfiguriert.\n\n1. Einstellungen offnen.\n2. Endpunkt und Schlussel setzen.\n3. Speichern, dann erneut aufnehmen.", L"Sin endpoint configurado.\n\n1. Abre Ajustes.\n2. Define endpoint y clave.\n3. Guarda y captura de nuevo."},
    {Str::PrefixCaptureFailed, L"Screen capture failed: ", L"Aufnahme fehlgeschlagen: ", L"Fallo la captura: "},
    {Str::PrefixRequestFailed, L"Request failed:\n", L"Anfrage fehlgeschlagen:\n", L"Fallo la solicitud:\n"},
    {Str::PrefixActivation, L"Activation: ", L"Aktivierung: ", L"Activacion: "},
    {Str::TitleSettings, L"Settings", L"Einstellungen", L"Ajustes"},
    {Str::TitleHistory, L"Answer history", L"Antwortverlauf", L"Historial"},
    {Str::TitlePick, L"Pick a window to capture", L"Fenster wahlen", L"Elige una ventana"},
    {Str::LblTransport, L"Transport", L"Transport", L"Transporte"},
    {Str::LblEndpoint, L"Endpoint URL", L"Endpunkt-URL", L"URL del endpoint"},
    {Str::LblApiKey, L"API key / token", L"API-Schlussel / Token", L"Clave / token"},
    {Str::LblModel, L"Model", L"Modell", L"Modelo"},
    {Str::LblCaptureMode, L"Capture mode", L"Aufnahmemodus", L"Modo de captura"},
    {Str::LblMaxSize, L"Max width / height px", L"Max. Breite / Hohe px", L"Ancho / alto max"},
    {Str::LblTimeout, L"Timeout ms", L"Zeitlimit ms", L"Tiempo limite"},
    {Str::LblRetries, L"Retries", L"Wiederholungen", L"Reintentos"},
    {Str::LblGrace, L"Offline grace hours", L"Offline-Kulanz", L"Horas sin conexion"},
    {Str::LblHotkeys, L"Hotkeys", L"Tastenkurzel", L"Atajos"},
    {Str::LblShowHide, L"show / hide", L"zeigen / verstecken", L"mostrar / ocultar"},
    {Str::LblProxy, L"Proxy host", L"Proxy-Host", L"Host proxy"},
    {Str::LblProxyPort, L"Port", L"Port", L"Puerto"},
    {Str::ChkStream, L"Stream the answer", L"Antwort streamen", L"Transmitir"},
    {Str::ChkHide, L"Hide window while capturing", L"Fenster verstecken", L"Ocultar al capturar"},
    {Str::ChkCursor, L"Include mouse cursor", L"Mauszeiger aufnehmen", L"Incluir cursor"},
    {Str::ChkTray, L"Tray icon", L"Infobereich-Symbol", L"Icono de bandeja"},
    {Str::ChkMinimize, L"Close to tray", L"In Infobereich schliessen", L"Cerrar a la bandeja"},
    {Str::ChkReuseCrop, L"Reuse last region", L"Letzten Bereich wiederverwenden", L"Reusar ultima region"},
    {Str::ModeDesktop, L"Whole desktop", L"Gesamter Desktop", L"Todo el escritorio"},
    {Str::ModeMonitor, L"Active monitor", L"Aktiver Monitor", L"Monitor activo"},
    {Str::ModeWindow, L"Foreground window", L"Vordergrundfenster", L"Ventana activa"},
    {Str::ModePick, L"Pick window", L"Fenster wahlen", L"Elegir ventana"},
    {Str::ModeRegion, L"Select region", L"Bereich wahlen", L"Elegir region"},
    {Str::ErrEndpointRequired, L"An endpoint URL is required.", L"Eine Endpunkt-URL ist erforderlich.", L"Se requiere una URL."},
    {Str::ErrEndpointScheme, L"The endpoint must start with https:// or http://.", L"Endpunkt muss mit https:// oder http:// beginnen.", L"Debe empezar por https:// o http://."},
    {Str::ErrHotkeyTaken, L"That hotkey is already in use.", L"Tastenkurzel bereits belegt.", L"Atajo ya en uso."},
    {Str::TrayCapture, L"Capture and ask", L"Aufnehmen und fragen", L"Capturar y preguntar"},
    {Str::TrayCopy, L"Copy answer", L"Antwort kopieren", L"Copiar respuesta"},
    {Str::TraySettings, L"Settings...", L"Einstellungen...", L"Ajustes..."},
    {Str::TrayHistory, L"History...", L"Verlauf...", L"Historial..."},
    {Str::TrayDiag, L"Diagnostics...", L"Diagnose...", L"Diagnostico..."},
    {Str::TrayQuit, L"Quit", L"Beenden", L"Salir"},
    {Str::CtxSelectAll, L"Select all", L"Alles auswahlen", L"Seleccionar todo"},
    {Str::CtxClear, L"Clear answer", L"Antwort leeren", L"Borrar respuesta"},
    {Str::HotkeyCapture, L"Capture:", L"Aufnehmen:", L"Capturar:"},
    {Str::HotkeyCopy, L"Copy answer:", L"Antwort kopieren:", L"Copiar:"},
    {Str::HotkeyToggle, L"Show / hide:", L"Zeigen / verstecken:", L"Mostrar / ocultar:"},
};

constexpr size_t kCount = (size_t)Str::Count;
static_assert(sizeof(kTable) / sizeof(kTable[0]) == kCount, "string table out of sync");

int g_column = 0;

}  // namespace

void SetLanguage(const std::wstring& code) {
    if (code == L"de") g_column = 1;
    else if (code == L"es") g_column = 2;
    else g_column = 0;
}

const wchar_t* S(Str id) {
    const size_t index = (size_t)id;
    if (index >= kCount) return L"";
    const Entry& entry = kTable[index];
    const wchar_t* text = g_column == 1 ? entry.de : g_column == 2 ? entry.es : entry.en;
    return text ? text : entry.en;
}

std::wstring SFormat(Str id, size_t value) {
    std::wstring text = S(id);
    const std::wstring token = L"{}";
    const size_t at = text.find(token);
    if (at != std::wstring::npos) text.replace(at, token.size(), std::to_wstring(value));
    return text;
}
