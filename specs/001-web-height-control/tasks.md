# Tasks: Web-Based Desk Height Controller

**Input**: Design documents from `/specs/001-web-height-control/`  
**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓

**Test-First Development (Constitution Principle I)**: Per constitution, tests MUST be written before implementation. Each user story section includes test tasks BEFORE implementation tasks to enforce the Red-Green-Refactor cycle.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `- [ ] [ID] [P?] [Story?] Description`

- **Checkbox**: `- [ ]` for tracking completion
- **[ID]**: Task identifier (T001, T002, etc.)
- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- File paths use repository root conventions per plan.md

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: PlatformIO project initialization and development environment

- [X] T001 Create PlatformIO project structure with `pio project init --board esp32dev`
- [X] T002 Configure platformio.ini with ESP32 settings, libraries (SparkFun VL53L5CX, ESPAsyncWebServer, AsyncTCP, Preferences), SPIFFS filesystem
- [X] T003 [P] Create src/Config.h with pin definitions (I2C SDA/SCL, MOSFET up/down pins), constants (min/max height, tolerance, stabilization duration)
- [X] T004 [P] Create data/ directory for SPIFFS web files (index.html, style.css, script.js)
- [X] T005 [P] Setup test/ directory structure with Unity framework configuration
- [X] T006 [P] Configure clang-format and cppcheck for code quality (per Constitution Principle II)
- [X] T007 Create README.md with hardware requirements, wiring instructions, quick start guide

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T008 Implement MovingAverageFilter class in src/utils/MovingAverageFilter.h/cpp (circular buffer, addSample, getAverage methods)
- [X] T009 Write unit tests for MovingAverageFilter in test/test_moving_average/test_moving_average.cpp (verify averaging, window size, edge cases)
- [X] T010 [P] Implement Logger utility in src/utils/Logger.h/cpp (serial output, optional SD logging, log levels)
- [X] T011 [P] Create SystemConfiguration class in src/Config.h (calibration constant, min/max height, tolerance, stabilization duration, movement timeout)
- [X] T012 Setup WiFiManager in src/WiFiManager.h/cpp (connect to WiFi, auto-reconnect, AP fallback mode "DeskController-[ID]" per FR-020, status reporting)
- [X] T013 Write integration test for WiFiManager in test/test_wifi/test_wifi_connection.cpp (connect, disconnect, reconnect scenarios)
- [X] T014 Initialize SPIFFS in src/main.cpp setup() (check mount success, log errors)
- [X] T015 Setup NVS Preferences namespaces in src/main.cpp setup() ("config" for system settings, "presets" for saved heights)

**Checkpoint**: Foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Manual Height Adjustment (Priority: P1) 🎯 MVP

**Goal**: Users can enter a target height in the web interface and the desk automatically moves to that position, stopping when within tolerance for 2 seconds.

**Independent Test**: Enter target height in web UI, verify desk moves to within ±1cm tolerance and stops after 2s stabilization. Delivers core value as basic height controller.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [X] T016 [P] [US1] Write unit tests for HeightController in test/test_height_controller/test_height_calculation.cpp (test calibration formula: height = constant - reading/10)
- [X] T017 [P] [US1] Write unit tests for HeightController filtering in test/test_height_controller/test_filtering.cpp (verify moving average applied before calculation)
- [X] T018 [P] [US1] Write unit tests for MovementController state machine in test/test_movement_controller/test_state_machine.cpp (IDLE→MOVING_UP→STABILIZING→IDLE transitions)
- [X] T019 [P] [US1] Write integration test for sensor reading in test/test_sensor/test_vl53l5cx_integration.cpp (I2C communication, single-zone mode, 5Hz sampling)
- [X] T020 [P] [US1] Write integration test for MOSFET control in test/test_gpio/test_mosfet_control.cpp (pin HIGH during movement, LOW when stopped, mutual exclusion)

### Implementation for User Story 1

- [X] T021 [P] [US1] Implement HeightController class in src/HeightController.h/cpp (VL53L5CX I2C initialization, readSensor, applyFilter, calculateHeight methods)
- [X] T022 [P] [US1] Implement MovementController class in src/MovementController.h/cpp (state machine: IDLE/MOVING_UP/MOVING_DOWN/STABILIZING/ERROR, MOSFET pin control)
- [X] T023 [US1] Integrate MovingAverageFilter into HeightController (filter sensor readings before height calculation per FR-001a)
- [X] T024 [US1] Implement stabilization timer in MovementController (2-second countdown, reset if height leaves tolerance per FR-008)
- [X] T025 [US1] Implement setTargetHeight method in MovementController (validate range, determine direction, activate appropriate MOSFET pin)
- [X] T026 [US1] Add loop task in src/main.cpp (call HeightController.update every 200ms, call MovementController.update for state management)
- [X] T027 [US1] Implement safety checks in MovementController (sensor validity check per FR-015, movement timeout per FR-016)
- [X] T028 [US1] Add error handling and logging for sensor failures and timeouts

**Checkpoint**: User Story 1 complete - desk moves to manual target height with stabilization

---

## Phase 4: User Story 2 - Real-Time Height Monitoring (Priority: P1)

**Goal**: Web interface displays current desk height in real-time with updates pushed via Server-Sent Events every 200ms.

**Independent Test**: Open web UI while manually moving desk (via hardware buttons), verify height display updates within 500ms and shows intermediate positions during movement.

### Tests for User Story 2

- [X] T029 [P] [US2] Write unit tests for SSE event formatting in test/test_webserver/test_sse_events.cpp (verify JSON payloads for height_update, status_change events)
- [X] T030 [P] [US2] Write integration test for SSE connection in test/test_webserver/test_sse_integration.cpp (client connect, receive events, auto-reconnect)

### Implementation for User Story 2

- [X] T031 [P] [US2] Create basic HTML structure in data/index.html (height display element, status indicator, diagnostics section)
- [X] T032 [P] [US2] Create CSS styles in data/style.css (responsive layout, status color coding: green=Idle, yellow=Moving, red=Error)
- [X] T033 [P] [US2] Implement WebServer class in src/WebServer.h/cpp (ESPAsyncWebServer initialization, serve static files from SPIFFS)
- [X] T034 [US2] Add SSE endpoint /events in src/WebServer.cpp (AsyncEventSource setup, client connection handling)
- [X] T035 [US2] Implement sendHeightUpdate method in WebServer (push height_update SSE event with raw_mm, filtered_mm, height_cm, timestamp)
- [X] T036 [US2] Add loop task in src/main.cpp to push SSE events every 200ms (call WebServer.sendHeightUpdate with current HeightController state)
- [X] T037 [US2] Implement JavaScript SSE client in data/script.js (EventSource connection, height_update listener, update DOM elements)
- [X] T038 [US2] Add status_change SSE event in WebServer (emit on MovementController state transitions, include previous/new state)
- [X] T039 [US2] Add JavaScript status_change listener in data/script.js (update status display, show visual feedback for state changes)
- [X] T040 [US2] Implement diagnostics display in data/index.html (show raw sensor reading, filtered reading, system state, last error)

**Checkpoint**: User Story 2 complete - real-time height monitoring works independently and integrates with US1

---

## Phase 5: User Story 3 - Preset Height Configuration (Priority: P2)

**Goal**: Users can configure and save up to 5 preset heights with labels, which persist across reboots and can be activated with one click.

**Independent Test**: Configure 5 presets, reboot ESP32, verify presets restored. Click preset button, verify desk moves to saved height (same as manual entry from US1).

### Tests for User Story 3

- [X] T041 [P] [US3] Write unit tests for PresetManager in test/test_preset_manager/test_preset_crud.cpp (save, load, validate, delete operations)
- [X] T042 [P] [US3] Write integration test for NVS persistence in test/test_preset_manager/test_nvs_persistence.cpp (write preset, reboot simulation, read preset)

### Implementation for User Story 3

- [X] T043 [P] [US3] Implement PresetConfiguration struct in src/PresetManager.h (slot, label, height_cm, last_modified_ms fields)
- [X] T044 [P] [US3] Implement PresetManager class in src/PresetManager.h/cpp (array of 5 PresetConfiguration, CRUD methods)
- [X] T045 [US3] Implement savePreset method in PresetManager (validate label length, height range, write to NVS namespace "presets")
- [X] T046 [US3] Implement loadPresets method in PresetManager (read from NVS, populate in-memory array, call in setup())
- [X] T047 [US3] Add GET /presets endpoint in src/WebServer.cpp (return JSON array of all 5 preset configurations)
- [X] T048 [US3] Add POST /preset/save endpoint in src/WebServer.cpp (accept slot, label, height_cm, call PresetManager.savePreset)
- [X] T049 [US3] Add POST /preset endpoint in src/WebServer.cpp (accept slot number, load preset, call MovementController.setTargetHeight)
- [X] T050 [US3] Add preset configuration UI in data/index.html (5 input groups: label field, height field, save button per slot)
- [X] T051 [US3] Add preset activation buttons in data/index.html (5 buttons showing "Label - XXcm", trigger POST /preset on click)
- [X] T052 [US3] Implement JavaScript preset handlers in data/script.js (save preset form submission, activate preset button click)
- [X] T053 [US3] Add preset_updated SSE event in WebServer (emit after successful save, refresh UI without page reload)
- [X] T054 [US3] Add JavaScript preset_updated listener in data/script.js (update button labels when preset saved)

**Checkpoint**: User Story 3 complete - presets work independently and integrate with US1 movement control

---

## Phase 6: User Story 4 - Movement Safety Controls (Priority: P2)

**Goal**: System prevents dangerous behavior (sensor failures, out-of-range inputs, runaway movement) with proper error handling and user feedback.

**Independent Test**: Simulate error conditions (disconnect sensor, enter invalid height, rapid target changes), verify safe behavior (movement stops, errors displayed, no oscillation).

### Tests for User Story 4

- [X] T055 [P] [US4] Write unit tests for input validation in test/test_webserver/test_input_validation.cpp (height range checks, reject <min or >max)
- [X] T056 [P] [US4] Write integration test for sensor failure handling in test/test_safety/test_sensor_failure.cpp (disconnect I2C, verify movement stops)
- [X] T057 [P] [US4] Write integration test for movement timeout in test/test_safety/test_movement_timeout.cpp (block desk mechanically, verify 30s timeout)

### Implementation for User Story 4

- [X] T058 [P] [US4] Add input validation in POST /target endpoint (check min_safe_height ≤ target ≤ max_safe_height, return 400 error if invalid)
- [X] T059 [P] [US4] Implement sensor validity check in HeightController (set validity=INVALID on I2C error or reading=0/>4000mm)
- [X] T060 [US4] Add sensor failure detection in MovementController (check HeightController.getValidity, stop movement if INVALID per FR-015)
- [X] T061 [US4] Implement movement timeout in MovementController (track movement start time, stop if duration > 30s per FR-016, set state=ERROR)
- [X] T062 [US4] Add target override logic in MovementController (new target clears pending movement per FR-017, only one operation active)
- [X] T063 [US4] Implement error SSE event in WebServer (emit on sensor failure, timeout, invalid input with error_code and message)
- [X] T064 [US4] Add JavaScript error listener in data/script.js (display error messages, disable controls during ERROR state)
- [X] T065 [US4] Add POST /stop endpoint in src/WebServer.cpp (emergency stop: set both MOSFET pins LOW, clear target, state→IDLE)
- [X] T066 [US4] Add emergency stop button in data/index.html (prominent red button, always enabled, triggers POST /stop)
- [X] T067 [US4] Add client-side validation in data/script.js (check height range before submission, show friendly error messages)
- [X] T068 [US4] Add confirmation dialog in data/script.js for height changes >30cm (compare Math.abs(target - current), show "Move desk 35cm up - are you sure?" modal before POST /target per UX-009)

**Checkpoint**: User Story 4 complete - all safety features implemented, system handles errors gracefully

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories, final quality checks

- [X] T069 [P] Add GET /status endpoint in src/WebServer.cpp (return current height, sensor readings, state, WiFi RSSI, uptime, free heap)
- [X] T070 [P] Add GET /config endpoint in src/WebServer.cpp (return calibration constant, min/max height, tolerance, stabilization duration)
- [X] T071 [P] Add POST /config endpoint in src/WebServer.cpp (update system configuration, write to NVS, hot-reload settings)
- [X] T072 [P] Add calibration wizard UI in data/index.html (enter known height H_cm, system reads sensor S_mm, calculates constant = H + S/10, saves to NVS per FR-019)
- [X] T073 [P] Implement ARIA labels and semantic HTML in data/index.html (meet WCAG 2.1 AA per UX-006)
- [X] T074 [P] Add keyboard navigation support in data/script.js (tab order, Enter key submission per UX-003)
- [X] T075 [P] Optimize CSS for mobile responsiveness in data/style.css (flexbox layout, min 16px font per UX-010)
- [X] T076 [P] Add WiFi status display in data/index.html (connection status, IP address, signal strength)
- [X] T077 [P] Add wifi_status SSE event in WebServer (emit on connection/disconnection, include SSID, IP, RSSI)
- [X] T078 [P] Create hardware wiring documentation in docs/hardware-setup.md (VL53L5CX I2C, MOSFET circuit, power supply)
- [X] T079 [P] Create calibration guide in docs/calibration.md (step-by-step procedure, troubleshooting)
- [X] T080 [P] Create troubleshooting guide in docs/troubleshooting.md (sensor not detected, desk won't move, height incorrect, WiFi issues)
- [ ] T081 Run all unit tests and verify 80%+ coverage (per Constitution Principle I)
- [ ] T082 Run all integration tests and verify hardware paths tested (sensor, MOSFET, WiFi)
- [ ] T083 Run E2E test scenarios from quickstart.md (calibration, manual adjustment, preset save/activate, error recovery)
- [ ] T084 Performance testing: verify sensor sampling at 5Hz, SSE latency <500ms, memory usage <150KB (per PERF-002, PERF-004)
- [ ] T084a [P] Benchmark NVS write latency for preset save operations in test/test_preset_manager/test_nvs_performance.cpp (measure Preferences.putString/putUInt timing, verify <1s per PERF-006)
- [ ] T085 Code review and refactoring (check Constitution compliance, error handling, documentation)
- [X] T086 Update README.md with final setup instructions, dependencies, quick start, troubleshooting links

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phases 3-6)**: All depend on Foundational phase completion
  - US1 and US2 (both P1) should be completed first as MVP
  - US3 and US4 (both P2) can follow in priority order
  - Within P1: US2 integrates with US1 (SSE displays height from HeightController)
  - Within P2: US3 uses US1's movement control, US4 adds safety to US1
- **Polish (Phase 7)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (Manual Height Adjustment)**: Depends only on Foundational - no dependencies on other stories
- **US2 (Real-Time Monitoring)**: Depends on Foundational - integrates with US1's HeightController but independently testable (can monitor manual desk movement)
- **US3 (Preset Configuration)**: Depends on Foundational - uses US1's MovementController.setTargetHeight but independently testable (presets just trigger same movement)
- **US4 (Safety Controls)**: Depends on Foundational - enhances US1's movement with safety checks but independently testable (error handling can be tested in isolation)

### Within Each User Story

1. **Tests FIRST** (if requested): Write all tests, verify they FAIL
2. **Models/Classes**: Core data structures and business logic
3. **Integration**: Wire components together
4. **API/UI**: Expose functionality via web interface
5. **Refinement**: Error handling, logging, edge cases

### Parallel Opportunities

**Phase 1 (Setup)**: T003, T004, T005, T006 can run in parallel (different files)

**Phase 2 (Foundational)**: T010, T011 can run in parallel with T012-T015 (different modules)

**Phase 3 (US1 Tests)**: T016, T017, T018, T019, T020 can all run in parallel (different test files)

**Phase 3 (US1 Implementation)**: T021, T022 can run in parallel (HeightController and MovementController are separate classes)

**Phase 4 (US2 Implementation)**: T031, T032, T033 can run in parallel (HTML, CSS, WebServer class are independent)

**Phase 5 (US3 Tests)**: T041, T042 can run in parallel (unit and integration tests in different files)

**Phase 5 (US3 Implementation)**: T043, T044 can run in parallel with T047-T049 if PresetManager interface is defined first

**Phase 6 (US4 Tests)**: T055, T056, T057 can all run in parallel (different test files)

**Phase 6 (US4 Implementation)**: T058, T059 can run in parallel (different modules: WebServer vs HeightController)

**Phase 7 (Polish)**: Most tasks (T069-T080) can run in parallel as they affect different files (endpoints, UI, docs)

---

## Parallel Execution Examples

### User Story 1 Test Phase
```bash
# All US1 tests can be written in parallel:
Developer A: T016 - Unit test height calculation
Developer B: T017 - Unit test filtering  
Developer C: T018 - Unit test state machine
Developer D: T019 - Integration test sensor
Developer E: T020 - Integration test MOSFET
```

### User Story 1 Implementation Phase
```bash
# Core classes can be built in parallel:
Developer A: T021 - HeightController class
Developer B: T022 - MovementController class

# Then sequential integration:
Developer A or B: T023-T028 - Integration and safety
```

### User Story 2 Implementation Phase
```bash
# UI and server can be built in parallel:
Developer A: T031, T032 - HTML/CSS
Developer B: T033, T034, T035 - WebServer SSE
Developer C: T037, T038, T039 - JavaScript client
```

---

## Implementation Strategy

### MVP First (US1 + US2 = P1 Priority)

1. ✓ Complete **Phase 1: Setup** (T001-T007)
2. ✓ Complete **Phase 2: Foundational** (T008-T015) - **CRITICAL GATE**
3. Complete **Phase 3: User Story 1** (T016-T028) - Manual height adjustment
4. Complete **Phase 4: User Story 2** (T029-T040) - Real-time monitoring
5. **STOP and VALIDATE**: Test P1 stories independently
   - US1: Enter target → desk moves → stabilizes → stops ✓
   - US2: Open UI → height updates in real-time ✓
6. **Deploy MVP**: Functional desk controller with manual control and live monitoring

### Incremental Delivery Beyond MVP

1. **MVP Deployed** (US1 + US2)
2. Add **Phase 5: User Story 3** (T041-T054) - Preset configuration
   - Test: Save presets → reboot → activate preset ✓
   - Deploy: Users can now save favorite heights
3. Add **Phase 6: User Story 4** (T055-T068) - Safety controls
   - Test: Simulate failures → verify safe handling ✓
   - Deploy: Production-ready with error handling
4. Add **Phase 7: Polish** (T069-T086) - Final quality
   - Deploy: Complete feature with documentation

### Parallel Team Strategy (if multiple developers)

After **Phase 2 Foundational** completes:

- **Developer A**: Phase 3 (US1 - Manual Height Adjustment)
- **Developer B**: Phase 4 (US2 - Real-Time Monitoring)
- **Developer C**: Phase 5 (US3 - Preset Configuration)
- **Developer D**: Phase 6 (US4 - Safety Controls)

Stories integrate naturally:
- US2 reads from US1's HeightController
- US3 calls US1's MovementController.setTargetHeight
- US4 adds validation to US1's movement control

---

## Task Count Summary

- **Phase 1 (Setup)**: 7 tasks
- **Phase 2 (Foundational)**: 8 tasks (blocks all stories)
- **Phase 3 (US1 - Manual Height Adjustment)**: 13 tasks (5 tests + 8 implementation)
- **Phase 4 (US2 - Real-Time Monitoring)**: 12 tasks (2 tests + 10 implementation)
- **Phase 5 (US3 - Preset Configuration)**: 14 tasks (2 tests + 12 implementation)
- **Phase 6 (US4 - Safety Controls)**: 14 tasks (3 tests + 11 implementation)
- **Phase 7 (Polish)**: 19 tasks (cross-cutting improvements + performance benchmarks)

**Total**: 87 tasks

**Parallel Opportunities**: 35+ tasks can run in parallel (marked with [P])

**Independent Test Criteria**:
- US1: Desk moves to manual target and stops after stabilization
- US2: Height display updates in real-time during movement
- US3: Presets persist across reboot and activate correctly
- US4: Errors handled safely with movement stopped

**MVP Scope**: Phases 1-4 (US1 + US2 = 40 tasks for functional desk controller)

---

## Notes

- **[P] marker**: Tasks that can run in parallel (different files, no dependencies)
- **[Story] label**: Maps task to user story for traceability and independent testing
- **Test-First**: All test tasks BEFORE implementation per Constitution Principle I
- **Checkpoints**: Each phase ends with validation - stop to test independently
- **MVP-ready**: Phases 1-4 deliver working desk controller (manual control + real-time display)
- **File paths**: Repository root structure per plan.md (src/, data/, test/, docs/)
- **Commit frequently**: After each task or logical group for safe progress tracking
