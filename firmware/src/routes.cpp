#include "routes.h"
#include "web_ui.h"
#include "config.h"
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <time.h>

static String getFormattedDateTime() {
    time_t now;
    time(&now);
    struct tm *ti = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M", ti);
    return String(buf);
}

// Accumulate body chunks into a String stored on the heap.
// We use a String* stored in _tempObject to avoid conflicts with
// ESPAsyncWebServer's internal use of _tempObject for param parsing.
struct BodyBuffer {
    uint8_t *data;
    size_t total;
};

static void accumulateBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    BodyBuffer *buf;
    if (index == 0) {
        buf = new BodyBuffer();
        buf->data = (uint8_t *)malloc(total + 1);
        buf->total = total;
        if (!buf->data) { delete buf; return; }
        request->_tempObject = buf;
    }
    buf = (BodyBuffer *)request->_tempObject;
    if (!buf || !buf->data) return;
    memcpy(buf->data + index, data, len);
    if (index + len == total) {
        buf->data[total] = '\0';
    }
}

static char *getBody(AsyncWebServerRequest *request) {
    BodyBuffer *buf = (BodyBuffer *)request->_tempObject;
    if (!buf || !buf->data) return nullptr;
    return (char *)buf->data;
}

static void freeBody(AsyncWebServerRequest *request) {
    BodyBuffer *buf = (BodyBuffer *)request->_tempObject;
    if (buf) {
        free(buf->data);
        delete buf;
        request->_tempObject = nullptr;
    }
}

void setupRoutes(AsyncWebServer &server, EscPosWriter *printer) {

    // --- GET / : Web UI ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", WEB_UI_HTML);
    });

    // --- GET /status ---
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["status"] = "online";
        doc["firmware"] = FW_VERSION;
        doc["hostname"] = MDNS_HOSTNAME;
        doc["uptime_s"] = millis() / 1000;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // --- POST /submit : Scribe-compatible endpoint ---
    server.on("/submit", HTTP_POST, [printer](AsyncWebServerRequest *request) {
        String message;
        String date;

        if (request->hasParam("message", true)) {
            message = request->getParam("message", true)->value();
        }
        if (request->hasParam("date", true)) {
            date = request->getParam("date", true)->value();
        }

        if (message.isEmpty()) {
            request->send(400, "text/plain", "Missing 'message' parameter");
            return;
        }

        if (date.isEmpty()) {
            date = getFormattedDateTime();
        }

        // Print header (inverse, like Scribe)
        printer->setAlign(1);
        printer->setInverse(true);
        printer->printLine((" " + date + " ").c_str());
        printer->setInverse(false);
        printer->setAlign(0);
        printer->printLine("");

        // Print message upside-down so it reads naturally when torn off
        printer->printWrappedReversed(message.c_str());
        printer->feed(4);

        Serial.println("[submit] " + message);
        request->send(200, "text/plain", "Printed!");
    });

    // --- POST /print/text ---
    server.on("/print/text", HTTP_POST,
        [printer](AsyncWebServerRequest *request) {
            char *body = getBody(request);
            if (!body) {
                request->send(400, "application/json", "{\"error\":\"No body\"}");
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, body, strlen(body))) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *text = doc["text"] | "";
            if (strlen(text) == 0) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Missing 'text'\"}");
                return;
            }

            bool bold = doc["bold"] | false;
            bool underline = doc["underline"] | false;
            uint8_t align = doc["align"] | 0;
            uint8_t fontW = doc["font_width"] | 1;
            uint8_t fontH = doc["font_height"] | 1;

            printer->setAlign(align);
            printer->setBold(bold);
            printer->setUnderline(underline);
            printer->setFontSize(fontW, fontH);

            printer->printLine(text);

            printer->setBold(false);
            printer->setUnderline(false);
            printer->setAlign(0);
            printer->setFontSize(1, 1);
            printer->feed(2);

            freeBody(request);
            request->send(200, "application/json", "{\"status\":\"printed\"}");
        },
        NULL,
        accumulateBody
    );

    // --- POST /print/receipt ---
    server.on("/print/receipt", HTTP_POST,
        [printer](AsyncWebServerRequest *request) {
            char *body = getBody(request);
            if (!body) {
                request->send(400, "application/json", "{\"error\":\"No body\"}");
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, body, strlen(body))) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *title = doc["title"] | "RECEIPT";

            printer->setAlign(1);
            printer->setBold(true);
            printer->setFontSize(2, 2);
            printer->printLine(title);
            printer->setFontSize(1, 1);
            printer->setBold(false);

            printer->printLine(getFormattedDateTime().c_str());
            printer->setAlign(0);
            printer->printLine("--------------------------------");

            JsonArray items = doc["items"].as<JsonArray>();
            for (JsonObject item : items) {
                const char *name = item["name"] | "";
                const char *price = item["price"] | "";
                int nameLen = strlen(name);
                int priceLen = strlen(price);
                int padding = max(1, CHARS_PER_LINE - nameLen - priceLen);
                String line = String(name);
                for (int i = 0; i < padding; i++) line += ' ';
                line += price;
                printer->printLine(line.c_str());
            }

            printer->printLine("--------------------------------");

            const char *total_str = doc["total"] | "";
            if (strlen(total_str) > 0) {
                printer->setBold(true);
                String totalLine = "TOTAL";
                int padding = max(1, CHARS_PER_LINE - 5 - (int)strlen(total_str));
                for (int i = 0; i < padding; i++) totalLine += ' ';
                totalLine += total_str;
                printer->printLine(totalLine.c_str());
                printer->setBold(false);
            }

            const char *footer = doc["footer"] | "";
            if (strlen(footer) > 0) {
                printer->printLine("");
                printer->setAlign(1);
                printer->printLine(footer);
                printer->setAlign(0);
            }

            printer->feed(3);
            freeBody(request);
            request->send(200, "application/json", "{\"status\":\"printed\"}");
        },
        NULL,
        accumulateBody
    );

    // --- POST /print/image ---
    server.on("/print/image", HTTP_POST,
        [printer](AsyncWebServerRequest *request) {
            char *body = getBody(request);
            if (!body) {
                request->send(400, "application/json", "{\"error\":\"No body\"}");
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, body, strlen(body))) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *b64 = doc["data"] | "";
            uint16_t width = doc["width"] | 384;
            uint16_t height = doc["height"] | 0;

            if (strlen(b64) == 0 || height == 0) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Missing 'data' or 'height'\"}");
                return;
            }

            size_t b64Len = strlen(b64);
            size_t outLen = 0;
            mbedtls_base64_decode(NULL, 0, &outLen, (const uint8_t *)b64, b64Len);

            uint8_t *bitmap = (uint8_t *)malloc(outLen);
            if (!bitmap) {
                freeBody(request);
                request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
                return;
            }

            size_t decoded = 0;
            mbedtls_base64_decode(bitmap, outLen, &decoded, (const uint8_t *)b64, b64Len);

            printer->printRasterBitmap(bitmap, width, height);
            printer->feed(2);
            free(bitmap);

            freeBody(request);
            request->send(200, "application/json", "{\"status\":\"printed\"}");
        },
        NULL,
        accumulateBody
    );

    // --- POST /print/qrcode ---
    server.on("/print/qrcode", HTTP_POST,
        [printer](AsyncWebServerRequest *request) {
            char *body = getBody(request);
            if (!body) {
                request->send(400, "application/json", "{\"error\":\"No body\"}");
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, body, strlen(body))) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *qrData = doc["data"] | "";
            const char *label = doc["label"] | "";
            uint8_t size = doc["size"] | 4;

            if (strlen(qrData) == 0) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Missing 'data'\"}");
                return;
            }

            printer->setAlign(1);
            printer->printQRCode(qrData, size);

            if (strlen(label) > 0) {
                printer->printLine(label);
            }

            printer->setAlign(0);
            printer->feed(2);

            freeBody(request);
            request->send(200, "application/json", "{\"status\":\"printed\"}");
        },
        NULL,
        accumulateBody
    );

    // --- POST /print/raw --- send raw ESC/POS bytes (base64-encoded)
    server.on("/print/raw", HTTP_POST,
        [printer](AsyncWebServerRequest *request) {
            char *body = getBody(request);
            if (!body) {
                request->send(400, "application/json", "{\"error\":\"No body\"}");
                return;
            }

            JsonDocument doc;
            if (deserializeJson(doc, body, strlen(body))) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *b64 = doc["data"] | "";
            if (strlen(b64) == 0) {
                freeBody(request);
                request->send(400, "application/json", "{\"error\":\"Missing 'data'\"}");
                return;
            }

            // Decode base64
            size_t b64Len = strlen(b64);
            size_t outLen = 0;
            mbedtls_base64_decode(NULL, 0, &outLen, (const uint8_t *)b64, b64Len);

            uint8_t *raw = (uint8_t *)malloc(outLen);
            if (!raw) {
                freeBody(request);
                request->send(500, "application/json", "{\"error\":\"Out of memory\"}");
                return;
            }

            size_t decoded = 0;
            mbedtls_base64_decode(raw, outLen, &decoded, (const uint8_t *)b64, b64Len);

            printer->sendRaw(raw, decoded);
            free(raw);
            freeBody(request);
            request->send(200, "application/json", "{\"status\":\"sent\"}");
        },
        NULL,
        accumulateBody
    );

    // --- POST /feed ---
    server.on("/feed", HTTP_POST, [printer](AsyncWebServerRequest *request) {
        uint8_t lines = 3;
        if (request->hasParam("lines", true)) {
            lines = request->getParam("lines", true)->value().toInt();
        }
        printer->feed(lines);
        request->send(200, "application/json", "{\"status\":\"fed\"}");
    });

    // --- POST /cut ---
    server.on("/cut", HTTP_POST, [printer](AsyncWebServerRequest *request) {
        printer->cut();
        request->send(200, "application/json", "{\"status\":\"cut\"}");
    });
}
