# Implementation Plan: Web-Based Desk Height Controller

**Branch**: `001-web-height-control` | **Date**: 2026-01-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-web-height-control/spec.md`

**Note**: This plan addresses the ESP32 microcontroller with VL53L5CX ToF sensor, MOSFET-based desk control, WiFi connectivity, and web server hosting.

## Summary

Build an ESP32-based web application to control motorized standing desk height. The system reads height from a VL53L5CX time-of-flight distance sensor, controls desk movement via two MOSFET-controlled pins (up/down), and hosts a responsive web interface for manual height adjustment and preset configuration. Real-time height updates use Server-Sent Events (SSE). All functionality accessible via WiFi network connection with automatic reconnection. Presets persist in ESP32 flash storage.

**Technical Approach**: Arduino framework for ESP32, SparkFun VL53L5CX library for sensor, AsyncWebServer for HTTP/SSE, SPIFFS for web files, Preferences library for preset storage, moving average filter for sensor smoothing, state machine for movement control with 2-second stabilization.

## Technical Context

**Language/Version**: C++ with Arduino framework for ESP32 (ESP-IDF v4.4+, Arduino Core 2.0+)  
**Primary Dependencies**: 
- SparkFun VL53L5CX Arduino Library (ToF sensor driver)
- ESPAsyncWebServer (async HTTP server with SSE support)
- AsyncTCP (async TCP for web server)
- Preferences (NVS flash storage for presets)
- SPIFFS (file system for static web files)

**Storage**: 
- SPIFFS partition for HTML/CSS/JS files (~200KB)
- NVS (Non-Volatile Storage) via Preferences for preset configurations (~1KB)
- VL53L5CX requires I2C communication (Wire library)

**Testing**: 
- Unit tests: PlatformIO Unity framework for control logic (height calculation, filtering, state machine)
- Integration tests: Hardware-in-loop tests for sensor readings, MOSFET control, WiFi connection
- E2E tests: Manual web interface testing with actual desk movement

**Target Platform**: ESP32 (recommended: ESP32-WROOM-32, 4MB flash minimum)  
**Project Type**: Single embedded project (src/ for firmware, data/ for web files)  
**Performance Goals**: 
- Sensor sampling: 5 Hz (200ms intervals) per PERF-002
- Web page load: <2s per PERF-001
- SSE update latency: <500ms per FR-004
- Movement response: <500ms from target submission per PERF-003

**Constraints**: 
- ESP32 memory: ~320KB RAM, <150KB available after WiFi/web server (~100KB web + 50KB control per PERF-004)
- VL53L5CX: I2C (default address 0x29), up to 400kHz, 64-zone ranging (use single zone for performance)
- WiFi: 2.4GHz only, WPA2 security
- MOSFET control: 3.3V logic level compatible (ESP32 GPIO output)

**Scale/Scope**: 
- Single desk controller
- Up to 5 concurrent web connections per PERF-005
- 5 preset configurations per FR-010
- Height range: 50cm-125cm (configurable per FR-014)
- VL53L5CX range: Up to 400cm (sufficient for desk heights)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**I. Test-First Development**
- [x] Test strategy defined: PlatformIO Unity for unit tests (control logic, state machine), hardware-in-loop for integration (sensor, MOSFET), manual E2E (web UI + desk movement)
- [x] Coverage targets: Unit 80%+ for HeightController, PresetManager, MovingAverageFilter; Integration for all sensor/MOSFET/WiFi paths; E2E for all 4 user stories
- [x] Red-Green-Refactor: Write tests for each module before implementation; verify test fails; implement; refactor for clarity

**II. Code Quality Standards**
- [x] Linting/formatting: clang-format (Google style), cppcheck static analysis, PlatformIO check
- [x] Code review: All commits reviewed via PR; reviewer checks constitution compliance, test coverage, error handling
- [x] Documentation: README (hardware setup, WiFi config, calibration), inline comments for complex logic (moving average, stabilization), function headers for public APIs
- [x] Error handling: All sensor failures stop movement; WiFi disconnection logged + retry; invalid inputs rejected with user-friendly messages

**III. User Experience Consistency**
- [x] Accessibility: Semantic HTML5, ARIA labels on all controls, keyboard nav (tab order: input → submit → presets), 16px min font, sufficient contrast ratios
- [x] Feedback: SSE pushes height updates <500ms; visual status indicator (Idle=green, Moving=yellow, Error=red); success confirmation on target reached
- [x] Error prevention: Client-side validation (50-125cm range); confirmation dialog for >30cm changes; disable submit during movement
- [x] User documentation: quickstart.md with calibration steps, preset configuration guide, troubleshooting common errors

**IV. Performance Requirements**
- [x] Response time: UI feedback <100ms (immediate DOM update); movement start <500ms (GPIO write after validation); sensor query <50ms (I2C read single zone)
- [x] Resource limits: Web server 100KB (AsyncWebServer + HTML/CSS/JS), control logic 50KB (state machine + filtering), VL53L5CX library ~30KB
- [x] Performance testing: Benchmark sensor sampling rate (target 5Hz), measure SSE latency under 5 connections, profile memory usage (heap fragmentation check)
- [x] Benchmarks: Sensor read latency, moving average computation time, MOSFET response time, WiFi reconnection time

**Violations**: None - standard embedded patterns sufficient

## Project Structure

### Documentation (this feature)

```text
specs/001-web-height-control/
├── plan.md              # This file (COMPLETE)
├── research.md          # Phase 0: ESP32 libraries, VL53L5CX integration, SSE patterns (COMPLETE)
├── data-model.md        # Phase 1: 6 entities with attributes, validation, relationships (COMPLETE)
├── quickstart.md        # Phase 1: Hardware setup, wiring, calibration, troubleshooting (COMPLETE)
├── contracts/           # Phase 1: HTTP API and SSE event schemas (COMPLETE)
│   ├── http-api.md      # 9 REST endpoints (GET /, POST /target, POST /preset, etc.)
│   └── sse-events.md    # 5 SSE event types (height_update, status_change, error, etc.)
└── tasks.md             # Phase 2: 86 tasks organized by user story (COMPLETE)
```

### Source Code (repository root)

```text
# ESP32 embedded project structure (PlatformIO)
/
├── platformio.ini               # PlatformIO config (ESP32, libraries, build flags)
├── src/
│   ├── main.cpp                 # Entry point (setup, loop, task scheduling)
│   ├── HeightController.h/cpp   # Height measurement, filtering, calibration
│   ├── MovementController.h/cpp # State machine, MOSFET control, stabilization
│   ├── PresetManager.h/cpp      # Preset CRUD, NVS persistence
│   ├── WebServer.h/cpp          # AsyncWebServer setup, route handlers, SSE
│   ├── WiFiManager.h/cpp        # WiFi connection, reconnection logic
│   ├── Config.h                 # Pin definitions, constants, WiFi credentials
│   └── utils/
│       ├── MovingAverageFilter.h/cpp  # Sensor smoothing algorithm
│       └── Logger.h/cpp         # Serial + optional SD logging
├── data/                        # SPIFFS web files
│   ├── index.html               # Main UI (height display, input, presets, diagnostics)
│   ├── style.css                # Responsive styles, status indicators
│   └── script.js                # SSE client, form handling, validation
├── test/
│   ├── test_height_controller/  # Unit tests for height calculation, filtering
│   ├── test_movement_controller/ # Unit tests for state machine, stabilization
│   ├── test_preset_manager/     # Unit tests for preset CRUD, NVS operations
│   └── README.md                # Test execution instructions
├── docs/
│   ├── hardware-setup.md        # Wiring diagram, VL53L5CX connection, MOSFET circuit
│   ├── calibration.md           # Step-by-step calibration procedure
│   └── troubleshooting.md       # Common issues and solutions
└── README.md                    # Project overview, quick start, dependencies
```

**Structure Decision**: ESP32 embedded project using PlatformIO for dependency management, library compatibility, and testing framework. Source files organized by functional module (height sensing, movement control, presets, web server, WiFi). Web interface served from SPIFFS partition. Tests use Unity framework with hardware-in-loop capability.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations - all constitution principles satisfied with standard embedded development patterns.

---

## Planning Artifacts Summary

### Phase 0: Research (COMPLETE)
- **research.md**: 7 technical decisions documented with rationale, alternatives, code examples
  1. VL53L5CX Integration (I2C single-zone mode, 5Hz sampling)
  2. SSE Implementation (ESPAsyncWebServer event source pattern)
  3. Moving Average Filter (5-sample SMA, circular buffer)
  4. WiFi Strategy (auto-reconnect, AP fallback)
  5. NVS Storage (Preferences library, namespace partitioning)
  6. MOSFET Control (mutual exclusion, debouncing, safety)
  7. Task Scheduling (millis()-based cooperative multitasking)

### Phase 1: Design & Contracts (COMPLETE)
- **data-model.md**: 6 entities with full attribute definitions, validation rules, state transitions
  1. Height Reading (raw → filtered → calculated pipeline)
  2. Target Height (manual or preset-triggered)
  3. Preset Configuration (5 slots, NVS-backed)
  4. Movement State (state machine with IDLE/MOVING_UP/MOVING_DOWN/STABILIZING/ERROR)
  5. System Configuration (calibration, limits, tuning parameters)
  6. Diagnostic State (real-time status for troubleshooting)

- **contracts/http-api.md**: 9 HTTP endpoints with request/response schemas
  - GET / (serve web UI)
  - POST /target (manual height adjustment)
  - POST /preset (activate saved preset)
  - POST /preset/save (store configuration)
  - GET /status (system diagnostics)
  - POST /stop (emergency halt)
  - GET/POST /config (system settings)
  - GET /presets (list all presets)

- **contracts/sse-events.md**: 5 SSE event types with payload schemas
  - height_update (5Hz sensor readings)
  - status_change (state transitions)
  - error (system failures)
  - preset_updated (configuration changes)
  - wifi_status (connection state)

- **quickstart.md**: Complete setup guide with:
  - Hardware requirements and wiring diagrams
  - Software installation (PlatformIO, dependencies)
  - First boot WiFi configuration
  - Calibration procedure
  - Basic usage examples
  - Troubleshooting (6 common issues)

- **Agent Context**: GitHub Copilot instructions updated with C++ ESP32 context

### Phase 2: Task Generation (COMPLETE)
- **tasks.md**: 87 tasks organized by user story with test-first approach
  - Phase 1: Setup (7 tasks) - PlatformIO initialization
  - Phase 2: Foundational (8 tasks) - Core infrastructure blocking all stories
  - Phase 3: US1 Manual Height Adjustment (13 tasks: 5 tests + 8 implementation)
  - Phase 4: US2 Real-Time Monitoring (12 tasks: 2 tests + 10 implementation)
  - Phase 5: US3 Preset Configuration (14 tasks: 2 tests + 12 implementation)
  - Phase 6: US4 Safety Controls (14 tasks: 3 tests + 11 implementation)
  - Phase 7: Polish (19 tasks) - Cross-cutting improvements + performance benchmarks
  - MVP Scope: Phases 1-4 (40 tasks for functional desk controller)
  - Parallel Opportunities: 35+ tasks marked [P]
  - Independent test criteria defined for each user story

---

## Next Steps

1. **Begin Implementation**: Start with Phase 1 Setup tasks (T001-T007)
2. **Complete Foundational**: Phase 2 blocks all user stories - critical gate (T008-T015)
3. **Build MVP**: Complete US1 + US2 (T016-T040) for functional desk controller
4. **Hardware Setup**: Wire ESP32, VL53L5CX sensor, MOSFETs per quickstart.md
5. **Deploy & Test**: Upload firmware, configure WiFi, run calibration, validate user stories

**Branch**: `001-web-height-control` ready for development  
**Planning Status**: All phases complete ✓ (Phase 0, Phase 1, Phase 2)
