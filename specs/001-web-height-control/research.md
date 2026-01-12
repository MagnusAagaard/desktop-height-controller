# Research: Web-Based Desk Height Controller

**Feature**: 001-web-height-control  
**Date**: 2026-01-11  
**Purpose**: Resolve technical unknowns for ESP32 + VL53L5CX sensor implementation

---

## 1. VL53L5CX ToF Sensor Integration

### Decision: Use SparkFun VL53L5CX Arduino Library with single-zone mode

**Rationale**:
- Official SparkFun library provides clean API for VL53L5CX 8x8 multi-zone sensor
- Single-zone mode (center zone only) reduces I2C data transfer and processing time
- Achieves required 5Hz (200ms) sampling rate with low CPU overhead
- Library handles sensor initialization, calibration, and distance reading

**Library**: `SparkFun VL53L5CX Arduino Library` (GitHub: sparkfun/SparkFun_VL53L5CX_Arduino_Library)

**Integration Pattern**:
```cpp
// Initialize I2C and sensor
Wire.begin(SDA_PIN, SCL_PIN);
sensor.begin();
sensor.setResolution(8*8); // 64 zones
sensor.setRangingFrequency(5); // 5Hz = 200ms intervals
sensor.startRanging();

// Read center zone distance (fastest single measurement)
sensor.getRangingData(&data);
uint16_t distance_mm = data.distance_mm[CENTER_ZONE]; // Center of 8x8 grid
```

**Alternatives Considered**:
- ST VL53L5CX ULD driver: More complex, requires manual I2C handling, harder to integrate with Arduino
- Averaging multiple zones: Increases latency and processing, unnecessary for single-point measurement

**Configuration**:
- I2C Speed: 400kHz (fast mode) for minimal read latency
- Ranging Mode: Continuous (sensor autonomously measures at set frequency)
- Integration Time: Auto (sensor optimizes for accuracy vs speed)
- Distance Mode: Medium (up to 300cm, sufficient for desk heights 50-125cm)

---

## 2. Server-Sent Events (SSE) for Real-Time Updates

### Decision: ESPAsyncWebServer with built-in SSE support

**Rationale**:
- AsyncWebServer provides native SSE endpoint creation
- Non-blocking architecture allows simultaneous HTTP requests + SSE connections
- Efficient memory usage (event queue managed by library)
- Automatic client reconnection handling (browser retries on disconnect)
- Simpler than WebSocket for one-way server-to-client updates

**Library**: `ESPAsyncWebServer` + `AsyncTCP` (GitHub: me-no-dev/ESPAsyncWebServer)

**Implementation Pattern**:
```cpp
AsyncWebServer server(80);
AsyncEventSource events("/events"); // SSE endpoint at /events

void setup() {
  events.onConnect([](AsyncEventSourceClient *client){
    client->send("Connected", NULL, millis(), 1000);
  });
  server.addHandler(&events);
  server.begin();
}

void loop() {
  // When height changes, push to all connected clients
  events.send(
    String("{\"height\":" + String(current_height) + ",\"status\":\"" + status + "\"}").c_str(),
    "height_update", 
    millis()
  );
}
```

**Client-Side (JavaScript)**:
```javascript
const eventSource = new EventSource('/events');
eventSource.addEventListener('height_update', (e) => {
  const data = JSON.parse(e.data);
  document.getElementById('height').textContent = data.height;
});
```

**Alternatives Considered**:
- HTTP Polling: Client requests every 200ms → high network overhead, server load
- WebSocket: Bidirectional but overkill for one-way updates; more complex state management
- MQTT: Requires broker, adds infrastructure complexity

---

## 3. Moving Average Filter for Sensor Smoothing

### Decision: Simple Moving Average (SMA) with 5-sample window

**Rationale**:
- SMA provides good noise reduction without excessive lag
- 5 samples at 5Hz = 1 second of history (balances responsiveness vs smoothing)
- Minimal memory footprint: circular buffer of 5 uint16_t values (10 bytes)
- Fast computation: O(1) with running sum technique

**Algorithm**:
```cpp
class MovingAverageFilter {
  static const uint8_t WINDOW_SIZE = 5;
  uint16_t buffer[WINDOW_SIZE] = {0};
  uint8_t index = 0;
  uint32_t sum = 0;
  bool filled = false;

public:
  uint16_t addSample(uint16_t new_sample) {
    sum -= buffer[index];              // Remove oldest
    buffer[index] = new_sample;        // Add new
    sum += new_sample;
    index = (index + 1) % WINDOW_SIZE; // Circular
    if (index == 0) filled = true;
    
    uint8_t count = filled ? WINDOW_SIZE : index;
    return sum / count;                // Return average
  }
};
```

**Alternatives Considered**:
- Exponential Moving Average (EMA): More responsive but retains all history (infinite memory)
- Median filter: Better outlier rejection but higher computational cost (sorting required)
- Kalman filter: Overkill for this application, complex tuning, higher CPU usage

**Tuning**:
- Window size configurable in SystemConfiguration (default 5, range 3-10)
- Larger windows = smoother but slower response to actual height changes
- Smaller windows = faster response but more noise

---

## 4. WiFi Connection and Reconnection Strategy

### Decision: WiFi.begin() with background auto-reconnect enabled

**Rationale**:
- ESP32 WiFi library handles reconnection automatically when enabled
- Non-blocking connection attempts prevent main loop delays
- Exponential backoff built into library reduces network spam
- System continues operating during WiFi outage (local sensor/MOSFET control remains functional)

**Implementation**:
```cpp
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait max 10s for initial connection
  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi connection failed, will retry in background");
  }
}

void loop() {
  // Check connection status for diagnostics only
  if (WiFi.status() != WL_CONNECTED) {
    diagnostics.connection_status = "Disconnected - Reconnecting...";
  } else {
    diagnostics.connection_status = "Connected (" + WiFi.localIP().toString() + ")";
  }
}
```

**Fallback Mode**:
- If WiFi fails to connect after initial timeout, system creates Access Point (AP) mode
- AP SSID: "DeskController-<chipID>" with password "deskcontrol123"
- User connects to AP, accesses 192.168.4.1 to configure WiFi credentials
- Credentials saved to NVS, ESP32 reboots and attempts saved network

**Alternatives Considered**:
- WiFiManager library: Full captive portal but adds 40KB+ code size, exceeds memory budget
- Manual reconnection loop: Reinventing what ESP32 WiFi library already provides
- Static IP: Simplifies network config but reduces portability across networks

---

## 5. Preset Storage in NVS (Non-Volatile Storage)

### Decision: Preferences library with namespace partitioning

**Rationale**:
- Preferences API provides simple key-value storage in ESP32 NVS partition
- Supports String (preset labels) and uint16_t (heights) types natively
- Wear leveling handled automatically by ESP-IDF
- Survives firmware updates (NVS partition separate from app partition)
- ~4KB partition size sufficient for 5 presets + system config

**Implementation**:
```cpp
#include <Preferences.h>
Preferences prefs;

// Save preset
void savePreset(uint8_t slot, const String& label, uint16_t height) {
  prefs.begin("presets", false); // Read-write mode
  prefs.putString(("label" + String(slot)).c_str(), label);
  prefs.putUInt(("height" + String(slot)).c_str(), height);
  prefs.end();
}

// Load preset
bool loadPreset(uint8_t slot, String& label, uint16_t& height) {
  prefs.begin("presets", true); // Read-only mode
  label = prefs.getString(("label" + String(slot)).c_str(), "");
  height = prefs.getUInt(("height" + String(slot)).c_str(), 0);
  prefs.end();
  return label.length() > 0; // Valid if label exists
}
```

**Data Schema**:
```
Namespace: "presets"
  label1: String (max 20 chars)
  height1: UInt16 (cm)
  label2: String
  height2: UInt16
  ...
  label5: String
  height5: UInt16

Namespace: "config"
  calibration_constant: UInt16 (cm)
  min_height: UInt16 (cm, default 50)
  max_height: UInt16 (cm, default 125)
  tolerance: UInt16 (mm, default 10 = 1cm)
  stabilization_duration: UInt16 (ms, default 2000)
  filter_window: UInt8 (samples, default 5)
```

**Alternatives Considered**:
- SPIFFS file storage: More flexible but requires file I/O overhead, JSON parsing
- EEPROM library: Legacy API, no wear leveling, fixed size limitations
- SD card: External hardware dependency, unnecessary complexity

---

## 6. MOSFET Control and Safety

### Decision: Direct GPIO control with safety debouncing and mutual exclusion

**Rationale**:
- ESP32 GPIO provides sufficient drive current for MOSFET gate (3.3V logic level)
- Continuous HIGH during movement simplifies state machine
- Mutual exclusion ensures only one pin active at a time (never both up+down)
- Emergency stop via watchdog timer (30s timeout)

**Hardware Assumption**:
- Logic-level N-channel MOSFET (Vgs_th < 3.3V, e.g., IRLZ44N)
- Pull-down resistor on gate (10kΩ) ensures OFF state during ESP32 boot
- Flyback diode across motor for inductive kickback protection

**Implementation**:
```cpp
#define PIN_MOTOR_UP   25  // GPIO25 → UP MOSFET gate
#define PIN_MOTOR_DOWN 26  // GPIO26 → DOWN MOSFET gate

void setup() {
  pinMode(PIN_MOTOR_UP, OUTPUT);
  pinMode(PIN_MOTOR_DOWN, OUTPUT);
  stopMovement(); // Ensure both LOW at startup
}

void moveUp() {
  digitalWrite(PIN_MOTOR_DOWN, LOW);  // Safety: stop down first
  delay(50);                          // Debounce delay
  digitalWrite(PIN_MOTOR_UP, HIGH);   // Start up movement
}

void moveDown() {
  digitalWrite(PIN_MOTOR_UP, LOW);    // Safety: stop up first
  delay(50);                          // Debounce delay
  digitalWrite(PIN_MOTOR_DOWN, HIGH); // Start down movement
}

void stopMovement() {
  digitalWrite(PIN_MOTOR_UP, LOW);
  digitalWrite(PIN_MOTOR_DOWN, LOW);
}
```

**Safety Mechanisms**:
- **Mutual exclusion**: 50ms delay between switching directions prevents simultaneous activation
- **Watchdog timer**: If movement exceeds 30s, emergency stop triggered
- **Sensor loss**: Invalid sensor reading immediately stops movement
- **Power-on state**: Both pins default LOW (desk stationary on boot)

**Alternatives Considered**:
- PWM speed control: Adds complexity, most desk motors are binary (on/off)
- H-bridge IC: Overkill for unidirectional control, adds component cost
- Relay control: Slower switching, higher power consumption, mechanical wear

---

## 7. Task Scheduling and Main Loop Architecture

### Decision: Arduino loop() with millis()-based scheduling (no RTOS tasks)

**Rationale**:
- Simple cooperative multitasking sufficient for this application
- millis() timing avoids blocking delays
- AsyncWebServer handles HTTP/SSE in callbacks (non-blocking)
- Predictable execution order for state machine

**Loop Structure**:
```cpp
unsigned long last_sensor_read = 0;
unsigned long last_sse_update = 0;
unsigned long movement_start_time = 0;

void loop() {
  unsigned long now = millis();
  
  // Task 1: Read sensor (5Hz = every 200ms)
  if (now - last_sensor_read >= 200) {
    uint16_t raw = readSensor();
    uint16_t filtered = filter.addSample(raw);
    current_height = calibration_constant - filtered;
    last_sensor_read = now;
  }
  
  // Task 2: Update state machine (control loop)
  updateMovementController(now);
  
  // Task 3: Push SSE updates (if height changed)
  if (height_changed && now - last_sse_update >= 100) {
    pushSSEUpdate();
    last_sse_update = now;
  }
  
  // Task 4: Check WiFi status (every 5s for diagnostics)
  if (now % 5000 < 10) {
    updateDiagnostics();
  }
  
  yield(); // Allow background WiFi/TCP tasks to run
}
```

**State Machine**:
```
States: IDLE, MOVING_UP, MOVING_DOWN, STABILIZING, ERROR
Transitions:
  IDLE → MOVING_UP/DOWN (on new target)
  MOVING → STABILIZING (height within tolerance)
  STABILIZING → IDLE (stable for 2s)
  STABILIZING → MOVING (out of tolerance, reset timer)
  ANY → ERROR (sensor failure, timeout)
  ERROR → IDLE (error cleared)
```

**Alternatives Considered**:
- FreeRTOS tasks: Unnecessary complexity, higher memory usage, potential race conditions
- Interrupts for sensor reading: I2C not interrupt-safe, requires careful synchronization
- Blocking delays: Prevents web server responsiveness, violates performance requirements

---

## Summary

All technical unknowns resolved. Ready to proceed to data modeling (Phase 1).

**Key Libraries**:
1. SparkFun VL53L5CX Arduino Library (sensor)
2. ESPAsyncWebServer + AsyncTCP (web server + SSE)
3. Preferences (NVS storage)
4. Wire (I2C communication)

**Best Practices Adopted**:
- Non-blocking architecture (millis() scheduling)
- Safety-first GPIO control (mutual exclusion, debouncing)
- Automatic WiFi reconnection with AP fallback
- Simple moving average for sensor smoothing
- NVS for persistent storage with wear leveling

**Configuration Parameters** (all stored in Config.h):
- WiFi credentials (SSID, password)
- Pin assignments (I2C SDA/SCL, MOSFET up/down)
- Physical constants (calibration constant, min/max height)
- Timing parameters (tolerance, stabilization duration, filter window)
