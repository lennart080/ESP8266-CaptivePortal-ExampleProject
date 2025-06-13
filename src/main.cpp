// Licensed under MIT
// SPDX-License-Identifier: MIT
// ESP8266 WiFi Setup with Captive Portal Example
// © 2025 Lennart Gutjahr

#include <Arduino.h>
#include <CaptivePortal.h>

const char* ap_ssid = "ESP8266-WiFi-Setup"; // SSID for the Access Point
const char* ap_password = "12345678"; // password for the Access Point

// Create a CaptivePortal instance
CaptivePortal portal;

// Variables to store received WiFi credentials
String receivedSSID;
String receivedPassword;

// Cached list of WiFi networks (as JSON)
String wifiList;

// Timeout for WiFi connection attempt (in milliseconds)
int wifiScanTimeout = 10000;

// Flag to indicate when to stop the Access Point (AP)
bool shouldStopAP = false;

// Waits for the WiFi mode to switch to the target mode, or until timeout
void waitForWiFiMode(WiFiMode_t targetMode, unsigned long timeoutMs = 2000) {
    unsigned long start = millis();
    while (WiFi.getMode() != targetMode && (millis() - start) < timeoutMs) {
        delay(10);
    }
}

// Scans for available WiFi networks and returns a JSON array of SSIDs
String createWifiJson() {
    int n = WiFi.scanNetworks();
    String ssids[n];
    int count = 0;

    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        // Skip empty SSIDs, the AP's own SSID, or SSIDs that are too long
        if (ssid.length() == 0 || ssid == ap_ssid || ssid.length() > 32) {
            continue;
        }
        // Avoid duplicate SSIDs
        bool duplicate = false;
        for (int j = 0; j < count; ++j) {
            if (ssids[j] == ssid) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            ssids[count++] = ssid;
        }
    }

    // Build JSON array string
    String list = "[";
    for (int i = 0; i < count; ++i) {
        if (i) list += ",";
        list += "\"" + ssids[i] + "\"";
    }
    list += "]";
    return list;
}

// Main loop for the Access Point (AP) mode
void apLoop() {
    while (!shouldStopAP) {
        portal.processDNS(); // Handle DNS requests for captive portal
        delay(50);  
    }
    delay(2000); // Wait before stopping AP
    if (portal.stopAP()) {
        shouldStopAP = false;
        Serial.println("Captive Portal stopped.");
    }
}

// Checks if the provided SSID and password are valid
bool paramCheck(String ssid, String password) {
    if (ssid.length() < 1 || ssid.length() > 32) {
        return false;
    }
    if (password.length() < 8 || password.length() > 63) {
        return false;
    }
    return true;
}

// Sets up the ESP8266 as an open WiFi Access Point with a captive portal
void apSetup() {
    WiFi.mode(WIFI_AP); // Set WiFi mode to Access Point (AP)
    waitForWiFiMode(WIFI_AP);

    wifiList = createWifiJson(); // Scan for WiFi networks and cache the list as JSON

    portal.initializeOpen(ap_ssid, "index.html"); // Start open AP with captive portal
    // To use a password for the AP, use the line below instead:
    // portal.initialize(ap_ssid, ap_password, "index.html");

    // Handle POST requests to /api/setupWiFi for receiving credentials
    portal.getServer().on("/api/setupWiFi", HTTP_POST, [&](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            receivedSSID = request->getParam("ssid", true)->value();
            receivedPassword = request->getParam("password", true)->value();

            if (paramCheck(receivedSSID, receivedPassword)) {
                request->send(200, "text/plain", "WiFi credentials and token received. This Portal will close now.");
                shouldStopAP = true; // Signal to stop AP
            } else {
                request->send(400, "text/plain", "Invalid parameters");
            }
        } else {
            request->send(400, "text/plain", "Missing parameters");
        }
    });

    // Handle GET requests to /api/scan for returning the WiFi list
    portal.getServer().on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", wifiList);
    });

    portal.startAP(); // Start the captive portal AP
    Serial.println("Captive Portal started. Waiting for WiFi credentials...");
    apLoop(); // Enter AP loop until credentials are received
}

// Arduino setup function
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Starting ESP8266 WiFi Setup...");

    apSetup(); // Start captive portal and wait for credentials

    Serial.println("Credentials received:");
    Serial.print("SSID: " + receivedSSID);
    Serial.print("Password: " + receivedPassword);

    WiFi.mode(WIFI_STA); // Switch to station mode
    waitForWiFiMode(WIFI_AP); // Wait for mode change
    WiFi.begin(receivedSSID, receivedPassword); // Attempt WiFi connection
    
    long start = millis();
    // Wait for connection or timeout
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < wifiScanTimeout) {
        Serial.print(".");
        delay(200);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("SSID: " + WiFi.SSID());
        Serial.print("Password: " + receivedPassword);
    } else {
        Serial.println("Failed to connect to WiFi");
        Serial.println("Please try again.");
    }

    // Your additional setup code here
}

// Arduino main loop function
void loop() {
    delay(1000);
    // Your main loop code here
}
