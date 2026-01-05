//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// DIY Smart Multipurpose Battery Tester - Web GUI Version
// Based on original work by Open Green Energy, INDIA
// Web GUI and state machine refactor added
// https://www.instructables.com/DIY-Smart-Multipurpose-Battery-Tester/
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <JC_Button.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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
    STATE_COMPLETE
};

DeviceState currentState = STATE_MENU;
DeviceState previousState = STATE_IDLE;

// ========================================= BATTERY SETTINGS ========================================
float cutoffVoltage = 3.0;
const float Min_BAT_level = 2.8;
const float Max_BAT_level = 3.2;
const float FULL_BAT_level = 4.18;
const float DAMAGE_BAT_level = 2.5;
const float NO_BAT_level = 0.3;

int Current[] = {0, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
int PWM[] = {0, 4, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
int Array_Size = sizeof(Current) / sizeof(Current[0]);
int currentOffset = 25;
int PWM_Value = 0;
int PWM_Index = 6;  // Default to 500mA

float Capacity_f = 0;
float Vref_Voltage = 1.26;
float Vcc = 3.3;
float BAT_Voltage = 0;
float internalResistance = 0;
float voltageNoLoad = 0;
float voltageLoad = 0;

// ========================================= TIMING ========================================
unsigned long previousMillis = 0;
const long displayInterval = 50;
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
unsigned long lastCapacityUpdate = 0;
unsigned long lastWsUpdate = 0;
unsigned long stateStartTime = 0;
unsigned long restStartTime = 0;

int Hour = 0;
int Minute = 0;
int Second = 0;

// ========================================= PINS ========================================
const byte PWM_Pin = D8;
const byte Buzzer = D7;
const int BAT_Pin = A0;
const int Vref_Pin = A1;
const byte Mosfet_Pin = D2;
const int CHRG_Pin = A2;      // LP4060 charge status (LOW = charging, HIGH = complete/no battery)

// Charge current set by R7 (1k) on LP4060: I = 1000mA
const int CHARGE_CURRENT_MA = 1000;

// Battery icon animation
int batteryLevel = 0;

// Voltage divider resistors
const float R1 = 200000.0;
const float R2 = 100000.0;

// Menu selection
int selectedMode = 0;

// ========================================= WEB SERVER ========================================
AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws(WEBSOCKET_PATH);

// ========================================= FUNCTION DECLARATIONS ========================================
void setupWiFi();
void setupWebServer();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void sendStatusUpdate();
void sendDataPoint();
void sendHistoryData(AsyncWebSocketClient *client);
void processCommand(JsonDocument& doc);

void readButtons();
void clearButtonStates();
void resetToIdle();
void beep(int duration);

float measureVcc();
float measureBatteryVoltage();
bool isCharging();
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
    pinMode(CHRG_Pin, INPUT);     // LP4060 charge status input
    analogWrite(PWM_Pin, 0);
    digitalWrite(Mosfet_Pin, LOW);

    // Initialize buttons
    UP_Button.begin();
    Down_Button.begin();
    Mode_Button.begin();

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
        default:
            currentState = STATE_MENU;
            break;
    }

    // Send WebSocket updates periodically
    if (millis() - lastWsUpdate > 1000) {
        sendStatusUpdate();
        lastWsUpdate = millis();
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
            // Send current status and history to new client
            sendStatusUpdate();
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
        if (currentState == STATE_MENU) {
            // Check battery first
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level) {
                // No battery - stay in menu
                return;
            }
            Capacity_f = 0;
            dataLogger.reset();
            startTime = millis();
            analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
            digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
            currentState = STATE_CHARGING;
            beep(100);
        }
    }
    else if (strcmp(cmd, "start_discharge") == 0) {
        if (currentState == STATE_MENU) {
            cutoffVoltage = doc["cutoff"] | 3.0;
            int reqCurrent = doc["current"] | 500;
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
    }
    else if (strcmp(cmd, "start_analyze") == 0) {
        if (currentState == STATE_MENU) {
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level) {
                return;
            }
            Capacity_f = 0;
            dataLogger.reset();
            startTime = millis();
            analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
            digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
            currentState = STATE_ANALYZE_CHARGE;
            beep(100);
        }
    }
    else if (strcmp(cmd, "start_ir") == 0) {
        if (currentState == STATE_MENU) {
            stateStartTime = millis();
            analogWrite(PWM_Pin, 0);
            digitalWrite(Mosfet_Pin, LOW);
            currentState = STATE_IR_MEASURE;
            beep(100);
        }
    }
    else if (strcmp(cmd, "abort") == 0) {
        resetToIdle();
        beep(100);
        delay(100);
        beep(100);
        currentState = STATE_MENU;
    }
    else if (strcmp(cmd, "wifi_config") == 0) {
        sta_ssid = doc["ssid"].as<String>();
        sta_password = doc["password"].as<String>();
        sta_enabled = true;
        // Could add STA connection logic here
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
        case STATE_ANALYZE_DISCHARGE: doc["mode"] = "analyze_discharge"; break;
        case STATE_IR_MEASURE:
        case STATE_IR_DISPLAY: doc["mode"] = "ir"; break;
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
    digitalWrite(Buzzer, HIGH);
    delay(duration);
    digitalWrite(Buzzer, LOW);
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

// Check if LP4060 is actively charging (CHRG pin is LOW when charging)
bool isCharging() {
    return digitalRead(CHRG_Pin) == LOW;
}

// Get current in mA based on current state
int getCurrentMA() {
    switch (currentState) {
        case STATE_CHARGING:
        case STATE_ANALYZE_CHARGE:
            // Return charge current if LP4060 is actively charging
            return isCharging() ? CHARGE_CURRENT_MA : 0;

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
    // Handle button navigation
    if (UP_Button.wasReleased()) {
        selectedMode = (selectedMode == 0) ? 3 : selectedMode - 1;
        beep(100);
    }
    if (Down_Button.wasReleased()) {
        selectedMode = (selectedMode == 3) ? 0 : selectedMode + 1;
        beep(100);
    }
    if (Mode_Button.wasReleased()) {
        beep(300);
        clearButtonStates();

        if (selectedMode == 0) {
            // Charge
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level) {
                display.clearDisplay();
                display.setCursor(15, 25);
                display.print("EMPTY BAT SLOT");
                display.display();
                delay(2000);
                return;
            }
            if (BAT_Voltage < DAMAGE_BAT_level) {
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
            analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
            digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
            currentState = STATE_CHARGING;
        }
        else if (selectedMode == 1) {
            // Discharge - go to cutoff selection
            currentState = STATE_SELECT_CUTOFF;
        }
        else if (selectedMode == 2) {
            // Analyze
            BAT_Voltage = measureBatteryVoltage();
            if (BAT_Voltage < NO_BAT_level || BAT_Voltage < DAMAGE_BAT_level) {
                display.clearDisplay();
                display.setCursor(15, 25);
                display.print(BAT_Voltage < NO_BAT_level ? "EMPTY BAT SLOT" : "BAT DAMAGED");
                display.display();
                delay(2000);
                return;
            }
            Capacity_f = 0;
            dataLogger.reset();
            startTime = millis();
            analogWrite(PWM_Pin, 0);        // Ensure discharge load is OFF
            digitalWrite(Mosfet_Pin, HIGH); // Enable charging circuit
            currentState = STATE_ANALYZE_CHARGE;
        }
        else if (selectedMode == 3) {
            // IR Test
            stateStartTime = millis();
            analogWrite(PWM_Pin, 0);
            digitalWrite(Mosfet_Pin, LOW);
            currentState = STATE_IR_MEASURE;
        }
    }

    // Display menu
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(25, 0);
    display.print("Select Mode:");
    display.setCursor(25, 12);
    display.print((selectedMode == 0) ? "> Charge" : "  Charge");
    display.setCursor(25, 26);
    display.print((selectedMode == 1) ? "> Discharge" : "  Discharge");
    display.setCursor(25, 40);
    display.print((selectedMode == 2) ? "> Analyze" : "  Analyze");
    display.setCursor(25, 54);
    display.print((selectedMode == 3) ? "> IR Test" : "  IR Test");
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
    display.setCursor(2, 10);
    display.print("Select Dischrg Curr:");
    display.setTextSize(2);
    display.setCursor(15, 30);
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

    // Log data with actual charge current from LP4060
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
    display.setCursor(25, 5);
    display.print("Charging..");
    display.setCursor(40, 25);
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setTextSize(2);
    display.setCursor(15, 40);
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

    // Log data with actual charge current from LP4060
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
    display.setCursor(10, 5);
    display.print("Analyzing - C");
    display.setCursor(40, 25);
    display.print(Hour);
    display.print(":");
    display.print(Minute);
    display.print(":");
    display.print(Second);
    display.setTextSize(2);
    display.setCursor(15, 40);
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
        // Start discharge
        cutoffVoltage = 3.0;
        PWM_Index = 6;  // 500mA
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
    display.setCursor(10, 5);
    display.print("Analyzing - D");
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
    // Wait for any button press
    if (Mode_Button.wasReleased() || UP_Button.wasReleased() || Down_Button.wasReleased()) {
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
