/**
 * @file MovingAverageFilter.cpp
 * @brief Implementation of circular buffer moving average filter
 */

#include "MovingAverageFilter.h"

MovingAverageFilter::MovingAverageFilter(uint8_t windowSize)
    : windowSize_(clampWindowSize(windowSize))
    , head_(0)
    , sampleCount_(0)
{
    buffer_ = new uint16_t[windowSize_];
    // Initialize buffer to 0
    for (uint8_t i = 0; i < windowSize_; i++) {
        buffer_[i] = 0;
    }
}

MovingAverageFilter::~MovingAverageFilter() {
    delete[] buffer_;
}

uint8_t MovingAverageFilter::clampWindowSize(uint8_t size) {
    if (size < MIN_FILTER_WINDOW_SIZE) {
        return MIN_FILTER_WINDOW_SIZE;
    }
    if (size > MAX_FILTER_WINDOW_SIZE) {
        return MAX_FILTER_WINDOW_SIZE;
    }
    return size;
}

void MovingAverageFilter::addSample(uint16_t sample) {
    buffer_[head_] = sample;
    head_ = (head_ + 1) % windowSize_;
    
    if (sampleCount_ < windowSize_) {
        sampleCount_++;
    }
}

uint16_t MovingAverageFilter::getAverage() const {
    if (sampleCount_ == 0) {
        return 0;
    }
    
    // Use uint32_t for sum to prevent overflow
    // Max sum: 10 samples * 65535 = 655350 (fits in uint32_t)
    uint32_t sum = 0;
    
    for (uint8_t i = 0; i < sampleCount_; i++) {
        sum += buffer_[i];
    }
    
    return static_cast<uint16_t>(sum / sampleCount_);
}

uint16_t MovingAverageFilter::getLastSample() const {
    if (sampleCount_ == 0) {
        return 0;
    }
    
    // head_ points to next write position, so last written is at (head_ - 1)
    // Handle wrap-around for circular buffer
    uint8_t lastIndex = (head_ == 0) ? (windowSize_ - 1) : (head_ - 1);
    return buffer_[lastIndex];
}

uint8_t MovingAverageFilter::getSampleCount() const {
    return sampleCount_;
}

uint8_t MovingAverageFilter::getWindowSize() const {
    return windowSize_;
}

bool MovingAverageFilter::isEmpty() const {
    return sampleCount_ == 0;
}

bool MovingAverageFilter::isFull() const {
    return sampleCount_ >= windowSize_;
}

void MovingAverageFilter::reset() {
    head_ = 0;
    sampleCount_ = 0;
    // Optionally clear buffer (not strictly necessary for correct operation)
    for (uint8_t i = 0; i < windowSize_; i++) {
        buffer_[i] = 0;
    }
}
