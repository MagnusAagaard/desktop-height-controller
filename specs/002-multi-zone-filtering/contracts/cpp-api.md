# Internal API Contract: HeightController Multi-Zone Filtering

**Feature**: Multi-Zone Sensor Filtering for Height Estimation  
**Branch**: `002-multi-zone-filtering`  
**Created**: 2026-01-12

## Overview

This document defines the internal API contract for multi-zone filtering within the HeightController class. This is an **internal refactoring** - the public API remains unchanged for backward compatibility.

## Public API (Unchanged)

These methods remain identical to preserve backward compatibility:

```cpp
class HeightController {
public:
    // Initialization
    bool init();
    
    // Runtime update (call every 200ms)
    void update();
    
    // Height queries
    uint16_t getCurrentHeight() const;        // Height in cm
    uint16_t getRawDistance() const;          // Distance in mm (now consensus, not zone 5)
    uint16_t getFilteredDistance() const;     // After temporal filter
    
    // Validity checks
    bool isValid() const;                     // Current reading valid
    ReadingValidity getValidity() const;      // Detailed status
    const HeightReading& getReading() const;  // Complete reading structure
    
    // Utility
    void resetFilter();                       // Clear filter history
    bool isSensorReady() const;               // Sensor initialized
    unsigned long getReadingAge() const;      // Time since last update
};
```

**Semantic Change**: `getRawDistance()` now returns the multi-zone consensus value instead of zone 5 alone. This is still "raw" in the sense of "before temporal filtering", maintaining the contract.

## New Public API (Diagnostic - Optional P3)

```cpp
class HeightController {
public:
    // Diagnostic methods for debugging/advanced users
    uint8_t getValidZoneCount() const;     // How many zones contributed to consensus
    uint8_t getOutlierCount() const;       // How many zones excluded as outliers
    String getZoneDiagnostics() const;     // JSON array of zone-level data
};
```

**Purpose**: Enable visibility into multi-zone filtering for troubleshooting and optimization.

**JSON Format** (getZoneDiagnostics):
```json
{
  "valid_count": 14,
  "outlier_count": 1,
  "consensus_mm": 845,
  "zones": [
    {"id": 0, "distance_mm": 840, "status": 5, "valid": true, "outlier": false},
    {"id": 1, "distance_mm": 850, "status": 5, "valid": true, "outlier": false},
    ...
    {"id": 7, "distance_mm": 1000, "status": 5, "valid": true, "outlier": true},
    ...
    {"id": 15, "distance_mm": 0, "status": 255, "valid": false, "outlier": false}
  ]
}
```

## Private API (New Internal Methods)

### Core Consensus Method

```cpp
ConsensusResult computeMultiZoneConsensus(const VL53L5CX_ResultsData& results);
```

**Purpose**: Compute consensus distance from all 16 sensor zones.

**Input**: `VL53L5CX_ResultsData` containing zone distance and status arrays.

**Output**: `ConsensusResult` with consensus distance, validity counts, and reliability flag.

**Algorithm**:
1. Extract 16 zones from results structure
2. Validate each zone (status codes, range checks)
3. If `valid_count < 4`: return `is_reliable = false`
4. Compute median of valid zone distances
5. Filter outliers: exclude zones where `|distance - median| > 30mm`
6. Compute mean of non-outlier zones
7. Return ConsensusResult with statistics

**Performance**: Must complete in <10ms.

**Error Handling**:
- If `getRangingData()` fails: return `is_reliable = false`, `consensus_distance_mm = 0`
- If all zones invalid: return `is_reliable = false`
- If 1-3 zones valid: return `is_reliable = false` (insufficient for consensus)

**Example Usage**:
```cpp
VL53L5CX_ResultsData results;
if (sensor_.getRangingData(&results)) {
    ConsensusResult consensus = computeMultiZoneConsensus(results);
    if (consensus.is_reliable) {
        filter_.addSample(consensus.consensus_distance_mm);
    } else {
        currentReading_.validity = ReadingValidity::INVALID;
    }
}
```

---

### Zone Validation

```cpp
bool isZoneValid(uint8_t status, uint16_t distance) const;
```

**Purpose**: Determine if a single zone measurement is valid.

**Input**:
- `status`: Target status code from sensor (0, 5, 6, 9, 255, etc.)
- `distance`: Distance reading in millimeters

**Output**: `true` if zone passes validation, `false` otherwise.

**Validation Logic**:
```cpp
bool isZoneValid(uint8_t status, uint16_t distance) const {
    // Status 0 = no target, 255 = invalid
    if (status == 0 || status == 255) {
        return false;
    }
    
    // Range check
    if (distance < SENSOR_MIN_VALID_MM || distance > SENSOR_MAX_RANGE_MM) {
        return false;
    }
    
    // Accept only high-confidence status codes: 5, 6, 9
    // Reject undefined codes (1-4, 7-8, 10+) until validated
    if (status != 5 && status != 6 && status != 9) {
        return false;
    }
    
    return true;
}
```

**Constants**:
- `SENSOR_MIN_VALID_MM`: Minimum valid distance (e.g., 200mm)
- `SENSOR_MAX_RANGE_MM`: Maximum sensor range (e.g., 4000mm)

---

### Statistical Functions

```cpp
uint16_t computeMedian(uint16_t* values, uint8_t count);
```

**Purpose**: Calculate median of an array (for outlier detection).

**Input**:
- `values`: Array of distances in mm (modified during computation)
- `count`: Number of elements (1-16)

**Output**: Median value in mm.

**Algorithm**: Partial sort or quickselect for middle element.

**Notes**:
- For even `count`, returns lower middle value: `values[count/2 - 1]` after sort
- For odd `count`, returns middle value: `values[count/2]` after sort
- Input array may be modified (sorted in-place for efficiency)

**Example**:
```cpp
uint16_t distances[] = {800, 850, 840, 860, 845};
uint16_t median = computeMedian(distances, 5);  // Returns 845
```

---

```cpp
uint16_t computeMean(uint16_t* values, uint8_t count);
```

**Purpose**: Calculate arithmetic mean of an array (for consensus).

**Input**:
- `values`: Array of distances in mm
- `count`: Number of elements (1-16)

**Output**: Mean value in mm (rounded to nearest integer).

**Algorithm**:
```cpp
uint32_t sum = 0;
for (uint8_t i = 0; i < count; i++) {
    sum += values[i];
}
return (uint16_t)(sum / count);
```

**Overflow Safety**: Use `uint32_t` for sum (max: 16 × 4000 = 64000, well within uint32_t).

---

```cpp
void filterOutliers(uint16_t* values, uint8_t count, uint16_t median,
                    bool* keep_flags, uint8_t& kept_count);
```

**Purpose**: Mark values as outliers if they deviate >30mm from median.

**Input**:
- `values`: Array of distances in mm
- `count`: Number of elements
- `median`: Previously computed median value
- `keep_flags`: Output array (bool[count]) marking which values to keep
- `kept_count`: Output reference for count of non-outliers

**Output**: `keep_flags` populated with true/false for each value, `kept_count` set.

**Algorithm**:
```cpp
kept_count = 0;
for (uint8_t i = 0; i < count; i++) {
    uint16_t deviation = (values[i] > median) 
        ? (values[i] - median) 
        : (median - values[i]);
    
    keep_flags[i] = (deviation <= MULTI_ZONE_OUTLIER_THRESHOLD_MM);
    if (keep_flags[i]) {
        kept_count++;
    }
}
```

**Constants**:
- `MULTI_ZONE_OUTLIER_THRESHOLD_MM = 30`

---

## Data Flow Contract

### Update Cycle (update() method)

```cpp
void HeightController::update() {
    if (!sensorInitialized_) {
        currentReading_.validity = ReadingValidity::INVALID;
        return;
    }
    
    // Check for new data
    if (!sensor_.isDataReady()) {
        if (millis() - currentReading_.timestamp_ms > READING_STALE_TIMEOUT_MS) {
            currentReading_.validity = ReadingValidity::STALE;
        }
        return;
    }
    
    // Get sensor data
    VL53L5CX_ResultsData results;
    if (!sensor_.getRangingData(&results)) {
        Logger::error(TAG, "Failed to get ranging data");
        currentReading_.validity = ReadingValidity::INVALID;
        return;
    }
    
    // MULTI-ZONE CONSENSUS (NEW)
    ConsensusResult consensus = computeMultiZoneConsensus(results);
    lastConsensus_ = consensus;  // Cache for diagnostics
    
    // Validate consensus
    if (!consensus.is_reliable) {
        Logger::warn(TAG, "Unreliable consensus: %d valid zones", 
                     consensus.valid_zone_count);
        currentReading_.validity = ReadingValidity::INVALID;
        return;
    }
    
    // Store consensus as "raw" value (semantic change from zone 5)
    currentReading_.raw_distance_mm = consensus.consensus_distance_mm;
    currentReading_.timestamp_ms = millis();
    currentReading_.validity = ReadingValidity::VALID;
    
    // TEMPORAL FILTERING (EXISTING, UNCHANGED)
    filter_.addSample(consensus.consensus_distance_mm);
    currentReading_.filtered_distance_mm = filter_.getAverage();
    
    // HEIGHT CALCULATION (EXISTING, UNCHANGED)
    currentReading_.calculated_height_cm = 
        calculateHeight(currentReading_.filtered_distance_mm);
    
    Logger::debug(TAG, "Consensus: %dmm (%d zones, %d outliers), Filtered: %dmm, Height: %dcm",
                  consensus.consensus_distance_mm, consensus.valid_zone_count,
                  consensus.outlier_count, currentReading_.filtered_distance_mm,
                  currentReading_.calculated_height_cm);
}
```

**Key Changes from Baseline**:
1. `readSensor()` replaced by `computeMultiZoneConsensus(results)`
2. Consensus reliability check before proceeding
3. `raw_distance_mm` now stores consensus, not zone 5
4. Temporal filter and height calculation unchanged

## Performance Contract

**Timing Requirements**:
- `computeMultiZoneConsensus()`: <10ms average, <15ms worst-case
- `computeMedian()`: <5ms for 16 elements
- `computeMean()`: <1ms for 16 elements
- `filterOutliers()`: <2ms for 16 elements
- Total `update()` cycle: <200ms (5Hz sampling maintained)

**Memory Requirements**:
- Stack usage per update: <150 bytes (temporary arrays)
- Heap allocations: None (all stack-allocated)
- Total additional memory: <1KB

**CPU Usage**:
- Average: <5% (target from PERF-001)
- Peak during update: <15%

## Error Handling Contract

**Failure Modes**:

1. **Sensor communication failure** (`getRangingData()` returns false):
   - Action: Mark reading INVALID, log error
   - Recovery: Retry next update cycle

2. **Insufficient valid zones** (<4 zones valid):
   - Action: Set `is_reliable = false`, mark reading INVALID
   - Recovery: Automatic when zones become valid

3. **All zones invalid** (sensor malfunction):
   - Action: Set `is_reliable = false`, mark reading INVALID
   - Logging: Warn-level message
   - Recovery: Sensor re-initialization may be required

**Graceful Degradation**:
- If 4-7 zones valid: Still compute consensus (meets minimum threshold)
- If 8-12 zones valid: Normal operation
- If 13-16 zones valid: Optimal operation

**No Silent Failures**:
- All error conditions logged via Logger
- Reading validity always reflects actual state
- Diagnostic methods expose zone-level details

## Testing Contract

**Unit Test Coverage**:
- `computeMedian()`: 100% coverage (odd/even/single/duplicates)
- `computeMean()`: 100% coverage (various counts, overflow safety)
- `filterOutliers()`: 100% coverage (no outliers, all outliers, partial)
- `isZoneValid()`: 100% coverage (status codes, range bounds)
- `computeMultiZoneConsensus()`: 100% coverage (all edge cases)

**Integration Test Scenarios**:
- Two-stage cascade (spatial → temporal)
- Baseline comparison (fluctuation reduction)
- Partial zone failures (4, 8, 12 zones)
- Rapid movement response
- Stationary stability

**Hardware Test Requirements**:
- Accuracy: ±5mm at 4 known heights
- Stability: ≥50% reduction in std deviation
- Obstruction tolerance: Valid with 4+ unobstructed zones
- Performance: <10ms average execution time

## Backward Compatibility Guarantee

**API Compatibility**:
- All public methods preserve signatures
- Return types unchanged
- Side effects equivalent (height calculation, validity states)

**Behavioral Changes** (documented):
- `getRawDistance()` returns consensus instead of zone 5
  - Still represents "pre-temporal-filter" distance
  - More stable than single zone (feature, not bug)
  
**Migration Path**:
- No changes required in WebServer, MovementController, or other consumers
- Existing tests may need update if they mock single-zone behavior
- Calibration procedures unchanged (still based on distance-to-height formula)

**Rollback Safety**:
- Feature can be disabled via compile flag if needed
- Fallback to single-zone logic possible in private methods
- No persistent state changes (runtime-only modification)
