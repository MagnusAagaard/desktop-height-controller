/**
 * @file multi_zone_filtering.h
 * @brief Standalone multi-zone filtering utilities for unit testing
 * 
 * This header provides the multi-zone filtering functions as standalone
 * implementations that can be tested without the full HeightController
 * infrastructure (no sensor, no WiFi, etc).
 * 
 * These implementations mirror HeightController's private methods exactly.
 */

#ifndef MULTI_ZONE_FILTERING_H
#define MULTI_ZONE_FILTERING_H

#include <cstdint>

// Constants (matching Config.h)
constexpr uint16_t MULTI_ZONE_OUTLIER_THRESHOLD_MM = 30;
constexpr uint8_t MULTI_ZONE_MIN_VALID_ZONES = 4;
constexpr uint8_t MULTI_ZONE_TOTAL_ZONES = 16;
constexpr uint16_t SENSOR_MIN_VALID_MM = 10;
constexpr uint16_t SENSOR_MAX_RANGE_MM = 4000;

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
inline uint16_t computeMedian(uint16_t* values, uint8_t count) {
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

/**
 * @brief Calculate arithmetic mean of an array
 * 
 * Uses uint32_t accumulator for overflow safety.
 * 
 * @param values Array of distances
 * @param count Number of elements (1-16)
 * @return Mean value in mm
 */
inline uint16_t computeMean(uint16_t* values, uint8_t count) {
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
inline void filterOutliers(uint16_t* values, uint8_t count, uint16_t median,
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

/**
 * @brief Check if a single zone measurement is valid
 * 
 * Validates status codes (5, 6, 9 valid; 0, 255 invalid) and range.
 * 
 * @param status Target status code from sensor
 * @param distance Distance reading in mm
 * @return true if zone passes validation
 */
inline bool isZoneValid(uint8_t status, uint16_t distance) {
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

#endif // MULTI_ZONE_FILTERING_H
