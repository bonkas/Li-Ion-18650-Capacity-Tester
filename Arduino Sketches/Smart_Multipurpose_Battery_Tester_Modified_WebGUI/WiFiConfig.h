#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// ========================================= WiFi CONFIGURATION ========================================

// Access Point (AP) Mode Settings - Device creates its own WiFi network
#define AP_SSID "BatteryTester"
#define AP_PASSWORD "battery123"  // Minimum 8 characters, or empty for open network
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4

// Station (STA) Mode Settings - Device connects to existing WiFi
// These can be configured via the web interface
String sta_ssid = "";
String sta_password = "";
bool sta_enabled = false;

// Web Server Settings
#define WEB_SERVER_PORT 80
#define WEBSOCKET_PATH "/ws"

// WiFi Mode (prefixed to avoid conflict with ESP32 WiFi library)
enum WiFiModeConfig {
    CFG_WIFI_AP,      // Access Point only
    CFG_WIFI_STA,     // Station only (connect to existing WiFi)
    CFG_WIFI_BOTH     // Both AP and STA
};

WiFiModeConfig wifiMode = CFG_WIFI_AP;  // Default to AP mode

// Connection timeout for STA mode (milliseconds)
#define STA_CONNECT_TIMEOUT 10000

// IP Address for AP mode
// Default: 192.168.4.1
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

#endif // WIFI_CONFIG_H
