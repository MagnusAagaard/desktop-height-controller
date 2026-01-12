/**
 * @file HeightController.h
 * @brief Height measurement controller with VL53L5CX sensor integration
 * 
 * Responsible for:
 * - VL53L5CX ToF sensor initialization and reading
 * - Moving average filtering of sensor data
 * - Height calculation using calibration formula
 * - Validity checking of readings
 * 
 * Per FR-001: height_cm = calibration_constant_cm - (sensor_reading_mm / 10)
 * Per FR-001a: Moving average filter applied to smooth sensor noise
 */

#ifndef HEIGHT_CONTROLLER_H
#define HEIGHT_CONTROLLER_H

#include <Arduino.h>
#include <SparkFun_VL53L5CX_Library.h>
#include "Config.h"
#include "SystemConfiguration.h"
#include "utils/MovingAverageFilter.h"

/**
 * @enum ReadingValidity
 * @brief Status of sensor reading per data-model.md Section 1
 */
enum class ReadingValidity : uint8_t {
    VALID,      ///< Reading is within valid range and fresh
    INVALID,    ///< Sensor error or out-of-range reading
    STALE       ///< Reading is too old (> 1000ms)
};

/**
 * @struct HeightReading
 * @brief Complete height measurement data per data-model.md Section 1
 */
struct HeightReading {
    uint16_t raw_distance_mm;         ///< Unprocessed sensor reading
    uint16_t filtered_distance_mm;    ///< After moving average
    uint16_t calculated_height_cm;    ///< Final desk height
    unsigned long timestamp_ms;       ///< When reading was captured
    ReadingValidity validity;         ///< Reading quality status
};

/**
 * @class HeightController
 * @brief Manages height sensing and calculation
 * 
 * Usage:
 *   HeightController height;
 *   if (height.init()) {
 *       // In loop every 200ms:
 *       height.update();
 *       if (height.isValid()) {
 *           uint16_t h = height.getCurrentHeight();
 *       }
 *   }
 */
class HeightController {
public:
    /**
     * @brief Construct HeightController
     */
    HeightController();
    
    /**
     * @brief Initialize sensor and I2C
     * @return true if sensor initialized successfully
     */
    bool init();
    
    /**
     * @brief Update sensor reading (call at 5Hz / 200ms intervals)
     * 
     * Reads sensor, applies filter, calculates height.
     * Call this from main loop.
     */
    void update();
    
    /**
     * @brief Get current calculated height
     * @return uint16_t Height in cm, or 0 if invalid
     */
    uint16_t getCurrentHeight() const;
    
    /**
     * @brief Get current raw distance reading
     * @return uint16_t Distance in mm
     */
    uint16_t getRawDistance() const;
    
    /**
     * @brief Get current filtered distance reading
     * @return uint16_t Filtered distance in mm
     */
    uint16_t getFilteredDistance() const;
    
    /**
     * @brief Check if current reading is valid
     * @return true if reading is valid and fresh
     */
    bool isValid() const;
    
    /**
     * @brief Get complete reading structure
     * @return const HeightReading& Current reading data
     */
    const HeightReading& getReading() const;
    
    /**
     * @brief Get reading validity status
     * @return ReadingValidity Current status
     */
    ReadingValidity getValidity() const;
    
    /**
     * @brief Reset filter and clear history
     * 
     * Called after calibration or error recovery.
     */
    void resetFilter();
    
    /**
     * @brief Perform calibration at known height
     * 
     * Per FR-019: calibration_constant_cm = H_cm + (S_mm / 10)
     * 
     * @param known_height_cm Actual desk height when calibrating
     * @return true if calibration successful
     */
    bool calibrate(uint16_t known_height_cm);
    
    /**
     * @brief Check if sensor is initialized and operational
     * @return true if sensor is ready
     */
    bool isSensorReady() const;
    
    /**
     * @brief Get time since last valid reading
     * @return unsigned long Milliseconds since last update
     */
    unsigned long getReadingAge() const;
    
    /**
     * @brief Get reading as JSON string (for API/SSE)
     * @return String JSON representation
     */
    String toJson() const;

private:
    SparkFun_VL53L5CX sensor_;
    MovingAverageFilter filter_;
    HeightReading currentReading_;
    bool sensorInitialized_;
    
    /**
     * @brief Read raw value from sensor
     * @return uint16_t Distance in mm, or 0 on error
     */
    uint16_t readSensor();
    
    /**
     * @brief Validate a raw reading
     * @param reading Raw sensor value in mm
     * @return ReadingValidity Validity status
     */
    ReadingValidity validateReading(uint16_t reading) const;
    
    /**
     * @brief Calculate height from filtered distance
     * @param filtered_mm Filtered distance in mm
     * @return uint16_t Height in cm
     */
    uint16_t calculateHeight(uint16_t filtered_mm) const;
};

#endif // HEIGHT_CONTROLLER_H
