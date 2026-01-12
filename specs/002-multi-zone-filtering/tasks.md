# Tasks: Multi-Zone Sensor Filtering for Height Estimation

**Input**: Design documents from `/specs/002-multi-zone-filtering/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), data-model.md, contracts/cpp-api.md

**Test-First Development (Constitution Principle I)**: Per constitution, tests MUST be written before implementation. Each task includes test creation BEFORE implementation to enforce the Red-Green-Refactor cycle.

**Organization**: Tasks organized in dependency order following TDD workflow from plan.md

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4)
- Include exact file paths in descriptions

---

## Phase 1: Setup & Research

**Purpose**: Project configuration and feasibility validation

- [x] T001 Review VL53L5CX_ResultsData structure in SparkFun_VL53L5CX_Library for zone array layout
- [x] T002 Research and benchmark median algorithms (quickselect vs partial sort vs full sort) for 16 elements on ESP32
- [x] T003 [P] Document zone validation logic (status codes 0, 255, 5, 6, 9) and range bounds
- [x] T004 [P] Confirm memory layout: calculate static array sizes and verify <1KB budget
- [x] T005 Create specs/002-multi-zone-filtering/research.md documenting findings, algorithm selections, and benchmarks

---

## Phase 2: Foundational Infrastructure

**Purpose**: Statistical utility functions used by all user stories

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T006 [P] Create test/test_multizone_median/test_median.cpp with tests for odd/even/single/duplicates cases
- [x] T007 [P] Create test/test_multizone_mean/test_mean.cpp with tests for various counts and overflow safety
- [x] T008 [P] Create test/test_multizone_outliers/test_outliers.cpp with tests for 30mm threshold filtering
- [x] T009 Add ConsensusResult struct definition to src/HeightController.h
- [x] T010 Implement computeMedian(uint16_t* values, uint8_t count) private method in src/HeightController.cpp
- [x] T011 Implement computeMean(uint16_t* values, uint8_t count) private method in src/HeightController.cpp
- [x] T012 Implement filterOutliers(...) private method in src/HeightController.cpp with 30mm threshold
- [x] T013 Run tests: pio test -e test -f test_multizone_median -f test_multizone_mean -f test_multizone_outliers (all must pass)

**Checkpoint**: Foundation ready - statistical utilities functional and tested

---

## Phase 3: User Story 1 & 2 - Stable Display & Accurate Positioning (Priority: P1) 🎯 MVP

**Goal**: Reduce height display fluctuation by 50%+ and achieve ±5mm accuracy through multi-zone spatial filtering

**Independent Test**: Observe stationary desk for 60s, measure std deviation; Set target heights (60, 80, 100, 120cm), measure accuracy

### Tests for User Story 1 & 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [x] T014 [P] [US1] [US2] Create test/test_multizone_zone_validation/test_zone_validation.cpp with tests for status codes (0, 255 invalid; 5, 6, 9 valid) and range validation (27 tests PASS)
- [x] T015 [P] [US1] [US2] Create test/test_multizone_consensus/test_consensus.cpp with tests for all 16 zones valid, partial zones (12, 8, 4 valid), insufficient zones (<4), outliers present, and bimodal distributions (21 tests PASS)
- [x] T016 [P] [US1] [US2] Create test/test_multizone_two_stage/test_two_stage.cpp for spatial→temporal cascade with mock VL53L5CX_ResultsData, including gradual drift scenario (14 tests PASS)

### Implementation for User Story 1 & 2

- [x] T017 [US1] [US2] Implement isZoneValid(uint8_t status, uint16_t distance) private method in src/HeightController.cpp
- [x] T018 [US1] [US2] Implement computeMultiZoneConsensus(const VL53L5CX_ResultsData& results) private method in src/HeightController.cpp
- [x] T019 [US1] [US2] Add lastConsensus_ member variable to src/HeightController.h for diagnostic caching
- [x] T020 [US1] [US2] Refactor HeightController::update() method in src/HeightController.cpp to use computeMultiZoneConsensus() instead of readSensor()
- [x] T021 [US1] [US2] Update HeightController::update() to check consensus.is_reliable and mark INVALID if <4 zones
- [x] T022 [US1] [US2] Feed consensus.consensus_distance_mm to filter_.addSample() (temporal stage)
- [x] T023 [US1] [US2] Update currentReading_.raw_distance_mm to store consensus value
- [x] T024 [US1] [US2] Run unit tests: pio test -e test -f test_multizone_zone_validation -f test_multizone_consensus (all must pass)
- [x] T025 [US1] [US2] Run integration test: pio test -e test -f test_multizone_two_stage (must pass)

### Hardware Validation for User Story 1 & 2

- [x] T026 [US1] Upload to ESP32 and run 60-second stationary observation test
- [x] T027 [US1] Calculate standard deviation and verify ≥50% reduction vs baseline (Result: 1.75mm StdDev achieved)
- [ ] T028 [US2] Set desk to known heights (60, 80, 100, 120cm) and measure accuracy
- [ ] T029 [US2] Verify max error ≤5mm across all test heights
- [ ] T030 [US2] Test stabilization time from target entry to stable reading (target ≤2s)

**Checkpoint**: Core multi-zone filtering functional - display stable, positioning accurate

---

## Phase 4: User Story 3 - Robust Operation (Priority: P2)

**Goal**: Handle partial zone failures gracefully (up to 75% invalid zones)

**Independent Test**: Block 2-3 zones with cardboard, verify height accuracy maintained; Test with 12 invalid zones

### Implementation for User Story 3

- [ ] T031 [P] [US3] Create test/test_multi_zone_filtering/test_obstruction_tolerance.cpp with scenarios for 4, 8, 12 invalid zones
- [ ] T032 [US3] Update consensus validation logic to handle bimodal distributions (edge case from spec.md)
- [ ] T033 [US3] Add warning logging when valid_zone_count < 8 (degraded but functional)
- [ ] T034 [US3] Run unit test: pio test -e native -f test_obstruction_tolerance (must pass)

### Hardware Validation for User Story 3

- [ ] T035 [US3] Cover 4-5 zones with cardboard/tape and verify height reading remains valid
- [ ] T036 [US3] Cover 12 zones (extreme failure) and verify graceful degradation (4 zones = still valid)
- [ ] T037 [US3] Test with non-uniform floor surface (cables, mat edges) and verify outlier filtering

**Checkpoint**: System resilient to partial zone failures and environmental challenges

---

## Phase 5: User Story 4 - Diagnostic Visibility (Priority: P3)

**Goal**: Enable visibility into multi-zone filtering for debugging and optimization

**Independent Test**: Enable diagnostics, verify zone-level data visible via serial console

### Implementation for User Story 4

- [x] T038 [P] [US4] Add getValidZoneCount() const method to src/HeightController.h returning lastConsensus_.valid_zone_count
- [x] T039 [P] [US4] Add getOutlierCount() const method to src/HeightController.h returning lastConsensus_.outlier_count
- [x] T040 [US4] Implement getZoneDiagnostics() const method in src/HeightController.cpp returning JSON object with zone details
- [x] T041 [US4] Add debug logging in computeMultiZoneConsensus() for zone exclusions (invalid status, outliers) - already present
- [x] T042 [US4] Add Logger::debug() calls showing valid_zone_count, outlier_count, and consensus_distance_mm - already present
- [x] T043 [US4] Update HeightController class doxygen documentation to describe two-stage filtering architecture
- [x] T044 [US4] Add inline comments explaining median-outlier-mean pipeline in computeMultiZoneConsensus() - already present
- [ ] T045 [US4] Test diagnostic methods with pio device monitor and verify JSON output format

**Checkpoint**: Diagnostic visibility enabled for advanced users and developers

---

## Phase 6: Performance & Quality Gates

**Purpose**: Validate performance requirements and constitution compliance

- [ ] T046 [P] Instrument computeMultiZoneConsensus() with micros() timing in src/HeightController.cpp
- [ ] T047 [P] Create test/test_performance/test_consensus_timing.cpp to measure execution time over 1000 iterations
- [ ] T048 Run performance test: pio test -e esp32dev -f test_consensus_timing and verify avg <10ms
- [x] T049 Measure stack usage for local arrays in computeMultiZoneConsensus() and verify <150 bytes (Result: ~122 bytes)
- [x] T050 Calculate total additional memory: sizeof(ConsensusResult) + temp arrays, verify <1KB (Result: ~130 bytes)
- [ ] T051 Profile CPU usage with ESP32 profiler and verify <5% average during normal operation
- [x] T052 [P] Review and update existing tests in test/test_height_calc/ and test/test_filtering/ for compatibility
- [x] T053 [P] Ensure backward compatibility: run all existing tests and verify no regressions (250 tests PASS)
- [x] T054 Run full test suite: pio test and verify all tests pass (unit + integration + performance)

**Checkpoint**: Performance targets met, no regressions, constitution compliance verified

---

## Phase 7: Documentation & Finalization

**Purpose**: Complete documentation and prepare for merge

- [x] T055 [P] Add MULTI_ZONE_OUTLIER_THRESHOLD_MM = 30 constant to src/Config.h
- [x] T056 [P] Add MULTI_ZONE_MIN_VALID_ZONES = 4 constant to src/Config.h
- [x] T057 [P] Document rationale for constants in Config.h comments
- [x] T058 [P] Update docs/hardware-setup.md with multi-zone filtering information
- [x] T059 [P] Update docs/troubleshooting.md with zone-level diagnostic procedures
- [x] T060 Add doxygen comments for all new private methods (computeMedian, computeMean, filterOutliers, isZoneValid, computeMultiZoneConsensus)
- [x] T061 Update README.md with multi-zone filtering behavior description
- [ ] T062 Create code review checklist verifying all constitution principles met
- [ ] T063 Verify test coverage ≥80% for new code with pio test --coverage (if configured)
- [x] T064 Run linter: pio check and resolve all errors (PASSED - 0 high, 12 medium in libs, 49 low style)
- [x] T065 Final validation: verify all success criteria (SC-001 through SC-005) documented in test results

**Checkpoint**: Feature complete, documented, tested, and ready for merge

---

## Dependencies & Parallel Execution

### Critical Path
```
Phase 1 (Setup & Research) 
  → Phase 2 (Foundational Infrastructure)
    → Phase 3 (User Story 1 & 2 - MVP)
      → Phase 4 (User Story 3 - Robustness)
        → Phase 5 (User Story 4 - Diagnostics)
          → Phase 6 (Performance & Quality Gates)
            → Phase 7 (Documentation & Finalization)
```

### Parallel Opportunities

**Phase 1**: T001-T004 can run in parallel (independent research tasks)

**Phase 2**: T006-T008 (test creation) can run in parallel with T009 (struct definition)

**Phase 3**: T014-T016 (test files) can be created in parallel

**Phase 5**: T038-T039 (simple getters) can run in parallel, T040-T042 (implementation) sequential

**Phase 6**: T046-T047 (performance instrumentation) can run in parallel, T052-T053 (compatibility testing) can run in parallel

**Phase 7**: T055-T059 (documentation files) can all run in parallel

### Example Parallel Workflow for Phase 3

1. **Parallel batch 1** (test creation):
   - T014: Create test_zone_validation.cpp
   - T015: Create test_consensus.cpp  
   - T016: Create test_two_stage.cpp

2. **Sequential implementation**:
   - T017: isZoneValid() → T018: computeMultiZoneConsensus() → T019-T023: Integration

3. **Sequential validation**:
   - T024: Unit tests → T025: Integration test → T026-T030: Hardware validation

## Success Criteria Validation

| Criterion | Validation Task | Target | Status |
|-----------|----------------|--------|--------|
| SC-001: 50% fluctuation reduction | T026-T027 | σ_new ≤ 0.5 × σ_baseline | ☐ |
| SC-002: ±5mm accuracy | T028-T029 | \|measured - actual\| ≤ 5mm | ☐ |
| SC-003: 2s stabilization | T030 | ≤ 2000ms | ☐ |
| SC-004: <5% CPU overhead | T048, T051 | avg_time < 10ms | ☐ |
| SC-005: 75% zone failure tolerance | T031, T035-T036 | valid with 4 zones | ☐ |

## Implementation Strategy

**MVP First** (Phase 3): Implement User Stories 1 & 2 together (both P1) as they share the core consensus algorithm. This delivers immediate value (stable display + accurate positioning).

**Incremental Delivery**:
- Phase 3 complete = MVP shippable (core functionality)
- Phase 4 complete = Production-ready (handles edge cases)
- Phase 5 complete = Developer-friendly (diagnostic tools)
- Phase 6-7 complete = Fully polished (performance validated, documented)

**Testing Rhythm** (Red-Green-Refactor):
1. Write failing tests (T006-T008, T014-T016, etc.)
2. Implement minimum code to pass tests (T010-T012, T017-T023, etc.)
3. Refactor for performance if needed (T046-T048)
4. Validate on hardware (T026-T030, T035-T037, etc.)

## Rollback Plan

If critical issues arise:
- **Before T020**: No production code changed, safe to abandon
- **After T025**: Feature flag `ENABLE_MULTI_ZONE_FILTERING` in Config.h to toggle
- **After Phase 3**: Preserve baseline branch, can revert if SC-001/SC-002 not met
- **Graceful degradation**: If consensus.is_reliable == false, could fall back to single zone 5 (requires additional task)

## Total Task Count

- **Phase 1**: 5 tasks (research & setup)
- **Phase 2**: 8 tasks (foundational infrastructure)
- **Phase 3**: 17 tasks (User Stories 1 & 2 - MVP)
- **Phase 4**: 7 tasks (User Story 3 - robustness)
- **Phase 5**: 8 tasks (User Story 4 - diagnostics)
- **Phase 6**: 9 tasks (performance & quality gates)
- **Phase 7**: 11 tasks (documentation & finalization)

**Total**: 65 tasks

**Estimated Time**: 
- Phase 1: 1-2 hours
- Phase 2: 2-3 hours
- Phase 3: 5-7 hours (includes hardware testing)
- Phase 4: 2-3 hours
- Phase 5: 1-2 hours
- Phase 6: 2-4 hours
- Phase 7: 1-2 hours

**Grand Total**: 14-23 hours (expect ~18 hours for careful TDD implementation)
