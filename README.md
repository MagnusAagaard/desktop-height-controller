# Desktop Height Controller

Web-based standing desk height controller using ESP32 and VL53L5CX ToF sensor.

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)](https://platformio.org/)

## Features

- 📏 **Real-time height monitoring** - VL53L5CX Time-of-Flight sensor with moving average filtering
- 🌐 **Web interface** - Responsive control panel accessible from any browser
- 🎯 **Preset positions** - Save up to 5 favorite heights with custom labels
- 🔄 **Server-Sent Events** - Live height updates without page refresh
- ⚡ **Fast response** - <500ms movement response time
- 🛡️ **Safety features** - Timeout protection, sensor failure detection, emergency stop

## Hardware Requirements

| Component | Specification |
|-----------|---------------|
| Microcontroller | ESP32-WROOM-32 (4MB flash) |
| Distance Sensor | VL53L5CX ToF sensor (I2C) |
| Motor Control | 2x N-channel MOSFETs (IRLZ44N) |
| Power | 5V USB for ESP32 |

For detailed wiring diagrams and component list, see [Quickstart Guide](specs/001-web-height-control/quickstart.md).

## Quick Start

### 1. Install Dependencies

```bash
# PlatformIO CLI
pip install platformio

# Or use VS Code with PlatformIO extension
```

### 2. Build and Upload

```bash
# Build firmware
pio run

# Upload to ESP32
pio run --target upload

# Upload web files to SPIFFS
pio run --target uploadfs
```

### 3. Configure WiFi

On first boot, the controller creates a WiFi access point:
- **SSID**: `DeskController-[CHIP_ID]`
- Connect and navigate to `http://192.168.4.1` to configure

### 4. Calibrate

1. Move desk to a known height (e.g., 75cm)
2. Open web interface
3. Go to Settings → Calibration
4. Enter current height and click "Calibrate"

## Project Structure

```
├── src/
│   ├── main.cpp                 # Entry point
│   ├── Config.h                 # Pin definitions and constants
│   ├── HeightController.h/cpp   # Height measurement and filtering
│   ├── MovementController.h/cpp # Motor control state machine
│   ├── PresetManager.h/cpp      # Preset storage (NVS)
│   ├── WebServer.h/cpp          # HTTP server and SSE
│   └── WiFiManager.h/cpp        # WiFi connection handling
├── data/                        # SPIFFS web files
│   ├── index.html
│   ├── style.css
│   └── script.js
├── test/                        # Unit tests (PlatformIO Unity)
└── specs/                       # Feature specifications
```

## Configuration

Edit `src/Config.h` for hardware-specific settings:

```cpp
// Motor control pins
constexpr uint8_t PIN_MOTOR_UP = 25;
constexpr uint8_t PIN_MOTOR_DOWN = 26;

// I2C pins for VL53L5CX
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

// Height limits (cm)
constexpr uint16_t DEFAULT_MIN_HEIGHT_CM = 50;
constexpr uint16_t DEFAULT_MAX_HEIGHT_CM = 125;
```

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web interface |
| `/target` | POST | Set target height |
| `/presets` | GET | Get all presets |
| `/preset` | POST | Activate preset |
| `/preset/save` | POST | Save preset |
| `/status` | GET | Current system status |
| `/stop` | POST | Emergency stop |
| `/config` | GET/POST | Configuration |
| `/events` | GET | SSE stream |

See [HTTP API Contract](specs/001-web-height-control/contracts/http-api.md) for full documentation.

## Testing

```bash
# Run all tests
pio test

# Run specific test
pio test -f test_moving_average_filter

# With verbose output
pio test -v
```

## Troubleshooting

### Sensor not detected
- Check I2C wiring (SDA to GPIO21, SCL to GPIO22)
- Verify 3.3V power supply (NOT 5V)
- Check I2C address (default 0x29)

### WiFi won't connect
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Check credentials in NVS or use AP mode to reconfigure

### Desk doesn't move
- Verify MOSFET wiring and gate resistors
- Check GPIO pin assignments in Config.h
- Ensure common ground between ESP32 and desk circuit

For more troubleshooting tips, see [Troubleshooting Guide](docs/troubleshooting.md).

## Documentation

- [Hardware Setup](docs/hardware-setup.md) - Wiring diagrams and components
- [Calibration Guide](docs/calibration.md) - Step-by-step calibration
- [Troubleshooting](docs/troubleshooting.md) - Common issues and solutions
- [Specification](specs/001-web-height-control/spec.md) - Feature requirements
- [Implementation Plan](specs/001-web-height-control/plan.md) - Technical architecture
- [Data Model](specs/001-web-height-control/data-model.md) - Entity definitions
- [Quickstart Guide](specs/001-web-height-control/quickstart.md) - Integration scenarios

## License

MIT
