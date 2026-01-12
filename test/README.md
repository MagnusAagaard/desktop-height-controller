# Unit Tests for Desk Height Controller

This directory contains unit tests using PlatformIO's Unity framework.

## Test Organization

```
test/
├── test_filtering/                # Filtering pipeline tests
├── test_height_calc/              # Height calculation tests
├── test_movement_controller/      # State machine tests
├── test_moving_average/           # MovingAverageFilter tests
├── test_multizone_*/              # Multi-zone filtering tests
├── test_preset_*/                 # PresetManager tests
├── test_safety_*/                 # Safety mechanism tests
├── test_webserver_*/              # WebServer API tests
└── README.md                      # This file
```

## Running Tests

### ESP32 Tests (Full Hardware Simulation)
Run all tests on ESP32 hardware/emulator (~3.5 minutes):
```bash
pio test -e esp32dev
```

### Native Tests (Fast, No Hardware)
Run all tests natively (~10 seconds, 25x faster):
```bash
pio test -e native
```

### Specific Test
Run specific test folder:
```bash
pio test -e native -f test_moving_average
```

### Verbose Output
```bash
pio test -e native -v
```

## Code Coverage

### Running Tests with Coverage
Use the `native_coverage` environment to collect coverage data:
```bash
pio test -e native_coverage
```

### Generating Coverage Report
After running tests with coverage, generate HTML report:
```bash
./scripts/generate_coverage.sh
```

This creates an HTML report at:
`.pio/build/native_coverage/coverage/html/index.html`

Open in browser:
```bash
xdg-open .pio/build/native_coverage/coverage/html/index.html
```

### Current Coverage
- **Line coverage**: 96.6% (2171/2248 lines)
- **Function coverage**: 97.1% (332/342 functions)

## Test Environments

| Environment | Platform | Tests | Time | Use Case |
|------------|----------|-------|------|----------|
| `native` | Linux/Mac | 240 | ~10s | Fast iteration, CI |
| `native_coverage` | Linux/Mac | 240 | ~10s | Coverage reports |
| `esp32dev` | ESP32 | 250 | ~3.5m | Hardware validation |

Note: 10 tests are ESP32-only (require `millis()`/`delay()`).

## Test Conventions

- Each module has its own test folder under `test/`
- Test files follow pattern: `test_<module>.cpp`
- Use descriptive test names that explain the behavior being tested
- Follow RED-GREEN-REFACTOR: Write failing test first, then implement
- Tests use `#ifdef NATIVE_TEST` for platform-specific code

## Coverage Goals

Per constitution requirements:
- 80%+ coverage for HeightController, PresetManager, MovingAverageFilter
- Integration coverage for all sensor/MOSFET/WiFi code paths

