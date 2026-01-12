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
 * @struct ConsensusResult
 * @brief Multi-zone consensus result per data-model.md Section 2
 * 
 * Aggregated distance estimate from multiple valid zones after outlier filtering.
 * Used for spatial filtering stage before temporal moving average.
 */
struct ConsensusResult {
    uint16_t consensus_distance_mm;   ///< Median-filtered mean of valid zones
    uint8_t valid_zone_count;         ///< Number of zones that passed validation (0-16)
    uint8_t outlier_count;            ///< Number of zones excluded as outliers
    bool is_reliable;                 ///< true if >= 4 valid zones (per FR-007)
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
    
    // =========================================================================
    // Multi-Zone Diagnostic Methods (per 002-multi-zone-filtering Phase 5)
    // =========================================================================
    
    /**
     * @brief Get number of valid zones from last consensus computation
     * @return uint8_t Count of zones that passed validation (0-16)
     */
    uint8_t getValidZoneCount() const;
    
    /**
     * @brief Get number of outliers filtered from last consensus computation
     * @return uint8_t Count of zones excluded as outliers
     */
    uint8_t getOutlierCount() const;
    
    /**
     * @brief Get last consensus result for diagnostics
     * @return const ConsensusResult& Last computed consensus
     */
    const ConsensusResult& getLastConsensus() const;
    
    /**
     * @brief Get zone diagnostics as JSON array
     * 
     * Returns detailed per-zone information for debugging.
     * Format: [{"zone":0,"status":5,"distance":1450,"valid":true}, ...]
     * 
     * @return String JSON array with zone details
     */
    String getZoneDiagnostics() const;

private:
    SparkFun_VL53L5CX sensor_;
    MovingAverageFilter filter_;
    HeightReading currentReading_;
    bool sensorInitialized_;
    ConsensusResult lastConsensus_;  ///< Cached for diagnostics (P3)
    
    /**
     * @brief Read raw value from sensor (legacy single-zone)
     * @return uint16_t Distance in mm, or 0 on error
     * @deprecated Use computeMultiZoneConsensus() instead
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
    
    // =========================================================================
    // Multi-Zone Filtering Methods (per 002-multi-zone-filtering feature)
    // =========================================================================
    
    /**
     * @brief Compute consensus distance from all 16 sensor zones
     * 
     * Two-stage spatial filtering:
     * 1. Validate each zone (status codes, range)
     * 2. Compute median of valid zones
     * 3. Filter outliers (>30mm from median)
     * 4. Compute mean of remaining non-outliers
     * 
     * @param results Sensor data structure with 16 zones
     * @return ConsensusResult with distance, counts, and reliability flag
     */
    ConsensusResult computeMultiZoneConsensus(const VL53L5CX_ResultsData& results);
    
    /**
     * @brief Check if a single zone measurement is valid
     * 
     * Validates status codes (5, 6, 9 valid; 0, 255 invalid) and range.
     * 
     * @param status Target status code from sensor
     * @param distance Distance reading in mm
     * @return true if zone passes validation
     */
    bool isZoneValid(uint8_t status, uint16_t distance) const;
    
    /**
     * @brief Calculate median of an array (for outlier detection)
     * 
     * Uses in-place insertion sort for small arrays.
     * For even count, returns lower middle value.
     * 
     * @param values Array of distances (may be modified)
     * @param count Number of elements (1-16)
     * @return Median value in mm
     */
    static uint16_t computeMedian(uint16_t* values, uint8_t count);
    
    /**
     * @brief Calculate arithmetic mean of an array
     * 
     * Uses uint32_t accumulator for overflow safety.
     * 
     * @param values Array of distances
     * @param count Number of elements (1-16)
     * @return Mean value in mm
     */
    static uint16_t computeMean(uint16_t* values, uint8_t count);
    
    /**
     * @brief Filter outliers based on deviation from median
     * 
     * Marks zones as outliers if |value - median| > 30mm threshold.
     * 
     * @param values Array of distances
     * @param count Number of elements
     * @param median Pre-computed median value
     * @param keep_flags Output array of bools (true = keep, false = outlier)
     * @param kept_count Output count of non-outlier values
     */
    static void filterOutliers(uint16_t* values, uint8_t count, uint16_t median,
                               bool* keep_flags, uint8_t& kept_count);
};

#endif // HEIGHT_CONTROLLER_H
