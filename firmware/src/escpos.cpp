#include "escpos.h"

void EscPosWriter::begin(HardwareSerial &serial) {
    _serial = &serial;
    reset();
    // Heat settings: max dots=15, heat time=150, interval=250
    uint8_t heat[] = {0x1B, 0x37, 0x0F, 0x96, 0xFA};
    sendCommand(heat, sizeof(heat));
    delay(500);
}

void EscPosWriter::reset() {
    uint8_t cmd[] = {0x1B, 0x40};  // ESC @
    sendCommand(cmd, sizeof(cmd));
    delay(50);
}

// --- Text output ---

void EscPosWriter::printText(const char *text) {
    _serial->print(text);
}

void EscPosWriter::printLine(const char *text) {
    _serial->println(text);
}

// --- Formatting ---

void EscPosWriter::setBold(bool on) {
    uint8_t cmd[] = {0x1B, 0x45, (uint8_t)(on ? 1 : 0)};  // ESC E n
    sendCommand(cmd, sizeof(cmd));
}

void EscPosWriter::setUnderline(bool on) {
    uint8_t cmd[] = {0x1B, 0x2D, (uint8_t)(on ? 1 : 0)};  // ESC - n
    sendCommand(cmd, sizeof(cmd));
}

void EscPosWriter::setInverse(bool on) {
    uint8_t cmd[] = {0x1D, 0x42, (uint8_t)(on ? 1 : 0)};  // GS B n
    sendCommand(cmd, sizeof(cmd));
}

void EscPosWriter::setAlign(uint8_t align) {
    uint8_t cmd[] = {0x1B, 0x61, align};  // ESC a n
    sendCommand(cmd, sizeof(cmd));
}

void EscPosWriter::setFontSize(uint8_t width, uint8_t height) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    // Width and height are 1-8, encoded as (w-1)<<4 | (h-1)
    uint8_t n = ((width - 1) & 0x07) << 4 | ((height - 1) & 0x07);
    uint8_t cmd[] = {0x1D, 0x21, n};  // GS ! n
    sendCommand(cmd, sizeof(cmd));
}

// --- Paper control ---

void EscPosWriter::feed(uint8_t lines) {
    uint8_t cmd[] = {0x1B, 0x64, lines};  // ESC d n
    sendCommand(cmd, sizeof(cmd));
}

void EscPosWriter::cut() {
    feed(3);
    uint8_t cmd[] = {0x1D, 0x56, 0x00};  // GS V 0 (full cut)
    sendCommand(cmd, sizeof(cmd));
}

// --- QR Code (GS ( k) ---

void EscPosWriter::printQRCode(const char *data, uint8_t moduleSize) {
    moduleSize = constrain(moduleSize, 1, 16);
    size_t len = strlen(data);

    // 1. Set model: QR Model 2
    uint8_t model[] = {0x1D, 0x28, 0x6B, 0x04, 0x00, 0x31, 0x41, 0x32, 0x00};
    sendCommand(model, sizeof(model));

    // 2. Set module size
    uint8_t size[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x43, moduleSize};
    sendCommand(size, sizeof(size));

    // 3. Set error correction level (M = 49)
    uint8_t ecl[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x45, 0x31};
    sendCommand(ecl, sizeof(ecl));

    // 4. Store data: pL pH = (len+3) low/high
    uint16_t storeLen = len + 3;
    uint8_t storeHead[] = {0x1D, 0x28, 0x6B,
                           (uint8_t)(storeLen & 0xFF),
                           (uint8_t)((storeLen >> 8) & 0xFF),
                           0x31, 0x50, 0x30};
    sendCommand(storeHead, sizeof(storeHead));
    writeBytes((const uint8_t *)data, len);

    // 5. Print
    uint8_t print[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30};
    sendCommand(print, sizeof(print));
    delay(100);
}

// --- Raster bitmap (GS v 0) ---

void EscPosWriter::printRasterBitmap(const uint8_t *data, uint16_t width, uint16_t height) {
    uint16_t bytesPerRow = (width + 7) / 8;

    // GS v 0 m xL xH yL yH
    uint8_t cmd[] = {0x1D, 0x76, 0x30, 0x00,
                     (uint8_t)(bytesPerRow & 0xFF),
                     (uint8_t)((bytesPerRow >> 8) & 0xFF),
                     (uint8_t)(height & 0xFF),
                     (uint8_t)((height >> 8) & 0xFF)};
    sendCommand(cmd, sizeof(cmd));

    // Stream bitmap data in chunks to avoid buffer overflow
    size_t totalBytes = (size_t)bytesPerRow * height;
    size_t offset = 0;
    while (offset < totalBytes) {
        size_t chunk = min((size_t)256, totalBytes - offset);
        writeBytes(data + offset, chunk);
        offset += chunk;
        delay(20);  // let printer buffer drain
    }
    delay(100);
}

// --- Upside-down printing (from Scribe) ---

void EscPosWriter::printWrappedReversed(const char *text) {
    // Enable 180-degree rotation
    uint8_t rotateOn[] = {0x1B, 0x7B, 0x01};  // ESC { 1
    sendCommand(rotateOn, sizeof(rotateOn));

    // Word-wrap into lines of CHARS_PER_LINE
    String input(text);
    String lines[64];
    int lineCount = 0;
    int pos = 0;

    while (pos < (int)input.length() && lineCount < 64) {
        // Find next newline within range
        int nlPos = input.indexOf('\n', pos);
        int endPos = (nlPos >= 0 && nlPos < pos + CHARS_PER_LINE) ? nlPos : -1;

        if (endPos >= 0) {
            lines[lineCount++] = input.substring(pos, endPos);
            pos = endPos + 1;
            continue;
        }

        if (pos + CHARS_PER_LINE >= (int)input.length()) {
            lines[lineCount++] = input.substring(pos);
            break;
        }

        // Find last space for word wrap
        int wrapAt = CHARS_PER_LINE;
        for (int i = pos + CHARS_PER_LINE; i > pos; i--) {
            if (input.charAt(i) == ' ') {
                wrapAt = i - pos;
                break;
            }
        }

        lines[lineCount++] = input.substring(pos, pos + wrapAt);
        pos += wrapAt;
        if (pos < (int)input.length() && input.charAt(pos) == ' ') pos++;
    }

    // Print in reverse order
    for (int i = lineCount - 1; i >= 0; i--) {
        _serial->println(lines[i]);
    }

    // Disable rotation
    uint8_t rotateOff[] = {0x1B, 0x7B, 0x00};  // ESC { 0
    sendCommand(rotateOff, sizeof(rotateOff));
}

// --- Internal helpers ---

void EscPosWriter::sendCommand(const uint8_t *cmd, size_t len) {
    if (_serial) {
        _serial->write(cmd, len);
    }
}

void EscPosWriter::writeBytes(const uint8_t *data, size_t len) {
    if (_serial) {
        _serial->write(data, len);
    }
}
