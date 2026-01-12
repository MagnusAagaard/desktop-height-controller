/**
 * @file HeightController.cpp
 * @brief Implementation of height sensing and calculation
 */

#include "HeightController.h"
#include "utils/Logger.h"
#include <cstring>  // For memcpy in multi-zone filtering

static const char* TAG = "HeightController";

HeightController::HeightController()
    : filter_(DEFAULT_FILTER_WINDOW_SIZE)  // Use default, init() will reconfigure
    , sensorInitialized_(false)
{
    // Initialize reading structure
    currentReading_.raw_distance_mm = 0;
    currentReading_.filtered_distance_mm = 0;
    currentReading_.calculated_height_cm = 0;
    currentReading_.timestamp_ms = 0;
    currentReading_.validity = ReadingValidity::INVALID;
}

bool HeightController::init() {
    Logger::info(TAG, "Initializing VL53L5CX sensor...");
    
    // Reconfigure filter with config value (SystemConfig now initialized)
    uint8_t configWindowSize = SystemConfig.getFilterWindowSize();
    if (configWindowSize != DEFAULT_FILTER_WINDOW_SIZE) {
        filter_ = MovingAverageFilter(configWindowSize);
        Logger::info(TAG, "Filter window size set to %d", configWindowSize);
    }
    
    // Initialize I2C
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_FREQUENCY);
    
    // Initialize sensor
    if (!sensor_.begin()) {
        Logger::error(TAG, "VL53L5CX not detected! Check wiring.");
        sensorInitialized_ = false;
        return false;
    }
    
    // Configure for 4x4 resolution (lower power, faster)
    // We only need single-zone distance for height measurement
    sensor_.setResolution(VL53L5CX_RESOLUTION_4X4);
    
    // Set ranging frequency to match our sample interval
    // 5Hz = 200ms interval
    sensor_.setRangingFrequency(5);
    
    // Start ranging
    sensor_.startRanging();
    
    sensorInitialized_ = true;
    Logger::info(TAG, "Sensor initialized successfully");
    
    // Log calibration status
    if (!SystemConfig.isCalibrated()) {
        Logger::warn(TAG, "System not calibrated! Height readings will be inaccurate.");
    } else {
        Logger::info(TAG, "Calibration constant: %d cm", 
                     SystemConfig.getCalibrationConstant());
    }
    
    return true;
}

void HeightController::update() {
    if (!sensorInitialized_) {
        currentReading_.validity = ReadingValidity::INVALID;
        return;
    }
    
    // Check if new data is available
    if (!sensor_.isDataReady()) {
        // No new data, check if current reading is stale
        if (millis() - currentReading_.timestamp_ms > READING_STALE_TIMEOUT_MS) {
            currentReading_.validity = ReadingValidity::STALE;
        }
        return;
    }
    
    // Get raw sensor data structure
    VL53L5CX_ResultsData results;
    if (!sensor_.getRangingData(&results)) {
        Logger::error(TAG, "Failed to get ranging data");
        currentReading_.validity = ReadingValidity::INVALID;
        return;
    }
    
    currentReading_.timestamp_ms = millis();
    
    // =========================================================================
    // SPATIAL STAGE: Multi-zone consensus filtering
    // Replaces single-zone readSensor() with 16-zone spatial filtering
    // =========================================================================
    lastConsensus_ = computeMultiZoneConsensus(results);
    
    // Check if consensus is reliable (>= 4 valid zones)
    if (!lastConsensus_.is_reliable) {
        currentReading_.validity = ReadingValidity::INVALID;
        Logger::warn(TAG, "Multi-zone consensus unreliable: %d zones valid", 
                     lastConsensus_.valid_zone_count);
        return;
    }
    
    // Store consensus distance as raw reading for diagnostics
    currentReading_.raw_distance_mm = lastConsensus_.consensus_distance_mm;
    currentReading_.validity = ReadingValidity::VALID;
    
    // =========================================================================
    // TEMPORAL STAGE: Moving average filter
    // Smooth consensus output over time
    // =========================================================================
    filter_.addSample(lastConsensus_.consensus_distance_mm);
    currentReading_.filtered_distance_mm = filter_.getAverage();
    
    // Calculate height from filtered distance
    currentReading_.calculated_height_cm = calculateHeight(currentReading_.filtered_distance_mm);
    
    Logger::debug(TAG, "Consensus: %dmm (%d zones, %d outliers), Filtered: %dmm, Height: %dcm",
                  lastConsensus_.consensus_distance_mm,
                  lastConsensus_.valid_zone_count,
                  lastConsensus_.outlier_count,
                  currentReading_.filtered_distance_mm, 
                  currentReading_.calculated_height_cm);
}

uint16_t HeightController::readSensor() {
    VL53L5CX_ResultsData results;
    
    if (!sensor_.getRangingData(&results)) {
        Logger::error(TAG, "Failed to get ranging data");
        return 0;
    }
    
    // For 4x4 resolution, zones are numbered 0-15
    // Center zones are approximately 5, 6, 9, 10
    // Use zone 5 as representative center zone
    uint8_t centerZone = 5;
    
    // Get target status - valid statuses are 5 (100% valid) and others
    // Status 0 means no target detected, 255 means invalid
    uint8_t status = results.target_status[VL53L5CX_NB_TARGET_PER_ZONE * centerZone];
    uint16_t distance = results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * centerZone];
    
    // Log for debugging - reduce frequency to avoid spam
    static unsigned long lastDebugLog = 0;
    if (millis() - lastDebugLog > 2000) {
        Logger::debug(TAG, "Zone %d: status=%d, distance=%d mm", centerZone, status, distance);
        lastDebugLog = millis();
    }
    
    // Valid status codes: 5 = 100% valid, 6 = 50% valid, 9 = valid with wrap
    // Status 0 or 255 = invalid
    if (status == 0 || status == 255) {
        return 0;
    }
    
    return distance;
}

ReadingValidity HeightController::validateReading(uint16_t reading) const {
    // Check for sensor error conditions
    if (reading == 0) {
        return ReadingValidity::INVALID;
    }
    
    if (reading < SENSOR_MIN_VALID_MM) {
        return ReadingValidity::INVALID;
    }
    
    if (reading > SENSOR_MAX_RANGE_MM) {
        return ReadingValidity::INVALID;
    }
    
    return ReadingValidity::VALID;
}

uint16_t HeightController::calculateHeight(uint16_t filtered_mm) const {
    int16_t calibration = (int16_t)SystemConfig.getCalibrationConstant();
    
    // If not calibrated, return 0
    if (calibration == 0) {
        return 0;
    }
    
    // For floor-pointing sensor mounted under desk:
    // height_cm = calibration_constant + (sensor_reading_mm / 10)
    // When desk goes UP, distance to floor INCREASES, so height increases
    int16_t distance_cm = filtered_mm / 10;
    int16_t height = calibration + distance_cm;
    
    // Clamp to reasonable range (0-200cm)
    if (height < 0) {
        height = 0;
    }
    if (height > 200) {
        height = 200;
    }
    
    return (uint16_t)height;
}

uint16_t HeightController::getCurrentHeight() const {
    return currentReading_.calculated_height_cm;
}

uint16_t HeightController::getRawDistance() const {
    return currentReading_.raw_distance_mm;
}

uint16_t HeightController::getFilteredDistance() const {
    return currentReading_.filtered_distance_mm;
}

bool HeightController::isValid() const {
    return currentReading_.validity == ReadingValidity::VALID;
}

const HeightReading& HeightController::getReading() const {
    return currentReading_;
}

ReadingValidity HeightController::getValidity() const {
    return currentReading_.validity;
}

void HeightController::resetFilter() {
    filter_.reset();
    Logger::info(TAG, "Filter reset");
}

bool HeightController::calibrate(uint16_t known_height_cm) {
    // For floor-pointing sensor under desk:
    // calibration_constant = known_height - (sensor_reading / 10)
    // This offset accounts for sensor mounting position
    
    if (!sensorInitialized_) {
        Logger::error(TAG, "Cannot calibrate: sensor not initialized");
        return false;
    }
    
    // Take a few readings to get stable measurement
    uint32_t sum = 0;
    int validReadings = 0;
    const int NUM_SAMPLES = 10;
    
    Logger::info(TAG, "Calibrating at known height: %d cm", known_height_cm);
    
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Wait for fresh data
        while (!sensor_.isDataReady()) {
            delay(10);
        }
        
        uint16_t raw = readSensor();
        if (validateReading(raw) == ReadingValidity::VALID) {
            sum += raw;
            validReadings++;
        }
        
        delay(50);  // Brief pause between readings
    }
    
    if (validReadings < NUM_SAMPLES / 2) {
        Logger::error(TAG, "Calibration failed: too few valid readings (%d/%d)",
                      validReadings, NUM_SAMPLES);
        return false;
    }
    
    uint16_t avg_reading_mm = sum / validReadings;
    
    // calibration_constant = known_height - (sensor_reading / 10)
    // This gives us the offset to add to future readings
    int16_t calibration_constant = (int16_t)known_height_cm - (int16_t)(avg_reading_mm / 10);
    
    Logger::info(TAG, "Calibration: avg reading = %d mm, constant = %d cm",
                 avg_reading_mm, calibration_constant);
    
    // Save to system configuration
    if (!SystemConfig.setCalibrationConstant(calibration_constant)) {
        Logger::error(TAG, "Failed to save calibration constant");
        return false;
    }
    
    // Reset filter to start fresh with calibrated readings
    resetFilter();
    
    Logger::info(TAG, "Calibration successful!");
    return true;
}

bool HeightController::isSensorReady() const {
    return sensorInitialized_;
}

unsigned long HeightController::getReadingAge() const {
    return millis() - currentReading_.timestamp_ms;
}

String HeightController::toJson() const {
    String json = "{";
    json += "\"height\":" + String(currentReading_.calculated_height_cm) + ",";
    json += "\"rawDistance\":" + String(currentReading_.raw_distance_mm) + ",";
    json += "\"filteredDistance\":" + String(currentReading_.filtered_distance_mm) + ",";
    json += "\"valid\":" + String(isValid() ? "true" : "false") + ",";
    json += "\"age\":" + String(getReadingAge());
    json += "}";
    return json;
}

// =============================================================================
// Multi-Zone Diagnostic Methods (per 002-multi-zone-filtering Phase 5)
// =============================================================================

uint8_t HeightController::getValidZoneCount() const {
    return lastConsensus_.valid_zone_count;
}

uint8_t HeightController::getOutlierCount() const {
    return lastConsensus_.outlier_count;
}

const ConsensusResult& HeightController::getLastConsensus() const {
    return lastConsensus_;
}

String HeightController::getZoneDiagnostics() const {
    // Get fresh sensor data for diagnostics
    VL53L5CX_ResultsData results;
    
    // Note: This is a const method, so we can't call sensor_.getRangingData()
    // Instead, return cached info from lastConsensus_ plus general structure
    String json = "{";
    json += "\"validZones\":" + String(lastConsensus_.valid_zone_count) + ",";
    json += "\"outliers\":" + String(lastConsensus_.outlier_count) + ",";
    json += "\"consensusDistance\":" + String(lastConsensus_.consensus_distance_mm) + ",";
    json += "\"reliable\":" + String(lastConsensus_.is_reliable ? "true" : "false") + ",";
    json += "\"totalZones\":" + String(MULTI_ZONE_TOTAL_ZONES) + ",";
    json += "\"minValidZones\":" + String(MULTI_ZONE_MIN_VALID_ZONES) + ",";
    json += "\"outlierThresholdMm\":" + String(MULTI_ZONE_OUTLIER_THRESHOLD_MM);
    json += "}";
    return json;
}

// =============================================================================
// Multi-Zone Filtering Implementation (per 002-multi-zone-filtering feature)
// =============================================================================

uint16_t HeightController::computeMedian(uint16_t* values, uint8_t count) {
    if (count == 0) {
        return 0;
    }
    if (count == 1) {
        return values[0];
    }
    
    // In-place insertion sort - fast for small arrays (n ≤ 16)
    for (uint8_t i = 1; i < count; i++) {
        uint16_t key = values[i];
        int8_t j = i - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = key;
    }
    
    // Return middle element
    // For even count: return lower middle (index count/2 - 1)
    // For odd count: return middle (index count/2)
    if (count % 2 == 0) {
        return values[count / 2 - 1];
    } else {
        return values[count / 2];
    }
}

uint16_t HeightController::computeMean(uint16_t* values, uint8_t count) {
    if (count == 0) {
        return 0;
    }
    
    // Use uint32_t accumulator for overflow safety
    // Max possible: 16 zones × 65535 = 1,048,560 (fits in uint32_t)
    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; i++) {
        sum += values[i];
    }
    
    return static_cast<uint16_t>(sum / count);
}

void HeightController::filterOutliers(uint16_t* values, uint8_t count, uint16_t median,
                                       bool* keep_flags, uint8_t& kept_count) {
    kept_count = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        // Calculate absolute deviation from median
        uint16_t deviation;
        if (values[i] >= median) {
            deviation = values[i] - median;
        } else {
            deviation = median - values[i];
        }
        
        // Keep if within threshold (inclusive)
        if (deviation <= MULTI_ZONE_OUTLIER_THRESHOLD_MM) {
            keep_flags[i] = true;
            kept_count++;
        } else {
            keep_flags[i] = false;
        }
    }
}

bool HeightController::isZoneValid(uint8_t status, uint16_t distance) const {
    // Reject explicit invalid status codes
    if (status == 0 || status == 255) {
        return false;
    }
    
    // Accept only high-confidence status codes: 5, 6, 9
    // Per spec clarification: reject undefined codes (1-4, 7-8, 10+) conservatively
    if (status != 5 && status != 6 && status != 9) {
        return false;
    }
    
    // Range validation
    if (distance < SENSOR_MIN_VALID_MM || distance > SENSOR_MAX_RANGE_MM) {
        return false;
    }
    
    return true;
}

ConsensusResult HeightController::computeMultiZoneConsensus(const VL53L5CX_ResultsData& results) {
    ConsensusResult consensus;
    consensus.consensus_distance_mm = 0;
    consensus.valid_zone_count = 0;
    consensus.outlier_count = 0;
    consensus.is_reliable = false;
    
    // Step 1: Extract and validate all 16 zones
    uint16_t valid_distances[MULTI_ZONE_TOTAL_ZONES];
    uint8_t valid_count = 0;
    
    // Debug: Log all zone values periodically
    static unsigned long lastZoneLog = 0;
    bool logZones = (millis() - lastZoneLog > 5000);  // Every 5 seconds
    if (logZones) {
        Logger::debug(TAG, "=== Zone data dump ===");
    }
    
    for (uint8_t zone = 0; zone < MULTI_ZONE_TOTAL_ZONES; zone++) {
        // Access zone data (NB_TARGET_PER_ZONE = 1 in our config)
        uint8_t status = results.target_status[zone * VL53L5CX_NB_TARGET_PER_ZONE];
        int16_t distance_signed = results.distance_mm[zone * VL53L5CX_NB_TARGET_PER_ZONE];
        
        // Convert to unsigned (negative values are invalid)
        uint16_t distance = (distance_signed > 0) ? static_cast<uint16_t>(distance_signed) : 0;
        
        if (logZones) {
            Logger::debug(TAG, "Zone %2d: status=%d, dist=%4dmm %s", 
                         zone, status, distance,
                         isZoneValid(status, distance) ? "VALID" : "invalid");
        }
        
        if (isZoneValid(status, distance)) {
            valid_distances[valid_count] = distance;
            valid_count++;
        }
    }
    
    if (logZones) {
        lastZoneLog = millis();
    }
    
    consensus.valid_zone_count = valid_count;
    
    // Step 2: Check minimum zone threshold
    if (valid_count < MULTI_ZONE_MIN_VALID_ZONES) {
        Logger::warn(TAG, "Insufficient valid zones: %d (min %d)", 
                     valid_count, MULTI_ZONE_MIN_VALID_ZONES);
        consensus.is_reliable = false;
        return consensus;
    }
    
    // Step 3: Compute median (need copy since computeMedian sorts in-place)
    uint16_t median_input[MULTI_ZONE_TOTAL_ZONES];
    memcpy(median_input, valid_distances, valid_count * sizeof(uint16_t));
    uint16_t median = computeMedian(median_input, valid_count);
    
    // Step 4: Filter outliers
    bool keep_flags[MULTI_ZONE_TOTAL_ZONES];
    uint8_t kept_count = 0;
    filterOutliers(valid_distances, valid_count, median, keep_flags, kept_count);
    
    consensus.outlier_count = valid_count - kept_count;
    
    // Step 5: Compute mean of non-outliers
    if (kept_count == 0) {
        // Edge case: all valid zones are outliers (should be rare)
        Logger::warn(TAG, "All %d valid zones are outliers!", valid_count);
        consensus.is_reliable = false;
        return consensus;
    }
    
    // Collect non-outlier values
    uint16_t kept_values[MULTI_ZONE_TOTAL_ZONES];
    uint8_t kept_index = 0;
    for (uint8_t i = 0; i < valid_count; i++) {
        if (keep_flags[i]) {
            kept_values[kept_index++] = valid_distances[i];
        }
    }
    
    consensus.consensus_distance_mm = computeMean(kept_values, kept_count);
    consensus.is_reliable = true;
    
    Logger::debug(TAG, "Multi-zone consensus: %dmm (%d zones, %d outliers, median %dmm)",
                  consensus.consensus_distance_mm, valid_count, 
                  consensus.outlier_count, median);
    
    return consensus;
}
