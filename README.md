# Crowman Study Assistant

Captures the screen on a hotkey and shows an explanation with worked steps in
its own window. Ordinary top-level window. No input injection, no memory reads.

## Build

```
python tools/make_icon.py
cmake -S . -B build -A x64 && cmake --build build --config Release
```

MinGW: see `.github/workflows/build.yml`

## Configuration

| key | default | meaning |
|---|---|---|
| mode | openai | openai or broker |
| endpoint | - | chat/completions URL |
| model | gpt-4o-mini | model name |
| license_url | - | broker activation URL |
| proxy_host | - | explicit proxy, empty = automatic |
| proxy_port | 0 | proxy port |
| proxy_user | - | proxy credentials user |
| ca_file | - | PEM/DER bundle imported to CurrentUser\Root |
| cert_pin_sha256 | - | leaf certificate pin, hex |
| max_width_px | 1600 | downscale width, 0 = off |
| max_height_px | 0 | downscale height, 0 = off |
| timeout_ms | 60000 | per-request timeout |
| max_retries | 2 | retries on 429/5xx |
| max_response_bytes | 8388608 | hard body cap |
| history_limit | 20 | stored answers |
| offline_grace_hours | 72 | licence grace |
| stream | 1 | stream the answer |
| capture_mode | monitor | desktop, monitor, window, pick, region |
| capture_cursor | 0 | include the pointer |
| hide_while_capturing | 1 | hide the window for the shot |
| reuse_crop | 1 | reuse the last region, Shift forces a new one |
| tray_icon | 1 | show a tray icon |
| minimize_to_tray | 0 | close hides instead of exiting |
| start_hidden | 0 | start hidden |
| hotkey_capture | ctrl+alt+s | capture |
| hotkey_copy | ctrl+alt+c | copy answer |
| hotkey_toggle | ctrl+alt+h | show / hide |
| language | en | en, de, es |

Credentials live in `%APPDATA%\CrowmanStudy\secrets.bin`, DPAPI sealed.

## Troubleshooting

| log message | meaning |
|---|---|
| WinHttpSendRequest failed (error 12029) | no route to host, check proxy |
| HTTP 401 | wrong key or revoked licence |
| HTTP 429, retrying | rate limited, backoff active |
| Response exceeded the 8 MB cap | raise max_response_bytes |
| secrets.bin did not unseal | copied from another profile |
| Offline grace expired | 72 h without contact, reconnect |
| RegisterHotKey failed | another program owns the combo |

Logs: `%LOCALAPPDATA%\CrowmanStudy\logs`. Diagnostics reveals the folder.

## Known deviations

- The region overlay uses `UpdateLayeredWindow` although N1 says no layered
  dimming. A12 names only `SetWindowDisplayAffinity` and
  `SetLayeredWindowAttributes`; neither is used.
- `Ctrl+Alt+H` hides the window although N1 says no hide hotkey. It is the only
  keyboard route back from the tray.
