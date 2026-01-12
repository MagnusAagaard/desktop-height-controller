/**
 * @file MovingAverageFilter.h
 * @brief Standalone MovingAverageFilter for native testing
 * 
 * This is a self-contained copy for native unit tests that don't have
 * access to Arduino-specific Config.h
 */

#ifndef MOVING_AVERAGE_FILTER_H
#define MOVING_AVERAGE_FILTER_H

#include <stdint.h>

// Constants (normally from Config.h)
constexpr uint8_t MIN_FILTER_WINDOW_SIZE = 3;
constexpr uint8_t MAX_FILTER_WINDOW_SIZE = 10;
constexpr uint8_t DEFAULT_FILTER_WINDOW_SIZE = 5;

class MovingAverageFilter {
public:
    explicit MovingAverageFilter(uint8_t windowSize = DEFAULT_FILTER_WINDOW_SIZE);
    ~MovingAverageFilter();
    
    void addSample(uint16_t sample);
    uint16_t getAverage() const;
    uint16_t getLastSample() const;
    uint8_t getSampleCount() const;
    uint8_t getWindowSize() const;
    bool isEmpty() const;
    bool isFull() const;
    void reset();
    
private:
    static uint8_t clampWindowSize(uint8_t size);
    
    uint16_t* buffer_;
    uint8_t windowSize_;
    uint8_t head_;
    uint8_t sampleCount_;
};

// Inline implementation for header-only usage
inline MovingAverageFilter::MovingAverageFilter(uint8_t windowSize)
    : windowSize_(clampWindowSize(windowSize))
    , head_(0)
    , sampleCount_(0)
{
    buffer_ = new uint16_t[windowSize_];
    for (uint8_t i = 0; i < windowSize_; i++) {
        buffer_[i] = 0;
    }
}

inline MovingAverageFilter::~MovingAverageFilter() {
    delete[] buffer_;
}

inline uint8_t MovingAverageFilter::clampWindowSize(uint8_t size) {
    if (size < MIN_FILTER_WINDOW_SIZE) return MIN_FILTER_WINDOW_SIZE;
    if (size > MAX_FILTER_WINDOW_SIZE) return MAX_FILTER_WINDOW_SIZE;
    return size;
}

inline void MovingAverageFilter::addSample(uint16_t sample) {
    buffer_[head_] = sample;
    head_ = (head_ + 1) % windowSize_;
    if (sampleCount_ < windowSize_) {
        sampleCount_++;
    }
}

inline uint16_t MovingAverageFilter::getAverage() const {
    if (sampleCount_ == 0) return 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < sampleCount_; i++) {
        sum += buffer_[i];
    }
    return static_cast<uint16_t>(sum / sampleCount_);
}

inline uint16_t MovingAverageFilter::getLastSample() const {
    if (sampleCount_ == 0) return 0;
    uint8_t lastIndex = (head_ == 0) ? (windowSize_ - 1) : (head_ - 1);
    return buffer_[lastIndex];
}

inline uint8_t MovingAverageFilter::getSampleCount() const {
    return sampleCount_;
}

inline uint8_t MovingAverageFilter::getWindowSize() const {
    return windowSize_;
}

inline bool MovingAverageFilter::isEmpty() const {
    return sampleCount_ == 0;
}

inline bool MovingAverageFilter::isFull() const {
    return sampleCount_ >= windowSize_;
}

inline void MovingAverageFilter::reset() {
    head_ = 0;
    sampleCount_ = 0;
    for (uint8_t i = 0; i < windowSize_; i++) {
        buffer_[i] = 0;
    }
}

#endif // MOVING_AVERAGE_FILTER_H
