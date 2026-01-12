# Feature Specification: Multi-Zone Sensor Filtering for Height Estimation

**Feature Branch**: `002-multi-zone-filtering`  
**Created**: 2026-01-12  
**Status**: Draft  
**Input**: User description: "Lets make a better height estimation. Instead of relying on a single measurement in the distance array of the sensor, lets use all of them and do some filtering to get a better less noisy distance estimate"

## Clarifications

### Session 2026-01-12

- Q: Should the system use median or mean for outlier detection and consensus calculation? → A: Use median for outlier detection, then mean for consensus (hybrid approach - faster mean calculation after outliers removed)
- Q: What is the minimum number of valid zones required before marking the entire reading as invalid? → A: Minimum 4 zones (25%) required - more tolerant of failures, still provides multi-zone benefit
- Q: What deviation threshold should be used to identify outlier zone measurements? → A: 30mm threshold - balanced approach for typical noise levels and floor variations (to be validated during Phase 0 research against actual sensor noise characterization; threshold represents ~3x expected noise margin)
- Q: What maximum percentage of invalid zones should the system successfully handle? → A: Update to 75% tolerance (12 invalid zones max) - matches the 4-zone minimum threshold exactly
- Q: Should multi-zone filtering replace, cascade with, or run parallel to the existing temporal moving average filter? → A: Two-stage cascade: multi-zone consensus → temporal moving average (complementary spatial + temporal filtering)
- Q: How should the system handle bimodal distributions where zones form distinct clusters (e.g., half zones at 800mm, half at 840mm due to mat edge)? → A: Accept bimodal distributions if all zones fall within median ± 30mm (outlier threshold). If cluster separation exceeds 60mm (2× threshold), this indicates non-uniform floor surface but system will still compute mean consensus. User responsibility to ensure reasonably uniform floor surface; system does not reject bimodal readings as this is a valid physical scenario.
- Q: Which VL53L5CX status codes should be considered valid for zone measurements? → A: Accept status codes 5, 6, 9 as high-confidence valid measurements. Reject status codes 0 (no target) and 255 (invalid). Other status codes (1-4, 7-8, 10+) are currently undefined and should be rejected conservatively until validated during Phase 0 sensor characterization.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Stable Height Display During Stationary Operation (Priority: P1)

A user observes the desk height display while the desk is stationary and expects to see minimal fluctuation in the displayed value, even when environmental factors (air movement, temperature changes, vibrations) might cause individual sensor readings to vary.

**Why this priority**: Core quality improvement - users need confidence in displayed height values to trust the system. A constantly fluctuating display undermines user confidence and makes it difficult to determine if the desk has truly reached the target position.

**Independent Test**: Can be tested by observing the height display on a stationary desk over 60 seconds, measuring the maximum deviation in displayed height, and comparing it to the baseline (single-zone) implementation. Should show 50% or greater reduction in display fluctuation.

**Acceptance Scenarios**:

1. **Given** the desk is stationary at any height, **When** observed over 60 seconds, **Then** the displayed height value varies by no more than 2mm (versus baseline 5mm+ fluctuation)
2. **Given** the desk is stationary, **When** minor environmental disturbances occur (air movement from HVAC, footsteps nearby), **Then** the displayed height remains stable without rapid value changes
3. **Given** the desk has just stopped moving, **When** the system stabilizes, **Then** the height display settles to a consistent value within 2 seconds
4. **Given** multiple measurement zones return slightly different values, **When** the system processes them, **Then** the displayed height represents a filtered consensus rather than a single noisy zone

---

### User Story 2 - Accurate Target Height Achievement (Priority: P1)

A user sets a target height and expects the desk to stop precisely at that height without overshooting or undershooting due to noisy sensor readings causing premature or delayed stop commands.

**Why this priority**: Direct impact on primary functionality - improved height estimation accuracy ensures the desk stops at the correct position. Poor filtering could cause the desk to stop too early or too late, failing to meet user expectations.

**Independent Test**: Can be tested by setting various target heights (60cm, 80cm, 100cm, 120cm) and measuring the final achieved height after the desk stops. Compare accuracy to baseline implementation.

**Acceptance Scenarios**:

1. **Given** a target height is set, **When** the desk approaches within tolerance range, **Then** the filtered height reading correctly determines when to stop (within ±5mm of target)
2. **Given** the desk is moving toward target, **When** individual zones temporarily report erratic values, **Then** the filtered estimate prevents false stop/start commands
3. **Given** the desk has reached the target height, **When** the system evaluates stabilization, **Then** filtered readings show consistent stability faster than single-zone baseline (2 seconds vs 3+ seconds)

---

### User Story 3 - Robust Operation in Challenging Conditions (Priority: P2)

A user operates the desk in less-than-ideal conditions (partial obstructions under desk, non-uniform floor surface, angled sensor mounting) and expects the system to maintain reliable height estimation by using multiple measurement zones.

**Why this priority**: Enhanced reliability and robustness - while not essential for basic operation, this improves system resilience in real-world deployment scenarios where sensor placement or environment isn't perfect.

**Independent Test**: Can be tested by introducing partial obstructions in some sensor zones or tilting the sensor slightly, then verifying that height estimation remains accurate by compensating with unobstructed zones.

**Acceptance Scenarios**:

1. **Given** one or two sensor zones are partially obstructed, **When** height is measured, **Then** the system uses remaining valid zones to provide accurate height reading
2. **Given** the floor surface is non-uniform (cables, mat edges), **When** different zones detect different distances, **Then** the system filters outliers and produces a reliable average
3. **Given** temporary interference affects one zone (reflective object), **When** that zone reports anomalous values, **Then** other zones maintain accurate height estimation without disruption

---

### User Story 4 - Diagnostic Visibility for Multi-Zone Data (Priority: P3)

A developer or advanced user wants to understand how the multi-zone filtering is performing by viewing diagnostic information about individual zone measurements and filtering effectiveness.

**Why this priority**: Useful for debugging and system optimization but not required for end-user operation. Helps validate the filtering algorithm and troubleshoot issues during development and testing.

**Independent Test**: Can be tested by enabling diagnostic logging and verifying that zone-specific data, filter statistics, and outlier detection results are visible through serial console or debug interface.

**Acceptance Scenarios**:

1. **Given** diagnostic mode is enabled, **When** sensor readings are captured, **Then** individual zone distances are logged for analysis
2. **Given** filtering is active, **When** outliers are detected, **Then** the system logs which zones were excluded and why
3. **Given** height calculation occurs, **When** viewed via diagnostics, **Then** both raw multi-zone average and final filtered value are available for comparison

---

### Edge Cases

- What happens when all sensor zones report invalid data simultaneously? → **Handled by FR-007**: Reading marked INVALID (0 < 4 minimum zones)
- How does the system handle gradual drift across all zones (sensor temperature effects)? → **Spatial filter passes drift** (no outliers detected when all zones shift together), **temporal filter smooths** the gradual change; validated in integration tests (Phase 3)
- What happens when exactly half the zones show one distance and half show significantly different distance? → **Handled by FR-009**: Bimodal distributions accepted if separation ≤60mm; consensus = mean of all non-outlier zones
- How does filtering perform during rapid movement when consecutive readings vary significantly? → **Two-stage cascade**: Spatial filter stabilizes per-update, temporal filter smooths inter-update variation; validated in integration tests
- What happens when one zone consistently reports outlier values (damaged zone)? → **Outlier filtering excludes it** each update (>30mm from median); system remains valid if ≥4 other zones available
- How does the system distinguish between genuine height changes and sensor noise? → **Temporal filter** smooths noise while tracking real movement; genuine height changes affect all zones simultaneously (consensus shifts), noise affects individual zones (filtered out)
- What happens when the desk is at extreme angles (manufacturer tilt tolerance)? → **Assumed within sensor spec**; if tilt causes zone variation >30mm, may trigger outlier filtering but ≥4 zones should remain valid

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST utilize all available sensor measurement zones (16 zones for 4x4 resolution) for height estimation instead of a single zone
- **FR-002**: System MUST validate each zone's measurement before including it in height calculation (check status codes, range validity)
- **FR-003**: System MUST filter out outlier zone measurements that deviate more than 30mm from the median of all valid zones
- **FR-004**: System MUST compute a consensus distance value as the mean of non-outlier zone measurements (after outlier removal via median-based filtering)
- **FR-005**: System MUST apply the existing moving average filter as a second stage to the multi-zone consensus value (cascade: spatial filtering → temporal filtering → height calculation)
- **FR-006**: System MUST maintain backward compatibility with existing height calculation formula (calibration_constant - distance/10)
- **FR-007**: System MUST mark reading as INVALID when fewer than 4 zones (25% of total) return valid data; otherwise use available valid zones for consensus calculation
- **FR-008**: System MUST preserve existing reading validity states (VALID, INVALID, STALE) with same semantics
- **FR-009**: System MUST accept bimodal zone distributions (multiple distinct distance clusters) and compute consensus as mean of all non-outlier zones, even when clusters are separated by up to 60mm; system does not reject bimodal readings as this represents valid physical floor variation (user responsibility to ensure reasonably uniform surface)

### User Experience Requirements

- **UX-001**: Height display MUST show visibly smoother values with reduced fluctuation compared to baseline (subjectively stable to human observation). Note: "Height display" refers to the web UI (data/index.html) which consumes height readings via WebServer API; this feature modifies sensor data processing only, not UI rendering. Web UI accessibility is out of scope (no UI code changes).
- **UX-002**: Height display updates MUST maintain the same frequency as baseline implementation (200ms interval, 5Hz)
- **UX-003**: System MUST NOT introduce perceptible lag in height updates when desk is actively moving
- **UX-004**: Error states MUST remain clear and actionable (sensor failure messages unchanged)

### Performance Requirements

- **PERF-001**: Multi-zone processing MUST complete within the existing 200ms update cycle without causing delays
- **PERF-002**: Memory usage for zone data storage MUST NOT exceed 1KB additional allocation (16 zones × 2 bytes distance + status array)
- **PERF-003**: CPU processing time for filtering algorithm MUST NOT exceed 10ms per update cycle
- **PERF-004**: System MUST maintain existing sensor sampling rate (5Hz) without degradation

### Key Entities

- **Sensor Zone Reading**: Represents a single distance measurement from one zone of the multi-zone sensor array, including distance value, target status code, and zone identifier (0-15)
- **Multi-Zone Consensus**: Aggregated distance value computed from multiple valid zone readings after outlier filtering (first stage: spatial filtering across zones)
- **Filtered Distance**: Final distance value after applying temporal moving average filter to the multi-zone consensus (second stage: temporal filtering over time), used for height calculation

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Height display fluctuation on stationary desk reduces by at least 50% compared to single-zone baseline (measured as standard deviation over 60-second observation)
- **SC-002**: Height measurement accuracy improves to within ±5mm of true height across the full operating range (60cm-125cm)
- **SC-003**: System achieves target height stabilization in 2 seconds or less (versus 3+ seconds baseline), measured from entry into tolerance range
- **SC-004**: Sensor processing overhead remains under 5% of total CPU time (measured via performance profiling)
- **SC-005**: System successfully handles partial zone failures (up to 75% zones invalid) without marking entire reading as invalid

## Assumptions

- The VL53L5CX sensor provides 16 zones in 4×4 resolution mode with independent distance measurements per zone
- Individual zones may occasionally report outlier values due to environmental factors (reflections, interference)
- The existing moving average filter implementation is effective and should be preserved as a second stage after multi-zone consensus
- Users value stability and accuracy in height display over absolute minimum latency
- The sensor update rate (5Hz) provides sufficient temporal resolution for effective filtering
- Zone status codes (0, 255 = invalid; 5, 6, 9 = valid variants) are reliable indicators of measurement quality
- Processing 16 zone measurements per update cycle is computationally feasible on the target microcontroller
- The existing calibration approach (fixed constant minus sensor reading) remains valid with multi-zone averaging
