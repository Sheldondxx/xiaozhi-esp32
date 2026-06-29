# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**小智 AI (XiaoZhi AI)** — an open-source ESP32 AI voice assistant firmware (v2.2.6, MIT license). It provides voice interaction via Qwen/DeepSeek LLMs through WebSocket or MQTT+UDP protocols, with device control through MCP (Model Context Protocol). Supports ~80 different ESP32-series development boards.

## Build System

- **Framework**: ESP-IDF >= 5.5.2 (CMake-based)
- **Docker image for CI**: `espressif/idf:v5.5.2`
- **Board selector**: Kconfig (`main/Kconfig.projbuild`) — `idf.py menuconfig` → "Xiaozhi Assistant" → Board Type
- **Build a specific board**: `python scripts/release.py <board-dir-name> [--name <variant-name>]`
  - Lists all variants: `python scripts/release.py --list-boards --json`
  - The board directory name under `main/boards/` identifies the target (e.g., `esp-box-3`, `m5stack-core-s3`, `waveshare/esp32-s3-touch-amoled-1.8`)
- **Standard IDF commands** also work directly:
  ```bash
  idf.py set-target <esp32|esp32s3|esp32c3|esp32c5|esp32c6|esp32p4>
  idf.py menuconfig   # select board type, language, network, etc.
  idf.py build
  idf.py flash
  ```
- **Dependencies**: Managed via `main/idf_component.yml` (IDF Component Manager). Run `idf.py update-deps` to sync.
- **Code style**: Google C++ (`.clang-format`), `-Wno-missing-field-initializers`
- **Minimal build**: `MINIMAL_BUILD ON` — only components referenced by `main` are built

## High-Level Architecture

```
main/
├── main.cc                    # app_main() entry point
├── application.cc/h           # Application singleton — owns all subsystems
├── device_state_machine.h     # Device states: IDLE → CONNECTING → LISTENING → SPEAKING
├── protocol.cc                # Protocol base class
├── mqtt_protocol.cc           # MQTT + UDP hybrid protocol implementation
├── websocket_protocol.cc      # WebSocket protocol implementation
├── mcp_server.cc/h            # Device-side MCP server for hardware control
├── ota.cc/h                   # OTA firmware update
├── settings.cc/h              # NVS-backed persistent settings
├── system_info.cc/h           # System information collection
├── assets.cc                  # Asset loading from spiffs partition
├── audio/                     # Audio subsystem
│   ├── audio_service.cc       # Audio pipeline manager
│   ├── audio_codec.cc         # Audio codec abstraction
│   ├── codecs/                # Hardware codec drivers (ES8311, ES8374, ES8388, ES8389, Box, Dummy)
│   ├── demuxer/               # OGG demuxer
│   ├── processors/            # Audio processors (AFE with AEC, debugger)
│   └── wake_words/            # Wake word engines (ESP Wakenet, AFE, Custom Multinet)
├── display/                   # Display subsystem
│   ├── display.cc             # Base display class
│   ├── lcd_display.cc         # LCD implementation
│   ├── oled_display.cc        # OLED implementation
│   ├── emote_display.cc       # Emote/animation display
│   └── lvgl_display/          # LVGL 9.x integration (themes, fonts, images, GIF, JPEG, emoji)
├── led/                       # LED drivers (single, circular strip, GPIO)
├── protocols/                 # Protocol implementations (reused by protocol.cc)
├── boards/                    # ~80 board support packages
│   ├── common/                # Shared: WifiBoard, Ml307Board, Nt26Board, DualNetworkBoard,
│   │                          #   Button, Knob, Backlight, Power (AXP2101/SY6970),
│   │                          #   ADC battery, Ethernet, RNDIS, ESP Camera/Video, Blufi
│   ├── <board-name>/          # Flat structure: config.h, config.json, xxx_board.cc/h
│   └── <manufacturer>/<board-name>/  # Grouped structure (e.g., waveshare/, m5stack/, lceda-course-examples/)
└── assets/                    # Embedded assets
    ├── locales/<lang>/        # Per-language OGG audio files + language.json
    └── common/                # Language-independent OGG sounds
```

### Subsystem Interaction

1. `app_main()` → `Application` singleton initializes board, audio, display, protocol
2. Board class (selected at compile time via Kconfig) provides hardware abstraction: GPIO pins, codec type, display type, network type
3. Audio pipeline: I2S → codec → OGG demuxer → OPUS decode → speaker; mic → OPUS encode → server
4. Protocol layer sends/receives binary OPUS frames + JSON control messages over WebSocket or MQTT+UDP
5. `DeviceStateMachine` orchestrates state transitions based on server messages and local events (wake word, button press)
6. MCP server exposes device capabilities (volume, LEDs, GPIO, motors, etc.) to the cloud for LLM tool-calling

### Board Architecture

Each board directory must contain:
- **`config.h`** — GPIO pin assignments, display resolution, audio sample rates, codec I2C addresses
- **`config.json`** — build configuration (target chip, flash size, sdkconfig overrides, partition table)
- **`xxx_board.cc/h`** — board init class inheriting from one of:
  - `WifiBoard` — WiFi-only devices
  - `Ml307Board` — 4G Cat.1 (ML307) devices
  - `Nt26Board` — 4G (NT26) devices
  - `DualNetworkBoard` — WiFi + 4G switchable
  - `RndisBoard` — USB RNDIS networking

Board selection goes through `main/CMakeLists.txt` which maps `CONFIG_BOARD_TYPE_*` Kconfig symbols to source file globs.

### Partition Tables

Two incompatible versions (v1/v2) under `partitions/`. v2 adds an `assets` partition (spiffs) for network-loadable wake word models, fonts, themes, and emoji packs. v1 uses a fixed `model` partition. Sizes: 4M, 8M, 16M, 16M_c3, 32M variants.

### Kconfig (`main/Kconfig.projbuild`)

Key menus: OTA URL → Flash Assets → Default Language (38 languages) → Network Type → Board Type → Display Style → Wake Word → Audio Processing → WiFi Config → Camera Config

Chip-specific sdkconfig defaults in root: `sdkconfig.defaults.esp32`, `.esp32c3`, `.esp32c5`, `.esp32c6`, `.esp32p4`, `.esp32s3`

## Adding a New Board

See `docs/custom-board_zh.md`. Steps:
1. Create directory `main/boards/<name>/` (use `manufacturer/name` for grouped boards)
2. Add `config.h` (pin definitions), `config.json` (build settings), and `xxx_board.cc/h` (board init class)
3. Add a `CONFIG_BOARD_TYPE_<YOUR_BOARD>` entry in `main/Kconfig.projbuild`
4. Add the corresponding `elseif(CONFIG_BOARD_TYPE_<YOUR_BOARD>)` branch in `main/CMakeLists.txt`
5. Run `python scripts/release.py <board-dir-name>` to build

## CI/CD

GitHub Actions (`.github/workflows/build.yml`):
- Push to `main` → builds ALL board variants
- PR to `main` → builds only affected boards (detected via `git diff` against changed `main/boards/*` directories; any change outside `main/boards/` triggers full build)
- Uses `espressif/idf:v5.5.2` Docker container, artifacts uploaded as `merged-binary.bin`

## Key Docs

- `docs/custom-board_zh.md` — custom board creation guide
- `docs/mcp-usage_zh.md` — MCP IoT control
- `docs/mcp-protocol_zh.md` — device MCP protocol implementation
- `docs/mqtt-udp_zh.md` — MQTT+UDP protocol spec
- `docs/websocket_zh.md` — WebSocket protocol spec
- `partitions/v2/README.md` — v2 partition table documentation
