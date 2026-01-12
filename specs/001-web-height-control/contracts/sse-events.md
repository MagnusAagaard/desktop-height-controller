# Server-Sent Events (SSE) Contract

**Feature**: 001-web-height-control  
**Date**: 2026-01-11  
**Endpoint**: `http://<ESP32_IP>/events`  
**Protocol**: text/event-stream  
**Direction**: Server → Client (unidirectional push)

---

## SSE Overview

Server-Sent Events enable real-time updates from ESP32 to web clients without polling.

### Connection Lifecycle

1. **Client Connection**:
   ```javascript
   const eventSource = new EventSource('/events');
   ```

2. **Server Response**:
   ```http
   HTTP/1.1 200 OK
   Content-Type: text/event-stream
   Cache-Control: no-cache
   Connection: keep-alive
   
   ```

3. **Event Stream**: Server sends events as they occur

4. **Auto-Reconnect**: Browser automatically reconnects if connection drops

---

## Event Format

### SSE Wire Protocol

Each event follows this structure:

```
event: <event_type>
data: <JSON payload>

```

**Important**:
- Each line starts with `event:` or `data:`
- JSON payload on the `data:` line
- Blank line terminates the event

### Example Event

```
event: height_update
data: {"height_cm": 95, "raw_mm": 1050, "filtered_mm": 1048}

```

---

## Event Types

### 1. height_update

**Purpose**: Periodic height sensor readings (sent every 200ms)

**Payload**:
```json
{
  "height_cm": 95,
  "raw_mm": 1050,
  "filtered_mm": 1048,
  "timestamp_ms": 12345678
}
```

**Fields**:

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `height_cm` | integer | centimeters | Calculated desk height |
| `raw_mm` | integer | millimeters | Raw VL53L5CX reading |
| `filtered_mm` | integer | millimeters | Moving-average smoothed reading |
| `timestamp_ms` | integer | milliseconds | ESP32 uptime when captured |

**Frequency**: 5 Hz (every 200ms) while clients connected

**Example**:
```
event: height_update
data: {"height_cm": 95, "raw_mm": 1050, "filtered_mm": 1048, "timestamp_ms": 12345678}

```

**Client Handler**:
```javascript
eventSource.addEventListener('height_update', (e) => {
  const data = JSON.parse(e.data);
  document.getElementById('height').textContent = data.height_cm;
  document.getElementById('raw').textContent = data.raw_mm;
  document.getElementById('filtered').textContent = data.filtered_mm;
});
```

---

### 2. status_change

**Purpose**: Movement state transitions (IDLE → MOVING_UP, etc.)

**Payload**:
```json
{
  "previous_state": "IDLE",
  "new_state": "MOVING_UP",
  "target_height_cm": 110,
  "current_height_cm": 75,
  "timestamp_ms": 12345678
}
```

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `previous_state` | string | State before transition (IDLE, MOVING_UP, MOVING_DOWN, STABILIZING, ERROR) |
| `new_state` | string | State after transition |
| `target_height_cm` | integer | Target height (null if new_state = IDLE) |
| `current_height_cm` | integer | Current height at transition |
| `timestamp_ms` | integer | ESP32 uptime when state changed |

**Frequency**: On-demand (only when state changes)

**Example State Transitions**:

1. **Start Movement**:
   ```
   event: status_change
   data: {"previous_state": "IDLE", "new_state": "MOVING_UP", "target_height_cm": 110, "current_height_cm": 75, "timestamp_ms": 12345678}

   ```

2. **Begin Stabilizing**:
   ```
   event: status_change
   data: {"previous_state": "MOVING_UP", "new_state": "STABILIZING", "target_height_cm": 110, "current_height_cm": 109, "timestamp_ms": 12356789}

   ```

3. **Movement Complete**:
   ```
   event: status_change
   data: {"previous_state": "STABILIZING", "new_state": "IDLE", "target_height_cm": null, "current_height_cm": 110, "timestamp_ms": 12358789}

   ```

**Client Handler**:
```javascript
eventSource.addEventListener('status_change', (e) => {
  const data = JSON.parse(e.data);
  document.getElementById('status').textContent = data.new_state;
  
  if (data.new_state === 'IDLE') {
    showNotification(`Height reached: ${data.current_height_cm} cm`);
  }
});
```

---

### 3. error

**Purpose**: System errors requiring user attention

**Payload**:
```json
{
  "error_code": "SENSOR_FAILURE",
  "message": "VL53L5CX not responding on I2C",
  "severity": "critical",
  "timestamp_ms": 12345678
}
```

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `error_code` | string | Machine-readable error identifier |
| `message` | string | Human-readable error description |
| `severity` | string | "warning", "critical" (affects UI presentation) |
| `timestamp_ms` | integer | ESP32 uptime when error occurred |

**Error Codes**:

| Code | Severity | Description | User Action |
|------|----------|-------------|-------------|
| `SENSOR_FAILURE` | critical | VL53L5CX I2C communication failed | Check sensor wiring, reboot |
| `MOVEMENT_TIMEOUT` | critical | Desk didn't reach target in 30s | Check mechanical obstruction, verify MOSFET wiring |
| `UNCALIBRATED` | warning | Calibration constant not set | Run calibration wizard |
| `WIFI_DISCONNECTED` | warning | WiFi connection lost | Check router, verify credentials |
| `LOW_MEMORY` | warning | Free heap < 50KB | Reduce active clients, reboot if persistent |

**Example**:
```
event: error
data: {"error_code": "SENSOR_FAILURE", "message": "VL53L5CX not responding on I2C", "severity": "critical", "timestamp_ms": 12345678}

```

**Client Handler**:
```javascript
eventSource.addEventListener('error', (e) => {
  const data = JSON.parse(e.data);
  
  const alertClass = data.severity === 'critical' ? 'alert-danger' : 'alert-warning';
  showAlert(data.message, alertClass);
  
  if (data.error_code === 'SENSOR_FAILURE') {
    disableMovementControls();
  }
});
```

---

### 4. preset_updated

**Purpose**: Notify clients when preset configuration changes

**Payload**:
```json
{
  "slot": 2,
  "label": "Standing",
  "height_cm": 110,
  "action": "saved"
}
```

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `slot` | integer | Preset slot number (1-5) |
| `label` | string | Updated label |
| `height_cm` | integer | Updated height |
| `action` | string | "saved" or "deleted" |

**Frequency**: On-demand (only when preset saved via POST /preset/save)

**Example**:
```
event: preset_updated
data: {"slot": 2, "label": "Standing", "height_cm": 110, "action": "saved"}

```

**Client Handler**:
```javascript
eventSource.addEventListener('preset_updated', (e) => {
  const data = JSON.parse(e.data);
  
  // Refresh preset buttons UI
  const button = document.getElementById(`preset-${data.slot}`);
  button.textContent = `${data.label} (${data.height_cm} cm)`;
  button.disabled = false;
  
  showNotification(`Preset ${data.slot} updated: ${data.label}`);
});
```

---

### 5. wifi_status

**Purpose**: Notify clients of WiFi connection changes

**Payload**:
```json
{
  "status": "connected",
  "ssid": "MyNetwork",
  "ip_address": "192.168.1.100",
  "rssi_dbm": -65,
  "timestamp_ms": 12345678
}
```

**Fields**:

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | "connected", "disconnected", "reconnecting" |
| `ssid` | string | Network name (null if disconnected) |
| `ip_address` | string | Assigned IP (null if disconnected) |
| `rssi_dbm` | integer | Signal strength (null if disconnected) |
| `timestamp_ms` | integer | ESP32 uptime when status changed |

**Frequency**: On-demand (only when WiFi state changes)

**Example (Disconnected)**:
```
event: wifi_status
data: {"status": "disconnected", "ssid": null, "ip_address": null, "rssi_dbm": null, "timestamp_ms": 12345678}

```

**Example (Connected)**:
```
event: wifi_status
data: {"status": "connected", "ssid": "MyNetwork", "ip_address": "192.168.1.100", "rssi_dbm": -65, "timestamp_ms": 12346789}

```

**Client Handler**:
```javascript
eventSource.addEventListener('wifi_status', (e) => {
  const data = JSON.parse(e.data);
  
  const statusEl = document.getElementById('wifi-status');
  if (data.status === 'connected') {
    statusEl.textContent = `Connected: ${data.ssid} (${data.rssi_dbm} dBm)`;
    statusEl.className = 'status-ok';
  } else {
    statusEl.textContent = 'Disconnected';
    statusEl.className = 'status-error';
  }
});
```

---

## Connection Management

### Client-Side Setup

```javascript
const eventSource = new EventSource('/events');

// Connection opened
eventSource.onopen = () => {
  console.log('SSE connection established');
  document.getElementById('connection-status').textContent = 'Connected';
};

// Connection error (triggers auto-reconnect)
eventSource.onerror = (error) => {
  console.error('SSE connection error:', error);
  document.getElementById('connection-status').textContent = 'Reconnecting...';
};

// Register event listeners
eventSource.addEventListener('height_update', handleHeightUpdate);
eventSource.addEventListener('status_change', handleStatusChange);
eventSource.addEventListener('error', handleError);
eventSource.addEventListener('preset_updated', handlePresetUpdated);
eventSource.addEventListener('wifi_status', handleWifiStatus);
```

### Server-Side Implementation (ESPAsyncWebServer)

```cpp
// Global SSE event source
AsyncEventSource events("/events");

void setup() {
  // Register SSE endpoint
  events.onConnect([](AsyncEventSourceClient *client) {
    Serial.printf("SSE client connected: %s\n", client->lastId());
    
    // Send initial state on connection
    String json = getInitialStateJSON();
    client->send(json.c_str(), "status_change", millis());
  });
  
  server.addHandler(&events);
}

void loop() {
  static unsigned long lastHeightUpdate = 0;
  
  // Send height updates every 200ms
  if (millis() - lastHeightUpdate >= 200) {
    String json = getHeightUpdateJSON();
    events.send(json.c_str(), "height_update", millis());
    lastHeightUpdate = millis();
  }
}

// Send status change event
void onMovementStateChange(MovementStatus newState) {
  String json = getStatusChangeJSON(newState);
  events.send(json.c_str(), "status_change", millis());
}

// Send error event
void onError(const char* errorCode, const char* message, const char* severity) {
  StaticJsonDocument<256> doc;
  doc["error_code"] = errorCode;
  doc["message"] = message;
  doc["severity"] = severity;
  doc["timestamp_ms"] = millis();
  
  String json;
  serializeJson(doc, json);
  events.send(json.c_str(), "error", millis());
}
```

---

## Performance Characteristics

### Bandwidth Usage

**Per Client** (assuming continuous monitoring):

| Event Type | Payload Size | Frequency | Bandwidth |
|------------|--------------|-----------|-----------|
| height_update | ~120 bytes | 5 Hz | 600 bytes/s |
| status_change | ~180 bytes | 0.1 Hz (avg) | 18 bytes/s |
| error | ~200 bytes | 0.01 Hz (avg) | 2 bytes/s |
| preset_updated | ~120 bytes | 0.001 Hz (avg) | 0.12 bytes/s |
| wifi_status | ~180 bytes | 0.001 Hz (avg) | 0.18 bytes/s |

**Total**: ~620 bytes/s per client (~5 kbps)

**Maximum Clients**: ESP32 can handle ~8 simultaneous SSE clients before memory pressure

---

## Error Handling

### Network Interruption

**Client Behavior**:
- Browser automatically attempts reconnection every 3 seconds
- No action required from web application

**Server Behavior**:
- Detects client disconnect via ESPAsyncWebServer
- Cleans up client resources
- Accepts reconnection attempts

### Buffering

**Server-Side**: 
- No event buffering (real-time data only)
- Clients receive current state on reconnection via initial state event

**Client-Side**:
- Browser buffers incomplete events until newline
- No application-level buffering needed

---

## Testing

### Manual Testing with curl

```bash
# Connect to SSE endpoint
curl -N -H "Accept: text/event-stream" http://192.168.1.100/events

# Expected output (streaming):
event: height_update
data: {"height_cm": 95, "raw_mm": 1050, "filtered_mm": 1048, "timestamp_ms": 12345678}

event: height_update
data: {"height_cm": 95, "raw_mm": 1051, "filtered_mm": 1049, "timestamp_ms": 12345878}

...
```

### Browser DevTools

1. Open Network tab
2. Filter by "EventStream"
3. Click `/events` request
4. View "EventStream" tab for real-time events

---

## Summary

SSE contract defines 5 event types:
- **height_update**: 5 Hz sensor readings
- **status_change**: Movement state transitions
- **error**: System errors
- **preset_updated**: Preset configuration changes
- **wifi_status**: WiFi connection changes

Provides real-time updates to web clients with auto-reconnection, low bandwidth usage (~5 kbps per client), and simple client-side integration via EventSource API.
