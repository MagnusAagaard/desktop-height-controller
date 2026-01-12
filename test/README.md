# Unit Tests for Desk Height Controller

This directory contains unit tests using PlatformIO's Unity framework.

## Test Organization

```
test/
├── test_moving_average_filter/    # MovingAverageFilter tests
├── test_height_controller/        # HeightController logic tests  
├── test_preset_manager/           # PresetManager tests
├── test_movement_controller/      # State machine tests
└── README.md                      # This file
```

## Running Tests

Run all tests:
```bash
pio test
```

Run specific test:
```bash
pio test -f test_moving_average_filter
```

Run with verbose output:
```bash
pio test -v
```

## Test Conventions

- Each module has its own test folder under `test/`
- Test files follow pattern: `test_<module>.cpp`
- Use descriptive test names that explain the behavior being tested
- Follow RED-GREEN-REFACTOR: Write failing test first, then implement

## Coverage Goals

Per constitution requirements:
- 80%+ coverage for HeightController, PresetManager, MovingAverageFilter
- Integration coverage for all sensor/MOSFET/WiFi code paths
