# Calibration Guide

This guide explains how to calibrate your Desktop Height Controller for accurate height measurements.

## Why Calibration is Needed

The VL53L5CX sensor measures the distance from itself to the floor. To convert this into the actual desk height, the system needs to know the relationship between sensor readings and actual height. This is done through a simple calibration process.

## Calibration Formula

The system uses the following formula:

```
desk_height_cm = calibration_constant - (sensor_reading_mm / 10)
```

Where:
- `desk_height_cm` = actual desk height in centimeters
- `calibration_constant` = value determined during calibration
- `sensor_reading_mm` = filtered sensor reading in millimeters

During calibration, you provide a known height, and the system calculates the calibration constant:

```
calibration_constant = known_height_cm + (sensor_reading_mm / 10)
```

## Before You Begin

1. Ensure the sensor is securely mounted
2. Clear any obstacles between sensor and floor
3. Have a measuring tape or ruler ready
4. Connect to the controller's web interface

## Calibration Procedure

### Step 1: Position the Desk

Move your desk to a comfortable, easily measurable height. The middle of the height range works well (around 75-80cm).

### Step 2: Measure the Actual Height

Using a measuring tape:
1. Measure from the floor to the top of your desk surface
2. Record this measurement in centimeters
3. Round to the nearest centimeter

### Step 3: Open the Web Interface

1. Connect to the controller's WiFi network
2. Open a web browser and navigate to the controller's IP address
3. Scroll down to the "Calibration" section

### Step 4: Enter the Known Height

1. In the calibration section, enter the measured height
2. Click "Calibrate"
3. Wait for confirmation message

### Step 5: Verify Calibration

After calibration:
1. Check the displayed height matches your measured height
2. Move the desk up and down slightly
3. Verify the displayed height changes accordingly

## Troubleshooting Calibration

### Height Reading is Wrong

If the displayed height doesn't match the actual height:

1. **Re-measure carefully** - Ensure you're measuring to the exact same point
2. **Check sensor mounting** - Sensor may have moved
3. **Recalibrate** - Repeat the process with a fresh measurement

### Height Changes in Wrong Direction

If raising the desk decreases the displayed height:

1. Check sensor orientation (should face downward)
2. Verify the calibration formula is applied correctly
3. Contact support if issue persists

### Readings are Unstable

If the height reading fluctuates excessively:

1. Ensure sensor is securely mounted
2. Check for vibrations
3. Verify no obstacles in sensor path
4. Wait for the moving average filter to stabilize

## Verification Procedure

To verify calibration accuracy:

1. **At calibration height**: Should match exactly
2. **At minimum height (~50cm)**: 
   - Move desk to lowest position
   - Measure actual height
   - Compare with displayed height
3. **At maximum height (~125cm)**:
   - Move desk to highest position
   - Measure actual height
   - Compare with displayed height

Expected accuracy: ±1cm across the entire range

## When to Recalibrate

Recalibrate if you notice:
- Consistent height offset (always X cm off)
- Accuracy changes at different heights
- After physically moving the sensor
- After significant temperature changes (rare)

## Technical Notes

### Filter Impact on Calibration

The system uses a 5-sample moving average filter. During calibration:
- Wait for the filter to fill (about 1 second)
- The calibration uses the filtered reading for accuracy
- Rapid sensor changes during calibration may affect results

### Storage

Calibration values are stored in NVS (Non-Volatile Storage):
- Persists across reboots
- Stored in the "config" namespace
- Can be factory reset if needed

### Typical Values

Typical calibration constants range from 100-200cm, depending on:
- Sensor mounting height
- Desk mechanism design
- Minimum/maximum desk heights

## Advanced: Manual Calibration

If the web interface calibration isn't working:

1. Note the sensor reading (shown in diagnostics)
2. Measure the actual desk height
3. Calculate: `constant = height + (reading / 10)`
4. Update via `/config` endpoint:
   ```bash
   curl -X POST http://[IP]/config \
        -H "Content-Type: application/json" \
        -d '{"calibrationConstant": [calculated value]}'
   ```

## Need Help?

See [Troubleshooting Guide](troubleshooting.md) for common issues.
