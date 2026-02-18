# Li-Ion 18650 Capacity Tester

A smart multipurpose battery tester for single-cell lithium-ion batteries. This device can charge, discharge, analyze capacity, and measure internal resistance of Li-Ion cells.

## Credits

This project is based on the excellent work by **Open Green Energy** (opengreenenergy).

- **Original Project**: [DIY Smart Multipurpose Battery Tester](https://www.instructables.com/DIY-Smart-Multipurpose-Battery-Tester/)
- **Author Website**: [www.opengreenenergy.com](http://www.opengreenenergy.com)

## Firmware Versions

This repository contains three versions of the firmware:

| Version | Location | Description |
|---------|----------|-------------|
| **Original** | `Arduino Sketches/Smart_Multipurpose_Battery_Tester_20241025/` | Unmodified original firmware |
| **Modified** | `Arduino Sketches/Smart_Multipurpose_Battery_Tester_Modified/` | Button fixes and abort functionality |
| **Web GUI** | `Arduino Sketches/Smart_Multipurpose_Battery_Tester_Modified_WebGUI/` | Full web interface with WiFi connectivity |

## Features

### Operating Modes

| Mode | Description |
|------|-------------|
| **Charge** | Charges battery to 4.18V using the LP4060 charging IC |
| **Discharge** | Discharges battery at selectable current (0-2000mA) to measure capacity |
| **Analyze** | Full cycle: charge to full, rest, then discharge to measure true capacity. Supports optional staged discharge with different currents. |
| **IR Test** | Measures internal resistance using voltage drop under load |
| **Bat Check** | Real-time voltage monitoring with battery status indicator - useful for calibration verification |
| **WiFi Info** | Displays current WiFi connection status and IP addresses (Web GUI version) |

### Key Capabilities

- Real-time OLED display showing voltage, current, capacity, and elapsed time
- Adjustable discharge cutoff voltage (2.8V - 3.2V)
- Selectable discharge current (0mA - 2000mA in steps)
- High current warning for 1500mA and 2000mA settings
- Battery protection against overcharge, over-discharge, and short circuits
- Abort any operation by pressing the MODE button
- **Web GUI version**: Full web interface accessible via WiFi

---

## Web GUI Version

The Web GUI version adds a complete web-based interface for monitoring and controlling the battery tester remotely.

### Web Interface Features

| Feature | Description |
|---------|-------------|
| **Real-time Monitoring** | Live voltage, current, capacity, and elapsed time display |
| **Interactive Chart** | Voltage and current plotted over time |
| **Remote Control** | Start/Stop operations from any device on the network |
| **Mode Selection** | Select Charge, Discharge, Analyze, or IR Test from the web |
| **Discharge Settings** | Configure cutoff voltage and discharge current via web UI |
| **Staged Analyze** | Optional two-stage discharge with configurable transition voltage and currents |
| **IR Test Results** | Internal resistance displayed in web interface (persists until next operation) |
| **Error Feedback** | Clear error messages when operations fail (no battery, damaged battery, etc.) |
| **WiFi Configuration** | Connect to existing WiFi networks through the web interface |
| **Auto-Reconnect** | Remembers last WiFi network and auto-connects on boot |

### WiFi Connectivity

The Web GUI version supports two WiFi modes:

#### Access Point (AP) Mode - Default
- Device creates its own WiFi network
- **SSID**: `BatteryTester`
- **Password**: `battery123`
- **IP Address**: `192.168.4.1`
- Connect to this network and navigate to `http://192.168.4.1`

#### Station (STA) Mode - Connect to Existing Network
- Configure via the WiFi panel in the web interface
- Device connects to your existing WiFi network
- Access via the IP address assigned by your router
- **AP auto-disable**: After connecting to a network, the device's AP remains active for 45 seconds (so you can see the new IP), then automatically disables
- **Credential Storage**: WiFi credentials are saved to non-volatile storage (NVS) and persist across reboots
- **Auto-Reconnect**: On boot, the device automatically attempts to connect to the last saved network
- **Forget Network**: Use the "Forget Network" button in the web interface to clear saved credentials

#### WiFi Info on OLED
- Select **WiFi Info** from the main menu to view:
  - AP status and IP address
  - Connected network name and IP address
- Useful for finding the device's IP address without needing another device

### Web GUI Files

| File | Purpose |
|------|---------|
| `Smart_Multipurpose_Battery_Tester_Modified_WebGUI.ino` | Main firmware with web server |
| `WebContent.h` | HTML, CSS, and JavaScript for web interface |
| `WiFiConfig.h` | WiFi configuration settings |
| `DataLogger.h` | Data logging for chart history |

### Additional Dependencies (Web GUI)

In addition to the base dependencies, the Web GUI version requires:

- `WiFi.h` - ESP32 WiFi library
- `ESPAsyncWebServer.h` - Async web server library
- `ArduinoJson.h` - JSON parsing for WebSocket communication

---

## Modifications (Fork Changes)

### Button Debounce Fixes

| Issue | Original Behavior | Fixed Behavior |
|-------|-------------------|----------------|
| **Button method** | Used `isPressed()` which triggers continuously while held | Changed to `wasReleased()` for one action per press |
| **Menu skipping** | Selecting Discharge would skip the cutoff voltage screen | Added `clearButtonStates()` to clear pending button events between screens |
| **Debounce timing** | 400ms delay, still experienced bounce | Reduced to 250ms with proper state-based detection |

### Abort Functionality

- **Press MODE during any operation** to safely abort and return to main menu
- Works during: Charge, Discharge, Analyze (all phases), and IR Test
- Hardware is automatically reset to safe idle state (charging off, load off)
- **Double beep** indicates abort (vs single beep for normal completion)

### High Current Discharge Options

The discharge current range has been extended to support higher currents:

| Current Range | Notes |
|---------------|-------|
| 0 - 1000mA | Standard range, safe for continuous use |
| 1500mA | High current - warning displayed |
| 2000mA | High current - warning displayed |

**Warning**: Currents above 1000mA may cause MOSFET overheating. Ensure adequate cooling (heatsink, airflow) when using high current settings. The OLED and web interface display warnings when high currents are selected.

### Helper Functions

| Function | Purpose |
|----------|---------|
| `clearButtonStates()` | Waits for all buttons to be released and clears pending events |
| `resetToIdle()` | Safely turns off charging MOSFET and discharge load |
| `checkAbort()` | Checks for MODE button press, resets hardware, returns abort status |
| `saveWiFiCredentials()` | Saves SSID and password to ESP32 non-volatile storage |
| `loadWiFiCredentials()` | Loads saved credentials from NVS on boot |
| `clearWiFiCredentials()` | Clears saved WiFi credentials from NVS |

### Other Changes

- **Voltage reference** adjusted from 1.227V to 1.26V for improved accuracy
- **IR Test comment** corrected: PWM_Index 6 = 500mA (not 1A as originally commented)
- **Code cleanup**: Consistent button handling across all menus

---

## Hardware

### Components

| Component | Reference | Description |
|-----------|-----------|-------------|
| **Microcontroller** | - | XIAO ESP32C3 |
| **Display** | - | 0.96" OLED 128x64 (I2C, address 0x3C) |
| **Charger IC** | - | LP4060 (CC/CV charging) |
| **Protection IC** | - | AP6685 |
| **Op-Amp** | - | LMV321B |
| **Load MOSFET** | - | IRL540 (with heatsink) |
| **Voltage Reference** | U6 | LM385-1.2V |
| **Power Input** | - | USB Type-C (5V) |

### Pin Configuration (XIAO ESP32C3)

| Pin | Function |
|-----|----------|
| D3 | MODE button |
| D6 | UP button |
| D9 | DOWN button |
| D8 | PWM output (discharge load control) |
| D7 | Buzzer |
| D2 | Charging MOSFET control |
| A0 | Battery voltage sense |
| A1 | Voltage reference sense |

### Battery Connections

- **18650 holder** for standard 18650 cells
- **JST connector** for other Li-Ion battery types

---

## Usage

### Navigation

- **UP/DOWN buttons**: Navigate menus or adjust values
- **MODE button**: Confirm selection or abort current operation

### Discharge Mode

1. Select **Discharge** from the main menu
2. Set the cutoff voltage (2.8V - 3.2V) using UP/DOWN, confirm with MODE
3. Set the discharge current (0 - 2000mA) using UP/DOWN, confirm with MODE
   - Warning displayed for currents above 1000mA
4. Discharge begins - display shows voltage, time, and accumulated capacity
5. Press MODE at any time to abort

### Analyze Mode

1. Select **Analyze** from the main menu
2. Choose between **Standard** or **Staged Discharge** mode:
   - **Standard**: Single discharge current throughout
   - **Staged**: Two-stage discharge with different currents
3. For **Staged Discharge**, configure:
   - **Stage 1**: Current and transition voltage (when to switch to Stage 2)
   - **Stage 2**: Current (must be ≤ Stage 1) and final cutoff voltage
4. The tester will:
   - Charge the battery to full (4.18V)
   - Rest for 3 minutes to stabilize
   - Discharge to the configured cutoff voltage (with stage transition if enabled)
5. Final capacity is displayed in mAh
6. A beep indicates stage transition during staged discharge
7. Press MODE at any time to abort

#### Staged Discharge Benefits

Staged discharge allows more accurate capacity measurement by:
- Using higher current initially when voltage is stable
- Switching to lower current near the cutoff where voltage drops more rapidly
- Reducing measurement error from internal resistance effects at low voltages

| Parameter | Default | Range |
|-----------|---------|-------|
| Stage 1 Current | 500mA | 100-2000mA |
| Transition Voltage | 3.3V | Must be > final cutoff |
| Stage 2 Current | 300mA | 100mA - Stage 1 current |
| Final Cutoff | 3.0V | 2.8V - 3.2V |

### IR Test Mode

1. Select **IR Test** from the main menu
2. The tester measures:
   - Open circuit voltage (no load)
   - Voltage under 500mA load
3. Internal resistance is calculated and displayed in milliohms

### Battery Check Mode

1. Select **Bat Check** from the main menu
2. Real-time voltage monitoring:
   - Voltage displayed continuously (updates every loop cycle)
   - Battery status indicator: No Battery, Damaged, Low, Good, Full
   - Visual battery level bar
3. Useful for:
   - Verifying voltage calibration against a multimeter
   - Quick battery health check before operations
   - Troubleshooting voltage reading accuracy (see calibration section)
4. Press any button to return to menu

### WiFi Info (Web GUI Version)

1. Select **WiFi Info** from the main menu
2. View current WiFi status:
   - AP mode: Shows SSID and IP (192.168.4.1)
   - STA mode: Shows connected network and assigned IP
3. Press any button to return to menu

### Using the Web Interface (Web GUI Version)

1. Connect to the `BatteryTester` WiFi network (password: `battery123`)
2. Open a browser and navigate to `http://192.168.4.1`
3. Select a mode using the buttons at the top
4. Configure settings (for Discharge mode)
5. Press **START** to begin the operation
6. Monitor progress in real-time via the stats and chart
7. Press **STOP** to abort if needed

#### Connecting to Your Home Network

1. Click the **WiFi** button in the web interface header
2. Enter your network SSID and password
3. Click **Connect to Network**
4. Note the new IP address shown (you have 45 seconds before AP disables)
5. Connect your device to your home network
6. Access the tester at the new IP address

**Note**: Your WiFi credentials are automatically saved. On next power-up, the device will auto-connect to your network.

#### Forgetting a Saved Network

1. Open the WiFi panel in the web interface
2. Click **Forget Network (Clear Saved)**
3. Confirm the action when prompted
4. The device will return to AP-only mode and won't auto-connect on next boot

---

## Calibration

### Voltage Reference Calibration

The voltage reference can be calibrated by adjusting `Vref_Voltage` in the firmware (default: 1.26V for LM385-1.2V reference U6).

```cpp
float Vref_Voltage = 1.26;  // LM385-1.2V reference voltage ( adjust it for calibration, 1.227 default )
```

### How to Calibrate

1. **Establish baseline**: Note the current value (default: 1.26V)
2. **Measure reference voltage**: Use a calibrated multimeter to check an actual battery voltage
3. **Compare with device**: Discharge the same battery on the tester and note the voltage reading
4. **Calculate correction factor**:
   - If device reads **HIGH** compared to multimeter: decrease `Vref_Voltage`
   - If device reads **LOW** compared to multimeter: increase `Vref_Voltage`
5. **Apply proportional adjustment**:
   - Example: If reading is 5% high, multiply by 0.95: `1.227 × 0.95 = 1.166`
   - Recompile and test with another battery
6. **Repeat** until readings match your reference multimeter (within ±2%)

### Notes

- The U6 component (LM385-1.2V) is the voltage reference IC used for ADC calibration
- Factory default is typically 1.227V; adjusted to 1.26V for improved accuracy in this version
- Small adjustments (±0.05V) will significantly affect readings
- Use a fully charged battery (4.0V+) and a calibrated multimeter for best calibration results

## Voltage Thresholds

| Threshold | Voltage | Purpose |
|-----------|---------|---------|
| `FULL_BAT_level` | 4.18V | Stop charging |
| `Max_BAT_level` | 3.2V | Maximum cutoff voltage setting |
| `Min_BAT_level` | 2.8V | Minimum cutoff voltage setting |
| `DAMAGE_BAT_level` | 2.5V | Battery considered damaged |
| `NO_BAT_level` | 0.3V | No battery detected |
| `HIGH_CURRENT_THRESHOLD` | 1000mA | Warning displayed above this |

## Dependencies

### Base Dependencies (All Versions)

- `Wire.h` - I2C communication
- `Adafruit_GFX.h` - Graphics library
- `Adafruit_SSD1306.h` - OLED display driver
- `JC_Button.h` - Button debouncing library

### Additional Dependencies (Web GUI Version)

- `WiFi.h` - ESP32 WiFi library (included with ESP32 board package)
- `Preferences.h` - ESP32 NVS storage library (included with ESP32 board package)
- `ESPAsyncWebServer.h` - [Async Web Server](https://github.com/me-no-dev/ESPAsyncWebServer)
- `ArduinoJson.h` - [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

## Safety Notes

- Only use with single-cell Li-Ion batteries (3.7V nominal, 4.2V max)
- Ensure adequate cooling when discharging at high currents (>1000mA)
- **High current warning**: 1500mA and 2000mA settings may cause MOSFET overheating
- Do not leave unattended during charge/discharge cycles
- The device will detect and reject damaged batteries (below 2.5V)

## License

Please refer to the original project by Open Green Energy for licensing information.
