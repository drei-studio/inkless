#pragma once

// -- Printer UART --
#define PRINTER_TX_PIN   17    // ESP32 GPIO17 (TX2) → QR204 RX
#define PRINTER_RX_PIN   16    // ESP32 GPIO16 (RX2) ← QR204 TX (optional)
#define PRINTER_BAUD     9600

// -- Network --
#define MDNS_HOSTNAME    "printer"   // printer.local
#define HTTP_PORT        80

// -- Printer specs --
#define DOTS_PER_LINE    384
#define BYTES_PER_LINE   (DOTS_PER_LINE / 8)  // 48
#define CHARS_PER_LINE   32

// -- Firmware --
#define FW_VERSION       "1.0.0"

// -- NTP --
#define NTP_SERVER       "pool.ntp.org"
#define UTC_OFFSET_SEC   0       // Adjust for your timezone
#define DST_OFFSET_SEC   0
