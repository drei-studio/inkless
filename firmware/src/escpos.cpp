#include "escpos.h"

void EscPosWriter::begin(HardwareSerial &serial) {
    _serial = &serial;
    _mutex = xSemaphoreCreateMutex();
    reset();
    // Heat settings: max dots=15, heat time=150, interval=250
    uint8_t heat[] = {0x1B, 0x37, 0x0F, 0x96, 0xFA};
    sendCommand(heat, sizeof(heat));
    // Tighter line spacing: ESC 3 22 (~2.75mm vs default ~4mm)
    uint8_t lineSpacing[] = {0x1B, 0x33, 0x06};
    sendCommand(lineSpacing, sizeof(lineSpacing));
    // Select codepage 437 (US) to avoid Chinese character fallback
    uint8_t codepage[] = {0x1B, 0x74, 0x00};  // ESC t 0
    sendCommand(codepage, sizeof(codepage));
    delay(500);
}

void EscPosWriter::lock() {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
}

void EscPosWriter::unlock() {
    if (_mutex) xSemaphoreGive(_mutex);
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

    // Print in strips of 16 rows to avoid buffer overflow
    const uint16_t STRIP_H = 16;
    uint16_t rowsDone = 0;

    while (rowsDone < height) {
        uint16_t stripRows = min((uint16_t)STRIP_H, (uint16_t)(height - rowsDone));

        // GS v 0 m xL xH yL yH
        uint8_t cmd[] = {0x1D, 0x76, 0x30, 0x00,
                         (uint8_t)(bytesPerRow & 0xFF),
                         (uint8_t)((bytesPerRow >> 8) & 0xFF),
                         (uint8_t)(stripRows & 0xFF),
                         (uint8_t)((stripRows >> 8) & 0xFF)};
        sendCommand(cmd, sizeof(cmd));

        size_t stripBytes = (size_t)bytesPerRow * stripRows;
        const uint8_t *stripData = data + (size_t)rowsDone * bytesPerRow;
        size_t offset = 0;
        while (offset < stripBytes) {
            size_t chunk = min((size_t)bytesPerRow, stripBytes - offset);
            writeBytes(stripData + offset, chunk);
            _serial->flush();  // wait for TX to fully drain
            offset += chunk;
            delay(20);
        }
        delay(200);  // let printer finish the strip
        rowsDone += stripRows;
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

void EscPosWriter::sendRaw(const uint8_t *data, size_t len) {
    writeBytes(data, len);
}

// --- ESC * bit-image printing ---
// Uses ESC * 33 nL nH d1..dk — mode 33 (24-dot double density, 203x203 DPI)
// Processes 24 rows at a time: 3 bytes per column (top, middle, bottom 8 dots).
// This is the standard approach used by most ESC/POS image printing libraries.

static void sendBitImageStrip24(HardwareSerial *serial, const uint8_t *colData, uint16_t nDots) {
    // ESC * 33 nL nH — 24-dot double density
    uint8_t cmd[] = {0x1B, 0x2A, 0x21,
                     (uint8_t)(nDots & 0xFF),
                     (uint8_t)((nDots >> 8) & 0xFF)};
    serial->write(cmd, sizeof(cmd));
    serial->write(colData, (size_t)nDots * 3);  // 3 bytes per column
    serial->write('\n');
    serial->flush();
    delay(30);
}

// Helper: convert row-major bitmap to 24-dot column format for one strip
static void rowsToColumns24(const uint8_t *rowData, uint16_t bytesPerRow,
                            uint16_t width, uint16_t rowsInStrip,
                            uint8_t *colBuf, bool invert) {
    memset(colBuf, 0, (size_t)width * 3);
    for (uint16_t x = 0; x < width; x++) {
        uint8_t bitMask = 0x80 >> (x % 8);
        uint16_t byteCol = x / 8;
        for (uint16_t bit = 0; bit < rowsInStrip; bit++) {
            bool pixel = rowData[bit * bytesPerRow + byteCol] & bitMask;
            if (invert) pixel = !pixel;
            if (pixel) {
                // 3 bytes per column: byte 0 = top 8, byte 1 = mid 8, byte 2 = bottom 8
                colBuf[x * 3 + bit / 8] |= (0x80 >> (bit % 8));
            }
        }
    }
}

void EscPosWriter::printBitImage(const uint8_t *data, uint16_t width, uint16_t height) {
    if (!_serial) return;

    uint16_t bytesPerRow = (width + 7) / 8;
    uint8_t colBuf[384 * 3];  // 3 bytes per column

    uint8_t lineSpacing[] = {0x1B, 0x33, 0x18};  // ESC 3 24
    sendCommand(lineSpacing, sizeof(lineSpacing));

    for (uint16_t stripY = 0; stripY < height; stripY += 24) {
        uint16_t rowsInStrip = min((uint16_t)24, (uint16_t)(height - stripY));
        const uint8_t *stripData = data + (size_t)stripY * bytesPerRow;
        rowsToColumns24(stripData, bytesPerRow, width, rowsInStrip, colBuf, false);
        sendBitImageStrip24(_serial, colBuf, width);
    }

    uint8_t defaultSpacing[] = {0x1B, 0x33, 0x06};  // ESC 3 22
    sendCommand(defaultSpacing, sizeof(defaultSpacing));
    delay(100);
    Serial.println("[escpos] Bit image done");
}

void EscPosWriter::printBitImageProgmem(const uint8_t *progmemData, uint16_t width, uint16_t height) {
    if (!_serial) return;

    uint16_t bytesPerRow = (width + 7) / 8;
    const uint16_t STRIP_H = 16;
    uint8_t rowBuf[48];  // one row, max 384 dots wide

    uint16_t rowsDone = 0;
    while (rowsDone < height) {
        uint16_t stripRows = min((uint16_t)STRIP_H, (uint16_t)(height - rowsDone));

        uint8_t cmd[] = {0x1D, 0x76, 0x30, 0x00,
                         (uint8_t)(bytesPerRow & 0xFF),
                         (uint8_t)((bytesPerRow >> 8) & 0xFF),
                         (uint8_t)(stripRows & 0xFF),
                         (uint8_t)((stripRows >> 8) & 0xFF)};
        sendCommand(cmd, sizeof(cmd));

        for (uint16_t r = 0; r < stripRows; r++) {
            memcpy_P(rowBuf, progmemData + (size_t)(rowsDone + r) * bytesPerRow, bytesPerRow);
            writeBytes(rowBuf, bytesPerRow);
            _serial->flush();
            delay(20);
        }
        delay(200);
        rowsDone += stripRows;
    }
    delay(100);
    Serial.println("[escpos] Bit image done (PROGMEM)");
}

void EscPosWriter::printBitImageProgmemInverted(const uint8_t *progmemData, uint16_t width, uint16_t height) {
    if (!_serial) return;

    uint16_t bytesPerRow = (width + 7) / 8;
    const uint16_t STRIP_H = 16;
    uint8_t rowBuf[48];  // one row, max 384 dots wide

    uint16_t rowsDone = 0;
    while (rowsDone < height) {
        uint16_t stripRows = min((uint16_t)STRIP_H, (uint16_t)(height - rowsDone));

        uint8_t cmd[] = {0x1D, 0x76, 0x30, 0x00,
                         (uint8_t)(bytesPerRow & 0xFF),
                         (uint8_t)((bytesPerRow >> 8) & 0xFF),
                         (uint8_t)(stripRows & 0xFF),
                         (uint8_t)((stripRows >> 8) & 0xFF)};
        sendCommand(cmd, sizeof(cmd));

        for (uint16_t r = 0; r < stripRows; r++) {
            memcpy_P(rowBuf, progmemData + (size_t)(rowsDone + r) * bytesPerRow, bytesPerRow);
            for (uint16_t b = 0; b < bytesPerRow; b++) rowBuf[b] ^= 0xFF;
            writeBytes(rowBuf, bytesPerRow);
            _serial->flush();
            delay(20);
        }
        delay(200);
        rowsDone += stripRows;
    }
    delay(100);
    Serial.println("[escpos] Bit image done (inverted)");
}

// --- Diagnostic test pattern ---

void EscPosWriter::printTestPattern() {
    if (!_serial) return;

    // Print 24 rows of solid black using ESC * mode 33
    uint8_t lineSpacing[] = {0x1B, 0x33, 0x18};
    sendCommand(lineSpacing, sizeof(lineSpacing));

    uint8_t colBuf[384 * 3];
    memset(colBuf, 0xFF, sizeof(colBuf));
    sendBitImageStrip24(_serial, colBuf, 384);

    uint8_t defaultSpacing[] = {0x1B, 0x33, 0x06};  // ESC 3 22
    sendCommand(defaultSpacing, sizeof(defaultSpacing));
    delay(100);
    Serial.println("[escpos] Test pattern printed");
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
