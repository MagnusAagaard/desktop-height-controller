# Feature Specification: Web-Based Desk Height Controller

**Feature Branch**: `001-web-height-control`  
**Created**: 2026-01-11  
**Status**: Draft  
**Input**: User description: "Build an application that can be used to control my desktop height using a microcontroller. The microcontroller interfaces with a distance sensor and is connected with two output pins, one for going up and one for going down. The microcontroller should host a webpage that shows the current height (distance to the ground) and have an input field that a desired height can be set, triggering the microcontroller to control the desk until it reaches the desired height. The microcontroller should only control the desk when a new target height is set and until it is within the tolerance for more than e.g. 2 seconds (ie. if the sensor afterwards reads heights outside the target height + tolerance, it should not continue to move). 5 target heights should be configurable through the web interface and persisted upon reboot"

## Clarifications

### Session 2026-01-11

- Q: How should the system calculate desk height from the distance sensor? → A: Sensor measures distance to floor; height = (fixed_constant - sensor_reading). Requires one-time calibration at known height
- Q: How should the web interface receive real-time height updates? → A: Server-Sent Events (SSE): Server pushes height updates when sensor reading changes
- Q: How should the system prevent oscillation when sensor readings fluctuate near the tolerance boundary? → A: Require consecutive stable readings within tolerance for full 2s period (time-based hysteresis), AND apply moving average filter to smooth sensor readings before comparison
- Q: What level of diagnostic information should the system provide? → A: Both web UI diagnostics (sensor reading, system state, last error, connection status) plus optional serial console for deep debugging
- Q: What is the expected output pin control strategy for desk movement? → A: Continuous HIGH signal: Pin held HIGH during movement, set LOW to stop (relay stays energized while moving)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Manual Height Adjustment (Priority: P1)

A user wants to adjust their desk to a specific height by entering a value and having the desk automatically move to that position.

**Why this priority**: Core functionality - without the ability to set and reach a target height, the system has no value. This is the fundamental use case.

**Independent Test**: Can be fully tested by entering a target height in the web interface and verifying the desk moves to within tolerance and stops. Delivers immediate value as a basic height controller.

**Acceptance Scenarios**:

1. **Given** the desk is at 70cm height, **When** user enters 100cm as target height, **Then** the desk moves upward until it reaches 100cm ±tolerance and stops
2. **Given** the desk is at 100cm height, **When** user enters 75cm as target height, **Then** the desk moves downward until it reaches 75cm ±tolerance and stops
3. **Given** the desk is moving to a target height, **When** the height stabilizes within tolerance for 2 seconds, **Then** the desk stops moving and remains stationary
4. **Given** the desk has reached target height and stopped, **When** external force moves the desk outside tolerance, **Then** the desk does NOT automatically re-adjust (only moves on new target)

---

### User Story 2 - Real-Time Height Monitoring (Priority: P1)

A user wants to see the current desk height displayed on the web interface so they know the desk's position at all times.

**Why this priority**: Essential safety and usability feature - users need visual feedback of desk position to understand system state and make informed decisions. Required for P1 manual adjustment to be safe.

**Independent Test**: Can be tested by observing the web interface while manually moving the desk (via hardware buttons) and verifying the displayed height updates in real-time.

**Acceptance Scenarios**:

1. **Given** the web interface is open, **When** the desk height changes, **Then** the displayed height value updates within 500ms
2. **Given** the desk is stationary, **When** user refreshes the page, **Then** the current height is displayed accurately
3. **Given** the desk is moving, **When** user observes the interface, **Then** the height value updates continuously showing intermediate positions

---

### User Story 3 - Preset Height Configuration (Priority: P2)

A user wants to configure and save up to 5 preset heights (e.g., "Sitting", "Standing", "Keyboard", "Monitor", "Presentation") so they can quickly switch between favorite positions.

**Why this priority**: Significant usability improvement but not essential for basic operation. Users can still manually enter heights, but presets reduce repetitive data entry and improve user experience.

**Independent Test**: Can be tested by configuring preset heights, rebooting the microcontroller, and verifying the presets are restored. Activating a preset should trigger the same height adjustment as manual entry.

**Acceptance Scenarios**:

1. **Given** user is on the web interface, **When** user configures 5 preset heights with labels (e.g., "Sitting: 72cm"), **Then** the presets are saved and displayed
2. **Given** preset heights are configured, **When** user clicks a preset button, **Then** the desk moves to the preset height as if manually entered
3. **Given** preset heights are saved, **When** the microcontroller reboots, **Then** the configured presets are restored and functional
4. **Given** user wants to change a preset, **When** user updates a preset height or label, **Then** the new value is saved and overwrites the previous preset

---

### User Story 4 - Movement Safety Controls (Priority: P2)

A user wants the system to prevent dangerous behavior like continuous movement, oscillation, or unexpected starts, ensuring safe operation.

**Why this priority**: Critical for safety but partially addressed by P1 (2-second stabilization, manual trigger only). This story adds explicit safety boundaries and error handling.

**Independent Test**: Can be tested by simulating error conditions (sensor disconnection, out-of-range values, rapid target changes) and verifying safe system behavior.

**Acceptance Scenarios**:

1. **Given** the desk is moving toward target height, **When** the sensor loses connection or reads invalid data, **Then** the desk immediately stops moving and displays an error
2. **Given** user enters a target height outside safe range (e.g., <50cm or >125cm), **When** user submits the value, **Then** the system rejects the input and displays an error message
3. **Given** the desk has reached target height, **When** user rapidly submits multiple new target heights, **Then** the system queues only the most recent target and ignores intermediate values
4. **Given** the desk is moving, **When** the movement duration exceeds expected maximum time (e.g., 30 seconds), **Then** the system stops movement and displays a timeout error

---

### Edge Cases

- What happens when the distance sensor reports erratic readings (e.g., due to interference)?
- How does the system handle power loss during movement?
- What happens if the desk reaches mechanical limits before achieving target height?
- How does the system prevent oscillation if the sensor reading fluctuates around the tolerance boundary?
- What happens when multiple users access the web interface simultaneously?
- How does the system behave if the microcontroller's web server becomes unresponsive?
- What happens when the desk cannot reach the target height due to physical obstacles?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST read distance measurements from a sensor (in millimeters) and calculate current desk height from ground (in centimeters) using formula: height_cm = (calibration_constant_cm - sensor_reading_mm / 10), where calibration_constant is set during one-time calibration procedure
- **FR-001a**: System MUST apply a moving average filter to sensor readings before height calculation to smooth fluctuations and reduce noise
- **FR-002**: System MUST control desk movement using two discrete output pins (up and down) with continuous HIGH signal strategy: pin held HIGH during movement, LOW when stopped
- **FR-003**: System MUST host a web server accessible on the local network serving a control interface
- **FR-004**: Web interface MUST display current desk height in real-time with updates at least every 500ms using Server-Sent Events (SSE) to push updates from server when sensor readings change
- **FR-005**: Web interface MUST provide an input field for users to enter a target height value
- **FR-006**: System MUST move the desk toward the target height when a new target is set (activate appropriate output pin)
- **FR-007**: System MUST stop movement when the current height is within defined tolerance of the target height AND remains stable within tolerance for at least 2 seconds; any sensor reading outside tolerance during this stabilization period resets the timer to prevent oscillation
- **FR-008**: System MUST NOT automatically re-adjust if the desk moves outside tolerance after stopping (only moves on new target)
- **FR-009**: System MUST support configuration of exactly 5 preset heights through the web interface
- **FR-010**: Each preset MUST include a user-defined label and a height value
- **FR-011**: System MUST persist all preset configurations to non-volatile storage
- **FR-012**: System MUST restore preset configurations from storage after reboot
- **FR-013**: System MUST validate target height inputs are within safe operational range (default: 50cm to 125cm, configurable)
- **FR-014**: System MUST immediately stop desk movement if sensor readings become invalid or unavailable
- **FR-015**: System MUST implement a maximum movement timeout (default: 30 seconds) to prevent runaway conditions
- **FR-016**: System MUST ensure only one movement operation is active at a time (subsequent targets override pending movements)
- **FR-017**: System MUST provide diagnostic information via web interface including: current sensor reading (raw and filtered), system state, last error message, and connection status
- **FR-018**: System MUST optionally support serial console logging for detailed debugging information when physical access to microcontroller is available
- **FR-019**: System MUST support one-time calibration procedure: user measures desk at known height H_cm, system reads sensor distance S_mm, calculates and stores calibration_constant_cm = H_cm + (S_mm / 10) to non-volatile storage
- **FR-020**: System MUST create WiFi access point with SSID "DeskController-[ID]" if connection to configured network fails, allowing initial setup or network reconfiguration

### User Experience Requirements *(Constitution Principle III)*

- **UX-001**: UI MUST provide feedback for target height submission within 100ms (confirmation message or visual indicator)
- **UX-002**: UI MUST display current system status (Idle, Moving Up, Moving Down, Error) prominently along with diagnostic information (current height, sensor reading, connection status)
- **UX-003**: All interactive elements MUST be keyboard accessible (input field, submit button, preset buttons)
- **UX-004**: Error messages MUST be clear and actionable (e.g., "Sensor disconnected - check wiring" not "Error 0x03")
- **UX-005**: UI terminology MUST be consistent: "Height" (always in cm), "Target Height", "Current Height", "Preset", "Move To"
- **UX-006**: Screen reader support MUST meet WCAG 2.1 AA standard (semantic HTML, ARIA labels, alt text)
- **UX-007**: UI MUST show visual confirmation when desk reaches target height (e.g., success message or color change)
- **UX-008**: Preset buttons MUST show both label and height value (e.g., "Sitting - 72cm")
- **UX-009**: UI MUST prevent accidental submissions (e.g., confirmation for extreme height changes >30cm)
- **UX-010**: Page layout MUST be responsive and usable on mobile devices (phone/tablet screens)

### Performance Requirements *(Constitution Principle IV)*

- **PERF-001**: Web interface MUST load and display initial page within 2 seconds on local network
- **PERF-002**: Height sensor readings MUST be sampled at least every 200ms for responsive display updates
- **PERF-003**: Target height submission MUST trigger desk movement within 500ms of user action
- **PERF-004**: Memory usage MUST NOT exceed 100KB for web server and 50KB for control logic (microcontroller constraints)
- **PERF-005**: System MUST handle at least 5 concurrent web interface connections without performance degradation
- **PERF-006**: Preset save operations MUST complete within 1 second
- **PERF-007**: System recovery from error conditions MUST occur within 3 seconds (sensor reconnection, timeout reset)

### Key Entities

- **Height Reading**: Raw sensor distance reading, filtered reading (post moving-average), calculated height (calibration_constant - filtered_reading), timestamp, validity status (valid/invalid/stale)
- **Target Height**: User-specified or preset-triggered desired height, tolerance range, activation timestamp
- **Preset Configuration**: Label (string, max 20 characters), height value (cm), slot number (1-5), last modified timestamp
- **Movement State**: Current operation (idle/moving_up/moving_down/stabilizing/error), start time, target height, stabilization timer
- **System Configuration**: Minimum safe height, maximum safe height, tolerance value (default ±1cm), stabilization duration (default 2s), movement timeout (default 30s), calibration constant (height_cm = constant_cm - sensor_mm/10), moving average window size (default 5 samples)
- **Diagnostic State**: Current sensor reading (raw), filtered sensor reading, calculated height, system status, last error message, connection status, uptime

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can adjust desk height from any position to any valid target within 30 seconds with 100% success rate
- **SC-002**: System achieves target height within ±1cm tolerance in 95% of attempts (allowing for sensor noise)
- **SC-003**: Web interface displays height updates with <500ms latency during desk movement
- **SC-004**: Preset configurations survive 100 consecutive reboot cycles without data loss
- **SC-005**: System prevents dangerous behavior (oscillation, runaway movement, unexpected starts) in 100% of test scenarios
- **SC-006**: Users can complete preset configuration workflow in under 2 minutes
- **SC-007**: System responds to target height submission within 500ms
- **SC-008**: Web interface remains responsive with up to 5 simultaneous connections
- **SC-009**: Error recovery occurs automatically within 3 seconds for transient sensor issues
- **SC-010**: 90% of users can successfully operate the system on first attempt without documentation (usability metric)

## Assumptions

- **Sensor Assumption**: Distance sensor measures distance to floor and provides reliable readings in cm with reasonable accuracy (±0.5cm typical); sensor is mounted in fixed position on desk pointing downward
- **Hardware Assumption**: Desk controller accepts continuous HIGH/LOW signals on up/down pins (standard relay or motor controller interface); desk moves while pin is HIGH, stops when pin is LOW
- **Network Assumption**: Microcontroller connects to local WiFi network or creates access point (common microcontroller capability)
- **Power Assumption**: Microcontroller has sufficient power supply for continuous operation and web hosting
- **Storage Assumption**: Microcontroller has non-volatile storage (EEPROM, flash, or SD card) for persisting presets
- **Safety Assumption**: Desk has built-in mechanical limit switches or safety features to prevent damage (system adds software safety layer)
- **Tolerance Assumption**: Default ±1cm tolerance is acceptable for typical standing desk use cases (adjustable if needed)
- **Concurrency Assumption**: Multiple simultaneous web users is edge case; single user is primary scenario
- **Browser Assumption**: Users access web interface from modern browsers with JavaScript enabled

## Dependencies

- **Hardware Dependency**: Compatible distance sensor (e.g., ultrasonic HC-SR04, IR sensor, or time-of-flight sensor)
- **Hardware Dependency**: Microcontroller with WiFi capability, GPIO pins, and web server support (e.g., ESP32, ESP8266, or similar)
- **Hardware Dependency**: Motorized standing desk with accessible control interface (relay-compatible)
- **Network Dependency**: WiFi network for web interface access (or microcontroller creates AP)

## Out of Scope

- **External API integration**: No cloud connectivity, external logging, or remote access required
- **User authentication**: Single-user system, no login or multi-user management
- **Mobile application**: Web interface only, no native mobile apps
- **Historical tracking**: No logging of height changes, usage patterns, or analytics
- **Advanced scheduling**: No time-based automatic adjustments or calendar integration
- **Voice control**: No voice assistant integration (Alexa, Google Home, etc.)
- **Multi-desk support**: Single desk controller per microcontroller instance
