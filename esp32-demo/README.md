# DSOTRON-CSI: WiFi Presence Detection on ESP32

ESP32 определяет движение в комнате **без сенсоров** — только по изменению WiFi сигнала (CSI).

Демонстрирует DSO-TRON: детерминированное ядро RTOS на Core 0, FreeRTOS/ESP-IDF на Core 1.

## How it works

```
Core 0 (DSO-TRON):                         Core 1 (FreeRTOS):
┌──────────────────────────────┐          ┌──────────────────────┐
│ CSI_PROC (T=100ms, off=0)   │          │ WiFi CSI callback →  │
│   read SPSC FIFO, smooth    │ ← FIFO ← │ extract I/Q amp      │
│ DETECTOR (T=100ms, off=50ms)│          │                      │
│   detect motion from delta  │          │ HTTP + WebSocket     │
│ STATS (T=1s)                │ → shared │ push JSON to browser │
│   publish to shared state   │  stats   │ CHAOS (malloc storm) │
└──────────────────────────────┘          └──────────────────────┘
```

## Requirements

- ESP32 board (any: WROOM, WROVER, DevKit)
- USB cable
- WiFi router near the board (for CSI packets)

## Build & Flash

```bash
# Install ESP-IDF v5.x
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
source export.sh

# Build DSOTRON-CSI
cd /path/to/dso-tron/esp32-demo
idf.py set-target esp32
idf.py menuconfig
#   → Component config → WiFi → enable "WiFi CSI"
#   → DSOTRON-CSI Configuration → set your SSID/password
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Usage

1. Flash ESP32, open serial monitor
2. ESP32 connects to your WiFi, starts HTTP server
3. Open `http://dsotron-csi.local/` or find IP in serial output
4. Walk near the ESP32 — CSI waterfall shows signal changes
5. Click "ACTIVATE CHAOS" — Core 1 gets stress, Core 0 unaffected

## Files

| File | Purpose |
|------|---------|
| `app_main.c` | Entry: init DSO kernel, register TT schedule |
| `csi_capture.c` | WiFi STA init, CSI enable, SPSC push |
| `csi_shared.h` | CSI sample struct, SPSC FIFO declaration |
| `presence.c` | Movement detection via CSI delta |
| `webserver.c` | HTTP + WebSocket real-time UI |
| `chaos.c` | malloc storm + CPU burn on Core 1 |
| `ws_html.h` | Embedded web UI (waterfall graph) |

## DSO Kernel Sources

DSO kernel (`kernel/`, `include/`) and Xtensa port (`sysdepend/xtensa/`)
are shared from the parent project. Modify with care.
