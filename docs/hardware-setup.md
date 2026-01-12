# Hardware Setup Guide

This guide covers the hardware wiring and assembly for the Desktop Height Controller.

## Components Required

### Main Components
- **ESP32-WROOM-32** development board (4MB flash recommended)
- **VL53L5CX** Time-of-Flight sensor module
- **2x N-channel MOSFET** (e.g., IRLZ44N) for motor control
- **2x Flyback diodes** (e.g., 1N4007) for motor protection
- **2x Pull-down resistors** (10kΩ) for MOSFET gates
- **Power supply** 5V 2A for ESP32, motor power per your desk controller specs

### Optional Components
- **Level shifter** (3.3V to 5V) if your desk controller requires 5V logic
- **Status LEDs** for visual feedback
- **Enclosure** for mounting

## Wiring Diagram

### ESP32 Pin Connections

| ESP32 Pin | Connection | Notes |
|-----------|------------|-------|
| GPIO 21 | VL53L5CX SDA | I2C Data |
| GPIO 22 | VL53L5CX SCL | I2C Clock |
| GPIO 25 | MOSFET UP Gate | Move desk up |
| GPIO 26 | MOSFET DOWN Gate | Move desk down |
| 3.3V | VL53L5CX VCC | Sensor power |
| GND | Common ground | Connect all grounds |

### VL53L5CX Connections

The VL53L5CX is connected via I2C:

```
ESP32           VL53L5CX
─────           ────────
3.3V ─────────── VCC
GND  ─────────── GND
GPIO 21 ──────── SDA
GPIO 22 ──────── SCL
GND  ─────────── I2C_RST (optional, tie low)
```

**I2C Address**: 0x29 (default)

### MOSFET Motor Control Circuit

Each MOSFET circuit controls one direction of desk movement:

```
                    +Vmotor (from desk PSU)
                        │
                        │
              ┌─────────┴─────────┐
              │                   │
             ─┴─                 ─┴─
            │   │ Motor         │   │
            │ ↑ │ UP            │ ↓ │ Motor DOWN
             ─┬─                 ─┬─
              │                   │
              │    ┌──────────────┤
              │    │              │
         D ───┤    │         D ───┤
  IRLZ44N    │├───┘    IRLZ44N   │├───┘
             ││                  ││
         G ──┤│              G ──┤│
              │                   │
              │                   │
         GPIO 25             GPIO 26
              │                   │
         ┌────┴────┐         ┌────┴────┐
         │         │         │         │
        10kΩ     ESP32      10kΩ     ESP32
         │         │         │         │
        GND       OUT       GND       OUT
```

**Important**: 
- Add flyback diodes across motor terminals (cathode to positive)
- 10kΩ pull-down resistors prevent floating gate states
- Never activate both MOSFETs simultaneously!

### Complete Wiring Checklist

- [ ] ESP32 powered via USB or 5V input
- [ ] VL53L5CX sensor mounted facing desk surface
- [ ] I2C connections (SDA, SCL) with 4.7kΩ pull-ups if needed
- [ ] MOSFET gates connected to GPIO 25 (UP) and GPIO 26 (DOWN)
- [ ] 10kΩ pull-down on each MOSFET gate
- [ ] Flyback diodes on motor connections
- [ ] Common ground between ESP32 and motor circuit
- [ ] Motor connections to desk controller

## Sensor Mounting

### Mounting Position
- Mount VL53L5CX sensor facing **downward** toward the floor
- Recommended height: Fixed position on desk underside
- Ensure clear line of sight to floor (no obstacles)

### Distance Calculation
The sensor measures distance to the floor. Height is calculated as:
```
desk_height = calibration_constant - (sensor_reading / 10)
```

Where:
- `calibration_constant` is determined during calibration
- `sensor_reading` is in millimeters
- Result is in centimeters

### Field of View
VL53L5CX has an 8x8 zone capability, but this project uses single-zone mode for:
- Faster sampling (5Hz target)
- Simpler processing
- Adequate accuracy for height measurement

## Safety Considerations

### Electrical Safety
1. **Isolate high-voltage circuits** - Motor control may involve high currents
2. **Use appropriate wire gauges** for motor connections
3. **Add fuses** on motor power lines
4. **Enclose the circuit** to prevent accidental contact

### Mechanical Safety
1. **Test with manual override** - Ensure you can disconnect power if needed
2. **Set appropriate height limits** - Configure min/max in software
3. **Don't exceed desk motor specs** - Check duty cycle requirements

### Software Safety Features
The firmware includes:
- Movement timeout (30 seconds max)
- Sensor validity checking
- Emergency stop command
- Mutual exclusion (one direction at a time)

## Troubleshooting

### Sensor Not Detected
1. Check I2C connections (SDA, SCL)
2. Verify power to sensor (3.3V)
3. Check I2C address (default 0x29)
4. Add pull-up resistors if needed

### Motor Not Moving
1. Verify MOSFET gate voltage (should be 3.3V when active)
2. Check pull-down resistors
3. Verify motor power supply
4. Test MOSFETs with multimeter

### Erratic Readings
1. Ensure stable sensor mounting
2. Clear obstacles in sensor path
3. Check for reflective surfaces
4. Verify power supply stability

## Next Steps

After wiring is complete:
1. Upload firmware via PlatformIO
2. Connect to "DeskController" WiFi network
3. Access web interface at http://192.168.4.1
4. Run calibration procedure (see [Calibration Guide](calibration.md))
