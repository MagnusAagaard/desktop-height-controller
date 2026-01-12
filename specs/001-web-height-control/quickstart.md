# Quickstart Guide: Desktop Height Controller

**Feature**: 001-web-height-control  
**Date**: 2026-01-11  
**Audience**: Developers and makers setting up the system for the first time

---

## Table of Contents

1. [Hardware Requirements](#hardware-requirements)
2. [Wiring Diagram](#wiring-diagram)
3. [Software Setup](#software-setup)
4. [First Boot Configuration](#first-boot-configuration)
5. [Calibration](#calibration)
6. [Basic Usage](#basic-usage)
7. [Troubleshooting](#troubleshooting)

---

## Hardware Requirements

### Components

| Component | Specification | Quantity | Notes |
|-----------|---------------|----------|-------|
| **Microcontroller** | ESP32-WROOM-32 (4MB flash) | 1 | ESP32 DevKit V1 or equivalent |
| **Distance Sensor** | VL53L5CX Time-of-Flight sensor | 1 | SparkFun Qwiic board recommended |
| **MOSFETs** | N-channel, logic-level (e.g., IRLZ44N) | 2 | For desk motor control |
| **Resistors** | 10kΩ pull-down | 2 | MOSFET gate resistors |
| **Diodes** | 1N4007 flyback diodes | 2 | Motor protection |
| **Power Supply** | 5V 2A USB | 1 | For ESP32 |
| **Jumper Wires** | - | ~10 | For connections |
| **Breadboard** (optional) | - | 1 | For prototyping |

### Desk Requirements

- Electric standing desk with **UP** and **DOWN** buttons
- Accessible motor control wires (we'll parallel the existing buttons)
- Power outlet for ESP32 supply

---

## Wiring Diagram

### VL53L5CX Sensor (I2C)

```
ESP32          VL53L5CX Sensor
─────          ──────────────
GPIO 21 (SDA)  ──→ SDA
GPIO 22 (SCL)  ──→ SCL
3.3V           ──→ VCC
GND            ──→ GND
```

**Important**: VL53L5CX operates at 3.3V. Do NOT connect to 5V.

### MOSFET Motor Control (2 channels)

#### Channel 1: UP Control

```
                        ┌──────────────┐
ESP32 GPIO 25 ──┬──────→ GATE         │
                │       │   MOSFET    │ DRAIN ───→ Desk Motor UP+
              10kΩ      │  (IRLZ44N)  │
                │       │             │ SOURCE ──→ GND
               GND      └──────────────┘
                                │
                            (1N4007 Diode)
                            Anode→GND, Cathode→Drain
```

#### Channel 2: DOWN Control

```
                        ┌──────────────┐
ESP32 GPIO 26 ──┬──────→ GATE         │
                │       │   MOSFET    │ DRAIN ───→ Desk Motor DOWN+
              10kΩ      │  (IRLZ44N)  │
                │       │             │ SOURCE ──→ GND
               GND      └──────────────┘
                                │
                            (1N4007 Diode)
                            Anode→GND, Cathode→Drain
```

### Complete Schematic

```
                       ESP32-WROOM-32
                    ┌──────────────────┐
                    │                  │
                    │  GPIO 21 (SDA)   ├──→ VL53L5CX SDA
                    │  GPIO 22 (SCL)   ├──→ VL53L5CX SCL
                    │  3.3V            ├──→ VL53L5CX VCC
                    │  GND             ├──→ VL53L5CX GND, MOSFETs SOURCE
                    │                  │
                    │  GPIO 25         ├──→ UP MOSFET GATE
                    │  GPIO 26         ├──→ DOWN MOSFET GATE
                    │                  │
                    │  USB (5V)        ├──→ Power Supply
                    └──────────────────┘

Desk Motor Connections:
- Desk UP button wire → UP MOSFET DRAIN (parallel connection)
- Desk DOWN button wire → DOWN MOSFET DRAIN (parallel connection)
- Desk GND → ESP32 GND (common ground)
```

**Safety Notes**:
- Ensure common ground between ESP32 and desk motor circuit
- MOSFETs act as electronic switches paralleling existing desk buttons
- Flyback diodes protect against inductive kickback from motors
- Never activate both MOSFETs simultaneously (enforced in software)

---

## Software Setup

### 1. Install PlatformIO

**VS Code Extension** (Recommended):
1. Open VS Code
2. Install "PlatformIO IDE" extension
3. Restart VS Code

**CLI Alternative**:
```bash
pip install platformio
```

### 2. Clone/Create Project

```bash
# If starting from scratch
mkdir desktop-height-controller
cd desktop-height-controller

# Initialize PlatformIO project
pio project init --board esp32dev
```

### 3. Configure platformio.ini

Edit `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps = 
    sparkfun/SparkFun VL53L5CX Arduino Library@^1.0.3
    me-no-dev/ESPAsyncWebServer@^1.2.3
    me-no-dev/AsyncTCP@^1.1.1

board_build.filesystem = spiffs
board_build.partitions = min_spiffs.csv
```

### 4. Install Dependencies

```bash
pio lib install
```

### 5. Upload Filesystem (Web UI)

1. Place `index.html`, `style.css`, `script.js` in `data/` directory
2. Upload to SPIFFS:
   ```bash
   pio run --target uploadfs
   ```

### 6. Compile and Upload Firmware

```bash
# Build
pio run

# Upload (ESP32 must be connected via USB)
pio run --target upload

# Monitor serial output
pio device monitor
```

---

## First Boot Configuration

### 1. WiFi Setup

**On first boot, ESP32 creates an Access Point:**

1. **Connect** to WiFi network: `DeskController-XXXXXX` (password: `desksetup`)
2. **Open browser** to `http://192.168.4.1`
3. **Enter your WiFi credentials**:
   - SSID: Your home WiFi network
   - Password: Your WiFi password
4. **Click "Connect"**
5. ESP32 will reboot and connect to your network

**Find ESP32 IP Address**:
- Check serial monitor output: `Connected to WiFi: 192.168.1.100`
- Or check your router's DHCP client list

### 2. Web Interface Access

Open browser to ESP32's IP address (e.g., `http://192.168.1.100`)

You should see:
```
┌────────────────────────────────┐
│   Desk Height Controller       │
├────────────────────────────────┤
│ Current Height: -- cm          │
│ Status: Uncalibrated           │
│                                │
│ [Calibrate System]             │
└────────────────────────────────┘
```

---

## Calibration

**Required before first use** to establish the height calculation baseline.

### Step-by-Step Calibration

1. **Measure Desk Height Manually**:
   - Use tape measure or ruler
   - Measure from floor to desktop surface
   - Example: 75 cm (sitting position)

2. **Open Calibration Wizard**:
   - Click "Settings" → "Calibration"

3. **Enter Known Height**:
   ```
   Current Desk Height: [75] cm
   [Calculate Calibration Constant]
   ```

4. **System Calculates**:
   - Reads sensor: `raw_distance_mm = 1050`
   - Computes: `calibration_constant = known_height + (raw_distance_mm / 10)`
   - Example: `145 = 75 + (1050 / 10)`

5. **Save**:
   - Constant stored in NVS (persistent across reboots)
   - Height readings now accurate

### Verification

After calibration:
1. Move desk manually to different height
2. Verify web UI shows correct height (±1 cm tolerance acceptable)
3. If incorrect, re-run calibration

---

## Basic Usage

### Manual Height Adjustment

1. **Navigate** to main web UI (`http://<ESP32_IP>`)

2. **Enter Target Height**:
   ```
   Target Height: [95] cm
   [Go to Height]
   ```

3. **Desk Moves Automatically**:
   - Status updates in real-time via SSE
   - Height display updates every 200ms
   - Movement stops when within ±1 cm of target

### Save a Preset

1. **Move desk to desired height** (manually or via target)

2. **Click "Save Preset"**:
   ```
   Preset Slot: [1]
   Label: [Standing]
   Height: 110 cm (current)
   [Save]
   ```

3. **Preset button appears**:
   ```
   ┌────────────────┐
   │  Standing      │
   │  110 cm        │
   └────────────────┘
   ```

### Activate Preset

1. **Click preset button** (e.g., "Standing - 110 cm")
2. Desk moves to saved height automatically

### Emergency Stop

- **Click "STOP" button** at any time
- Desk halts immediately
- Or press physical desk button (hardware override always works)

---

## Troubleshooting

### Problem: Sensor Not Detected

**Symptoms**:
- Web UI shows "Sensor Failure"
- Serial output: `VL53L5CX init failed`

**Solutions**:
1. **Check Wiring**:
   - SDA/SCL not swapped?
   - Connections secure?
   - Sensor powered by 3.3V (not 5V)?

2. **I2C Scan**:
   ```bash
   # In serial monitor
   > scan_i2c
   # Should show device at 0x29
   ```

3. **Try Different I2C Pins** (edit `Config.h`):
   ```cpp
   #define I2C_SDA 18
   #define I2C_SCL 19
   ```

---

### Problem: Desk Doesn't Move

**Symptoms**:
- Web UI shows "MOVING_UP" but desk stationary
- MOSFETs not switching

**Solutions**:
1. **Verify MOSFET Connections**:
   - Gate resistor present?
   - MOSFET source to GND?
   - Drain to desk motor wire?

2. **Test MOSFET Manually**:
   ```bash
   # In serial monitor
   > test_gpio 25 HIGH
   # Should activate UP motor
   ```

3. **Check Desk Wiring**:
   - Parallel connection correct?
   - Common ground established?

---

### Problem: Height Reading Incorrect

**Symptoms**:
- Web UI shows 150 cm when desk is 75 cm
- Or negative heights

**Solutions**:
1. **Re-run Calibration** (see Calibration section)

2. **Verify Sensor Orientation**:
   - Sensor facing floor (not ceiling)?
   - No obstructions in sensor field of view?

3. **Check Sensor Range**:
   - Desk height 50-200 cm? (VL53L5CX range 0-400 cm)
   - If desk >200 cm, modify `max_safe_height` in config

---

### Problem: WiFi Connection Drops

**Symptoms**:
- Web UI shows "Reconnecting..."
- SSE events stop

**Solutions**:
1. **Check WiFi Signal**:
   - ESP32 near router?
   - Signal strength >-70 dBm?

2. **Enable WiFi Auto-Reconnect** (default in firmware):
   ```cpp
   WiFi.setAutoReconnect(true);
   ```

3. **Check Router DHCP**:
   - Assign static IP to ESP32?
   - Increase DHCP lease time?

---

### Problem: Web UI Won't Load

**Symptoms**:
- Browser shows "Connection Refused" or "Timeout"

**Solutions**:
1. **Verify ESP32 IP**:
   ```bash
   # Check serial monitor for:
   # "Web server started on http://192.168.1.100"
   ```

2. **Ping ESP32**:
   ```bash
   ping 192.168.1.100
   ```

3. **Re-upload Filesystem**:
   ```bash
   pio run --target uploadfs
   ```

---

## Advanced Configuration

### Serial Console Commands

Access via PlatformIO Serial Monitor (115200 baud):

| Command | Description | Example |
|---------|-------------|---------|
| `status` | Show system status | `> status` |
| `calibrate <height>` | Set calibration constant | `> calibrate 75` |
| `preset <slot> <label> <height>` | Save preset | `> preset 1 Sitting 75` |
| `config <key> <value>` | Update setting | `> config min_height 60` |
| `wifi <ssid> <password>` | Change WiFi | `> wifi MyNetwork pass123` |
| `reboot` | Restart ESP32 | `> reboot` |
| `factory_reset` | Erase NVS (all settings) | `> factory_reset` |

### Adjusting Performance

**Reduce Memory Usage** (if <50KB free heap):
- Limit SSE clients (modify `MAX_SSE_CLIENTS` in Config.h)
- Reduce filter window size (3 instead of 5)

**Improve Responsiveness**:
- Increase sensor frequency (10 Hz instead of 5 Hz)
- Reduce stabilization duration (1000ms instead of 2000ms)

**Increase Precision**:
- Tighten tolerance (5mm instead of 10mm)
- Increase filter window (7 or 10 samples)

---

## Next Steps

1. **Test All Presets**: Ensure reliable movement to each saved height
2. **Monitor for Errors**: Check serial logs for sensor/WiFi issues
3. **Tune Performance**: Adjust stabilization/tolerance for your desk's characteristics
4. **Add Automation** (optional): Integrate with Home Assistant, IFTTT, or voice assistants

---

## Support Resources

- **Specification**: `specs/001-web-height-control/spec.md`
- **Data Model**: `specs/001-web-height-control/data-model.md`
- **API Docs**: `specs/001-web-height-control/contracts/http-api.md`
- **SSE Events**: `specs/001-web-height-control/contracts/sse-events.md`

For hardware issues, consult:
- [SparkFun VL53L5CX Hookup Guide](https://learn.sparkfun.com/tutorials/qwiic-vl53l5cx-hookup-guide)
- [ESP32 Pinout Reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)
- [MOSFET Tutorial](https://www.electronics-tutorials.ws/transistor/tran_7.html)

---

## Safety Warnings

⚠️ **IMPORTANT**:
- Always test manual desk controls work (hardware override)
- Verify MOSFET heat dissipation (should be cool to touch)
- Never modify firmware while desk is moving
- Keep sensor unobstructed (pets, cables, etc.)
- Set conservative min/max heights initially (50-125 cm)

🔧 **Maintenance**:
- Check sensor alignment monthly
- Re-calibrate if height readings drift (>2 cm error)
- Update firmware for bug fixes/features

---

**Congratulations!** Your desk height controller is ready to use. Enjoy hands-free height adjustments! 🎉
