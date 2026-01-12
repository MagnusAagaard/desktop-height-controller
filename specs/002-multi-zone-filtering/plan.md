# Implementation Plan: Multi-Zone Sensor Filtering for Height Estimation

**Branch**: `002-multi-zone-filtering` | **Date**: 2026-01-12 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-multi-zone-filtering/spec.md`

## Summary

Replace single-zone sensor reading with multi-zone spatial filtering to improve height estimation stability and accuracy. The VL53L5CX sensor provides 16 measurement zones (4×4 array); currently only zone 5 is used. Implement two-stage filtering: (1) spatial - compute median-filtered consensus across all valid zones, (2) temporal - apply existing moving average filter to consensus value. Target 50%+ reduction in display fluctuation and ±5mm accuracy.

## Technical Context

**Language/Version**: C++ (Arduino framework for ESP32)
**Primary Dependencies**: SparkFun_VL53L5CX_Library, existing MovingAverageFilter class
**Storage**: N/A (runtime filtering only, no persistence)
**Testing**: PlatformIO unit tests (native environment), hardware integration tests
**Target Platform**: ESP32 microcontroller (desktop height controller embedded system)
**Project Type**: Embedded firmware (single-threaded event loop, 5Hz sensor polling)
**Performance Goals**: <10ms processing time per update cycle, <5% CPU overhead, <1KB additional memory
**Constraints**: Must complete within existing 200ms update interval, preserve 5Hz sampling rate, backward compatible with existing calibration and height calculation
**Scale/Scope**: Single HeightController class modification, add multi-zone filtering logic, 16 zones × 2 bytes distance + status arrays

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Verify compliance with all principles defined in `.specify/memory/constitution.md`:

**I. Test-First Development**
- [x] Test strategy defined before implementation: Unit tests for filtering algorithm (median, mean, outlier detection), integration tests with mock sensor data, hardware tests for performance/accuracy validation
- [x] Coverage targets specified: 80%+ unit coverage for new filtering functions, 100% coverage for edge cases (all zones invalid, partial failures, bimodal distributions)
- [x] Red-Green-Refactor cycle plan documented: Write failing tests for multi-zone consensus → implement filtering → refactor for performance → validate against success criteria

**II. Code Quality Standards**
- [x] Linting/formatting tools configured: PlatformIO check (cppcheck, clangtidy), existing project standards
- [x] Code review process defined: PR review before merge, verify constitution compliance checklist
- [x] Documentation requirements identified: Inline doxygen comments for filtering functions, update HeightController class documentation, add diagnostic logging for zone-level visibility
- [x] Error handling strategy specified: Graceful degradation when <4 zones valid (mark INVALID), preserve existing error states, no silent failures

**III. User Experience Consistency**
- [x] Accessibility requirements defined: No user-facing UI code changes (this feature modifies HeightController sensor processing only; web UI consumes data via unchanged API). Diagnostic logging (US4) outputs to serial console, not accessible UI.
- [x] Feedback mechanisms specified: Diagnostic logging for zone exclusions/outliers (P3), existing height display updates maintain 200ms frequency
- [x] Error prevention measures identified: Validate zone count before consensus, preserve INVALID state when insufficient zones, maintain existing error semantics

**IV. Performance Requirements**
- [x] Response time targets: <10ms processing per update, maintain 200ms interval, 2-second stabilization
- [x] Resource usage limits: <1KB additional memory (zone arrays), <5% CPU overhead
- [x] Reliability requirements: 99.9% uptime preserved, graceful handling of 75% zone failures
- [x] Performance testing plan: Benchmark filtering algorithm execution time, measure memory footprint, profile CPU usage under load

**Quality Gates Assessment**:
- Constitution compliance: ✅ All principles addressed
- Complexity justification: ✅ Two-stage filtering necessary for spatial+temporal noise reduction
- Architecture impact: ✅ Contained within HeightController, preserves existing interfaces

## Phase 0: Research & Feasibility

*GATE: Resolve all NEEDS CLARIFICATION before proceeding to Phase 1.*

### Investigation Tasks

1. **Multi-Zone Data Access Patterns**
   - Review VL53L5CX_ResultsData structure for zone array layout
   - Confirm zone indexing (0-15 for 4×4), status codes per zone
   - Validate NB_TARGET_PER_ZONE multiplier usage for accessing arrays
   - Document zone ordering (row-major vs column-major)

2. **Median Calculation Performance**
   - Research efficient median algorithms for small arrays (≤16 elements)
   - Compare quickselect vs partial sort vs full sort for 16 elements
   - Benchmark candidate implementations on ESP32 (target <5ms)
   - Select algorithm balancing simplicity and performance

3. **Outlier Detection Strategy**
   - Validate 30mm threshold against sensor noise characteristics
   - Research median absolute deviation (MAD) vs simple median-based filtering
   - Confirm selected approach: median-based with 30mm threshold (per clarification)
   - Document rationale for threshold value

4. **Zone Validation Logic**
   - Review VL53L5CX status codes (0, 255 invalid; 5, 6, 9 valid variants)
   - Confirm range validation (SENSOR_MIN_VALID_MM, SENSOR_MAX_RANGE_MM)
   - Design validation function: isZoneValid(status, distance) → bool
   - Handle edge cases (all invalid, exactly 3 valid zones)

5. **Memory Layout Optimization**
   - Calculate static array sizes: uint16_t distances[16], uint8_t statuses[16]
   - Evaluate stack vs heap allocation for temporary arrays
   - Confirm <1KB budget: 16×2 + 16×1 = 48 bytes (well within limit)
   - Consider alignment/padding for ESP32 architecture

**Deliverable**: `research.md` documenting findings, algorithm choices, performance benchmarks, memory layout

## Phase 1: Design & Contracts

*GATE: Design must align with spec requirements and constitution. Update agent context after completion.*

### 1. Data Model

**Entities** (runtime structures):

```cpp
// Zone-level reading (transient, per-update)
struct ZoneReading {
    uint8_t zone_id;       // 0-15
    uint16_t distance_mm;  // Raw distance
    uint8_t status;        // Target status code
    bool is_valid;         // Passed validation
};

// Multi-zone consensus result
struct ConsensusResult {
    uint16_t consensus_distance_mm;  // Median-filtered mean
    uint8_t valid_zone_count;        // How many zones contributed
    uint8_t outlier_count;           // How many excluded as outliers
    bool is_reliable;                // >= 4 valid zones
};
```

**Relationships**:
- VL53L5CX_ResultsData → 16 ZoneReadings (extraction + validation)
- Valid ZoneReadings → ConsensusResult (outlier filtering + mean)
- ConsensusResult.consensus_distance_mm → MovingAverageFilter (temporal stage)
- Filtered value → HeightReading.calculated_height_cm (existing flow)

**State Transitions**:
- Raw zones → Validated zones (status + range checks)
- Validated zones → Outlier-filtered zones (median ± 30mm threshold)
- Filtered zones → Consensus mean
- Consensus → Temporal filter → Height

### 2. API Contracts

**Modified HeightController Interface**:

```cpp
class HeightController {
public:
    // Existing interface preserved (backward compatible)
    bool init();
    void update();  // Internal implementation changes
    uint16_t getCurrentHeight() const;
    uint16_t getRawDistance() const;      // Now returns consensus, not zone 5
    uint16_t getFilteredDistance() const; // After temporal filter
    bool isValid() const;
    
    // New diagnostic methods (P3 - optional)
    uint8_t getValidZoneCount() const;      // For debugging
    uint8_t getOutlierCount() const;        // For diagnostics
    String getZoneDiagnostics() const;      // JSON array of zone data
    
private:
    // New private methods
    ConsensusResult computeMultiZoneConsensus(const VL53L5CX_ResultsData& results);
    bool isZoneValid(uint8_t status, uint16_t distance) const;
    uint16_t computeMedian(uint16_t* values, uint8_t count);
    uint16_t computeMean(uint16_t* values, uint8_t count);
    void filterOutliers(uint16_t* values, uint8_t count, uint16_t median, 
                        bool* keep_flags, uint8_t& kept_count);
    
    // New member variables
    ConsensusResult lastConsensus_;  // Cache for diagnostics
};
```

**Internal Data Flow** (update() method):

```
1. sensor_.getRangingData(&results) → VL53L5CX_ResultsData
2. computeMultiZoneConsensus(results) → ConsensusResult
   2a. Extract 16 zones, validate each → valid_zones[]
   2b. If valid_count < 4: return unreliable
   2c. Compute median of valid_zones
   2d. Filter outliers (>30mm from median)
   2e. Compute mean of non-outliers → consensus_distance_mm
3. Check ConsensusResult.is_reliable
   3a. If false: mark reading INVALID, return
   3b. If true: proceed with consensus value
4. filter_.addSample(consensus_distance_mm)  // Temporal stage
5. filtered_mm = filter_.getAverage()
6. calculated_height_cm = calculateHeight(filtered_mm)  // Existing
```

### 3. Component Architecture

**Modified Components**:

1. **HeightController** (src/HeightController.{h,cpp})
   - Replace readSensor() single-zone logic with multi-zone consensus
   - Add computeMultiZoneConsensus() private method
   - Add helper methods for median, mean, outlier filtering
   - Cache ConsensusResult for diagnostic access
   - Update documentation to reflect two-stage filtering

2. **Config.h** (if needed)
   - Add MULTI_ZONE_OUTLIER_THRESHOLD_MM = 30
   - Add MULTI_ZONE_MIN_VALID_ZONES = 4
   - Keep existing SENSOR_MIN_VALID_MM, SENSOR_MAX_RANGE_MM

**Unchanged Components**:
- MovingAverageFilter (reused as-is for temporal stage)
- SystemConfiguration (calibration unchanged)
- WebServer, MovementController (consume height via existing API)

**New Utilities** (if needed):
- MedianFilter utility class (or inline implementation in HeightController)
- Statistical functions (median, mean) - possibly extracted to utils/Statistics.{h,cpp}

### 4. Test Strategy

**Unit Tests** (test/test_multi_zone_filtering/):

```cpp
// test_median_calculation.cpp
- test_median_odd_count()   // 5 values
- test_median_even_count()  // 4 values
- test_median_single()      // 1 value
- test_median_duplicates()  // Multiple same values

// test_outlier_detection.cpp
- test_outlier_filtering_30mm_threshold()
- test_no_outliers_when_all_close()
- test_all_outliers_except_one()
- test_bimodal_distribution()  // Half at 500mm, half at 600mm

// test_consensus_calculation.cpp
- test_consensus_all_zones_valid()
- test_consensus_partial_zones_12_valid()
- test_consensus_minimum_zones_4_valid()
- test_consensus_insufficient_zones_3_valid()  // Should fail
- test_consensus_all_zones_invalid()            // Should fail
- test_consensus_with_outliers_excluded()

// test_zone_validation.cpp
- test_zone_valid_status_5()
- test_zone_valid_status_6_and_9()
- test_zone_invalid_status_0_and_255()
- test_zone_invalid_distance_too_low()
- test_zone_invalid_distance_too_high()
```

**Integration Tests** (test/test_integration_filtering/):

```cpp
// test_two_stage_filtering.cpp
- test_spatial_then_temporal_cascade()
- test_fluctuation_reduction_vs_baseline()
- test_rapid_movement_response()
- test_stationary_stability()
```

**Hardware Tests** (manual/semi-automated):

- Accuracy validation: Measure at known heights (60, 80, 100, 120cm)
- Stability test: 60-second observation, measure std deviation
- Obstruction tolerance: Block 2-3 zones, verify height accuracy
- Performance profiling: Measure execution time with oscilloscope/timer

**Test Execution Order** (TDD):
1. Write failing unit tests for median/mean/outlier functions
2. Implement statistical functions, pass tests
3. Write failing tests for zone validation and consensus
4. Implement consensus logic, pass tests
5. Write integration tests for two-stage cascade
6. Integrate into HeightController.update(), pass integration tests
7. Run hardware tests to validate success criteria

### 5. Success Criteria Mapping

| Criterion | Validation Method | Target |
|-----------|-------------------|--------|
| SC-001: 50% fluctuation reduction | Hardware test: 60s stationary observation, compare std dev to baseline | σ_new ≤ 0.5 × σ_baseline |
| SC-002: ±5mm accuracy | Hardware test: Measure at 4 known heights, compare error | \|measured - actual\| ≤ 5mm |
| SC-003: 2s stabilization | Integration test: Time from entry to tolerance until stable | ≤ 2000ms |
| SC-004: <5% CPU overhead | Performance profiling: Measure update() execution time | avg_time < 10ms (5% of 200ms) |
| SC-005: 75% zone failure tolerance | Unit test: Feed consensus with 12 invalid zones | is_reliable == true with 4 valid |

### 6. Agent Context Update

After completing design, run:
```bash
.specify/scripts/bash/update-agent-context.sh copilot
```

Add to agent context:
- Multi-zone filtering architecture (spatial → temporal cascade)
- C++ statistical utilities (median, mean, outlier detection)
- ESP32 performance constraints (<10ms, <1KB memory)
- VL53L5CX 4×4 zone array handling

## Phase 2: Implementation Tasks

*GATE: All Phase 1 artifacts complete. Constitution re-verified post-design.*

### Task Breakdown (TDD Order)

**Epic 1: Statistical Utilities** (Foundation)

- [ ] **Task 1.1**: Create test/test_multi_zone_filtering/test_median.cpp
  - Write tests for median calculation (odd, even, single, duplicates)
  - Expected outcome: 4 failing tests
  
- [ ] **Task 1.2**: Implement median calculation in HeightController
  - Add private method: uint16_t computeMedian(uint16_t* values, uint8_t count)
  - Use quickselect or partial sort for n=16
  - Pass all median tests
  
- [ ] **Task 1.3**: Create test/test_multi_zone_filtering/test_outliers.cpp
  - Write tests for outlier filtering with 30mm threshold
  - Include edge cases (all outliers, none, bimodal)
  - Expected outcome: 4 failing tests
  
- [ ] **Task 1.4**: Implement outlier filtering
  - Add private method: void filterOutliers(...)
  - Apply median ± 30mm logic, populate keep_flags
  - Pass all outlier tests
  
- [ ] **Task 1.5**: Implement mean calculation
  - Add private method: uint16_t computeMean(uint16_t* values, uint8_t count)
  - Simple sum/count, handle uint32_t overflow
  - Add tests and pass

**Epic 2: Zone Validation & Consensus** (Core Logic)

- [ ] **Task 2.1**: Create test/test_multi_zone_filtering/test_zone_validation.cpp
  - Write tests for status code validation (0, 255 invalid; 5, 6, 9 valid)
  - Write tests for range validation (min/max)
  - Expected outcome: 5 failing tests
  
- [ ] **Task 2.2**: Implement zone validation
  - Add private method: bool isZoneValid(uint8_t status, uint16_t distance)
  - Check status codes and range bounds
  - Pass all validation tests
  
- [ ] **Task 2.3**: Create test/test_multi_zone_filtering/test_consensus.cpp
  - Write tests for consensus with varying valid zone counts (4, 8, 12, 16)
  - Write tests for insufficient zones (<4)
  - Write tests with outliers present
  - Expected outcome: 6 failing tests
  
- [ ] **Task 2.4**: Implement computeMultiZoneConsensus()
  - Extract 16 zones from VL53L5CX_ResultsData
  - Validate each zone, collect valid distances
  - Check valid_count >= 4, return unreliable if not
  - Compute median, filter outliers, compute mean
  - Return ConsensusResult
  - Pass all consensus tests

**Epic 3: Integration with HeightController** (Wiring)

- [ ] **Task 3.1**: Update HeightController.h
  - Add ConsensusResult struct definition
  - Declare new private methods (consensus, median, mean, outliers, validation)
  - Add lastConsensus_ member for caching
  - Add diagnostic methods (getValidZoneCount, etc.)
  
- [ ] **Task 3.2**: Refactor readSensor() to use multi-zone
  - Rename readSensor() to computeMultiZoneConsensus() (or keep wrapper)
  - Replace zone-5-only logic with full zone iteration
  - Return consensus_distance_mm instead of single zone
  - Update HeightController::update() to use consensus
  
- [ ] **Task 3.3**: Create test/test_integration_filtering/test_two_stage.cpp
  - Mock VL53L5CX_ResultsData with known zone values
  - Feed through consensus → temporal filter → height
  - Verify cascade behavior
  - Expected outcome: Failing integration test
  
- [ ] **Task 3.4**: Wire consensus into update() flow
  - Call computeMultiZoneConsensus() in update()
  - Check is_reliable, mark INVALID if false
  - Feed consensus_distance_mm to filter_.addSample()
  - Existing temporal filter and height calculation unchanged
  - Pass integration test

**Epic 4: Diagnostic & Logging** (P3 - Optional but Recommended)

- [ ] **Task 4.1**: Implement diagnostic methods
  - getValidZoneCount(): return lastConsensus_.valid_zone_count
  - getOutlierCount(): return lastConsensus_.outlier_count
  - getZoneDiagnostics(): JSON array with zone distances, statuses, validity
  
- [ ] **Task 4.2**: Add debug logging for zone exclusions
  - Log when zones marked invalid (status, range)
  - Log when zones excluded as outliers
  - Use Logger::debug() to avoid spam, or reduce frequency
  
- [ ] **Task 4.3**: Update documentation
  - Doxygen comments for all new methods
  - Update HeightController class doc to describe two-stage filtering
  - Add inline comments explaining median-outlier-mean pipeline

**Epic 5: Performance & Validation** (Quality Gates)

- [ ] **Task 5.1**: Profile consensus computation time
  - Instrument computeMultiZoneConsensus() with micros()
  - Run 1000 iterations, measure average execution time
  - Target: <10ms average
  - If over budget: Optimize median algorithm or reduce logging
  
- [ ] **Task 5.2**: Measure memory footprint
  - Check stack usage for local arrays (distances[16], statuses[16])
  - Verify total <1KB: sizeof(ConsensusResult) + temp arrays
  - Use ESP32 memory profiler or manual calculation
  
- [ ] **Task 5.3**: Hardware accuracy test
  - Set desk to known heights (60, 80, 100, 120cm)
  - Record 10 readings at each height
  - Calculate mean error and max error
  - Target: ±5mm max error
  
- [ ] **Task 5.4**: Hardware stability test
  - Park desk at mid-height (90cm)
  - Record height readings every 200ms for 60 seconds
  - Calculate standard deviation
  - Compare to baseline (pre-multi-zone)
  - Target: ≥50% reduction in σ
  
- [ ] **Task 5.5**: Obstruction tolerance test
  - Block 4-5 zones with cardboard/tape
  - Verify height reading remains valid (≥4 zones unobstructed)
  - Move to extreme zone failure (12 invalid)
  - Verify graceful degradation (still valid with 4 zones)
  
- [ ] **Task 5.6**: Update existing tests
  - Review test/test_height_calc/ and test/test_filtering/
  - Update any tests that mock single-zone behavior
  - Ensure backward compatibility tests still pass

**Epic 6: Documentation & Cleanup**

- [ ] **Task 6.1**: Update README or docs/
  - Document multi-zone filtering behavior
  - Explain two-stage cascade (spatial + temporal)
  - Note diagnostic methods for advanced users
  
- [ ] **Task 6.2**: Update Config.h constants
  - Add MULTI_ZONE_OUTLIER_THRESHOLD_MM = 30
  - Add MULTI_ZONE_MIN_VALID_ZONES = 4
  - Document rationale in comments
  
- [ ] **Task 6.3**: Code review checklist
  - Verify all constitution principles met
  - Check test coverage (80%+ unit, 100% edge cases)
  - Validate performance targets (SC-004)
  - Confirm backward compatibility (existing API unchanged)
  
- [ ] **Task 6.4**: Final validation
  - Run full test suite (unit + integration + hardware)
  - Verify all success criteria (SC-001 through SC-005)
  - Confirm no regressions in baseline functionality
  - Sign off on constitution compliance

### Dependency Graph

```
Epic 1 (Statistical Utilities)
  └─> Epic 2 (Zone Validation & Consensus)
      └─> Epic 3 (Integration with HeightController)
          ├─> Epic 4 (Diagnostic & Logging)
          └─> Epic 5 (Performance & Validation)
              └─> Epic 6 (Documentation & Cleanup)
```

**Critical Path**: Epic 1 → Epic 2 → Epic 3 → Epic 5.3-5.5 (Hardware tests validate success criteria)

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Median calculation >10ms | Medium | High | Pre-research algorithms (Phase 0), benchmark on target hardware, use quickselect instead of full sort |
| Insufficient zones in real conditions | Low | High | Test with partial obstructions (Epic 5.5), confirm 4-zone minimum sufficient, add warning logging |
| Temporal filter window size mismatch | Low | Medium | Existing window size (5 samples) works with single zone, should improve with consensus input |
| Breaking existing height API | Low | High | Maintain backward compatibility (Epic 3.4), comprehensive regression tests (Epic 5.6) |
| Performance regression on ESP32 | Medium | High | Profile early (Epic 5.1), optimize hot paths, consider lookup tables if needed |
| Bimodal floor surfaces (half zones high, half low) | Medium | Medium | Outlier filtering handles ±30mm, test explicitly (Epic 2.3), may need wider threshold if issues arise |

## Rollback Plan

If critical issues arise during implementation:

1. **Preserve baseline branch**: Keep current single-zone implementation in separate branch
2. **Feature flag**: Add compile-time flag `ENABLE_MULTI_ZONE_FILTERING` to toggle behavior
3. **Graceful degradation**: If consensus fails (unreliable), fall back to single zone 5
4. **Revert path**: PR can be reverted if success criteria not met after Epic 5

## Post-Implementation

**Monitoring** (if deployed):
- Track height reading validity rate (should remain >99%)
- Monitor average execution time in production
- Log zone failure patterns to identify hardware issues

**Future Enhancements** (out of scope):
- Adaptive outlier threshold based on variance
- Zone weighting (prefer center zones over edges)
- Machine learning outlier detection
- 8×8 resolution support (64 zones)

**Documentation Handoff**:
- Update troubleshooting.md with multi-zone diagnostic procedures
- Add calibration.md note about multi-zone improving accuracy
- Include zone heatmap visualization in future web UI (P4+)
