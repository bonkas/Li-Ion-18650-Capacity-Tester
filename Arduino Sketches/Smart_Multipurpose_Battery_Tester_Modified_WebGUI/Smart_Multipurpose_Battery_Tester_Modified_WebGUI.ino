//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// DIY Smart Multipurpose Battery Tester - Web GUI Version
// Based on original work by Open Green Energy, INDIA
// Web GUI and state machine refactor added
// https://www.instructables.com/DIY-Smart-Multipurpose-Battery-Tester/
// modified by bonkas https://www.github.com/bonkas
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
//
// CALIBRATION NOTE: Voltage Measurement Accuracy
// ==================================================
// The voltage reference (Vref_Voltage) can be adjusted to improve accuracy.
// This constant represents the LM385-1.2V reference voltage (U6 on PCB).
// Default value: 1.26V (factory default: 1.227V)
//
// How to calibrate:
// 1. Set the constant to a known good value (start with 1.227)
// 2. Measure a known battery voltage with this device
// 3. Compare with a calibrated multimeter
// 4. Calculate the error percentage:
//    Error% = (Device_Reading - Multimeter_Reading) / Multimeter_Reading * 100
// 5. Apply proportional adjustment:
//    If reading is HIGH (positive error):
//      Correction_Factor = 1 - (Error% / 100)
//      New_Vref = Old_Vref × Correction_Factor
//      Example: If 5% high: 1.227 × (1 - 0.05) = 1.227 × 0.95 = 1.166
//    If reading is LOW (negative error):
//      Correction_Factor = 1 - (Error% / 100)  [negative error = subtract]
//      New_Vref = Old_Vref × Correction_Factor
//      Example: If 3% low: 1.227 × (1 + 0.03) = 1.227 × 1.03 = 1.264
// 6. Recompile and test with another battery
// 7. Repeat steps 2-6 until readings match your multimeter (within ±2%)
//
// HARDWARE NOTES:
// ==================================================
// - Only use with single-cell Li-Ion batteries (3.7V nominal, 4.2V max)
// - High discharge currents (>1000mA): Ensure MOSFET has proper cooling
// - 1500mA and 2000mA settings require heatsink and adequate airflow
// - Never leave device unattended during charge/discharge cycles
//
// COMPONENT REFERENCES:
// ==================================================
// U6: LM385-1.2V (Voltage reference for ADC calibration)

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <JC_Button.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Include our header files
#include "WiFiConfig.h"
#include "DataLogger.h"
#include "WebContent.h"

// ========================================= OLED DISPLAY ========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========================================= BUTTONS ========================================
#define MODE_PIN D3
#define UP_PIN D6
#define DOWN_PIN D9

Button Mode_Button(MODE_PIN, 25, false, true);
Button UP_Button(UP_PIN, 25, false, true);
Button Down_Button(DOWN_PIN, 25, false, true);

// ========================================= STATE MACHINE ========================================
enum DeviceState {
    STATE_IDLE,
    STATE_MENU,
    STATE_SELECT_CUTOFF,
    STATE_SELECT_CURRENT,
    STATE_CHARGING,
    STATE_DISCHARGING,
    STATE_ANALYZE_CHARGE,
    STATE_ANALYZE_REST,
    STATE_ANALYZE_DISCHARGE,
    STATE_IR_MEASURE,
    STATE_IR_DISPLAY,
    STATE_COMPLETE,
    STATE_WIFI_INFO,
    STATE_BATTERY_CHECK,            // Real-time voltage monitoring
    STATE_ANALYZE_CONFIG_TOGGLE,    // Enable/disable staged mode
    STATE_ANALYZE_CONFIG_STAGE1,    // Stage 1: current + transition voltage
    STATE_ANALYZE_CONFIG_STAGE2,    // Stage 2: current + final cutoff
    STATE_STORAGE_PREP              // Storage prep: charge/discharge to 3.8V
};

DeviceState currentState = STATE_MENU;
DeviceState previousState = STATE_IDLE;
bool abortRequested = false;  // Flag for abort requests from web GUI

// ========================================= BATTERY SETTINGS ========================================
float cutoffVoltage = 3.0;  // Default discharge cutoff voltage
const float Min_BAT_level = 2.8;
const float Max_BAT_level = 3.2;
const float FULL_BAT_level = 4.18;
const float DAMAGE_BAT_level = 2.5;
const float NO_BAT_level = 0.3;

int Current[] = {0, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1500, 2000};
int PWM[] = {0, 4, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 150, 200};
int Array_Size = sizeof(Current) / sizeof(Current[0]);
int currentOffset = 25;
const int HIGH_CURRENT_THRESHOLD = 1000;  // mA - warn user above this
int PWM_Value = 0;
int PWM_Index = 6;  // Default to 500mA

float Capacity_f = 0;
float Vref_Voltage = 1.26;  // LM385-1.2V reference voltage ( adjust it for calibration, 1.227 default )
float Vcc = 3.3;
float BAT_Voltage = 0;
float internalResistance = 0;
float voltageNoLoad = 0;
float voltageLoad = 0;

// ========================================= STAGED ANALYZE SETTINGS ========================================
bool stagedAnalyzeEnabled = false;
int stage1CurrentIndex = 6;           // Index into Current[] array (default: 500mA)
float stage1TransitionVoltage = 3.3;  // Voltage to transition to Stage 2
int stage2CurrentIndex = 4;           // Index into Current[] array (default: 300mA)
float stage2FinalCutoff = 3.0;        // Final cutoff voltage
int analyzeDischargeStage = 1;        // Current stage during discharge (1 or 2)

// ========================================= TIMING ========================================
unsigned long previousMillis = 0;
const long displayInterval = 50;
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
unsigned long lastCapacityUpdate = 0;
unsigned long lastWsUpdate = 0;
unsigned long stateStartTime = 0;
unsigned long restStartTime = 0;

// AP disable timer - keep AP on for a period after STA connection so user can see new IP
unsigned long apDisableTime = 0;
const unsigned long AP_DISABLE_DELAY = 45000;  // 45 seconds before disabling AP
bool apDisablePending = false;

int Hour = 0;
int Minute = 0;
int Second = 0;

// ========================================= PINS ========================================
const byte PWM_Pin = D8;
const byte Buzzer = D7;
const int BAT_Pin = A0;
const int Vref_Pin = A1;
const byte Mosfet_Pin = D2;
// Note: A2 and D2 are the SAME pin (GPIO4) on XIAO ESP32C3!
// Cannot use A2 for CHRG reading as it conflicts with Mosfet_Pin

// Charge current set by R7 (1k) on LP4060: I = 1000mA
const int CHARGE_CURRENT_MA = 1000;
const int STORAGE_PREP_CURRENT_MA = 400;  // Lower current for storage prep precision
const float STORAGE_TARGET_VOLTAGE = 3.8;  // Ideal storage voltage for Li-Ion
const float STORAGE_VOLTAGE_TOLERANCE = 0.05;  // ±0.05V tolerance (3.75-3.85V range)

// Battery icon animation
int batteryLevel = 0;

// Voltage divider resistors
const float R1 = 200000.0;
const float R2 = 100000.0;

// Menu selection
int selectedMode = 0;

// ========================================= BUZZER/TONE SETTINGS ========================================
// LEDC PWM configuration for tone generation
const int LEDC_CHANNEL = 0;
const int LEDC_TIMER = 0;
const int LEDC_FREQUENCY = 5000;  // 5kHz PWM frequency
const int LEDC_RESOLUTION = 8;    // 8-bit resolution

// ========================================= WEB SERVER ========================================
AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws(WEBSOCKET_PATH);

// ========================================= PREFERENCES (NVS) ========================================
Preferences preferences;
const char* PREF_NAMESPACE = "wifi";
const char* PREF_SSID = "ssid";
const char* PREF_PASS = "password";

// ========================================= FUNCTION DECLARATIONS ========================================
void setupWiFi();
void setupWebServer();
bool connectToSTAWiFi();
void sendWiFiStatus();
void saveWiFiCredentials();
bool loadWiFiCredentials();
void clearWiFiCredentials();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void sendStatusUpdate();
void sendDataPoint();
void sendHistoryData(AsyncWebSocketClient *client);
void sendError(const char* message);
void processCommand(JsonDocument& doc);

void readButtons();
void clearButtonStates();
void resetToIdle();
void beep(int duration);
void playTone(int frequency, int duration);
void playStartupChime();
void playCompletionChime();
void playErrorChime();

float measureVcc();
float measureBatteryVoltage();
int getCurrentMA();
void updateTiming();
void updateDisplay();

void handleMenuState();
void handleSelectCutoffState();
void handleSelectCurrentState();
void handleChargingState();
void handleDischargingState();
void handleAnalyzeChargeState();
void handleAnalyzeRestState();
void handleAnalyzeDischargeState();
void handleIRMeasureState();
void handleIRDisplayState();
void handleCompleteState();
void handleWiFiInfoState();
void handleBatteryCheckState();
void handleAnalyzeConfigToggleState();
void handleAnalyzeConfigStage1State();
void handleAnalyzeConfigStage2State();
void handleStoragePrepState();

void drawBatteryOutline();
void drawBatteryFill(int level);
void updateBatteryDisplay(bool charging);

// ========================================= SETUP ========================================
void setup() {
    Serial.begin(115200);
    Serial.println("Battery Tester Starting...");

    // Initialize pins
    pinMode(PWM_Pin, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    pinMode(Mosfet_Pin, OUTPUT);
    analogWrite(PWM_Pin, 0);
    digitalWrite(Mosfet_Pin, LOW);

    // Initialize buttons
    UP_Button.begin();
    Down_Button.begin();
    Mode_Button.begin();
    
    // Clear any spurious button presses from initialization
    delay(100);
    clearButtonStates();

    // Setup LEDC for tone generation on buzzer pin (new ESP32 API)
    ledcAttach(Buzzer, LEDC_FREQUENCY, LEDC_RESOLUTION);

    // Initialize OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed");
        for (;;);
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.print("Battery Tester");
    display.setCursor(10, 35);
    display.print("Starting WiFi...");
    display.display();

    // Setup WiFi
    setupWiFi();

    // Wait for WiFi stack to fully initialize
    delay(1000);

    // Setup Web Server
    setupWebServer();

    // Show ready message
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print("WiFi: ");
    display.print(AP_SSID);
    display.setCursor(10, 35);
    display.print("IP: ");
    display.print(WiFi.softAPIP());
    display.display();
    delay(2000);

    // Play startup chime
    playStartupChime();

    // Initialize state
    currentState = STATE_MENU;
    Serial.println("Setup complete");
}

// ========================================= MAIN LOOP ========================================
void loop() {
    // Always clean up WebSocket clients
    ws.cleanupClients();

    // Read button states
    readButtons();

    // State machine
    switch (currentState) {
        case STATE_MENU:
            handleMenuState();
            break;
        case STATE_SELECT_CUTOFF:
            handleSelectCutoffState();
            break;
        case STATE_SELECT_CURRENT:
            handleSelectCurrentState();
            break;
        case STATE_CHARGING:
            handleChargingState();
            break;
        case STATE_DISCHARGING:
            handleDischargingState();
            break;
        case STATE_ANALYZE_CHARGE:
            handleAnalyzeChargeState();
            break;
        case STATE_ANALYZE_REST:
            handleAnalyzeRestState();
            break;
        case STATE_ANALYZE_DISCHARGE:
            handleAnalyzeDischargeState();
            break;
        case STATE_IR_MEASURE:
            handleIRMeasureState();
            break;
        case STATE_IR_DISPLAY:
            handleIRDisplayState();
            break;
        case STATE_COMPLETE:
            handleCompleteState();
            break;
        case STATE_WIFI_INFO:
            handleWiFiInfoState();
            break;
        case STATE_BATTERY_CHECK:
            handleBatteryCheckState();
            break;
        case STATE_ANALYZE_CONFIG_TOGGLE:
            handleAnalyzeConfigToggleState();
            break;
        case STATE_ANALYZE_CONFIG_STAGE1:
            handleAnalyzeConfigStage1State();
            break;
        case STATE_ANALYZE_CONFIG_STAGE2:
            handleAnalyzeConfigStage2State();
            break;
        case STATE_STORAGE_PREP:
            handleStoragePrepState();
            break;
        default:
            currentState = STATE_MENU;
            break;
    }

    // Send WebSocket updates periodically
    if (millis() - lastWsUpdate > 1000) {
        sendStatusUpdate();
        // Also send WiFi status if AP disable is pending (for countdown)
        if (apDisablePending) {
            sendWiFiStatus();
        }
        lastWsUpdate = millis();
    }

    // Check if it's time to disable AP after STA connection
    if (apDisablePending && millis() >= apDisableTime) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Disabling AP after delay...");
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            wifiMode = CFG_WIFI_STA;
            Serial.println("AP disabled, running in STA mode only");
            sendWiFiStatus();  // Notify clients of the change
        }
        apDisablePending = false;
    }
}

// ========================================= WIFI SETUP ========================================
void setupWiFi() {
    Serial.println("Setting WiFi mode...");
    WiFi.mode(WIFI_AP);

    Serial.println("Configuring AP...");
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

    Serial.println("Starting AP...");
    bool result = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
    Serial.print("AP started: ");
    Serial.println(result ? "Success" : "Failed");

    // Wait for AP to be fully ready
    while (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Check for saved WiFi credentials and attempt auto-connect
    if (loadWiFiCredentials()) {
        Serial.println("Attempting auto-connect to saved network...");
        if (connectToSTAWiFi()) {
            Serial.println("Auto-connect successful!");
        } else {
            Serial.println("Auto-connect failed, continuing in AP mode");
        }
    }
}

// Connect to an existing WiFi network (STA mode)
// Returns true if connection successful
bool connectToSTAWiFi() {
    if (sta_ssid.length() == 0) {
        Serial.println("No SSID configured");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(sta_ssid);

    // Switch to AP+STA mode to maintain AP while connecting
    WiFi.mode(WIFI_AP_STA);

    // Reconfigure AP (needed after mode change)
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);

    // Start STA connection
    WiFi.begin(sta_ssid.c_str(), sta_password.c_str());

    // Wait for connection with timeout
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < STA_CONNECT_TIMEOUT) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        sta_enabled = true;
        wifiMode = CFG_WIFI_BOTH;
        Serial.print("Connected! STA IP: ");
        Serial.println(WiFi.localIP());

        // Save credentials for auto-reconnect on next boot
        saveWiFiCredentials();

        // Schedule AP to be disabled after delay (so user can see new IP)
        apDisableTime = millis() + AP_DISABLE_DELAY;
        apDisablePending = true;
        Serial.printf("AP will be disabled in %lu seconds\n", AP_DISABLE_DELAY / 1000);

        return true;
    } else {
        Serial.println("Connection failed");
        // Revert to AP-only mode
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
        WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
        sta_enabled = false;
        wifiMode = CFG_WIFI_AP;
        apDisablePending = false;
        return false;
    }
}

// Send WiFi status to all connected WebSocket clients
void sendWiFiStatus() {
    if (ws.count() == 0) return;

    StaticJsonDocument<256> doc;
    doc["type"] = "wifi_status";

    // AP status
    bool apEnabled = (wifiMode == CFG_WIFI_AP || wifiMode == CFG_WIFI_BOTH);
    doc["ap_enabled"] = apEnabled;
    doc["ap_ssid"] = AP_SSID;
    doc["ap_ip"] = WiFi.softAPIP().toString();

    // Show countdown if AP disable is pending
    if (apDisablePending && apEnabled) {
        unsigned long remaining = (apDisableTime > millis()) ? (apDisableTime - millis()) / 1000 : 0;
        doc["ap_disable_in"] = remaining;
    }

    // STA status
    if (WiFi.status() == WL_CONNECTED) {
        doc["sta_connected"] = true;
        doc["sta_ssid"] = sta_ssid;
        doc["sta_ip"] = WiFi.localIP().toString();
    } else {
        doc["sta_connected"] = false;
        doc["sta_ssid"] = sta_ssid;
        doc["sta_ip"] = "";
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

// Save WiFi credentials to non-volatile storage
void saveWiFiCredentials() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putString(PREF_SSID, sta_ssid);
    preferences.putString(PREF_PASS, sta_password);
    preferences.end();
    Serial.println("WiFi credentials saved to NVS");
}

// Load WiFi credentials from non-volatile storage
// Returns true if credentials were found
bool loadWiFiCredentials() {
    preferences.begin(PREF_NAMESPACE, true);
    sta_ssid = preferences.getString(PREF_SSID, "");
    sta_password = preferences.getString(PREF_PASS, "");
    preferences.end();

    if (sta_ssid.length() > 0) {
        Serial.print("Loaded saved WiFi credentials for: ");
        Serial.println(sta_ssid);
        return true;
    }
    Serial.println("No saved WiFi credentials found");
    return false;
}

// Clear saved WiFi credentials from non-volatile storage
void clearWiFiCredentials() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    sta_ssid = "";
    sta_password = "";
    Serial.println("WiFi credentials cleared from NVS");
}

// ========================================= WEB SERVER SETUP ========================================
void setupWebServer() {
    // WebSocket handler
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Serve main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });

    // Start server
    server.begin();
    Serial.println("Web server started");
}

// ========================================= WEBSOCKET HANDLERS ========================================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected\n", client->id());
            // Send current status, WiFi status, and history to new client
            sendStatusUpdate();
            sendWiFiStatus();
            sendHistoryData(client);
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        Serial.printf("Received: %s\n", (char*)data);

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, (char*)data);

        if (!error) {
            processCommand(doc);
        }
    }
}

void processCommand(JsonDocument& doc) {
    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "start_charge") == 0) {
        if (currentState != STATE_MENU) {
            sendError("Operation already in progress");
            return;
        }
        // Check battery first
        BAT_Voltage = measureBatteryVoltage();
        if (BAT_Voltage < NO_BAT_level) {
            sendError("No battery detected");
            return;
        }
        if (BAT_Voltage < DAMAGE_BAT_level) {
            sendError("Battery damaged (below 2.5V)");
            return;
        }
        if (BAT_Voltage >= FULL_BAT_level) {
            sendError("Battery already full");
            return;
        }
        Capacity_f = 0;
        dataLogger.reset();
        startTime = millis();
        lastCapacityUpdate = millis();
        analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
        digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
        currentState = STATE_CHARGING;
        beep(100);
    }
    else if (strcmp(cmd, "start_discharge") == 0) {
        if (currentState != STATE_MENU) {
            sendError("Operation already in progress");
            return;
        }
        // Check battery first
        BAT_Voltage = measureBatteryVoltage();
        if (BAT_Voltage < NO_BAT_level) {
            sendError("No battery detected");
            return;
        }
        if (BAT_Voltage < DAMAGE_BAT_level) {
            sendError("Battery damaged (below 2.5V)");
            return;
        }
        cutoffVoltage = doc["cutoff"] | 3.0;
        int reqCurrent = doc["current"] | 500;
        // Check if battery is already below cutoff
        if (BAT_Voltage <= cutoffVoltage) {
            sendError("Battery already below cutoff voltage");
            return;
        }
        // Find matching PWM index
        PWM_Index = 6; // Default 500mA
        for (int i = 0; i < Array_Size; i++) {
            if (Current[i] == reqCurrent) {
                PWM_Index = i;
                break;
            }
        }
        PWM_Value = PWM[PWM_Index];
        Capacity_f = 0;
        dataLogger.reset();
        startTime = millis();
        lastCapacityUpdate = millis();
        digitalWrite(Mosfet_Pin, LOW);
        analogWrite(PWM_Pin, PWM_Value);
        currentState = STATE_DISCHARGING;
        beep(100);
    }
    else if (strcmp(cmd, "start_analyze") == 0) {
        if (currentState != STATE_MENU) {
            sendError("Operation already in progress");
            return;
        }
        BAT_Voltage = measureBatteryVoltage();
        if (BAT_Voltage < NO_BAT_level) {
            sendError("No battery detected");
            return;
        }
        if (BAT_Voltage < DAMAGE_BAT_level) {
            sendError("Battery damaged (below 2.5V)");
            return;
        }

        // Parse optional staged discharge parameters
        stagedAnalyzeEnabled = doc["staged"] | false;

        if (stagedAnalyzeEnabled) {
            // Stage 1 settings
            int s1Current = doc["stage1_current"] | 500;
            float s1TransV = doc["stage1_transition"] | 3.3;

            // Stage 2 settings
            int s2Current = doc["stage2_current"] | 300;
            float s2Cutoff = doc["stage2_cutoff"] | 3.0;

            // Validate: transition voltage > final cutoff
            if (s1TransV <= s2Cutoff) {
                sendError("Transition voltage must be > final cutoff");
                return;
            }

            // Validate: Stage 2 current <= Stage 1 current
            if (s2Current > s1Current) {
                sendError("Stage 2 current must be <= Stage 1 current");
                return;
            }

            // Find matching PWM indices
            stage1CurrentIndex = 6;  // Default
            stage2CurrentIndex = 4;  // Default
            for (int i = 0; i < Array_Size; i++) {
                if (Current[i] == s1Current) stage1CurrentIndex = i;
                if (Current[i] == s2Current) stage2CurrentIndex = i;
            }

            stage1TransitionVoltage = s1TransV;
            stage2FinalCutoff = s2Cutoff;
        } else {
            // Single stage defaults
            stage1CurrentIndex = 6;  // 500mA
            stage2FinalCutoff = 3.0;
        }

        analyzeDischargeStage = 1;
        Capacity_f = 0;
        dataLogger.reset();
        startTime = millis();
        lastCapacityUpdate = millis();
        analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
        digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
        currentState = STATE_ANALYZE_CHARGE;
        beep(100);
    }
    else if (strcmp(cmd, "start_ir") == 0) {
        if (currentState != STATE_MENU) {
            sendError("Operation already in progress");
            return;
        }
        BAT_Voltage = measureBatteryVoltage();
        if (BAT_Voltage < NO_BAT_level) {
            sendError("No battery detected");
            return;
        }
        if (BAT_Voltage < DAMAGE_BAT_level) {
            sendError("Battery damaged (below 2.5V)");
            return;
        }
        stateStartTime = millis();
        analogWrite(PWM_Pin, 0);
        digitalWrite(Mosfet_Pin, LOW);
        currentState = STATE_IR_MEASURE;
        beep(100);
    }
    else if (strcmp(cmd, "start_batcheck") == 0) {
        if (currentState != STATE_MENU) {
            sendError("Operation already in progress");
            return;
        }
        currentState = STATE_BATTERY_CHECK;
        beep(100);
    }
    else if (strcmp(cmd, "start_storage") == 0) {
        if (currentState != STATE_MENU) {
            sendError("Operation already in progress");
            return;
        }
        BAT_Voltage = measureBatteryVoltage();
        if (BAT_Voltage < NO_BAT_level) {
            sendError("No battery detected");
            return;
        }
        if (BAT_Voltage < DAMAGE_BAT_level) {
            sendError("Battery damaged (below 2.5V)");
            return;
        }
        stateStartTime = millis();
        Hour = Minute = Second = 0;
        // Start charging or discharging immediately based on current voltage
        if (BAT_Voltage < (STORAGE_TARGET_VOLTAGE - STORAGE_VOLTAGE_TOLERANCE)) {
            // Below range: charge
            digitalWrite(Mosfet_Pin, HIGH);
            analogWrite(PWM_Pin, 0);  // Charging (0% PWM)
        } else if (BAT_Voltage > (STORAGE_TARGET_VOLTAGE + STORAGE_VOLTAGE_TOLERANCE)) {
            // Above range: discharge
            digitalWrite(Mosfet_Pin, LOW);
            // Calculate PWM for storage prep current
            int targetPWM = map(STORAGE_PREP_CURRENT_MA, 0, Current[Array_Size - 1], 0, 255);
            analogWrite(PWM_Pin, targetPWM);
        } else {
            // Already in range: go straight to complete
            digitalWrite(Mosfet_Pin, LOW);
            analogWrite(PWM_Pin, 0);
            playCompletionChime();
            currentState = STATE_COMPLETE;
            return;
        }
        currentState = STATE_STORAGE_PREP;
        beep(100);
    }
    else if (strcmp(cmd, "abort") == 0) {
        abortRequested = true;  // Set flag so current handler can process abort
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
    }
    else if (strcmp(cmd, "wifi_config") == 0) {
        sta_ssid = doc["ssid"].as<String>();
        sta_password = doc["password"].as<String>();

        // Attempt to connect
        if (connectToSTAWiFi()) {
            sendWiFiStatus();
        } else {
            sendError("WiFi connection failed");
            sendWiFiStatus();
        }
    }
    else if (strcmp(cmd, "wifi_disconnect") == 0) {
        WiFi.disconnect();
        sta_enabled = false;
        apDisablePending = false;  // Cancel any pending AP disable
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
        WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
        wifiMode = CFG_WIFI_AP;
        sendWiFiStatus();
    }
    else if (strcmp(cmd, "wifi_forget") == 0) {
        // Disconnect and clear saved credentials
        WiFi.disconnect();
        sta_enabled = false;
        apDisablePending = false;
        clearWiFiCredentials();
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
        WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
        wifiMode = CFG_WIFI_AP;
        sendWiFiStatus();
    }
    else if (strcmp(cmd, "get_wifi_status") == 0) {
        sendWiFiStatus();
    }
}

void sendStatusUpdate() {
    if (ws.count() == 0) return;

    StaticJsonDocument<256> doc;
    doc["type"] = "status";

    // Mode string
    switch (currentState) {
        case STATE_MENU: doc["mode"] = "idle"; break;
        case STATE_CHARGING: doc["mode"] = "charge"; break;
        case STATE_DISCHARGING: doc["mode"] = "discharge"; break;
        case STATE_ANALYZE_CHARGE: doc["mode"] = "analyze_charge"; break;
        case STATE_ANALYZE_REST: doc["mode"] = "analyze_rest"; break;
        case STATE_ANALYZE_DISCHARGE:
            if (stagedAnalyzeEnabled) {
                doc["mode"] = (analyzeDischargeStage == 1) ? "analyze_discharge_s1" : "analyze_discharge_s2";
            } else {
                doc["mode"] = "analyze_discharge";
            }
            break;
        case STATE_IR_MEASURE:
        case STATE_IR_DISPLAY: doc["mode"] = "ir"; break;
        case STATE_BATTERY_CHECK: doc["mode"] = "batcheck"; break;
        case STATE_STORAGE_PREP: doc["mode"] = "storage"; break;
        case STATE_COMPLETE: doc["mode"] = "complete"; break;
        default: doc["mode"] = "idle"; break;
    }

    doc["status"] = (currentState != STATE_MENU && currentState != STATE_COMPLETE) ? "Running" : "Ready";
    doc["voltage"] = BAT_Voltage;
    doc["current"] = getCurrentMA();
    doc["capacity"] = Capacity_f;

    char timeStr[12];
    sprintf(timeStr, "%02d:%02d:%02d", Hour, Minute, Second);
    doc["time"] = timeStr;
    doc["cutoff"] = cutoffVoltage;

    // Include IR measurement results
    if (currentState == STATE_IR_DISPLAY) {
        doc["ir"] = internalResistance * 1000;  // Send in milliohms
    }

    // Include staged discharge info when in analyze discharge
    if (currentState == STATE_ANALYZE_DISCHARGE && stagedAnalyzeEnabled) {
        doc["stage"] = analyzeDischargeStage;
        doc["stage1_current"] = Current[stage1CurrentIndex];
        doc["stage1_transition"] = stage1TransitionVoltage;
        doc["stage2_current"] = Current[stage2CurrentIndex];
        doc["stage2_cutoff"] = stage2FinalCutoff;
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void sendDataPoint() {
    if (ws.count() == 0) return;

    StaticJsonDocument<128> doc;
    doc["type"] = "datapoint";
    doc["t"] = millis() - startTime;
    doc["v"] = BAT_Voltage;
    doc["c"] = getCurrentMA();

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void sendHistoryData(AsyncWebSocketClient *client) {
    if (!dataLogger.hasData()) return;

    uint16_t count = dataLogger.getDataForTransmit(360);
    if (count == 0) return;

    // Build JSON array of points
    DynamicJsonDocument doc(8192);
    doc["type"] = "history";
    JsonArray points = doc.createNestedArray("points");

    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = dataLogger.getDownsampledIndex(i, 360);
        DataPoint pt;
        if (dataLogger.getDataPoint(idx, pt)) {
            JsonObject p = points.createNestedObject();
            p["t"] = pt.timestamp;
            p["v"] = pt.voltage;
            p["c"] = pt.current;
        }
    }

    String output;
    serializeJson(doc, output);
    client->text(output);
}

void sendError(const char* message) {
    if (ws.count() == 0) return;

    StaticJsonDocument<128> doc;
    doc["type"] = "error";
    doc["message"] = message;

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

// ========================================= BUTTON HANDLING ========================================
void readButtons() {
    Mode_Button.read();
    UP_Button.read();
    Down_Button.read();
}

void clearButtonStates() {
    do {
        Mode_Button.read();
        UP_Button.read();
        Down_Button.read();
        delay(10);
    } while (Mode_Button.isPressed() || UP_Button.isPressed() || Down_Button.isPressed());
    Mode_Button.read();
    UP_Button.read();
    Down_Button.read();
}

void resetToIdle() {
    digitalWrite(Mosfet_Pin, LOW);
    analogWrite(PWM_Pin, 0);
}

void beep(int duration) {
    // Simple beep using LEDC at 1000 Hz
    ledcChangeFrequency(Buzzer, 1000, LEDC_RESOLUTION);
    ledcWrite(Buzzer, 128);  // 50% duty cycle
    delay(duration);
    ledcWrite(Buzzer, 0);    // Stop
}

// ========================================= TONE GENERATION ========================================
// Play a single tone at specified frequency and duration
void playTone(int frequency, int duration) {
    if (frequency == 0) {
        // Rest/silence
        ledcWrite(Buzzer, 0);
        delay(duration);
    } else {
        // Change frequency and set duty cycle to 50% for audible tone
        ledcChangeFrequency(Buzzer, frequency, LEDC_RESOLUTION);
        ledcWrite(Buzzer, 128);  // 50% duty cycle = 128 out of 255
        delay(duration);
        ledcWrite(Buzzer, 0);    // Stop tone
    }
}

// Play startup chime (ascending pattern)
void playStartupChime() {
    // Rising scale: C5 -> D5 -> E5 -> G5 (pleasant ascending pattern)
    playTone(523, 150);   // C5 (Middle C + 12 semitones)
    delay(50);
    playTone(587, 150);   // D5
    delay(50);
    playTone(659, 150);   // E5
    delay(50);
    playTone(784, 300);   // G5 (held longer for resolution)
    delay(100);
}

// Play completion chime (descending pattern)
void playCompletionChime() {
    // Descending pattern: G5 -> E5 -> D5 -> C5 (pleasant descending resolution)
    playTone(784, 150);   // G5
    delay(50);
    playTone(659, 150);   // E5
    delay(50);
    playTone(587, 150);   // D5
    delay(50);
    playTone(523, 300);   // C5 (held longer)
    delay(100);
}

// Play error chime (warning tone - descending low notes)
void playErrorChime() {
    // Warning pattern: Low G -> Low F -> Low G (urgent warning)
    playTone(392, 100);   // G4 (lower octave)
    delay(50);
    playTone(349, 100);   // F4
    delay(50);
    playTone(392, 200);   // G4 (held longer)
    delay(50);
}

// ========================================= VOLTAGE MEASUREMENT ========================================
float measureVcc() {
    float vrefSum = 0;
    for (int i = 0; i < 50; i++) {
        vrefSum += analogRead(Vref_Pin);
        delay(1);
    }
    float averageVrefReading = vrefSum / 50.0;
    float vcc = (Vref_Voltage * 4096.0) / averageVrefReading;
    return vcc;
}

float measureBatteryVoltage() {
    Vcc = measureVcc();
    float batterySum = 0;
    for (int i = 0; i < 50; i++) {
        batterySum += analogRead(BAT_Pin);
        delay(1);
    }
    float averageBatteryReading = batterySum / 50.0;
    float voltageDividerRatio = (R1 + R2) / R2;
    float batteryVoltage = (averageBatteryReading * Vcc / 4096.0) * voltageDividerRatio;
    return batteryVoltage;
}

void updateTiming() {
    elapsedTime = millis() - startTime;
    Second = (elapsedTime / 1000) % 60;
    Minute = (elapsedTime / (1000 * 60)) % 60;
    Hour = (elapsedTime / (1000 * 60 * 60));
}

// Get current in mA based on current state
// Note: Cannot read LP4060 CHRG pin as A2 conflicts with D2 (Mosfet_Pin)
// Returns estimated current based on mode
int getCurrentMA() {
    switch (currentState) {
        case STATE_CHARGING:
        case STATE_ANALYZE_CHARGE:
            // Return estimated charge current (set by R7 on LP4060)
            return CHARGE_CURRENT_MA;

        case STATE_DISCHARGING:
        case STATE_ANALYZE_DISCHARGE:
            // Return set discharge current
            return Current[PWM_Index];

        default:
            return 0;
    }
}

// ========================================= STATE HANDLERS ========================================
void handleMenuState() {
    // Handle button navigation (7 menu items: 0-6)
    if (UP_Button.wasReleased()) {
        selectedMode = (selectedMode == 0) ? 6 : selectedMode - 1;
        beep(100);
    }
    if (Down_Button.wasReleased()) {
        selectedMode = (selectedMode == 6) ? 0 : selectedMode + 1;
        beep(100);
    }
    if (Mode_Button.wasReleased()) {
        beep(300);
        clearButtonStates();

        if (selectedMode == 0) {
            // Charge
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level) {
                playErrorChime();
                display.clearDisplay();
                display.setCursor(15, 25);
                display.print("EMPTY BAT SLOT");
                display.display();
                delay(2000);
                return;
            }
            if (BAT_Voltage < DAMAGE_BAT_level) {
                playErrorChime();
                display.clearDisplay();
                display.setCursor(25, 25);
                display.print("BAT DAMAGED");
                display.display();
                delay(2000);
                return;
            }
            Capacity_f = 0;
            dataLogger.reset();
            startTime = millis();
            lastCapacityUpdate = millis();
            analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
            digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
            currentState = STATE_CHARGING;
        }
        else if (selectedMode == 1) {
            // Discharge - go to cutoff selection
            currentState = STATE_SELECT_CUTOFF;
        }
        else if (selectedMode == 2) {
            // Analyze - go to configuration first
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level || BAT_Voltage < DAMAGE_BAT_level) {
                playErrorChime();
                display.clearDisplay();
                display.setCursor(15, 25);
                display.print(BAT_Voltage < NO_BAT_level ? "EMPTY BAT SLOT" : "BAT DAMAGED");
                display.display();
                delay(2000);
                return;
            }
            currentState = STATE_ANALYZE_CONFIG_TOGGLE;
        }
        else if (selectedMode == 3) {
            // IR Test
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level) {
                playErrorChime();
                display.clearDisplay();
                display.setCursor(15, 25);
                display.print("EMPTY BAT SLOT");
                display.display();
                delay(2000);
                return;
            }
            if (BAT_Voltage < DAMAGE_BAT_level) {
                playErrorChime();
                display.clearDisplay();
                display.setCursor(25, 25);
                display.print("BAT DAMAGED");
                display.display();
                delay(2000);
                return;
            }
            stateStartTime = millis();
            analogWrite(PWM_Pin, 0);
            digitalWrite(Mosfet_Pin, LOW);
            currentState = STATE_IR_MEASURE;
        }
        else if (selectedMode == 4) {
            // Battery Check
            currentState = STATE_BATTERY_CHECK;
        }
        else if (selectedMode == 5) {
            // Storage Prep
            stateStartTime = millis();
            Hour = Minute = Second = 0;
            // Start charging or discharging immediately based on current voltage
            if (BAT_Voltage < (STORAGE_TARGET_VOLTAGE - STORAGE_VOLTAGE_TOLERANCE)) {
                // Below range: charge
                digitalWrite(Mosfet_Pin, HIGH);
                analogWrite(PWM_Pin, 0);  // Charging (0% PWM)
            } else if (BAT_Voltage > (STORAGE_TARGET_VOLTAGE + STORAGE_VOLTAGE_TOLERANCE)) {
                // Above range: discharge
                digitalWrite(Mosfet_Pin, LOW);
                // Calculate PWM for storage prep current
                int targetPWM = map(STORAGE_PREP_CURRENT_MA, 0, Current[Array_Size - 1], 0, 255);
                analogWrite(PWM_Pin, targetPWM);
            } else {
                // Already in range: go straight to complete
                digitalWrite(Mosfet_Pin, LOW);
                analogWrite(PWM_Pin, 0);
                currentState = STATE_COMPLETE;
                return;
            }
            currentState = STATE_STORAGE_PREP;
        }
        else if (selectedMode == 6) {
            // WiFi Info
            currentState = STATE_WIFI_INFO;
        }
    }

    // Display menu with scrolling (show max 4 items)
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("Select Mode:");
    
    // Calculate scroll position - keep selected item in view
    int scrollOffset = 0;
    if (selectedMode > 2) {
        scrollOffset = selectedMode - 2;  // Keep selection near bottom
    }
    
    // Display up to 4 menu items
    const char* modes[] = {"Charge", "Discharge", "Analyze", "IR Test", "Bat Check", "Storage", "WiFi Info"};
    int yPos = 12;
    for (int i = 0; i < 4 && (scrollOffset + i) < 7; i++) {
        int modeIdx = scrollOffset + i;
        display.setCursor(15, yPos);
        if (modeIdx == selectedMode) {
            display.print("> ");
        } else {
            display.print("  ");
        }
        display.print(modes[modeIdx]);
        yPos += 12;
    }
    display.display();
}

void handleSelectCutoffState() {
    if (UP_Button.wasReleased() && cutoffVoltage < Max_BAT_level) {
        cutoffVoltage += 0.1;
        beep(100);
    }
    if (Down_Button.wasReleased() && cutoffVoltage > Min_BAT_level) {
        cutoffVoltage -= 0.1;
        beep(100);
    }
    if (Mode_Button.wasReleased()) {
        beep(300);
        clearButtonStates();
        currentState = STATE_SELECT_CURRENT;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(2, 10);
    display.print("Select Cutoff Volt:");
    display.setTextSize(2);
    display.setCursor(20, 30);
    display.print("V:");
    display.print(cutoffVoltage, 1);
    display.print("V");
    display.display();
}

void handleSelectCurrentState() {
    if (UP_Button.wasReleased() && PWM_Index < (Array_Size - 1)) {
        PWM_Index++;
        beep(100);
    }
    if (Down_Button.wasReleased() && PWM_Index > 0) {
        PWM_Index--;
        beep(100);
    }
    if (Mode_Button.wasReleased()) {
        beep(300);
        PWM_Value = PWM[PWM_Index];
        Capacity_f = 0;
        dataLogger.reset();
        startTime = millis();
        lastCapacityUpdate = millis();
        digitalWrite(Mosfet_Pin, LOW);
        analogWrite(PWM_Pin, PWM_Value);
        clearButtonStates();
        currentState = STATE_DISCHARGING;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(2, 5);
    display.print("Select Dischrg Curr:");

    // Show warning for high currents
    if (Current[PWM_Index] > HIGH_CURRENT_THRESHOLD) {
        display.setCursor(2, 52);
        display.print("!! HIGH CURRENT !!");
    }

    display.setTextSize(2);
    display.setCursor(15, 25);
    display.print("I:");
    display.print(Current[PWM_Index]);
    display.print("mA");
    display.display();
}

void handleChargingState() {
    // Check for abort
    if (Mode_Button.wasReleased()) {
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    updateTiming();
    BAT_Voltage = measureBatteryVoltage();

    // Calculate estimated capacity (using known charge current from LP4060)
    // Note: This is an estimate - actual current tapers during CV phase
    unsigned long currentTime = millis();
    float elapsedTimeInHours = (currentTime - lastCapacityUpdate) / 3600000.0;
    Capacity_f += CHARGE_CURRENT_MA * elapsedTimeInHours;
    lastCapacityUpdate = currentTime;

    // Log data
    dataLogger.addDataPoint(BAT_Voltage, getCurrentMA(), Capacity_f);

    // Check if charging complete
    if (BAT_Voltage >= FULL_BAT_level) {
        digitalWrite(Mosfet_Pin, LOW);
        beep(300);
        currentState = STATE_COMPLETE;
        return;
    }

    // Update display
    display.clearDisplay();
    updateBatteryDisplay(true);
    display.setTextSize(1);
    display.setCursor(5, 5);
    display.print("Charging..");
    display.setCursor(5, 18);
    display.print("Time:");
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setCursor(5, 31);
    display.print("~Cap:");
    display.print(Capacity_f, 0);
    display.print("mAh");
    display.setCursor(5, 48);
    display.setTextSize(2);
    display.print("V:");
    display.print(BAT_Voltage, 2);
    display.print("V");
    display.display();

    // Send data point to web clients
    sendDataPoint();
}

void handleDischargingState() {
    // Check for abort
    if (Mode_Button.wasReleased()) {
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    updateTiming();
    BAT_Voltage = measureBatteryVoltage();

    // Calculate capacity
    unsigned long currentTime = millis();
    float elapsedTimeInHours = (currentTime - lastCapacityUpdate) / 3600000.0;
    Capacity_f += (Current[PWM_Index] + currentOffset) * elapsedTimeInHours;
    lastCapacityUpdate = currentTime;

    // Log data
    dataLogger.addDataPoint(BAT_Voltage, Current[PWM_Index], Capacity_f);

    // Check if discharge complete
    if (BAT_Voltage <= cutoffVoltage) {
        analogWrite(PWM_Pin, 0);
        beep(300);
        currentState = STATE_COMPLETE;
        return;
    }

    // Update display
    display.clearDisplay();
    updateBatteryDisplay(false);
    display.setTextSize(1);
    display.setCursor(15, 5);
    display.print("Discharging..");
    display.setCursor(15, 20);
    display.print("Time: ");
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setCursor(15, 35);
    display.print("Cap:");
    display.print(Capacity_f, 1);
    display.print("mAh");
    display.setCursor(15, 50);
    display.print("V: ");
    display.print(BAT_Voltage, 2);
    display.print("V");
    display.display();

    // Send data point to web clients
    sendDataPoint();
}

void handleAnalyzeChargeState() {
    // Check for abort
    if (Mode_Button.wasReleased()) {
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    updateTiming();
    BAT_Voltage = measureBatteryVoltage();

    // Calculate estimated capacity during charge phase
    unsigned long currentTime = millis();
    float elapsedTimeInHours = (currentTime - lastCapacityUpdate) / 3600000.0;
    Capacity_f += CHARGE_CURRENT_MA * elapsedTimeInHours;
    lastCapacityUpdate = currentTime;

    // Log data
    dataLogger.addDataPoint(BAT_Voltage, getCurrentMA(), Capacity_f);

    // Check if charging complete
    if (BAT_Voltage >= FULL_BAT_level) {
        digitalWrite(Mosfet_Pin, LOW);
        restStartTime = millis();
        currentState = STATE_ANALYZE_REST;
        return;
    }

    // Update display
    display.clearDisplay();
    updateBatteryDisplay(true);
    display.setTextSize(1);
    display.setCursor(5, 5);
    display.print("Analyze - Charging");
    display.setCursor(5, 18);
    display.print("Time:");
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setCursor(5, 31);
    display.print("~Cap:");
    display.print(Capacity_f, 0);
    display.print("mAh");
    display.setCursor(5, 48);
    display.setTextSize(2);
    display.print("V:");
    display.print(BAT_Voltage, 2);
    display.print("V");
    display.display();

    sendDataPoint();
}

void handleAnalyzeRestState() {
    // Check for abort
    if (Mode_Button.wasReleased()) {
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    // Wait for 3 minutes
    if (millis() - restStartTime >= 180000) {
        // Start discharge with configured values
        analyzeDischargeStage = 1;

        if (stagedAnalyzeEnabled) {
            // Use Stage 1 settings first
            cutoffVoltage = stage1TransitionVoltage;
            PWM_Index = stage1CurrentIndex;
        } else {
            // Single stage: use configured current and final cutoff
            cutoffVoltage = stage2FinalCutoff;
            PWM_Index = stage1CurrentIndex;
        }

        PWM_Value = PWM[PWM_Index];
        Capacity_f = 0;
        startTime = millis();
        lastCapacityUpdate = millis();
        digitalWrite(Mosfet_Pin, LOW);
        analogWrite(PWM_Pin, PWM_Value);
        currentState = STATE_ANALYZE_DISCHARGE;
        return;
    }

    // Update display
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(5, 25);
    display.print("Resting..");
    display.display();
}

void handleAnalyzeDischargeState() {
    // Check for abort
    if (Mode_Button.wasReleased()) {
        resetToIdle();
        analyzeDischargeStage = 1;  // Reset stage
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    updateTiming();
    BAT_Voltage = measureBatteryVoltage();

    // Calculate capacity
    unsigned long currentTime = millis();
    float elapsedTimeInHours = (currentTime - lastCapacityUpdate) / 3600000.0;
    Capacity_f += (Current[PWM_Index] + currentOffset) * elapsedTimeInHours;
    lastCapacityUpdate = currentTime;

    // Log data
    dataLogger.addDataPoint(BAT_Voltage, Current[PWM_Index], Capacity_f);

    // Check for stage transition (staged mode, stage 1, voltage reached transition point)
    if (stagedAnalyzeEnabled && analyzeDischargeStage == 1) {
        if (BAT_Voltage <= stage1TransitionVoltage) {
            // Transition to Stage 2
            analyzeDischargeStage = 2;
            cutoffVoltage = stage2FinalCutoff;
            PWM_Index = stage2CurrentIndex;
            PWM_Value = PWM[PWM_Index];
            analogWrite(PWM_Pin, PWM_Value);
            beep(100);  // Audible feedback for stage transition
        }
    }

    // Check if discharge complete (final cutoff reached)
    if (BAT_Voltage <= cutoffVoltage) {
        // In staged mode, only complete if we're in stage 2
        // In single-stage mode, complete immediately
        if (!stagedAnalyzeEnabled || analyzeDischargeStage == 2) {
            analogWrite(PWM_Pin, 0);
            analyzeDischargeStage = 1;  // Reset for next run
            beep(300);
            currentState = STATE_COMPLETE;
            return;
        }
    }

    // Update display
    display.clearDisplay();
    updateBatteryDisplay(false);
    display.setTextSize(1);
    display.setCursor(10, 5);
    if (stagedAnalyzeEnabled) {
        display.print("Analyze - S");
        display.print(analyzeDischargeStage);
    } else {
        display.print("Analyzing - D");
    }
    display.setCursor(15, 20);
    display.print("Time: ");
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setCursor(15, 35);
    display.print("Cap:");
    display.print(Capacity_f, 1);
    display.print("mAh");
    display.setCursor(15, 50);
    display.print("V: ");
    display.print(BAT_Voltage, 2);
    display.print("V");
    display.display();

    sendDataPoint();
}

void handleIRMeasureState() {
    static int irStep = 0;

    // Check for abort
    if (Mode_Button.wasReleased()) {
        resetToIdle();
        irStep = 0;
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    // Display measuring
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 25);
    display.print("Measuring IR...");
    display.display();

    if (irStep == 0) {
        // Wait 500ms for voltage to stabilize
        if (millis() - stateStartTime >= 500) {
            voltageNoLoad = measureBatteryVoltage();
            PWM_Index = 6;  // 500mA
            PWM_Value = PWM[PWM_Index];
            analogWrite(PWM_Pin, PWM_Value);
            stateStartTime = millis();
            irStep = 1;
        }
    }
    else if (irStep == 1) {
        // Wait 500ms under load
        if (millis() - stateStartTime >= 500) {
            voltageLoad = measureBatteryVoltage();
            analogWrite(PWM_Pin, 0);

            // Calculate IR
            float currentDrawn = Current[PWM_Index] / 1000.0;
            if (currentDrawn > 0) {
                internalResistance = (voltageNoLoad - voltageLoad) / currentDrawn;
            } else {
                internalResistance = 0;
            }

            beep(300);
            stateStartTime = millis();
            irStep = 0;
            currentState = STATE_IR_DISPLAY;
        }
    }
}

void handleIRDisplayState() {
    // Check for any button or timeout
    if (Mode_Button.wasReleased() || UP_Button.wasReleased() || Down_Button.wasReleased()) {
        currentState = STATE_MENU;
        return;
    }

    // Auto-return after 5 seconds
    if (millis() - stateStartTime >= 5000) {
        currentState = STATE_MENU;
        return;
    }

    // Display IR result
    display.clearDisplay();
    display.drawLine(34, 15, 54, 15, SSD1306_WHITE);
    display.drawLine(54, 15, 59, 20, SSD1306_WHITE);
    display.drawLine(59, 20, 64, 10, SSD1306_WHITE);
    display.drawLine(64, 10, 69, 20, SSD1306_WHITE);
    display.drawLine(69, 20, 74, 15, SSD1306_WHITE);
    display.drawLine(74, 15, 94, 15, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(2, 35);
    display.print("IR:");
    display.print(internalResistance * 1000, 0);
    display.print("mOhm");
    display.display();
}

void handleCompleteState() {
    // Play completion chime once on entry
    static bool chimePlayedComplete = false;
    if (!chimePlayedComplete) {
        playCompletionChime();
        chimePlayedComplete = true;
    }

    // Reset flag when leaving this state (respond to buttons or abort command)
    if (abortRequested || Mode_Button.wasReleased() || UP_Button.wasReleased() || Down_Button.wasReleased()) {
        abortRequested = false;
        chimePlayedComplete = false;
        currentState = STATE_MENU;
        return;
    }

    // Display complete screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(15, 5);
    display.print("Complete");
    display.setCursor(15, 20);
    display.print("Time: ");
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setCursor(15, 35);
    display.print("Cap:");
    display.print(Capacity_f, 1);
    display.print("mAh");
    display.setCursor(15, 50);
    display.print("V: ");
    display.print(BAT_Voltage, 2);
    display.print("V");

    drawBatteryOutline();
    drawBatteryFill(Capacity_f > 0 ? 0 : 100);

    display.display();
}

void handleWiFiInfoState() {
    // Return to menu on any button press
    if (Mode_Button.wasReleased() || UP_Button.wasReleased() || Down_Button.wasReleased()) {
        currentState = STATE_MENU;
        return;
    }

    // Display WiFi information
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("=== WiFi Info ===");

    // Access Point info
    display.setCursor(0, 12);
    if (wifiMode == CFG_WIFI_AP || wifiMode == CFG_WIFI_BOTH) {
        display.print("AP: ");
        display.print(AP_SSID);
        display.setCursor(0, 22);
        display.print("    ");
        display.print(WiFi.softAPIP());
    } else {
        display.print("AP: Disabled");
    }

    // Station info
    display.setCursor(0, 34);
    if (WiFi.status() == WL_CONNECTED) {
        display.print("Net: ");
        // Truncate SSID if too long
        if (sta_ssid.length() > 15) {
            display.print(sta_ssid.substring(0, 12));
            display.print("...");
        } else {
            display.print(sta_ssid);
        }
        display.setCursor(0, 44);
        display.print("    ");
        display.print(WiFi.localIP());
    } else {
        display.print("Net: Not connected");
    }

    display.setCursor(0, 56);
    display.print("Press any btn: Back");
    display.display();
}

// ========================================= BATTERY CHECK HANDLER ========================================
void handleBatteryCheckState() {
    // Return to menu on any button press or abort request
    if (abortRequested || Mode_Button.wasReleased() || UP_Button.wasReleased() || Down_Button.wasReleased()) {
        abortRequested = false;
        currentState = STATE_MENU;
        return;
    }

    // Measure voltage continuously
    BAT_Voltage = measureBatteryVoltage();

    // Display battery voltage and status only
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 5);
    display.print(BAT_Voltage, 3);
    display.print("V");

    // Display battery status
    display.setTextSize(1);
    display.setCursor(0, 25);
    if (BAT_Voltage < NO_BAT_level) {
        display.print("Status: No Battery");
    } else if (BAT_Voltage < DAMAGE_BAT_level) {
        display.print("Status: DAMAGED");
    } else if (BAT_Voltage >= FULL_BAT_level) {
        display.print("Status: FULL");
    } else if (BAT_Voltage >= Max_BAT_level) {
        display.print("Status: Good");
    } else if (BAT_Voltage >= Min_BAT_level) {
        display.print("Status: Low");
    } else {
        display.print("Status: Very Low");
    }

    // Display charge percentage
    int batteryPercent = constrain((BAT_Voltage - Min_BAT_level) / (FULL_BAT_level - Min_BAT_level) * 100, 0, 100);
    display.setCursor(0, 38);
    display.print("Charge: ");
    display.print(batteryPercent);
    display.print("%");

    display.setCursor(0, 56);
    display.print("Press any btn: Back");
    display.display();
}

// ========================================= STORAGE PREP HANDLER ========================================
void handleStoragePrepState() {
    // Check for abort from web GUI or MODE button
    if (abortRequested || Mode_Button.wasReleased()) {
        abortRequested = false;
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
        return;
    }

    updateTiming();
    BAT_Voltage = measureBatteryVoltage();

    // Check if battery is in acceptable range (3.75V - 3.85V)
    if (BAT_Voltage >= (STORAGE_TARGET_VOLTAGE - STORAGE_VOLTAGE_TOLERANCE) &&
        BAT_Voltage <= (STORAGE_TARGET_VOLTAGE + STORAGE_VOLTAGE_TOLERANCE)) {
        // Target reached! Stop charging/discharging
        digitalWrite(Mosfet_Pin, LOW);
        analogWrite(PWM_Pin, 0);
        beep(100);
        delay(50);
        beep(100);
        currentState = STATE_COMPLETE;
        return;
    }

    // Check for battery errors
    if (BAT_Voltage < NO_BAT_level) {
        playErrorChime();
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(35, 20);
        display.print("No Battery");
        display.setCursor(25, 35);
        display.print("Detected!");
        display.display();
        delay(2000);
        resetToIdle();
        currentState = STATE_MENU;
        return;
    }
    
    if (BAT_Voltage < DAMAGE_BAT_level) {
        playErrorChime();
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(30, 20);
        display.print("Battery");
        display.setCursor(20, 35);
        display.print("DAMAGED!");
        display.display();
        delay(2000);
        resetToIdle();
        currentState = STATE_MENU;
        return;
    }

    // Adjust charging/discharging based on current voltage
    if (BAT_Voltage < (STORAGE_TARGET_VOLTAGE - STORAGE_VOLTAGE_TOLERANCE)) {
        // Below range: charge
        digitalWrite(Mosfet_Pin, HIGH);
        analogWrite(PWM_Pin, 0);  // Charging (0% PWM)
    } else {
        // Above range: discharge
        digitalWrite(Mosfet_Pin, LOW);
        // Calculate PWM for storage prep current
        int targetPWM = map(STORAGE_PREP_CURRENT_MA, 0, Current[Array_Size - 1], 0, 255);
        analogWrite(PWM_Pin, targetPWM);
    }

    // Update display
    display.clearDisplay();
    updateBatteryDisplay(true);
    display.setTextSize(1);
    display.setCursor(5, 5);
    if (BAT_Voltage < (STORAGE_TARGET_VOLTAGE - STORAGE_VOLTAGE_TOLERANCE)) {
        display.print("Storage: Charging");
    } else {
        display.print("Storage: Discharging");
    }
    display.setCursor(5, 18);
    display.print("Time:");
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setCursor(5, 31);
    display.print("Target: 3.8V");
    display.setCursor(5, 48);
    display.setTextSize(2);
    display.print("V:");
    display.print(BAT_Voltage, 2);
    display.print("V");
    display.display();
}

// ========================================= ANALYZE CONFIG HANDLERS ========================================
void handleAnalyzeConfigToggleState() {
    // UP/DOWN toggles staged mode
    if (UP_Button.wasReleased() || Down_Button.wasReleased()) {
        stagedAnalyzeEnabled = !stagedAnalyzeEnabled;
        beep(100);
    }

    // MODE button advances
    if (Mode_Button.wasReleased()) {
        beep(300);
        clearButtonStates();

        if (stagedAnalyzeEnabled) {
            // Go to Stage 1 configuration
            currentState = STATE_ANALYZE_CONFIG_STAGE1;
        } else {
            // Start analyze with defaults (single-stage: 500mA, 3.0V cutoff)
            stage1CurrentIndex = 6;  // 500mA
            stage2FinalCutoff = 3.0;
            analyzeDischargeStage = 1;
            Capacity_f = 0;
            dataLogger.reset();
            startTime = millis();
            lastCapacityUpdate = millis();
            analogWrite(PWM_Pin, 0);
            digitalWrite(Mosfet_Pin, HIGH);
            currentState = STATE_ANALYZE_CHARGE;
        }
        return;
    }

    // Display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(15, 0);
    display.print("Analyze Config");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    display.setCursor(5, 18);
    display.print("Staged Discharge:");

    display.setTextSize(2);
    display.setCursor(40, 32);
    display.print(stagedAnalyzeEnabled ? "ON" : "OFF");

    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print("UP/DN:Toggle MODE:OK");
    display.display();
}

void handleAnalyzeConfigStage1State() {
    static int configField = 0;  // 0 = current, 1 = transition voltage

    if (UP_Button.wasReleased()) {
        beep(100);
        if (configField == 0) {
            // Increase current (move up in array)
            if (stage1CurrentIndex < Array_Size - 1) {
                stage1CurrentIndex++;
            }
        } else {
            // Increase transition voltage (max 4.0V)
            if (stage1TransitionVoltage < 4.0) {
                stage1TransitionVoltage += 0.1;
            }
        }
    }

    if (Down_Button.wasReleased()) {
        beep(100);
        if (configField == 0) {
            // Decrease current (min 50mA)
            if (stage1CurrentIndex > 1) {
                stage1CurrentIndex--;
            }
        } else {
            // Decrease transition voltage (min must be > stage2FinalCutoff)
            if (stage1TransitionVoltage > stage2FinalCutoff + 0.1) {
                stage1TransitionVoltage -= 0.1;
            }
        }
    }

    if (Mode_Button.wasReleased()) {
        beep(300);
        clearButtonStates();
        if (configField == 0) {
            configField = 1;  // Move to transition voltage
        } else {
            configField = 0;  // Reset for next time
            // Ensure stage2 current doesn't exceed stage1
            if (stage2CurrentIndex > stage1CurrentIndex) {
                stage2CurrentIndex = stage1CurrentIndex;
            }
            currentState = STATE_ANALYZE_CONFIG_STAGE2;
        }
        return;
    }

    // Display - Stage 1 Configuration
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("Stage 1 Config");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Current selection
    display.setCursor(5, 16);
    display.print(configField == 0 ? ">" : " ");
    display.print("Current: ");
    display.print(Current[stage1CurrentIndex]);
    display.print("mA");

    // Transition voltage
    display.setCursor(5, 28);
    display.print(configField == 1 ? ">" : " ");
    display.print("Trans V: ");
    display.print(stage1TransitionVoltage, 1);
    display.print("V");

    // Visual separator
    display.drawLine(0, 40, 128, 40, SSD1306_WHITE);

    // Instructions
    display.setCursor(5, 44);
    display.print("UP/DN: Adjust value");
    display.setCursor(5, 54);
    display.print("MODE: ");
    display.print(configField == 0 ? "Next field" : "Continue");

    display.display();
}

void handleAnalyzeConfigStage2State() {
    static int configField = 0;  // 0 = current, 1 = final cutoff

    if (UP_Button.wasReleased()) {
        beep(100);
        if (configField == 0) {
            // Increase current (but not above stage 1)
            if (stage2CurrentIndex < stage1CurrentIndex) {
                stage2CurrentIndex++;
            }
        } else {
            // Increase final cutoff (max must be < transition voltage)
            if (stage2FinalCutoff < stage1TransitionVoltage - 0.1) {
                stage2FinalCutoff += 0.1;
            }
        }
    }

    if (Down_Button.wasReleased()) {
        beep(100);
        if (configField == 0) {
            // Decrease current (min 50mA)
            if (stage2CurrentIndex > 1) {
                stage2CurrentIndex--;
            }
        } else {
            // Decrease final cutoff (min 2.8V)
            if (stage2FinalCutoff > Min_BAT_level) {
                stage2FinalCutoff -= 0.1;
            }
        }
    }

    if (Mode_Button.wasReleased()) {
        beep(300);
        clearButtonStates();
        if (configField == 0) {
            configField = 1;  // Move to cutoff voltage
        } else {
            configField = 0;  // Reset for next time
            // Start the analyze operation
            analyzeDischargeStage = 1;
            Capacity_f = 0;
            dataLogger.reset();
            startTime = millis();
            lastCapacityUpdate = millis();
            analogWrite(PWM_Pin, 0);
            digitalWrite(Mosfet_Pin, HIGH);
            currentState = STATE_ANALYZE_CHARGE;
        }
        return;
    }

    // Display - Stage 2 Configuration
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("Stage 2 Config");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Current selection (with constraint indicator)
    display.setCursor(5, 16);
    display.print(configField == 0 ? ">" : " ");
    display.print("Current: ");
    display.print(Current[stage2CurrentIndex]);
    display.print("mA");

    // Final cutoff voltage
    display.setCursor(5, 28);
    display.print(configField == 1 ? ">" : " ");
    display.print("Cutoff: ");
    display.print(stage2FinalCutoff, 1);
    display.print("V");

    // Visual separator
    display.drawLine(0, 40, 128, 40, SSD1306_WHITE);

    // Instructions
    display.setCursor(5, 44);
    display.print("UP/DN: Adjust value");
    display.setCursor(5, 54);
    display.print("MODE: ");
    display.print(configField == 0 ? "Next field" : "START");

    display.display();
}

// ========================================= DISPLAY HELPERS ========================================
void drawBatteryOutline() {
    display.drawRect(100, 15, 12, 20, SSD1306_WHITE);
    display.drawRect(102, 12, 8, 3, SSD1306_WHITE);
}

void drawBatteryFill(int level) {
    int fillHeight = map(level, 0, 100, 0, 18);
    display.fillRect(102, 33 - fillHeight, 8, fillHeight, SSD1306_WHITE);
}

void updateBatteryDisplay(bool charging) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= displayInterval) {
        previousMillis = currentMillis;

        if (charging) {
            batteryLevel += 4;
            if (batteryLevel > 100) batteryLevel = 0;
        } else {
            batteryLevel -= 4;
            if (batteryLevel < 0) batteryLevel = 100;
        }

        drawBatteryOutline();
        drawBatteryFill(batteryLevel);
    }
}
