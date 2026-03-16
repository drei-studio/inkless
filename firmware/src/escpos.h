#pragma once

#include <Arduino.h>
#include "config.h"

class EscPosWriter {
public:
    void begin(HardwareSerial &serial);

    // Reset
    void reset();

    // Text output
    void printText(const char *text);
    void printLine(const char *text);

    // Formatting
    void setBold(bool on);
    void setUnderline(bool on);
    void setInverse(bool on);
    void setAlign(uint8_t align);  // 0=left, 1=center, 2=right
    void setFontSize(uint8_t width, uint8_t height);  // 1-8x multipliers

    // Paper control
    void feed(uint8_t lines = 3);
    void cut();

    // Special
    void printQRCode(const char *data, uint8_t moduleSize = 4);
    void printRasterBitmap(const uint8_t *data, uint16_t width, uint16_t height);
    void printWrappedReversed(const char *text);
    void sendRaw(const uint8_t *data, size_t len);

private:
    HardwareSerial *_serial = nullptr;
    void sendCommand(const uint8_t *cmd, size_t len);
    void writeBytes(const uint8_t *data, size_t len);
};
