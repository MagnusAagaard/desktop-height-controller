/**
 * @file HeightController.cpp
 * @brief Implementation of height sensing and calculation
 */

#include "HeightController.h"
#include "utils/Logger.h"

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
    
    // Read raw sensor value
    uint16_t raw = readSensor();
    currentReading_.raw_distance_mm = raw;
    currentReading_.timestamp_ms = millis();
    
    // Validate reading
    ReadingValidity validity = validateReading(raw);
    currentReading_.validity = validity;
    
    if (validity != ReadingValidity::VALID) {
        Logger::warn(TAG, "Invalid reading: %d mm", raw);
        return;
    }
    
    // Apply moving average filter
    filter_.addSample(raw);
    currentReading_.filtered_distance_mm = filter_.getAverage();
    
    // Calculate height
    currentReading_.calculated_height_cm = calculateHeight(currentReading_.filtered_distance_mm);
    
    Logger::debug(TAG, "Raw: %dmm, Filtered: %dmm, Height: %dcm",
                  raw, currentReading_.filtered_distance_mm, 
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
