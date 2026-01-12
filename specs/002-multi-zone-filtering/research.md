# Research: Multi-Zone Sensor Filtering

**Feature**: Multi-Zone Sensor Filtering for Height Estimation  
**Branch**: `002-multi-zone-filtering`  
**Date**: 2026-01-12

## 1. VL53L5CX Sensor Data Structure

### VL53L5CX_ResultsData Structure

From SparkFun_VL53L5CX_Library (`vl53l5cx_api.h` lines 337-398):

```cpp
typedef struct {
    uint32_t ambient_per_spad[VL53L5CX_RESOLUTION_8X8];       // 64 zones max
    uint8_t nb_target_detected[VL53L5CX_RESOLUTION_8X8];      // 64 zones max
    uint32_t nb_spads_enabled[VL53L5CX_RESOLUTION_8X8];       // 64 zones max
    uint32_t signal_per_spad[(VL53L5CX_RESOLUTION_8X8 * VL53L5CX_NB_TARGET_PER_ZONE)];
    uint16_t range_sigma_mm[(VL53L5CX_RESOLUTION_8X8 * VL53L5CX_NB_TARGET_PER_ZONE)];
    int16_t distance_mm[(VL53L5CX_RESOLUTION_8X8 * VL53L5CX_NB_TARGET_PER_ZONE)];
    uint8_t reflectance[(VL53L5CX_RESOLUTION_8X8 * VL53L5CX_NB_TARGET_PER_ZONE)];
    uint8_t target_status[(VL53L5CX_RESOLUTION_8X8 * VL53L5CX_NB_TARGET_PER_ZONE)];
    // ... motion indicator data
} VL53L5CX_ResultsData;
```

### Key Constants (from `platform.h` and `vl53l5cx_api.h`)

- `VL53L5CX_RESOLUTION_4X4 = 16` (16 zones in 4×4 mode)
- `VL53L5CX_RESOLUTION_8X8 = 64` (64 zones in 8×8 mode)  
- `VL53L5CX_NB_TARGET_PER_ZONE = 1` (single target per zone, default config)

### Zone Array Access Pattern

For 4×4 resolution mode (our configuration):
- Zone indices: 0-15
- Array access: `results.distance_mm[zone_id * VL53L5CX_NB_TARGET_PER_ZONE]`
- Since NB_TARGET_PER_ZONE=1: `results.distance_mm[zone_id]`
- Status access: `results.target_status[zone_id]`

### Zone Layout (4×4 Grid)

```
Row-major ordering (top-left origin):
+----+----+----+----+
| 0  | 1  | 2  | 3  |  Row 0
+----+----+----+----+
| 4  | 5  | 6  | 7  |  Row 1  (zone 5 = current center)
+----+----+----+----+
| 8  | 9  | 10 | 11 |  Row 2  (zones 5,6,9,10 = center cluster)
+----+----+----+----+
| 12 | 13 | 14 | 15 |  Row 3
+----+----+----+----+
```

**Center zones** (most reliable for floor measurement): 5, 6, 9, 10

### Distance Data Type

- `distance_mm` is `int16_t` (signed 16-bit) - can represent negative values for errors
- Valid range: 0 to 4000mm (SENSOR_MAX_RANGE_MM)
- Our code casts to `uint16_t` after validation

---

## 2. Target Status Codes

From VL53L5CX documentation and empirical observation:

| Status | Meaning | Valid? |
|--------|---------|--------|
| 0 | No target detected | ❌ Invalid |
| 5 | Valid measurement (100% confidence) | ✅ Valid |
| 6 | Valid measurement (50% confidence, wrap around) | ✅ Valid |
| 9 | Valid measurement (with sigma failure) | ✅ Valid |
| 255 | Invalid measurement | ❌ Invalid |
| 1-4, 7-8 | Undefined/untested | ⚠️ Reject (conservative) |
| 10+ | Undefined | ⚠️ Reject (conservative) |

### Validation Logic

```cpp
bool isZoneValid(uint8_t status, uint16_t distance) {
    // Reject explicit invalid codes
    if (status == 0 || status == 255) return false;
    
    // Accept only documented high-confidence codes
    if (status != 5 && status != 6 && status != 9) return false;
    
    // Range validation
    if (distance < SENSOR_MIN_VALID_MM) return false;
    if (distance > SENSOR_MAX_RANGE_MM) return false;
    
    return true;
}
```

---

## 3. Median Algorithm Selection

### Requirements
- Array size: ≤16 elements (4×4 zones)
- Performance target: <5ms on ESP32 (240MHz dual-core)
- Memory: Stack allocation preferred

### Algorithm Comparison

| Algorithm | Time Complexity | Space | Implementation | Best For |
|-----------|----------------|-------|----------------|----------|
| Full Sort (insertion) | O(n²) | O(1) | Simple | n ≤ 20 |
| Full Sort (std::sort) | O(n log n) | O(log n) | Library | Large n |
| Partial Sort | O(n log k) | O(1) | Moderate | k << n |
| Quickselect | O(n) avg, O(n²) worst | O(log n) | Complex | Large n |
| nth_element | O(n) avg | O(1) | Library | Large n |

### Selection: Insertion Sort + Middle Element

**Rationale**:
1. For n=16, O(n²) = 256 operations - trivial on ESP32
2. Simple implementation, no recursion, predictable performance
3. No library dependencies (portable to PlatformIO native tests)
4. Stable, deterministic (no random pivot)

**Benchmark Estimate** (ESP32 @ 240MHz):
- 256 comparisons + 128 swaps (avg)
- ~4 cycles per operation = 1024 cycles
- 1024 / 240,000,000 = ~4 microseconds
- Well under 5ms target ✅

### Implementation

```cpp
uint16_t computeMedian(uint16_t* values, uint8_t count) {
    if (count == 0) return 0;
    if (count == 1) return values[0];
    
    // In-place insertion sort (fast for small arrays)
    for (uint8_t i = 1; i < count; i++) {
        uint16_t key = values[i];
        int8_t j = i - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = key;
    }
    
    // Return middle element(s)
    // For even count: return lower middle (index count/2 - 1)
    // For odd count: return middle (index count/2)
    if (count % 2 == 0) {
        return values[count / 2 - 1];  // Lower middle for even
    } else {
        return values[count / 2];      // True middle for odd
    }
}
```

**Note**: For even counts, we return the lower of two middle values (simpler than averaging, acceptable for distance filtering where integer precision is sufficient).

---

## 4. Memory Layout Analysis

### Per-Update Temporary Allocations

| Variable | Type | Size | Location |
|----------|------|------|----------|
| valid_distances[16] | uint16_t | 32 bytes | Stack |
| valid_count | uint8_t | 1 byte | Stack |
| median | uint16_t | 2 bytes | Stack |
| keep_flags[16] | bool | 16 bytes | Stack |
| kept_count | uint8_t | 1 byte | Stack |
| consensus | ConsensusResult | 5 bytes | Stack |

**Total per-update**: ~57 bytes stack

### Persistent Allocations

| Variable | Type | Size | Location |
|----------|------|------|----------|
| lastConsensus_ | ConsensusResult | 5 bytes | HeightController member |

**Total persistent**: 5 bytes additional

### ConsensusResult Struct

```cpp
struct ConsensusResult {
    uint16_t consensus_distance_mm;  // 2 bytes
    uint8_t valid_zone_count;        // 1 byte
    uint8_t outlier_count;           // 1 byte
    bool is_reliable;                // 1 byte
    // Padding: 0 bytes (packed)
};  // Total: 5 bytes
```

### Memory Budget Verification

| Category | Allocation | Budget |
|----------|-----------|--------|
| Temporary stack | 57 bytes | <150 bytes ✅ |
| Persistent heap | 5 bytes | <1KB ✅ |
| **Total additional** | **62 bytes** | **<1KB** ✅ |

Well within the 1KB budget specified in PERF-002.

---

## 5. Performance Estimates

### computeMultiZoneConsensus() Breakdown

| Step | Operations | Estimated Time |
|------|------------|----------------|
| Zone extraction (16 zones) | 32 array accesses | <10 µs |
| Zone validation (16 zones) | 16 comparisons | <5 µs |
| Copy valid distances | ≤16 copies | <10 µs |
| Median calculation | 256 ops (insertion sort) | <50 µs |
| Outlier filtering | ≤16 comparisons | <10 µs |
| Mean calculation | ≤16 adds + 1 divide | <20 µs |
| **Total** | | **<105 µs** |

**Safety margin**: 100× under the 10ms budget ✅

### Full Update Cycle

| Component | Time Budget |
|-----------|-------------|
| Sensor data fetch | ~2-5ms (I2C transfer) |
| Multi-zone consensus | <1ms |
| Moving average filter | <0.1ms |
| Height calculation | <0.1ms |
| **Total** | **<10ms** |

Within 200ms update interval ✅ (<5% CPU overhead)

---

## 6. Algorithm Design Decisions

### Why Median for Outlier Detection?

1. **Robust to outliers**: Median is not affected by extreme values
2. **Simple threshold**: `|value - median| > 30mm` is intuitive
3. **No distribution assumptions**: Works regardless of value distribution

### Why Mean for Consensus?

1. **Computational efficiency**: Single pass sum/divide
2. **After outlier removal**: Mean is appropriate when outliers filtered
3. **Preserves precision**: Better than median for smooth values

### Why 30mm Outlier Threshold?

1. **Sensor noise**: VL53L5CX has ~1-5mm noise (typical)
2. **Floor variation**: Normal desk environments have <20mm variation
3. **Safety margin**: 30mm = 3× noise + 2× variation
4. **Validated by clarification**: User approved this threshold

### Why Minimum 4 Zones?

1. **Statistical validity**: 4 samples minimum for meaningful consensus
2. **Robustness**: Handles 75% zone failures (12 invalid)
3. **User alignment**: Matches 25% minimum threshold in spec

---

## 7. Test Strategy Validation

### Unit Tests Required

1. **computeMedian()**
   - Odd count: [800, 850, 840] → 840
   - Even count: [800, 850, 840, 860] → 840 (lower middle)
   - Single: [800] → 800
   - Duplicates: [800, 800, 850, 850] → 800

2. **computeMean()**
   - Normal: [800, 850, 900] → 850
   - Overflow safety: [60000, 60000, 60000] → 60000
   - Single: [800] → 800

3. **filterOutliers()**
   - No outliers: all within 30mm
   - One outlier: 15 @ 850, 1 @ 1000
   - All outliers: impossible (median always included)

4. **isZoneValid()**
   - Valid status 5, 6, 9
   - Invalid status 0, 255
   - Range validation

5. **computeMultiZoneConsensus()**
   - All 16 valid
   - Partial (12, 8, 4 valid)
   - Insufficient (<4 valid)
   - Bimodal (8 @ 800, 8 @ 840)

### Integration Tests Required

1. **Two-stage cascade**: spatial → temporal
2. **Gradual drift**: +10mm/cycle for 10 cycles
3. **Rapid movement**: large inter-update variation

---

## 8. Conclusions

### Algorithm Selections

| Component | Algorithm | Rationale |
|-----------|-----------|-----------|
| Median | Insertion sort + middle | Fast for n=16, simple, no deps |
| Mean | Sum/divide with uint32_t | Overflow safe for 16×4000mm |
| Outlier | Median ± 30mm | Simple, effective, validated |

### Performance Validation

- **Processing time**: <1ms (100× under 10ms budget)
- **Memory**: 62 bytes (16× under 1KB budget)
- **CPU overhead**: <0.5% (10× under 5% budget)

### Risk Mitigations

1. **Conservative validation**: Only accept status 5, 6, 9
2. **Graceful degradation**: INVALID when <4 zones
3. **No heap allocation**: All stack-based, deterministic

### Next Steps

1. ✅ T001-T005: Research complete (this document)
2. ⏳ T006-T008: Create test files (TDD phase)
3. ⏳ T009-T012: Implement statistical utilities
4. ⏳ T013: Run tests, verify passing
