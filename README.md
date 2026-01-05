# Li-Ion 18650 Capacity Tester

A smart multipurpose battery tester for single-cell lithium-ion batteries. This device can charge, discharge, analyze capacity, and measure internal resistance of Li-Ion cells.

## Credits

This project is based on the excellent work by **Open Green Energy** (opengreenenergy).

- **Original Project**: [DIY Smart Multipurpose Battery Tester](https://www.instructables.com/DIY-Smart-Multipurpose-Battery-Tester/)
- **Author Website**: [www.opengreenenergy.com](http://www.opengreenenergy.com)

## Modifications (Fork Changes)

This fork includes several improvements to the original firmware, primarily focused on button handling and user experience:

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

### New Helper Functions

| Function | Purpose |
|----------|---------|
| `clearButtonStates()` | Waits for all buttons to be released and clears pending events |
| `resetToIdle()` | Safely turns off charging MOSFET and discharge load |
| `checkAbort()` | Checks for MODE button press, resets hardware, returns abort status |

### Other Changes

- **Voltage reference** adjusted from 1.227V to 1.26V for improved accuracy
- **IR Test comment** corrected: PWM_Index 6 = 500mA (not 1A as originally commented)
- **Code cleanup**: Consistent button handling across all menus

### Modified Files

The modified Arduino sketch is located in:
```
Arduino Sketches/Smart_Multipurpose_Battery_Tester_Modified/
```

The original unmodified sketch is preserved in:
```
Arduino Sketches/Smart_Multipurpose_Battery_Tester_20241025/
```

## Features

### Operating Modes

| Mode | Description |
|------|-------------|
| **Charge** | Charges battery to 4.18V using the LP4060 charging IC |
| **Discharge** | Discharges battery at selectable current (0-1000mA) to measure capacity |
| **Analyze** | Full cycle: charge to full, rest, then discharge to measure true capacity |
| **IR Test** | Measures internal resistance using voltage drop under load |

### Key Capabilities

- Real-time OLED display showing voltage, current, capacity, and elapsed time
- Adjustable discharge cutoff voltage (2.8V - 3.2V)
- Selectable discharge current (0mA - 1000mA in steps)
- Battery protection against overcharge, over-discharge, and short circuits
- Abort any operation by pressing the MODE button

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| **Microcontroller** | XIAO ESP32C3 |
| **Display** | 0.96" OLED 128x64 (I2C, address 0x3C) |
| **Charger IC** | LP4060 (CC/CV charging) |
| **Protection IC** | AP6685 |
| **Op-Amp** | LMV321B |
| **Load MOSFET** | IRL540 (with heatsink) |
| **Voltage Reference** | LM385-1.2V |
| **Power Input** | USB Type-C (5V) |

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

## Usage

### Navigation

- **UP/DOWN buttons**: Navigate menus or adjust values
- **MODE button**: Confirm selection or abort current operation

### Discharge Mode

1. Select **Discharge** from the main menu
2. Set the cutoff voltage (2.8V - 3.2V) using UP/DOWN, confirm with MODE
3. Set the discharge current (0 - 1000mA) using UP/DOWN, confirm with MODE
4. Discharge begins - display shows voltage, time, and accumulated capacity
5. Press MODE at any time to abort

### Analyze Mode

1. Select **Analyze** from the main menu
2. The tester will:
   - Charge the battery to full (4.18V)
   - Rest for 3 minutes to stabilize
   - Discharge at 500mA to 3.0V cutoff
3. Final capacity is displayed in mAh
4. Press MODE at any time to abort

### IR Test Mode

1. Select **IR Test** from the main menu
2. The tester measures:
   - Open circuit voltage (no load)
   - Voltage under 500mA load
3. Internal resistance is calculated and displayed in milliohms

## Calibration

The voltage reference can be calibrated by adjusting `Vref_Voltage` in the code (default: 1.26V for LM385-1.2V).

```cpp
float Vref_Voltage = 1.26;  // Adjust for calibration
```

## Voltage Thresholds

| Threshold | Voltage | Purpose |
|-----------|---------|---------|
| `FULL_BAT_level` | 4.18V | Stop charging |
| `Max_BAT_level` | 3.2V | Maximum cutoff voltage setting |
| `Min_BAT_level` | 2.8V | Minimum cutoff voltage setting |
| `DAMAGE_BAT_level` | 2.5V | Battery considered damaged |
| `NO_BAT_level` | 0.3V | No battery detected |

## Dependencies

The following Arduino libraries are required:

- `Wire.h` - I2C communication
- `Adafruit_GFX.h` - Graphics library
- `Adafruit_SSD1306.h` - OLED display driver
- `JC_Button.h` - Button debouncing library

## Safety Notes

- Only use with single-cell Li-Ion batteries (3.7V nominal, 4.2V max)
- Ensure adequate cooling when discharging at high currents
- Do not leave unattended during charge/discharge cycles
- The device will detect and reject damaged batteries (below 2.5V)

## License

Please refer to the original project by Open Green Energy for licensing information.
