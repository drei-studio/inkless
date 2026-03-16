#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <time.h>

#include "config.h"
#include "escpos.h"
#include "routes.h"

// WiFi credentials from gitignored secrets.h
#include "secrets.h"

EscPosWriter printer;
AsyncWebServer server(HTTP_PORT);

void setup() {
    Serial.begin(115200);
    Serial.println("\n[receipt-printer] Starting...");

    // Init printer on Serial2
    Serial2.begin(PRINTER_BAUD, SERIAL_8N1, PRINTER_RX_PIN, PRINTER_TX_PIN);
    printer.begin(Serial2);

    // Connect WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(MDNS_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[wifi] Connecting");

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 30000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] FAILED - restarting in 5s");
        delay(5000);
        ESP.restart();
    }

    Serial.print("[wifi] Connected: ");
    Serial.println(WiFi.localIP());

    // Start mDNS
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[mdns] http://%s.local\n", MDNS_HOSTNAME);
    }

    // NTP time sync
    configTime(UTC_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    Serial.println("[ntp] Syncing...");

    // HTTP routes
    setupRoutes(server, &printer);
    server.begin();
    Serial.println("[http] Server started on port 80");

    // Print startup receipt
    printer.setAlign(1);
    printer.setBold(true);
    printer.setFontSize(2, 2);
    printer.printLine("PRINTER ONLINE");
    printer.setFontSize(1, 1);
    printer.setBold(false);
    printer.printLine("");
    printer.printLine(WiFi.localIP().toString().c_str());
    printer.printLine((String(MDNS_HOSTNAME) + ".local").c_str());
    printer.printLine("");
    printer.printLine(("FW " + String(FW_VERSION)).c_str());
    printer.setAlign(0);
    printer.feed(3);
}

void loop() {
    // ESPAsyncWebServer handles requests in the background
    delay(10);
}
