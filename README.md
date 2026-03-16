# Inkless

A wireless thermal receipt printer powered by an ESP32 and a QR204 58mm thermal print mechanism. Print text, images, QR codes, and receipts over WiFi from a browser or the command line.

## Hardware

### Parts

| Part | Notes |
|------|-------|
| ESP32 dev board | Any ESP32-WROOM-32 variant |
| QR204 58mm thermal printer | 384 dots/line, 203 DPI, ESC/POS, 5–9V ([datasheet](https://probots.co.in/technical_data/QR204%20-Thermal%20printer%20_Datasheet%20&%20User%20manual.pdf)) |
| 58mm thermal paper rolls | Standard receipt paper |
| 5V power supply | Shared or separate for printer; USB for ESP32 |

### Wiring

```
ESP32 GPIO17 (TX2) ──→ QR204 RX
ESP32 GND          ──→ QR204 GND
5V supply           ──→ QR204 VH (printer power)
```

The printer UART runs at **9600 baud** (8N1). GPIO16 (RX2) is reserved but the printer doesn't send data back.

## Setup

### 1. Clone and configure WiFi

```bash
git clone <repo-url>
cd receipt-printer
cp firmware/include/secrets.h.example firmware/include/secrets.h
```

Edit `firmware/include/secrets.h` with your WiFi credentials:

```cpp
#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD  "your-wifi-password"
```

### 2. Flash firmware

Requires [PlatformIO](https://platformio.org/install/cli). Connect the ESP32 via USB:

```bash
cd firmware
pio run -e esp32 -t upload
```

On success the printer prints a startup receipt with its IP address and `printer.local`.

### 3. Install the CLI

Requires Python 3.10+:

```bash
cd cli
pip install -e .
```

Verify:

```bash
rp status
# Online | FW 1.0.0 | Uptime 42s
```

## Usage

### Web UI

Open **http://printer.local** in a browser. The web interface has a text box for quick messages, plus feed and cut buttons.

### CLI commands

Every command accepts `--printer URL` (or `-p`, or the `PRINTER_URL` env var) to override the default `http://printer.local`.

```bash
# Print text
rp text "Hello World"
rp text "BOLD CENTERED" --bold --align center --size 2

# Print a QR code
rp qr "https://example.com" --label "Scan me" --size 6

# Print an image (auto-resized to 384px, dithered to 1-bit)
rp image photo.png

# Print a formatted note
rp note "Grocery List" "Eggs, milk, bread, coffee"

# Print a todo checklist
rp todo "Morning" "Make bed" "Meditate" "Exercise"

# Print a receipt
# (use the HTTP API directly for receipts — see below)

# Paper control
rp feed          # feed 3 lines
rp feed -n 6     # feed 6 lines
rp cut           # cut paper

# Send raw ESC/POS bytes (hex-encoded)
rp raw "1b40"    # printer reset

# Check status
rp status
```

### HTTP API

All endpoints are at `http://printer.local`.

| Method | Path | Content-Type | Description |
|--------|------|-------------|-------------|
| `GET` | `/` | — | Web UI |
| `GET` | `/status` | — | `{status, firmware, hostname, uptime_s}` |
| `POST` | `/print/text` | `application/json` | Print formatted text |
| `POST` | `/print/receipt` | `application/json` | Print a receipt |
| `POST` | `/print/image` | `application/json` | Print a raster bitmap |
| `POST` | `/print/qrcode` | `application/json` | Print a QR code |
| `POST` | `/print/raw` | `application/json` | Send raw ESC/POS bytes |
| `POST` | `/submit` | `form-data` | Quick print (used by web UI) |
| `POST` | `/feed` | `form-data` | Feed paper |
| `POST` | `/cut` | — | Cut paper |
| `GET` | `/update` | — | OTA firmware upload form |
| `POST` | `/update` | `multipart/form-data` | OTA firmware upload |

#### Examples

```bash
# Print text
curl -X POST http://printer.local/print/text \
  -H "Content-Type: application/json" \
  -d '{"text": "Hello!", "bold": true, "align": 1}'

# Print receipt
curl -X POST http://printer.local/print/receipt \
  -H "Content-Type: application/json" \
  -d '{
    "title": "COFFEE SHOP",
    "items": [
      {"name": "Latte", "price": "$4.50"},
      {"name": "Croissant", "price": "$3.00"}
    ],
    "total": "$7.50",
    "footer": "Thank you!"
  }'

# Print QR code
curl -X POST http://printer.local/print/qrcode \
  -H "Content-Type: application/json" \
  -d '{"data": "https://example.com", "label": "Scan me", "size": 6}'
```

## OTA Firmware Updates

After the initial USB flash, all future updates can be done over WiFi:

```bash
# Option 1: PlatformIO OTA (builds + uploads)
cd firmware
pio run -e ota -t upload

# Option 2: CLI (upload a pre-built .bin)
rp update firmware/.pio/build/esp32/firmware.bin

# Option 3: Browser
# Open http://printer.local/update and upload the .bin file
```

The printer reboots automatically after a successful update. Run `rp status` after a few seconds to confirm it's back online.

## Project Structure

```
receipt-printer/
├── firmware/
│   ├── platformio.ini          # Build config (esp32 + ota envs)
│   ├── include/
│   │   ├── secrets.h.example   # WiFi credentials template
│   │   └── secrets.h           # Your credentials (gitignored)
│   └── src/
│       ├── main.cpp            # WiFi, mDNS, NTP, ArduinoOTA, startup
│       ├── config.h            # Pin definitions, constants
│       ├── escpos.h / .cpp     # ESC/POS printer driver
│       ├── routes.h / .cpp     # HTTP API endpoints
│       └── web_ui.h            # Embedded HTML (PROGMEM)
└── cli/
    ├── pyproject.toml          # Python package config
    └── src/receipt_printer/
        ├── main.py             # Typer CLI (rp command)
        ├── client.py           # HTTP client
        ├── imaging.py          # Image → 1-bit raster bitmap
        └── formatting.py       # Note/todo print templates
```

## Printer Specs

| Spec | Value |
|------|-------|
| Print width | 48mm (384 dots) |
| Resolution | 203 DPI (8 dots/mm) |
| Characters/line | 32 |
| Paper width | 58mm |
| Interface | TTL UART 9600 baud |
| Power | 5–9V DC |
| Command set | ESC/POS |
| Print speed | 50–90 mm/s |
