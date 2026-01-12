# Data Model: Web-Based Desk Height Controller

**Feature**: 001-web-height-control  
**Date**: 2026-01-11  
**Purpose**: Define all data entities, their attributes, relationships, and validation rules

---

## 1. Height Reading

**Purpose**: Represents a single sensor measurement with processing pipeline (raw → filtered → calculated)

### Attributes

| Attribute | Type | Unit | Range | Description |
|-----------|------|------|-------|-------------|
| `raw_distance_mm` | `uint16_t` | millimeters | 0-4000 | Unprocessed VL53L5CX sensor reading from floor |
| `filtered_distance_mm` | `uint16_t` | millimeters | 0-4000 | Moving-average smoothed distance |
| `calculated_height_cm` | `uint16_t` | centimeters | 0-200 | Desk height = (calibration_constant - filtered_distance_mm/10) |
| `timestamp_ms` | `unsigned long` | milliseconds | 0-2³² | millis() when reading captured |
| `validity` | `enum ValidityStatus` | - | VALID, INVALID, STALE | Reading quality status |

### Validation Rules

- **Raw Distance**: 
  - INVALID if sensor I2C communication fails
  - INVALID if reading = 0 or > 4000mm (sensor error)
  - STALE if timestamp > 1000ms old

- **Filtered Distance**: 
  - Calculated only from VALID raw readings
  - First N-1 samples use partial average (N = filter window size)

- **Calculated Height**:
  - Only computed if filtered_distance is from VALID reading
  - Result clamped to [0, 200]cm range
  - Compared against min_height/max_height from SystemConfiguration

### State Transitions

```
Sensor I2C Read → raw_distance_mm
    ↓
Validity Check → validity = {VALID, INVALID}
    ↓ (if VALID)
Moving Average Filter → filtered_distance_mm
    ↓
Height Calculation → calculated_height_cm = (calibration_constant - filtered_distance_mm/10)
```

### Relationships

- **Used by**: MovementState (determines if target reached)
- **Used by**: DiagnosticState (displayed to user)
- **Configured by**: SystemConfiguration (calibration_constant)

---

## 2. Target Height

**Purpose**: Represents user's desired desk height (from manual input or preset activation)

### Attributes

| Attribute | Type | Unit | Range | Description |
|-----------|------|------|-------|-------------|
| `target_height_cm` | `uint16_t` | centimeters | min_height-max_height | User-specified or preset height |
| `tolerance_mm` | `uint16_t` | millimeters | 5-50 | Acceptable deviation (default 10mm = ±1cm) |
| `activation_timestamp_ms` | `unsigned long` | milliseconds | 0-2³² | When target was set |
| `source` | `enum TargetSource` | - | MANUAL, PRESET | How target was triggered |
| `source_id` | `uint8_t` | - | 0-4 | Preset slot number (if source=PRESET), else 0 |

### Validation Rules

- **Target Height**:
  - MUST be >= min_height AND <= max_height (from SystemConfiguration)
  - MUST differ from current height by > tolerance (ignore if already at target)
  - Validation occurs on web server before accepting request

- **Tolerance**:
  - Configurable via SystemConfiguration (default 10mm)
  - Smaller tolerance = more precise but higher oscillation risk
  - Larger tolerance = faster stabilization but less accurate

### Relationships

- **Set by**: User via web interface (manual input or preset button)
- **Used by**: MovementState (determines movement direction and stop condition)
- **Cleared on**: Movement completion (state = IDLE) or new target set

---

## 3. Preset Configuration

**Purpose**: Stores user-defined height shortcuts with labels for quick access

### Attributes

| Attribute | Type | Unit | Range | Constraints | Description |
|-----------|------|------|-------|-------------|-------------|
| `slot_number` | `uint8_t` | - | 1-5 | Primary key | Preset position (1-5) |
| `label` | `String` | - | 1-20 chars | UTF-8 | User-defined name (e.g., "Sitting") |
| `height_cm` | `uint16_t` | centimeters | min_height-max_height | Valid height | Target height for this preset |
| `last_modified_ms` | `unsigned long` | milliseconds | 0-2³² | Optional | Timestamp of last update |

### Validation Rules

- **Slot Number**: Must be 1-5 (enforced by array indexing)
- **Label**: 
  - Minimum 1 character (empty labels = preset not configured)
  - Maximum 20 characters (UI display constraint)
  - Allowed characters: alphanumeric, spaces, hyphens, underscores
  - Trimmed whitespace on save

- **Height**: Same validation as TargetHeight.target_height_cm

### Persistence

Stored in ESP32 NVS (Non-Volatile Storage) via Preferences library:

```cpp
Namespace: "presets"
Keys: "label1", "height1", "label2", "height2", ..., "label5", "height5"
```

**Write Behavior**:
- Immediate write on user save action
- Survives power cycles and firmware updates
- Wear leveling handled by ESP-IDF NVS

**Read Behavior**:
- Loaded once at boot into PresetManager memory
- Hot-reloaded on successful save (no reboot required)

### Relationships

- **Triggers**: TargetHeight when preset button clicked
- **Displayed in**: Web UI preset panel
- **Managed by**: PresetManager module

---

## 4. Movement State

**Purpose**: Tracks desk movement status, direction, timing for state machine control

### Attributes

| Attribute | Type | Unit | Range | Description |
|-----------|------|------|-------|-------------|
| `current_state` | `enum MovementStatus` | - | IDLE, MOVING_UP, MOVING_DOWN, STABILIZING, ERROR | Active state |
| `start_timestamp_ms` | `unsigned long` | milliseconds | 0-2³² | When movement began |
| `stabilization_start_ms` | `unsigned long` | milliseconds | 0-2³² | When entered STABILIZING state |
| `active_target` | `TargetHeight*` | - | NULL or valid | Pointer to current target (NULL if IDLE) |
| `last_error` | `String` | - | 0-100 chars | Human-readable error message |

### State Definitions

| State | Description | Entry Condition | Exit Condition |
|-------|-------------|-----------------|----------------|
| `IDLE` | No movement, desk stationary | Boot, movement complete, error cleared | New target set |
| `MOVING_UP` | Desk rising toward target | target > current, up pin HIGH | Height within tolerance OR error |
| `MOVING_DOWN` | Desk lowering toward target | target < current, down pin HIGH | Height within tolerance OR error |
| `STABILIZING` | Within tolerance, waiting for stability | Height within tolerance for 1st time | Stable for 2s → IDLE OR height out of tolerance → resume movement |
| `ERROR` | Movement stopped due to fault | Sensor failure, timeout, invalid state | Error acknowledged or auto-recovery |

### State Transitions

```
       New Target Set
IDLE ───────────────→ MOVING_UP (if target > current)
  ↑                   MOVING_DOWN (if target < current)
  │                          │
  │                          │ Height within tolerance
  │                          ↓
  │                    STABILIZING
  │                     ↙    ↓    ↘
  │          Out of tolerance │  Stable 2s
  │             (reset timer) │
  └────────────────────────── ┘
  
  Any State → ERROR (sensor fail, timeout)
  ERROR → IDLE (recovery/acknowledge)
```

### Validation Rules

- **Timeout**: If in MOVING_* for > movement_timeout_ms (default 30s) → ERROR
- **Sensor Loss**: If HeightReading.validity = INVALID while MOVING → ERROR
- **Stabilization**: Timer reset if height leaves tolerance during STABILIZING
- **Mutual Exclusion**: Only one MOSFET active at a time (never both up+down)

### Relationships

- **Controls**: MOSFET GPIO pins (via MovementController)
- **Reads**: HeightReading (current height vs target)
- **Uses**: TargetHeight (destination)
- **Configured by**: SystemConfiguration (stabilization_duration, movement_timeout)

---

## 5. System Configuration

**Purpose**: Global settings for hardware calibration, safety limits, and tuning parameters

### Attributes

| Attribute | Type | Unit | Default | Range | Description |
|-----------|------|------|---------|-------|-------------|
| `calibration_constant_cm` | `uint16_t` | centimeters | 0 (uncalibrated) | 50-200 | Height when sensor reads 0mm |
| `min_safe_height_cm` | `uint16_t` | centimeters | 50 | 30-100 | Minimum allowed target height |
| `max_safe_height_cm` | `uint16_t` | centimeters | 125 | 100-150 | Maximum allowed target height |
| `tolerance_mm` | `uint16_t` | millimeters | 10 | 5-50 | Target height tolerance (±) |
| `stabilization_duration_ms` | `uint16_t` | milliseconds | 2000 | 500-10000 | Time to remain stable before stop |
| `movement_timeout_ms` | `uint16_t` | milliseconds | 30000 | 10000-60000 | Max movement duration |
| `filter_window_size` | `uint8_t` | samples | 5 | 3-10 | Moving average window |
| `wifi_ssid` | `String` | - | "" | 1-32 chars | WiFi network name |
| `wifi_password` | `String` | - | "" | 8-63 chars | WiFi password |

### Validation Rules

- **Calibration Constant**: Set via calibration procedure (measure desk at known height)
- **Height Limits**: max_safe_height > min_safe_height + 20cm (minimum useful range)
- **Tolerance**: Affects stabilization sensitivity (smaller = more precise, higher oscillation risk)
- **Stabilization Duration**: Must be >= 500ms (below this, sensor noise dominates)
- **Movement Timeout**: Safety cutoff for runaway conditions
- **Filter Window**: Larger = smoother but slower response

### Persistence

Stored in NVS namespace "config":

```cpp
Namespace: "config"
Keys: "cal_const", "min_h", "max_h", "tolerance", "stab_dur", "move_timeout", "filter_win"
WiFi stored separately: "wifi_ssid", "wifi_pass"
```

### Factory Defaults

Applied on first boot or after NVS erase:

```cpp
calibration_constant_cm = 0;         // Requires calibration
min_safe_height_cm = 50;
max_safe_height_cm = 125;
tolerance_mm = 10;
stabilization_duration_ms = 2000;
movement_timeout_ms = 30000;
filter_window_size = 5;
wifi_ssid = "";                      // Must be configured
wifi_password = "";
```

### Relationships

- **Used by**: HeightController (calibration_constant, filter_window_size)
- **Used by**: MovementController (stabilization_duration, movement_timeout)
- **Used by**: TargetHeight validation (min/max height, tolerance)
- **Used by**: WiFiManager (wifi_ssid, wifi_password)

---

## 6. Diagnostic State

**Purpose**: Real-time system status for troubleshooting and user feedback

### Attributes

| Attribute | Type | Unit | Description |
|-----------|------|------|-------------|
| `raw_sensor_reading_mm` | `uint16_t` | millimeters | Latest raw VL53L5CX distance |
| `filtered_sensor_reading_mm` | `uint16_t` | millimeters | Latest filtered distance |
| `calculated_height_cm` | `uint16_t` | centimeters | Current desk height |
| `system_status` | `String` | - | Human-readable state (e.g., "Idle", "Moving Up") |
| `last_error_message` | `String` | - | Most recent error or "" if none |
| `wifi_connection_status` | `String` | - | "Connected (192.168.1.100)" or "Disconnected" |
| `wifi_signal_strength_dbm` | `int8_t` | dBm | WiFi RSSI (-30 to -90) |
| `uptime_ms` | `unsigned long` | milliseconds | Time since boot |
| `free_heap_bytes` | `uint32_t` | bytes | Available RAM |

### Update Frequency

- **Sensor Readings**: Every 200ms (matches sensor sampling)
- **System Status**: On state change (IDLE → MOVING, etc.)
- **WiFi Status**: Every 5 seconds (low priority)
- **Memory**: Every 10 seconds (diagnostic only)

### Web UI Display

```html
<div class="diagnostics">
  <p>Current Height: <span id="height">--</span> cm</p>
  <p>Status: <span id="status">--</span></p>
  <p>Sensor (raw): <span id="raw">--</span> mm</p>
  <p>Sensor (filtered): <span id="filtered">--</span> mm</p>
  <p>WiFi: <span id="wifi">--</span></p>
  <p>Last Error: <span id="error">None</span></p>
</div>
```

### Relationships

- **Populated by**: All modules (HeightController, MovementController, WiFiManager)
- **Pushed via**: SSE events to web clients
- **Logged to**: Serial console (optional SD card)

---

## Entity Relationship Diagram

```
┌─────────────────────┐
│ SystemConfiguration │ (Singleton, NVS-backed)
└──────────┬──────────┘
           │ configures
           ↓
┌──────────────────┐      uses      ┌──────────────┐
│ HeightController │ ←───────────── │ HeightReading│
└─────────┬────────┘                └──────┬───────┘
          │ provides                       │ informs
          ↓                                ↓
┌───────────────────┐      reads    ┌────────────────┐
│MovementController │ ←───────────── │ TargetHeight   │
└─────────┬─────────┘                └────────┬───────┘
          │ updates                           │ activated by
          ↓                                   ↓
┌──────────────────┐                ┌──────────────────┐
│ MovementState    │                │PresetConfiguration│
└─────────┬────────┘                └──────────────────┘
          │ publishes                (5 instances, NVS-backed)
          ↓
┌──────────────────┐
│ DiagnosticState  │
└──────────────────┘
          │ pushed via SSE
          ↓
     [Web Clients]
```

---

## Data Flow Example: Height Adjustment

1. **User Action**: Click preset "Standing - 110cm"
   - Web UI → HTTP POST /preset/2 → WebServer

2. **Target Creation**:
   - PresetManager loads preset slot 2 from memory
   - Creates TargetHeight {height=110, tolerance=10mm, source=PRESET, source_id=2}

3. **Movement Initiation**:
   - MovementController reads current HeightReading (calculated_height_cm = 75)
   - Determines direction: 110 > 75 → MOVING_UP
   - Activates PIN_MOTOR_UP = HIGH

4. **Loop Execution**:
   - Every 200ms: HeightController reads VL53L5CX → raw_distance_mm
   - Filters via MovingAverageFilter → filtered_distance_mm
   - Calculates height = (calibration_constant - filtered_distance_mm/10)
   - Pushes DiagnosticState via SSE → web clients update UI

5. **Stabilization**:
   - When calculated_height_cm ∈ [109, 111] (110 ± 1cm tolerance)
   - State → STABILIZING, start timer
   - If stable for 2000ms → PIN_MOTOR_UP = LOW, state → IDLE
   - If height drifts outside tolerance → reset timer, resume MOVING_UP

6. **Completion**:
   - MovementState.current_state = IDLE
   - Web UI shows success message: "Height reached: 110 cm"

---

## Summary

All data entities defined with attributes, validation, relationships, and persistence strategy. Ready for contract generation (Phase 1 continuation).
