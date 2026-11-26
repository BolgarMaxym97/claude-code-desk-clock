# Development

```
firmware/   ESP8266 firmware (PlatformIO, Arduino core). lib/model = pure C++ logic with native unit tests
host/       claude-clock program for macOS / Linux / Windows (Go, no dependencies)
install/    one-line installers (install.sh, install.ps1)
.github/    CI (tests + build on every push) and releases (on a v* tag)
```

## Firmware

The PlatformIO toolchain for the ESP8266 is x86-only. The helper scripts build inside Docker, which works on any OS
including Apple Silicon:

```sh
cd firmware
./tools/test.sh    # native unit + layout tests
./tools/build.sh   # -> .pio/build/clock/firmware.bin
```

With PlatformIO installed natively: `pio test -e native` and `pio run -e clock`.

Install your build on a paired clock: `claude-clock update --file .pio/build/clock/firmware.bin`.

`include/secrets.h` is optional and ignored by git. You can copy `secrets.h.example` to build in a Wi-Fi network,
a static IP, a fixed token or a birthday for your own clock. Release builds have none of these.

### Clock HTTP API

| Method & path | Auth | Purpose |
|---|---|---|
| `GET /status` | — | Firmware, IP, screen, sessions, limits, Wi-Fi, timezone (JSON) |
| `POST /api/pair/start` | — | Show a 6-character code on the screen for 5 minutes |
| `POST /api/pair` `{"code"}` | the code | Returns `{"token"}`; 5 wrong codes lock pairing until reboot |
| `POST /api/event` | token | Claude Code hook event `{session, event, project, tool, kind, target, branch}` |
| `POST /api/limits` | token | `{h5:{pct,resets_at}, d7:{pct,resets_at}}` |
| `POST /api/settings` | token | `{"tz": "<POSIX TZ>", "birthday": "MM-DD" \| ""}` |
| `POST /api/wifi-setup` | token | Open the Wi-Fi setup access point for 10 minutes |
| `POST /api/preview-scene?name=…` / `?all=1` | token | Play one or all idle animations |
| `POST /api/preview-newyear[?midnight=1]` | token | Preview the New Year look |
| `POST /update` (multipart `firmware`) | basic `clock:<token>` | Install firmware |
| `/setup` | setup AP clients only | Wi-Fi setup page (captive portal) |

Send the token in the `X-Clock-Token` header.

## claude-clock (host)

```sh
cd host
go test ./...
go build ./cmd/claude-clock
```

`claude-clock hook` and `claude-clock statusline` run inside Claude Code on every event. They must return at
once, print nothing, and never fail. The network request runs in a detached child process (`claude-clock send`).
After a failed send, further sends are skipped for 30 seconds.

## Releases

Push a tag such as `v1.0.0`. The release workflow:

1. Runs all tests.
2. Builds `SDP_claude-clock-v1.0.0.bin` (the stock update page wants the `SDP` prefix) and its `.sha256`.
3. Builds `claude-clock` for 6 platforms plus `checksums.txt`.
4. Publishes them as a GitHub release.

The installers and `claude-clock update` always take the latest release.
