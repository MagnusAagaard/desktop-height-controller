# Troubleshooting Guide

This guide helps diagnose and fix common issues with the Desktop Height Controller.

## Quick Diagnostics

Open the web interface and expand the "Diagnostics" section to see:
- Raw sensor reading (mm)
- Filtered sensor reading (mm)
- Calculated height (cm)
- Sensor validity
- Movement state
- Last error

## Common Issues

---

### Sensor Not Detected

**Symptoms:**
- Web interface shows "Sensor error"
- Height displays as "--" or "0"
- Diagnostics show "Valid: No"

**Solutions:**

1. **Check I2C connections**
   ```
   ESP32 GPIO 21 → VL53L5CX SDA
   ESP32 GPIO 22 → VL53L5CX SCL
   ```

2. **Verify sensor power**
   - VL53L5CX needs 3.3V (not 5V!)
   - Check with multimeter

3. **Check I2C pull-ups**
   - Some setups need 4.7kΩ pull-ups on SDA/SCL
   - Many breakout boards include these

4. **Scan I2C bus**
   ```cpp
   // Add to setup() temporarily
   Wire.begin(21, 22);
   for (byte i = 1; i < 127; i++) {
     Wire.beginTransmission(i);
     if (Wire.endTransmission() == 0) {
       Serial.printf("Found device at 0x%02X\n", i);
     }
   }
   ```
   - VL53L5CX should appear at 0x29

5. **Try a different sensor**
   - Sensor may be faulty

---

### Desk Won't Move

**Symptoms:**
- Setting target height has no effect
- Status stays "Idle"
- No motor activity

**Solutions:**

1. **Check calibration status**
   - System won't move if not calibrated
   - Run calibration procedure first

2. **Verify MOSFET connections**
   ```
   GPIO 25 → MOSFET gate (UP)
   GPIO 26 → MOSFET gate (DOWN)
   ```

3. **Check MOSFET gate voltage**
   - Should be 3.3V when active, 0V when inactive
   - Use multimeter to verify

4. **Verify motor power**
   - Check motor power supply voltage
   - Verify connections to desk controller

5. **Check pull-down resistors**
   - 10kΩ from each gate to ground
   - Prevents floating gates

6. **Test MOSFETs manually**
   - Briefly apply 3.3V to gate
   - Motor should respond

7. **Check sensor validity**
   - Movement stops if sensor is invalid
   - See "Sensor Not Detected" section

---

### Height Reading is Incorrect

**Symptoms:**
- Displayed height doesn't match actual height
- Consistent offset (e.g., always 5cm too high)
- Readings vary from actual

**Solutions:**

1. **Recalibrate**
   - Most common fix
   - See [Calibration Guide](calibration.md)

2. **Check sensor mounting**
   - Sensor should face straight down
   - No tilt or angle
   - Secure mounting (no vibration)

3. **Clear obstacles**
   - Nothing between sensor and floor
   - Check for cables, objects

4. **Check floor surface**
   - Very dark floors may affect readings
   - Very reflective floors may cause issues
   - Try placing a piece of cardboard as test

5. **Verify measurement technique**
   - Measure to same point as sensor target
   - Account for desk thickness if needed

---

### WiFi Connection Issues

**Symptoms:**
- Can't connect to "DeskController" network
- AP mode doesn't start
- Keeps disconnecting

**Solutions:**

1. **Check if AP mode is active**
   - Default AP: "DeskController-XXXX"
   - Password: none (open network)
   - IP: 192.168.4.1

2. **Configure station mode**
   - Connect to AP mode first
   - Use web interface to enter WiFi credentials
   - Device will restart in station mode

3. **Check WiFi credentials**
   - Verify SSID and password
   - 2.4GHz only (ESP32 doesn't support 5GHz)

4. **Check signal strength**
   - Move ESP32 closer to router
   - Diagnostics show RSSI value
   - Below -80 dBm is poor

5. **Factory reset WiFi**
   - Hold BOOT button during startup
   - Or use serial command to clear NVS

---

### Movement Timeout Error

**Symptoms:**
- Desk stops moving mid-operation
- Error: "Movement timeout"
- State shows "ERROR"

**Solutions:**

1. **Check for mechanical issues**
   - Desk may be stuck
   - Check for obstructions
   - Verify desk can move manually

2. **Reduce target distance**
   - Very long moves may timeout
   - Try moving in smaller increments

3. **Check motor response**
   - Motor may be too slow
   - Check motor power supply

4. **Increase timeout** (advanced)
   - Default is 30 seconds
   - Can adjust via /config endpoint

5. **Clear error and retry**
   - Use emergency stop button
   - Wait a moment
   - Try again

---

### Web Interface Not Loading

**Symptoms:**
- Cannot access web interface
- Page loads but is broken
- "404 Not Found" errors

**Solutions:**

1. **Check SPIFFS upload**
   - Files must be in `data/` folder
   - Run `pio run -t uploadfs`

2. **Verify IP address**
   - AP mode: 192.168.4.1
   - Station mode: Check router DHCP or serial output

3. **Check file sizes**
   - SPIFFS has limited space
   - Total < 1MB recommended

4. **Clear browser cache**
   - Ctrl+Shift+R to force refresh
   - Or try incognito mode

5. **Check serial output**
   - Connect via USB
   - Look for SPIFFS mount errors

---

### Desk Oscillates / Won't Stabilize

**Symptoms:**
- Desk moves up and down repeatedly
- Never reaches "Idle" state
- Movement seems unstable

**Solutions:**

1. **Increase tolerance**
   - Default: ±1cm
   - Can increase via /config if needed

2. **Check sensor stability**
   - Ensure secure mounting
   - No vibration affecting readings

3. **Extend stabilization time**
   - Default: 2 seconds
   - May need longer for some desks

4. **Check for mechanical backlash**
   - Some desks have play in mechanism
   - May need larger tolerance

---

### Presets Not Saving

**Symptoms:**
- Presets lost after reboot
- Save button doesn't work
- Error saving preset

**Solutions:**

1. **Check NVS storage**
   - NVS may be full
   - Try factory reset

2. **Verify valid values**
   - Height must be within range (50-125cm)
   - Name max 16 characters

3. **Check serial output**
   - Look for NVS errors
   - May indicate flash issues

---

## Error Codes

| Code | Meaning | Solution |
|------|---------|----------|
| SENSOR_ERROR | Sensor read failed | Check I2C connections |
| SENSOR_TIMEOUT | No response from sensor | Verify sensor power |
| MOVEMENT_TIMEOUT | Movement took > 30s | Check mechanical issues |
| INVALID_HEIGHT | Height out of range | Use valid range (50-125) |
| NOT_CALIBRATED | System not calibrated | Run calibration |
| NVS_ERROR | Storage error | Try factory reset |

---

## Factory Reset

To reset all settings:

1. **Via serial command:**
   ```
   Connect at 115200 baud
   Send: FACTORY_RESET
   ```

2. **Via button:**
   - Hold BOOT button for 10 seconds during startup

3. **Via code:**
   - Uncomment factory reset in setup()
   - Upload, run once, revert

---

## Getting Help

If issues persist:

1. **Check serial output** at 115200 baud
2. **Note error messages** from diagnostics
3. **Document your hardware** setup
4. **Create minimal reproducible** example

---

## Related Documentation

- [Hardware Setup](hardware-setup.md)
- [Calibration Guide](calibration.md)
