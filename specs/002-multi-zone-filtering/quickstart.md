# Quickstart: Multi-Zone Sensor Filtering Implementation

**Feature**: Multi-Zone Sensor Filtering for Height Estimation  
**Branch**: `002-multi-zone-filtering`  
**For**: Developers implementing this feature

## TL;DR

Replace single-zone (zone 5) sensor reading with 16-zone spatial consensus + existing temporal filter for improved stability and accuracy.

**Key Changes**:
- `readSensor()` → `computeMultiZoneConsensus()` (all 16 zones)
- Outlier filtering: median ± 30mm threshold
- Consensus: mean of non-outlier zones
- Minimum 4 zones required (handles 75% failures)
- Two-stage cascade: spatial (multi-zone) → temporal (moving average)

## Prerequisites

- Existing HeightController with VL53L5CX sensor configured for 4×4 resolution
- MovingAverageFilter utility class functional
- PlatformIO test environment set up
- Access to hardware for validation testing

## Implementation Steps (TDD Order)

### Phase 0: Research (1-2 hours)

1. **Review sensor data structure**:
   ```bash
   # Find VL53L5CX_ResultsData definition in library
   grep -r "VL53L5CX_ResultsData" .pio/libdeps/
   ```
   - Confirm zone array indexing
   - Document status code meanings

2. **Benchmark median algorithms**:
   - Test quickselect vs std::nth_element vs manual partial sort
   - Target <5ms for 16 elements on ESP32
   - Document choice in research.md

3. **Create `specs/002-multi-zone-filtering/research.md`**:
   - Algorithm selections
   - Performance benchmarks
   - Memory layout decisions

### Phase 1: Statistical Utilities (2-3 hours)

**Test-first workflow**:

1. **Create test file**:
   ```bash
   mkdir -p test/test_multi_zone_filtering
   touch test/test_multi_zone_filtering/test_median.cpp
   ```

2. **Write failing tests**:
   ```cpp
   #include <unity.h>
   #include "HeightController.h"
   
   void test_median_odd_count() {
       uint16_t values[] = {800, 850, 840, 860, 845};
       uint16_t median = computeMedian(values, 5);
       TEST_ASSERT_EQUAL_UINT16(845, median);
   }
   
   void test_median_even_count() {
       uint16_t values[] = {800, 850, 840, 860};
       uint16_t median = computeMedian(values, 4);
       TEST_ASSERT_EQUAL_UINT16(840, median);  // Lower middle
   }
   ```

3. **Run tests** (should fail):
   ```bash
   pio test -e native -f test_median
   ```

4. **Implement in HeightController.h/cpp**:
   ```cpp
   // HeightController.h (private section)
   uint16_t computeMedian(uint16_t* values, uint8_t count);
   uint16_t computeMean(uint16_t* values, uint8_t count);
   void filterOutliers(uint16_t* values, uint8_t count, uint16_t median,
                       bool* keep_flags, uint8_t& kept_count);
   ```

5. **Pass tests**:
   ```bash
   pio test -e native -f test_median
   # All tests should pass
   ```

6. **Repeat for outliers and mean** (test_outliers.cpp, test_mean.cpp)

### Phase 2: Zone Validation & Consensus (3-4 hours)

1. **Add data structures to HeightController.h**:
   ```cpp
   struct ConsensusResult {
       uint16_t consensus_distance_mm;
       uint8_t valid_zone_count;
       uint8_t outlier_count;
       bool is_reliable;
   };
   ```

2. **Write test_zone_validation.cpp**:
   ```cpp
   void test_zone_valid_status_5() {
       HeightController hc;
       bool valid = hc.isZoneValid(5, 850);  // Status 5, normal distance
       TEST_ASSERT_TRUE(valid);
   }
   
   void test_zone_invalid_status_0() {
       HeightController hc;
       bool valid = hc.isZoneValid(0, 850);  // No target
       TEST_ASSERT_FALSE(valid);
   }
   ```

3. **Implement `isZoneValid()`**:
   ```cpp
   bool HeightController::isZoneValid(uint8_t status, uint16_t distance) const {
       if (status == 0 || status == 255) return false;
       if (distance < SENSOR_MIN_VALID_MM || distance > SENSOR_MAX_RANGE_MM) {
           return false;
       }
       return true;
   }
   ```

4. **Write test_consensus.cpp** with edge cases:
   - All 16 zones valid
   - Exactly 4 zones valid (minimum)
   - 3 zones valid (should fail: unreliable)
   - Outliers present

5. **Implement `computeMultiZoneConsensus()`** (see contracts/cpp-api.md)

6. **Pass all consensus tests**

### Phase 3: Integration (2-3 hours)

1. **Update HeightController.h**:
   ```cpp
   private:
       ConsensusResult lastConsensus_;  // Cache for diagnostics
       ConsensusResult computeMultiZoneConsensus(const VL53L5CX_ResultsData& results);
   ```

2. **Modify `update()` method**:
   ```cpp
   void HeightController::update() {
       // ... existing checks ...
       
       VL53L5CX_ResultsData results;
       if (!sensor_.getRangingData(&results)) {
           currentReading_.validity = ReadingValidity::INVALID;
           return;
       }
       
       // NEW: Multi-zone consensus
       ConsensusResult consensus = computeMultiZoneConsensus(results);
       lastConsensus_ = consensus;
       
       if (!consensus.is_reliable) {
           currentReading_.validity = ReadingValidity::INVALID;
           return;
       }
       
       currentReading_.raw_distance_mm = consensus.consensus_distance_mm;
       currentReading_.timestamp_ms = millis();
       
       // EXISTING: Temporal filter
       filter_.addSample(consensus.consensus_distance_mm);
       currentReading_.filtered_distance_mm = filter_.getAverage();
       currentReading_.calculated_height_cm = calculateHeight(filtered_distance_mm);
       currentReading_.validity = ReadingValidity::VALID;
   }
   ```

3. **Write integration test** (test_two_stage_filtering.cpp):
   - Mock VL53L5CX_ResultsData
   - Verify spatial → temporal cascade
   - Compare fluctuation to baseline

4. **Run full test suite**:
   ```bash
   pio test -e native
   ```

### Phase 4: Diagnostics (Optional, 1-2 hours)

1. **Add diagnostic methods**:
   ```cpp
   uint8_t HeightController::getValidZoneCount() const {
       return lastConsensus_.valid_zone_count;
   }
   
   uint8_t HeightController::getOutlierCount() const {
       return lastConsensus_.outlier_count;
   }
   
   String HeightController::getZoneDiagnostics() const {
       // Build JSON with zone details
       // See contracts/cpp-api.md for format
   }
   ```

2. **Add debug logging in `computeMultiZoneConsensus()`**:
   ```cpp
   Logger::debug(TAG, "Multi-zone: %d valid, %d outliers, consensus: %dmm",
                 consensus.valid_zone_count, consensus.outlier_count,
                 consensus.consensus_distance_mm);
   ```

### Phase 5: Hardware Validation (2-4 hours)

1. **Upload to ESP32**:
   ```bash
   pio run -t upload
   pio device monitor
   ```

2. **Accuracy test**:
   - Set desk to 60, 80, 100, 120cm (measure with tape measure)
   - Record height readings
   - Calculate error: |measured - actual|
   - Target: ±5mm max error

3. **Stability test**:
   - Park desk at 90cm
   - Log height readings for 60 seconds
   - Calculate std deviation
   - Compare to baseline (pre-multi-zone)
   - Target: ≥50% reduction

4. **Obstruction test**:
   - Cover 4-5 zones with cardboard
   - Verify height remains valid
   - Cover 12 zones (extreme)
   - Verify graceful degradation

5. **Performance profiling**:
   ```cpp
   unsigned long start = micros();
   ConsensusResult consensus = computeMultiZoneConsensus(results);
   unsigned long duration = micros() - start;
   Logger::info(TAG, "Consensus time: %lu μs", duration);
   ```
   - Target: <10ms (10,000 μs) average

### Phase 6: Documentation & Cleanup (1 hour)

1. **Update Config.h**:
   ```cpp
   #define MULTI_ZONE_OUTLIER_THRESHOLD_MM 30
   #define MULTI_ZONE_MIN_VALID_ZONES 4
   ```

2. **Add doxygen comments** to all new methods

3. **Update HeightController class documentation**

4. **Run linter**:
   ```bash
   pio check
   ```

5. **Final test run**:
   ```bash
   pio test
   # All tests must pass
   ```

## Verification Checklist

Before marking complete:

- [ ] All unit tests pass (median, mean, outliers, validation, consensus)
- [ ] Integration tests pass (two-stage cascade)
- [ ] Hardware accuracy test: ±5mm at 4 heights
- [ ] Hardware stability test: ≥50% fluctuation reduction
- [ ] Obstruction tolerance: valid with 4+ zones
- [ ] Performance: <10ms average consensus time
- [ ] Code coverage: ≥80% for new code
- [ ] Linter clean (no errors)
- [ ] Documentation complete (doxygen, inline comments)
- [ ] Backward compatibility: existing API unchanged

## Common Issues & Solutions

**Issue**: Median calculation >10ms
- Solution: Use quickselect instead of full sort, or std::nth_element

**Issue**: All zones marked invalid
- Check sensor initialization (4×4 resolution set?)
- Verify I2C communication
- Check status code interpretation

**Issue**: Frequent unreliable consensus (<4 zones)
- Review zone validation logic
- Check SENSOR_MIN_VALID_MM / SENSOR_MAX_RANGE_MM values
- Inspect sensor mounting (obstructions?)

**Issue**: No improvement in stability
- Verify temporal filter still active after consensus
- Check outlier threshold (30mm may need tuning)
- Ensure consensus fed to MovingAverageFilter correctly

**Issue**: Breaking existing height calculation
- Verify `calculateHeight()` unchanged
- Check `getRawDistance()` semantics (consensus, not zone 5)
- Ensure calibration formula still applied

## Performance Optimization Tips

1. **Median**: Use partial sort or nth_element (O(n) average vs O(n log n))
2. **Avoid heap allocation**: Use stack arrays for zone data
3. **Reduce logging**: Debug logs only, or reduce frequency
4. **Early exit**: Check `valid_count < 4` before median calculation
5. **Cache median**: Don't recompute for outlier filtering

## Testing Commands

```bash
# Run all tests
pio test

# Run specific test suite
pio test -f test_multi_zone_filtering

# Run on hardware (ESP32)
pio test -e esp32dev

# With verbose output
pio test -v

# Coverage report (if configured)
pio test --coverage
```

## Success Criteria Validation

Use this table to track progress against spec requirements:

| SC-001 | Fluctuation ≥50% reduction | Test: 60s stationary observation | ☐ Pass ☐ Fail |
| SC-002 | Accuracy ±5mm | Test: 4 known heights | ☐ Pass ☐ Fail |
| SC-003 | Stabilization ≤2s | Test: Time to stable | ☐ Pass ☐ Fail |
| SC-004 | CPU <5% overhead | Test: Execution time <10ms | ☐ Pass ☐ Fail |
| SC-005 | 75% zone failure tolerance | Test: 12 invalid zones | ☐ Pass ☐ Fail |

## Next Steps After Implementation

1. Monitor production performance (if deployed)
2. Collect zone failure statistics for hardware diagnostics
3. Consider future enhancements (adaptive thresholds, zone weighting)
4. Update troubleshooting.md with multi-zone diagnostic procedures

## References

- [spec.md](./spec.md) - Feature specification
- [plan.md](./plan.md) - Detailed implementation plan
- [data-model.md](./data-model.md) - Data structures and relationships
- [contracts/cpp-api.md](./contracts/cpp-api.md) - Internal API contract
- Constitution: `.specify/memory/constitution.md`
