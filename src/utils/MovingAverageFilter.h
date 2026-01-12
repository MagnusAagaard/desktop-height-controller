/**
 * @file MovingAverageFilter.h
 * @brief Circular buffer implementation of moving average filter for sensor smoothing
 * 
 * Implements a configurable-size moving average filter using a circular buffer.
 * Designed for smoothing VL53L5CX distance sensor readings.
 * 
 * Per spec (FR-001a): "Moving average filter applied to smooth sensor noise"
 * Per data-model.md: Filter window size configurable (default 5, range 3-10)
 */

#ifndef MOVING_AVERAGE_FILTER_H
#define MOVING_AVERAGE_FILTER_H

#include <stdint.h>
#include "../Config.h"

/**
 * @class MovingAverageFilter
 * @brief Implements a moving average filter using a circular buffer
 * 
 * Usage:
 *   MovingAverageFilter filter(5);  // 5-sample window
 *   filter.addSample(rawReading);
 *   uint16_t smoothed = filter.getAverage();
 */
class MovingAverageFilter {
public:
    /**
     * @brief Construct a new Moving Average Filter
     * @param windowSize Number of samples to average (clamped to MIN_FILTER_WINDOW_SIZE..MAX_FILTER_WINDOW_SIZE)
     */
    explicit MovingAverageFilter(uint8_t windowSize = DEFAULT_FILTER_WINDOW_SIZE);
    
    /**
     * @brief Destroy the Moving Average Filter
     */
    ~MovingAverageFilter();
    
    /**
     * @brief Add a new sample to the filter
     * 
     * If buffer is full, oldest sample is overwritten (circular buffer behavior).
     * 
     * @param sample Raw sensor reading (0-4000mm for VL53L5CX)
     */
    void addSample(uint16_t sample);
    
    /**
     * @brief Get the current moving average
     * 
     * - If no samples: returns 0
     * - If partial window: returns average of available samples
     * - If full window: returns average of windowSize samples
     * 
     * @return uint16_t Averaged value (same unit as input samples)
     */
    uint16_t getAverage() const;
    
    /**
     * @brief Get the most recently added sample
     * @return uint16_t Last sample value, or 0 if empty
     */
    uint16_t getLastSample() const;
    
    /**
     * @brief Get the current number of samples in buffer
     * @return uint8_t Sample count (0 to windowSize)
     */
    uint8_t getSampleCount() const;
    
    /**
     * @brief Get the configured window size
     * @return uint8_t Window size (3-10)
     */
    uint8_t getWindowSize() const;
    
    /**
     * @brief Check if filter has no samples
     * @return true if no samples have been added or reset was called
     */
    bool isEmpty() const;
    
    /**
     * @brief Check if filter has a full window of samples
     * @return true if sampleCount >= windowSize
     */
    bool isFull() const;
    
    /**
     * @brief Clear all samples from the filter
     * 
     * Called when sensor is recalibrated or on error recovery.
     */
    void reset();

private:
    uint16_t* buffer_;       ///< Circular buffer for samples
    uint8_t windowSize_;     ///< Configured window size (3-10)
    uint8_t head_;           ///< Index of next write position
    uint8_t sampleCount_;    ///< Number of valid samples in buffer (0 to windowSize)
    
    /**
     * @brief Clamp window size to valid range
     * @param size Requested window size
     * @return uint8_t Clamped to MIN_FILTER_WINDOW_SIZE..MAX_FILTER_WINDOW_SIZE
     */
    static uint8_t clampWindowSize(uint8_t size);
};

#endif // MOVING_AVERAGE_FILTER_H
