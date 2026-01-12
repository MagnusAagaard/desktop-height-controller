# HTTP API Contract

**Feature**: 001-web-height-control  
**Date**: 2026-01-11  
**Base URL**: `http://<ESP32_IP>/`  
**Protocol**: HTTP/1.1  
**Server**: ESPAsyncWebServer  

---

## General Conventions

### Response Format

All JSON responses follow this structure:

```json
{
  "success": true,
  "data": { ... },
  "error": null
}
```

On error:

```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable error description"
  }
}
```

### Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `INVALID_HEIGHT` | 400 | Target height outside safe range |
| `INVALID_PRESET` | 400 | Preset slot not in 1-5 range |
| `ALREADY_MOVING` | 409 | Cannot start movement while already in motion |
| `SENSOR_FAILURE` | 503 | VL53L5CX sensor not responding |
| `UNCALIBRATED` | 503 | System not calibrated (constant = 0) |
| `INTERNAL_ERROR` | 500 | Unexpected server-side failure |

---

## Endpoints

### 1. GET / - Serve Web UI

**Purpose**: Return the HTML/CSS/JS web interface

**Request**:
```http
GET / HTTP/1.1
Host: 192.168.1.100
```

**Response**:
```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 45321

<!DOCTYPE html>
<html>
  <head>
    <title>Desk Height Controller</title>
    ...
  </head>
  <body>
    ...
  </body>
</html>
```

**Files Served**:
- `/` → `data/index.html`
- `/style.css` → `data/style.css`
- `/script.js` → `data/script.js`

**Storage**: SPIFFS partition (~200KB)

---

### 2. POST /target - Set Manual Target Height

**Purpose**: Move desk to user-specified height

**Request**:
```http
POST /target HTTP/1.1
Host: 192.168.1.100
Content-Type: application/json

{
  "height_cm": 95
}
```

**Parameters**:

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `height_cm` | integer | Yes | min_safe_height ≤ value ≤ max_safe_height | Target height in centimeters |

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "current_height_cm": 75,
    "target_height_cm": 95,
    "direction": "up",
    "estimated_duration_s": 12
  },
  "error": null
}
```

**Error Response (400 Bad Request - Invalid Height)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "INVALID_HEIGHT",
    "message": "Height 145 exceeds maximum safe height 125"
  }
}
```

**Error Response (409 Conflict - Already Moving)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "ALREADY_MOVING",
    "message": "Cannot set new target while moving to 110 cm"
  }
}
```

**Business Logic**:
1. Validate `height_cm` against SystemConfiguration min/max
2. Check MovementState.current_state != MOVING_UP/MOVING_DOWN
3. Create TargetHeight {height, source=MANUAL, source_id=0}
4. Trigger MovementController
5. Return current state snapshot

---

### 3. POST /preset - Activate Preset

**Purpose**: Move desk to saved preset height

**Request**:
```http
POST /preset HTTP/1.1
Host: 192.168.1.100
Content-Type: application/json

{
  "slot": 2
}
```

**Parameters**:

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `slot` | integer | Yes | 1-5 | Preset slot number |

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "preset_label": "Standing",
    "current_height_cm": 75,
    "target_height_cm": 110,
    "direction": "up",
    "estimated_duration_s": 18
  },
  "error": null
}
```

**Error Response (400 Bad Request - Invalid Slot)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "INVALID_PRESET",
    "message": "Preset slot 6 out of range (1-5)"
  }
}
```

**Error Response (400 Bad Request - Empty Preset)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "INVALID_PRESET",
    "message": "Preset slot 3 is not configured"
  }
}
```

**Business Logic**:
1. Validate slot ∈ [1, 5]
2. Load PresetConfiguration from memory
3. Validate preset is configured (label != "")
4. Same movement logic as POST /target
5. Set source=PRESET, source_id=slot

---

### 4. POST /preset/save - Save Preset Configuration

**Purpose**: Store current height as a preset

**Request**:
```http
POST /preset/save HTTP/1.1
Host: 192.168.1.100
Content-Type: application/json

{
  "slot": 1,
  "label": "Sitting",
  "height_cm": 75
}
```

**Parameters**:

| Field | Type | Required | Constraints | Description |
|-------|------|----------|-------------|-------------|
| `slot` | integer | Yes | 1-5 | Preset slot to save |
| `label` | string | Yes | 1-20 chars, alphanumeric + spaces | User-friendly name |
| `height_cm` | integer | Yes | min_safe_height ≤ value ≤ max_safe_height | Height to save |

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "slot": 1,
    "label": "Sitting",
    "height_cm": 75,
    "saved_at_ms": 1234567890
  },
  "error": null
}
```

**Error Response (400 Bad Request - Invalid Label)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "INVALID_PRESET",
    "message": "Label exceeds 20 character limit"
  }
}
```

**Business Logic**:
1. Validate slot, label, height
2. Trim whitespace from label
3. Write to NVS namespace "presets" keys `label{slot}`, `height{slot}`
4. Update PresetManager in-memory cache
5. Broadcast preset_updated SSE event

---

### 5. GET /status - Get Current System Status

**Purpose**: Retrieve real-time diagnostic information

**Request**:
```http
GET /status HTTP/1.1
Host: 192.168.1.100
```

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "current_height_cm": 95,
    "raw_sensor_mm": 1050,
    "filtered_sensor_mm": 1048,
    "movement_state": "IDLE",
    "target_height_cm": null,
    "wifi_rssi_dbm": -65,
    "uptime_ms": 3600000,
    "free_heap_bytes": 145280,
    "last_error": null
  },
  "error": null
}
```

**Response When Moving**:
```json
{
  "success": true,
  "data": {
    "current_height_cm": 87,
    "raw_sensor_mm": 1330,
    "filtered_sensor_mm": 1328,
    "movement_state": "MOVING_UP",
    "target_height_cm": 110,
    "wifi_rssi_dbm": -65,
    "uptime_ms": 3602000,
    "free_heap_bytes": 144512,
    "last_error": null
  },
  "error": null
}
```

**Error Response (503 Service Unavailable - Sensor Failure)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "SENSOR_FAILURE",
    "message": "VL53L5CX not responding on I2C"
  }
}
```

---

### 6. POST /stop - Emergency Stop

**Purpose**: Immediately halt desk movement

**Request**:
```http
POST /stop HTTP/1.1
Host: 192.168.1.100
```

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "stopped_at_height_cm": 92,
    "previous_state": "MOVING_UP",
    "new_state": "IDLE"
  },
  "error": null
}
```

**Business Logic**:
1. Set both MOSFET pins LOW
2. Clear TargetHeight
3. MovementState → IDLE
4. Broadcast status_change SSE event

---

### 7. GET /config - Get System Configuration

**Purpose**: Retrieve current settings

**Request**:
```http
GET /config HTTP/1.1
Host: 192.168.1.100
```

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "calibration_constant_cm": 145,
    "min_safe_height_cm": 50,
    "max_safe_height_cm": 125,
    "tolerance_mm": 10,
    "stabilization_duration_ms": 2000,
    "movement_timeout_ms": 30000,
    "filter_window_size": 5,
    "wifi_ssid": "MyNetwork"
  },
  "error": null
}
```

**Note**: WiFi password is never returned in API responses (security)

---

### 8. POST /config - Update System Configuration

**Purpose**: Modify settings (requires caution - can affect safety)

**Request**:
```http
POST /config HTTP/1.1
Host: 192.168.1.100
Content-Type: application/json

{
  "min_safe_height_cm": 60,
  "max_safe_height_cm": 120,
  "tolerance_mm": 15
}
```

**Parameters**: Any subset of SystemConfiguration attributes (partial update)

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "updated_fields": ["min_safe_height_cm", "max_safe_height_cm", "tolerance_mm"],
    "reboot_required": false
  },
  "error": null
}
```

**Error Response (400 Bad Request - Invalid Configuration)**:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "INVALID_CONFIG",
    "message": "max_safe_height (120) must be > min_safe_height (125)"
  }
}
```

**Business Logic**:
1. Validate each field
2. Check cross-field constraints (max > min, etc.)
3. Write to NVS namespace "config"
4. Hot-reload into SystemConfiguration (no reboot needed for most fields)
5. If wifi_ssid/wifi_password changed → reboot_required = true

---

### 9. GET /presets - List All Presets

**Purpose**: Retrieve all configured presets for UI population

**Request**:
```http
GET /presets HTTP/1.1
Host: 192.168.1.100
```

**Success Response (200 OK)**:
```json
{
  "success": true,
  "data": {
    "presets": [
      {
        "slot": 1,
        "label": "Sitting",
        "height_cm": 75,
        "configured": true
      },
      {
        "slot": 2,
        "label": "Standing",
        "height_cm": 110,
        "configured": true
      },
      {
        "slot": 3,
        "label": "",
        "height_cm": 0,
        "configured": false
      },
      {
        "slot": 4,
        "label": "Typing",
        "height_cm": 95,
        "configured": true
      },
      {
        "slot": 5,
        "label": "",
        "height_cm": 0,
        "configured": false
      }
    ]
  },
  "error": null
}
```

**Business Logic**:
1. Load all 5 PresetConfiguration slots from memory
2. Mark configured = false if label is empty
3. Return full list (allows UI to show empty slots for configuration)

---

## CORS Headers

All responses include:

```http
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

Rationale: ESP32 typically accessed on local network; no cross-origin security concerns in typical use case.

---

## Rate Limiting

**Not implemented** - Single-user device on trusted local network. If needed in future:

- Limit POST requests to 1 per second per endpoint
- Emergency stop exempt from rate limiting

---

## Authentication

**Not implemented in v1.0** - Device on private LAN. Future enhancement:

- Basic HTTP Auth (username/password)
- Stored in NVS, configurable via setup wizard

---

## Summary

9 HTTP endpoints defined covering:
- Web UI serving (GET /)
- Height control (POST /target, POST /preset, POST /stop)
- Configuration (GET/POST /config, GET/POST /presets)
- Status monitoring (GET /status)

All endpoints return JSON with success/error structure. Ready for SSE contract definition.
