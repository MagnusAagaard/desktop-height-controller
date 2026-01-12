# Data Model: Multi-Zone Sensor Filtering

**Feature**: Multi-Zone Sensor Filtering for Height Estimation  
**Branch**: `002-multi-zone-filtering`  
**Created**: 2026-01-12

## Overview

This document defines the runtime data structures for multi-zone sensor filtering. All entities are transient (no persistence) and exist only during the sensor update cycle.

## Entities

### 1. ZoneReading (Transient)

**Purpose**: Represents a single measurement zone from the VL53L5CX sensor array.

**Attributes**:
- `zone_id` (uint8_t): Zone identifier (0-15 for 4×4 array)
- `distance_mm` (uint16_t): Raw distance measurement in millimeters
- `status` (uint8_t): Target status code from sensor
- `is_valid` (bool): Whether zone passed validation checks

**Validation Rules**:
- Zone is valid if:
  - `status` is 5, 6, or 9 (high-confidence valid measurements per VL53L5CX spec)
  - `status` is NOT 0 (no target detected) or 255 (measurement invalid)
  - `status` codes 1-4, 7-8, 10+ are rejected conservatively (undefined/untested)
  - `distance_mm >= SENSOR_MIN_VALID_MM` (e.g., 200mm)
  - `distance_mm <= SENSOR_MAX_RANGE_MM` (e.g., 4000mm)

**Lifecycle**: Created per-update from VL53L5CX_ResultsData, discarded after consensus calculation

**Example**:
```cpp
ZoneReading zone;
zone.zone_id = 5;
zone.distance_mm = 850;
zone.status = 5;  // 100% valid
zone.is_valid = true;
```

---

### 2. ConsensusResult (Transient)

**Purpose**: Aggregated distance estimate from multiple valid zones after outlier filtering.

**Attributes**:
- `consensus_distance_mm` (uint16_t): Final consensus distance (median-filtered mean)
- `valid_zone_count` (uint8_t): Number of zones that passed validation (0-16)
- `outlier_count` (uint8_t): Number of zones excluded as outliers
- `is_reliable` (bool): Whether consensus is trustworthy (>= 4 valid zones)

**Derivation Rules**:
- `is_reliable = (valid_zone_count >= 4)`
- `outlier_count` = zones exceeding ±30mm from median
- `consensus_distance_mm` = mean of non-outlier zones

**Lifecycle**: Created per-update in `computeMultiZoneConsensus()`, cached in `HeightController::lastConsensus_` for diagnostics

**Example**:
```cpp
ConsensusResult consensus;
consensus.consensus_distance_mm = 845;
consensus.valid_zone_count = 14;  // 14 of 16 zones valid
consensus.outlier_count = 1;       // 1 zone excluded as outlier
consensus.is_reliable = true;      // >= 4 valid zones
```

---

### 3. HeightReading (Existing, Modified)

**Purpose**: Complete height measurement including raw, consensus, filtered, and calculated values.

**Attributes** (updated):
- `raw_distance_mm` (uint16_t): **Now stores consensus distance** (was single zone 5)
- `filtered_distance_mm` (uint16_t): After temporal moving average filter
- `calculated_height_cm` (uint16_t): Final desk height via calibration formula
- `timestamp_ms` (unsigned long): Millis when reading captured
- `validity` (ReadingValidity): VALID, INVALID, or STALE

**Changes from Baseline**:
- `raw_distance_mm` semantics changed: now represents multi-zone consensus instead of zone 5
- Backward compatibility maintained: external API unchanged, value is still "raw before temporal filter"

**Lifecycle**: Persistent member of HeightController, updated every 200ms

---

## Relationships

```
VL53L5CX_ResultsData (16 zones)
  │
  ├─> ZoneReading[0..15]  (extraction + validation)
  │
  └─> ConsensusResult  (outlier filtering + mean)
       │
       └─> HeightReading.raw_distance_mm  (spatial filtering output)
            │
            └─> MovingAverageFilter  (temporal filtering)
                 │
                 └─> HeightReading.filtered_distance_mm
                      │
                      └─> HeightReading.calculated_height_cm  (calibration formula)
```

## State Transitions

### Zone Processing Pipeline

1. **Extraction**: VL53L5CX_ResultsData → 16 ZoneReading instances
   - For each zone `i` (0-15):
     - `distance_mm = results.distance_mm[NB_TARGET_PER_ZONE * i]`
     - `status = results.target_status[NB_TARGET_PER_ZONE * i]`

2. **Validation**: ZoneReading → Validated subset
   - Apply `isZoneValid(status, distance_mm)` to each zone
   - Mark `is_valid = true/false` based on status codes and range

3. **Consensus Calculation**: Validated zones → ConsensusResult
   - Collect valid zone distances into array
   - If `valid_count < 4`: set `is_reliable = false`, return early
   - Compute median of valid distances
   - Filter outliers: exclude zones where `|distance - median| > 30mm`
   - Compute mean of remaining non-outlier zones → `consensus_distance_mm`

4. **Temporal Filtering**: Consensus → Filtered value
   - Feed `consensus_distance_mm` to existing `MovingAverageFilter`
   - Output: `filtered_distance_mm` (smoothed over time window)

5. **Height Calculation**: Filtered value → Final height
   - Apply calibration formula: `height_cm = calibration_constant - (filtered_mm / 10)`
   - Clamp to reasonable range (0-200cm)

### Validity State Machine

```
INVALID (no data) 
  │
  ├─> VALID (reliable consensus, fresh timestamp)
  │    │
  │    └─> STALE (>1000ms since last update)
  │         │
  │         └─> VALID (new reliable consensus)
  │
  └─> INVALID (unreliable consensus: <4 zones)
       │
       └─> VALID (sufficient zones restored)
```

**Transitions**:
- `INVALID → VALID`: Consensus becomes reliable (>= 4 valid zones)
- `VALID → STALE`: >1000ms elapsed without new data
- `STALE → VALID`: New reliable consensus received
- `VALID → INVALID`: Consensus becomes unreliable (< 4 valid zones)

## Constraints

**Performance**:
- Zone extraction: O(16) = constant
- Median calculation: O(n log n) where n ≤ 16, target <5ms
- Outlier filtering: O(n) where n ≤ 16
- Mean calculation: O(n) where n ≤ 13 (after outliers removed)
- Total consensus computation: <10ms

**Memory**:
- ZoneReading: 5 bytes × 16 = 80 bytes (stack allocation)
- ConsensusResult: 5 bytes (stack allocation)
- Temporary arrays: uint16_t[16] = 32 bytes, bool[16] = 16 bytes
- Total overhead: <150 bytes per update cycle

**Reliability**:
- Minimum 4 zones required for reliable consensus
- Maximum 12 zones can fail (75% tolerance) before marking INVALID
- Outlier threshold (30mm) handles typical sensor noise without over-filtering

## Edge Cases

1. **All zones invalid**: `valid_zone_count = 0`, `is_reliable = false`, reading marked INVALID
2. **Exactly 4 zones valid**: Minimum threshold, `is_reliable = true`, uses all 4 for consensus
3. **Bimodal distribution** (8 zones at 500mm, 8 at 600mm):
   - Median ≈ 550mm
   - All zones within 50mm of median (no outliers excluded)
   - Consensus = mean of all 16 ≈ 550mm
4. **Single outlier zone** (15 at 850mm, 1 at 1000mm):
   - Median = 850mm
   - 1 zone excluded (|1000-850| = 150mm > 30mm)
   - Consensus = mean of 15 zones ≈ 850mm
5. **Gradual drift** (all zones shift +10mm per cycle):
   - No outliers detected (all within 30mm of median)
   - Consensus tracks drift
   - Temporal filter smooths the trend

## Backward Compatibility

**External API**: No changes to public HeightController interface
- `getCurrentHeight()`: Still returns calculated height in cm
- `getRawDistance()`: Now returns consensus instead of zone 5 (semantic change, but same type/units)
- `getFilteredDistance()`: Still returns temporally filtered value
- `isValid()`: Still checks ReadingValidity state

**Internal Changes**: Private method implementations only
- `readSensor()` replaced by `computeMultiZoneConsensus()`
- `update()` logic modified to call new consensus method
- Existing calibration, validation, and height calculation unchanged
