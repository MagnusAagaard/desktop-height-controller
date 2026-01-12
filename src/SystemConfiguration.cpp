/**
 * @file SystemConfiguration.cpp
 * @brief Implementation of system configuration management
 */

#include "SystemConfiguration.h"
#include "utils/Logger.h"

static const char* TAG = "SystemConfig";

// Global reference
SystemConfiguration& SystemConfig = SystemConfiguration::getInstance();

// NVS Keys
static const char* KEY_CAL_CONST = "cal_const";
static const char* KEY_MIN_HEIGHT = "min_h";
static const char* KEY_MAX_HEIGHT = "max_h";
static const char* KEY_TOLERANCE = "tolerance";
static const char* KEY_STAB_DUR = "stab_dur";
static const char* KEY_MOVE_TIMEOUT = "move_timeout";
static const char* KEY_FILTER_WIN = "filter_win";

SystemConfiguration::SystemConfiguration()
    : initialized_(false)
{
    applyDefaults();
}

SystemConfiguration& SystemConfiguration::getInstance() {
    static SystemConfiguration instance;
    return instance;
}

bool SystemConfiguration::init() {
    if (initialized_) {
        Logger::warn(TAG, "Already initialized");
        return true;
    }
    
    // Open NVS namespace in read-write mode
    bool success = preferences_.begin(NVS_NAMESPACE_CONFIG, false);
    if (!success) {
        Logger::error(TAG, "Failed to open NVS namespace '%s'", NVS_NAMESPACE_CONFIG);
        applyDefaults();
        return false;
    }
    
    loadFromNVS();
    initialized_ = true;
    
    Logger::info(TAG, "Initialized - calibrated: %s, min: %dcm, max: %dcm",
                 isCalibrated() ? "yes" : "no",
                 minHeight_,
                 maxHeight_);
    
    return true;
}

void SystemConfiguration::applyDefaults() {
    calibrationConstant_ = DEFAULT_CALIBRATION_CONSTANT_CM;
    minHeight_ = DEFAULT_MIN_HEIGHT_CM;
    maxHeight_ = DEFAULT_MAX_HEIGHT_CM;
    tolerance_ = DEFAULT_TOLERANCE_MM;
    stabilizationDuration_ = DEFAULT_STABILIZATION_DURATION_MS;
    movementTimeout_ = DEFAULT_MOVEMENT_TIMEOUT_MS;
    filterWindowSize_ = DEFAULT_FILTER_WINDOW_SIZE;
}

void SystemConfiguration::loadFromNVS() {
    // Load each value, falling back to current (default) value if not found
    // Cast calibration constant to int16_t (can be negative for floor-mounted sensor)
    calibrationConstant_ = (int16_t)preferences_.getUShort(KEY_CAL_CONST, (uint16_t)calibrationConstant_);
    minHeight_ = preferences_.getUShort(KEY_MIN_HEIGHT, minHeight_);
    maxHeight_ = preferences_.getUShort(KEY_MAX_HEIGHT, maxHeight_);
    tolerance_ = preferences_.getUShort(KEY_TOLERANCE, tolerance_);
    stabilizationDuration_ = preferences_.getUShort(KEY_STAB_DUR, stabilizationDuration_);
    movementTimeout_ = preferences_.getUShort(KEY_MOVE_TIMEOUT, movementTimeout_);
    filterWindowSize_ = preferences_.getUChar(KEY_FILTER_WIN, filterWindowSize_);
    // WiFi credentials are loaded from secrets.h at compile time, not from NVS
    
    // Validate and clamp filter window size
    if (filterWindowSize_ < MIN_FILTER_WINDOW_SIZE) {
        filterWindowSize_ = MIN_FILTER_WINDOW_SIZE;
    }
    if (filterWindowSize_ > MAX_FILTER_WINDOW_SIZE) {
        filterWindowSize_ = MAX_FILTER_WINDOW_SIZE;
    }
}

bool SystemConfiguration::isCalibrated() const {
    return calibrationConstant_ != 0;
}

// Getters
int16_t SystemConfiguration::getCalibrationConstant() const { return calibrationConstant_; }
uint16_t SystemConfiguration::getMinHeight() const { return minHeight_; }
uint16_t SystemConfiguration::getMaxHeight() const { return maxHeight_; }
uint16_t SystemConfiguration::getTolerance() const { return tolerance_; }
uint16_t SystemConfiguration::getStabilizationDuration() const { return stabilizationDuration_; }
uint16_t SystemConfiguration::getMovementTimeout() const { return movementTimeout_; }
uint8_t SystemConfiguration::getFilterWindowSize() const { return filterWindowSize_; }

// Setters with NVS persistence
bool SystemConfiguration::setCalibrationConstant(int16_t value) {
    // Store as uint16_t in NVS (bit pattern preserved)
    if (saveUInt16(KEY_CAL_CONST, (uint16_t)value)) {
        calibrationConstant_ = value;
        Logger::info(TAG, "Calibration constant set to %d cm", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::setMinHeight(uint16_t value) {
    if (value >= maxHeight_) {
        Logger::error(TAG, "Min height (%d) must be less than max height (%d)", value, maxHeight_);
        return false;
    }
    if (saveUInt16(KEY_MIN_HEIGHT, value)) {
        minHeight_ = value;
        Logger::info(TAG, "Min height set to %d cm", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::setMaxHeight(uint16_t value) {
    if (value <= minHeight_) {
        Logger::error(TAG, "Max height (%d) must be greater than min height (%d)", value, minHeight_);
        return false;
    }
    if (saveUInt16(KEY_MAX_HEIGHT, value)) {
        maxHeight_ = value;
        Logger::info(TAG, "Max height set to %d cm", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::setTolerance(uint16_t value) {
    // Clamp to reasonable range
    if (value < 5) value = 5;
    if (value > 50) value = 50;
    
    if (saveUInt16(KEY_TOLERANCE, value)) {
        tolerance_ = value;
        Logger::info(TAG, "Tolerance set to %d mm", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::setStabilizationDuration(uint16_t value) {
    // Clamp to reasonable range
    if (value < 500) value = 500;
    if (value > 10000) value = 10000;
    
    if (saveUInt16(KEY_STAB_DUR, value)) {
        stabilizationDuration_ = value;
        Logger::info(TAG, "Stabilization duration set to %d ms", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::setMovementTimeout(uint16_t value) {
    // Clamp to reasonable range
    if (value < 10000) value = 10000;
    if (value > 60000) value = 60000;
    
    if (saveUInt16(KEY_MOVE_TIMEOUT, value)) {
        movementTimeout_ = value;
        Logger::info(TAG, "Movement timeout set to %d ms", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::setFilterWindowSize(uint8_t value) {
    // Clamp to valid range
    if (value < MIN_FILTER_WINDOW_SIZE) value = MIN_FILTER_WINDOW_SIZE;
    if (value > MAX_FILTER_WINDOW_SIZE) value = MAX_FILTER_WINDOW_SIZE;
    
    if (saveUInt8(KEY_FILTER_WIN, value)) {
        filterWindowSize_ = value;
        Logger::info(TAG, "Filter window size set to %d", value);
        return true;
    }
    return false;
}

bool SystemConfiguration::isValidHeight(uint16_t height) const {
    return height >= minHeight_ && height <= maxHeight_;
}

bool SystemConfiguration::factoryReset() {
    Logger::warn(TAG, "Factory reset initiated");
    
    preferences_.clear();
    applyDefaults();
    
    // Re-save defaults to NVS
    bool success = true;
    success &= saveUInt16(KEY_CAL_CONST, calibrationConstant_);
    success &= saveUInt16(KEY_MIN_HEIGHT, minHeight_);
    success &= saveUInt16(KEY_MAX_HEIGHT, maxHeight_);
    success &= saveUInt16(KEY_TOLERANCE, tolerance_);
    success &= saveUInt16(KEY_STAB_DUR, stabilizationDuration_);
    success &= saveUInt16(KEY_MOVE_TIMEOUT, movementTimeout_);
    success &= saveUInt8(KEY_FILTER_WIN, filterWindowSize_);
    // Don't save empty WiFi credentials
    
    if (success) {
        Logger::info(TAG, "Factory reset complete");
    } else {
        Logger::error(TAG, "Factory reset failed to save defaults");
    }
    
    return success;
}

String SystemConfiguration::toJson() const {
    String json = "{";
    json += "\"calibrationConstant\":" + String(calibrationConstant_) + ",";
    json += "\"minHeight\":" + String(minHeight_) + ",";
    json += "\"maxHeight\":" + String(maxHeight_) + ",";
    json += "\"tolerance\":" + String(tolerance_) + ",";
    json += "\"stabilizationDuration\":" + String(stabilizationDuration_) + ",";
    json += "\"movementTimeout\":" + String(movementTimeout_) + ",";
    json += "\"filterWindowSize\":" + String(filterWindowSize_) + ",";
    json += "\"isCalibrated\":" + String(isCalibrated() ? "true" : "false");
    json += "}";
    return json;
}

// Private save helpers
bool SystemConfiguration::saveUInt16(const char* key, uint16_t value) {
    size_t written = preferences_.putUShort(key, value);
    if (written == 0) {
        Logger::error(TAG, "Failed to save %s", key);
        return false;
    }
    return true;
}

bool SystemConfiguration::saveUInt8(const char* key, uint8_t value) {
    size_t written = preferences_.putUChar(key, value);
    if (written == 0) {
        Logger::error(TAG, "Failed to save %s", key);
        return false;
    }
    return true;
}

bool SystemConfiguration::saveString(const char* key, const String& value) {
    size_t written = preferences_.putString(key, value);
    if (written == 0 && value.length() > 0) {
        Logger::error(TAG, "Failed to save %s", key);
        return false;
    }
    return true;
}
