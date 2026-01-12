/**
 * @file SystemConfiguration.h
 * @brief System configuration management with NVS persistence
 * 
 * Manages all configurable parameters:
 * - Calibration constant for height calculation
 * - Safety limits (min/max height)
 * - Movement parameters (tolerance, stabilization, timeout)
 * - Filter settings
 * 
 * WiFi credentials are managed separately via secrets.h
 * 
 * Per data-model.md Section 5: System Configuration
 */

#ifndef SYSTEM_CONFIGURATION_H
#define SYSTEM_CONFIGURATION_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

/**
 * @class SystemConfiguration
 * @brief Singleton for managing system configuration with NVS persistence
 * 
 * Usage:
 *   SystemConfig.init();  // Load from NVS or use defaults
 *   uint16_t cal = SystemConfig.getCalibrationConstant();
 *   SystemConfig.setCalibrationConstant(newValue);  // Auto-saves to NVS
 */
class SystemConfiguration {
public:
    /**
     * @brief Get singleton instance
     * @return SystemConfiguration& Reference to singleton
     */
    static SystemConfiguration& getInstance();
    
    /**
     * @brief Initialize configuration (load from NVS or use defaults)
     * @return true if loaded successfully, false if using defaults
     */
    bool init();
    
    /**
     * @brief Check if calibration has been performed
     * @return true if calibration_constant != 0
     */
    bool isCalibrated() const;
    
    // =========================================================================
    // Getters
    // =========================================================================
    
    /**
     * @brief Get calibration constant
     * @return int16_t Calibration constant in cm (0 = uncalibrated)
     */
    int16_t getCalibrationConstant() const;
    
    /**
     * @brief Get minimum safe height
     * @return uint16_t Min height in cm
     */
    uint16_t getMinHeight() const;
    
    /**
     * @brief Get maximum safe height
     * @return uint16_t Max height in cm
     */
    uint16_t getMaxHeight() const;
    
    /**
     * @brief Get target height tolerance
     * @return uint16_t Tolerance in mm
     */
    uint16_t getTolerance() const;
    
    /**
     * @brief Get stabilization duration
     * @return uint16_t Duration in ms
     */
    uint16_t getStabilizationDuration() const;
    
    /**
     * @brief Get movement timeout
     * @return uint16_t Timeout in ms
     */
    uint16_t getMovementTimeout() const;
    
    /**
     * @brief Get filter window size
     * @return uint8_t Window size (3-10)
     */
    uint8_t getFilterWindowSize() const;
    
    // =========================================================================
    // Setters (auto-save to NVS)
    // =========================================================================
    
    /**
     * @brief Set calibration constant
     * @param value Constant in cm
     * @return true if saved successfully
     */
    bool setCalibrationConstant(int16_t value);
    
    /**
     * @brief Set minimum safe height
     * @param value Min height in cm
     * @return true if saved successfully
     */
    bool setMinHeight(uint16_t value);
    
    /**
     * @brief Set maximum safe height
     * @param value Max height in cm
     * @return true if saved successfully
     */
    bool setMaxHeight(uint16_t value);
    
    /**
     * @brief Set target height tolerance
     * @param value Tolerance in mm
     * @return true if saved successfully
     */
    bool setTolerance(uint16_t value);
    
    /**
     * @brief Set stabilization duration
     * @param value Duration in ms
     * @return true if saved successfully
     */
    bool setStabilizationDuration(uint16_t value);
    
    /**
     * @brief Set movement timeout
     * @param value Timeout in ms
     * @return true if saved successfully
     */
    bool setMovementTimeout(uint16_t value);
    
    /**
     * @brief Set filter window size
     * @param value Window size (clamped to 3-10)
     * @return true if saved successfully
     */
    bool setFilterWindowSize(uint8_t value);
    
    // =========================================================================
    // Validation
    // =========================================================================
    
    /**
     * @brief Validate a height value against current limits
     * @param height Height in cm
     * @return true if min <= height <= max
     */
    bool isValidHeight(uint16_t height) const;
    
    /**
     * @brief Reset all settings to factory defaults
     * @return true if cleared successfully
     */
    bool factoryReset();
    
    /**
     * @brief Get all config as JSON string (for API)
     * @return String JSON representation
     */
    String toJson() const;

private:
    // Singleton pattern
    SystemConfiguration();
    SystemConfiguration(const SystemConfiguration&) = delete;
    SystemConfiguration& operator=(const SystemConfiguration&) = delete;
    
    // NVS preferences
    Preferences preferences_;
    bool initialized_;
    
    // Cached values (avoid frequent NVS reads)
    int16_t calibrationConstant_;
    uint16_t minHeight_;
    uint16_t maxHeight_;
    uint16_t tolerance_;
    uint16_t stabilizationDuration_;
    uint16_t movementTimeout_;
    uint8_t filterWindowSize_;
    
    /**
     * @brief Load all values from NVS
     */
    void loadFromNVS();
    
    /**
     * @brief Apply factory defaults to cached values
     */
    void applyDefaults();
    
    /**
     * @brief Save a single uint16_t value to NVS
     * @param key NVS key
     * @param value Value to save
     * @return true if successful
     */
    bool saveUInt16(const char* key, uint16_t value);
    
    /**
     * @brief Save a single uint8_t value to NVS
     * @param key NVS key
     * @param value Value to save
     * @return true if successful
     */
    bool saveUInt8(const char* key, uint8_t value);
    
    /**
     * @brief Save a string value to NVS
     * @param key NVS key
     * @param value Value to save
     * @return true if successful
     */
    bool saveString(const char* key, const String& value);
};

// Global shorthand reference
extern SystemConfiguration& SystemConfig;

#endif // SYSTEM_CONFIGURATION_H
