/**
 * @file test_two_stage.cpp
 * @brief Integration tests for two-stage filtering: spatial→temporal cascade
 * 
 * TDD Phase: These tests verify the complete filtering pipeline.
 * Per Constitution Principle I: Test-First Development
 * 
 * Two-stage filtering:
 * 1. Spatial: Multi-zone consensus (median→outlier filter→mean)
 * 2. Temporal: Moving average filter on consensus output
 * 
 * Tests simulate realistic sensor behavior including:
 * - Gradual temperature drift
 * - Sudden noise spikes
 * - Continuous stable readings
 */

#ifdef NATIVE_TEST
#include <ArduinoFake.h>
using namespace fakeit;
#else
#include <Arduino.h>
#endif
#include <unity.h>
#include <cstring>

// ============================================
// Constants (match Config.h)
// ============================================
constexpr uint16_t SENSOR_MIN_VALID_MM = 10;
constexpr uint16_t SENSOR_MAX_RANGE_MM = 4000;
constexpr uint16_t OUTLIER_THRESHOLD_MM = 30;
constexpr uint8_t MIN_VALID_ZONES = 4;
constexpr uint8_t MAX_ZONES = 16;
constexpr uint8_t FILTER_WINDOW_SIZE = 5;  // Match existing MovingAverageFilter

// ============================================
// Mock Data Structures
// ============================================

struct MockSensorData {
    int16_t distance_mm[MAX_ZONES];
    uint8_t target_status[MAX_ZONES];
};

struct ConsensusResult {
    uint16_t consensus_distance_mm;
    uint8_t valid_zone_count;
    uint8_t outlier_count;
    bool is_reliable;
};

// ============================================
// Mock MovingAverageFilter (simplified)
// ============================================

class MockMovingAverageFilter {
public:
    MockMovingAverageFilter(uint8_t window_size) 
        : window_size_(window_size), count_(0), sum_(0), head_(0) {
        memset(samples_, 0, sizeof(samples_));
    }
    
    void addSample(uint16_t sample) {
        if (count_ < window_size_) {
            samples_[count_++] = sample;
            sum_ += sample;
        } else {
            sum_ -= samples_[head_];
            samples_[head_] = sample;
            sum_ += sample;
            head_ = (head_ + 1) % window_size_;
        }
    }
    
    uint16_t getAverage() const {
        if (count_ == 0) return 0;
        return (uint16_t)(sum_ / count_);
    }
    
    bool isFull() const {
        return count_ >= window_size_;
    }
    
    void reset() {
        count_ = 0;
        sum_ = 0;
        head_ = 0;
        memset(samples_, 0, sizeof(samples_));
    }
    
    uint8_t getSampleCount() const {
        return count_;
    }

private:
    uint16_t samples_[10];  // Max window size
    uint8_t window_size_;
    uint8_t count_;
    uint32_t sum_;
    uint8_t head_;
};

// ============================================
// Utility Functions
// ============================================

bool isZoneValid(uint8_t status, uint16_t distance) {
    if (status == 0 || status == 255) return false;
    if (status != 5 && status != 6 && status != 9) return false;
    if (distance < SENSOR_MIN_VALID_MM) return false;
    if (distance > SENSOR_MAX_RANGE_MM) return false;
    return true;
}

uint16_t computeMedian(uint16_t* values, uint8_t count) {
    if (count == 0) return 0;
    uint16_t sorted[MAX_ZONES];
    memcpy(sorted, values, count * sizeof(uint16_t));
    for (uint8_t i = 1; i < count; i++) {
        uint16_t key = sorted[i];
        int8_t j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    return sorted[(count - 1) / 2];
}

uint16_t computeMean(uint16_t* values, uint8_t count) {
    if (count == 0) return 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return (uint16_t)(sum / count);
}

void filterOutliers(uint16_t* values, uint8_t count, uint16_t median,
                    bool* keep_flags, uint8_t& kept_count) {
    kept_count = 0;
    for (uint8_t i = 0; i < count; i++) {
        uint16_t diff = (values[i] > median) ? (values[i] - median) : (median - values[i]);
        keep_flags[i] = (diff <= OUTLIER_THRESHOLD_MM);
        if (keep_flags[i]) kept_count++;
    }
}

ConsensusResult computeMultiZoneConsensus(const MockSensorData& data) {
    ConsensusResult result = {0, 0, 0, false};
    
    uint16_t valid_distances[MAX_ZONES];
    uint8_t valid_count = 0;
    
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        uint16_t dist = (uint16_t)data.distance_mm[i];
        if (isZoneValid(data.target_status[i], dist)) {
            valid_distances[valid_count++] = dist;
        }
    }
    
    result.valid_zone_count = valid_count;
    
    if (valid_count < MIN_VALID_ZONES) {
        result.is_reliable = false;
        return result;
    }
    
    uint16_t median = computeMedian(valid_distances, valid_count);
    
    bool keep_flags[MAX_ZONES];
    uint8_t kept_count = 0;
    filterOutliers(valid_distances, valid_count, median, keep_flags, kept_count);
    
    result.outlier_count = valid_count - kept_count;
    
    if (kept_count == 0) {
        result.consensus_distance_mm = median;
        result.is_reliable = false;
        return result;
    }
    
    uint16_t filtered_values[MAX_ZONES];
    uint8_t filtered_count = 0;
    for (uint8_t i = 0; i < valid_count; i++) {
        if (keep_flags[i]) {
            filtered_values[filtered_count++] = valid_distances[i];
        }
    }
    
    result.consensus_distance_mm = computeMean(filtered_values, filtered_count);
    result.is_reliable = (valid_count >= MIN_VALID_ZONES);
    
    return result;
}

/**
 * @brief Simulates the two-stage filtering pipeline
 */
class TwoStageFilter {
public:
    TwoStageFilter() : temporal_filter_(FILTER_WINDOW_SIZE) {}
    
    /**
     * @brief Process sensor data through both stages
     * @param data Mock sensor data (spatial input)
     * @return Final filtered height in mm
     */
    uint16_t process(const MockSensorData& data) {
        // Stage 1: Spatial consensus
        last_consensus_ = computeMultiZoneConsensus(data);
        
        if (!last_consensus_.is_reliable) {
            // Don't update temporal filter with unreliable data
            return temporal_filter_.getAverage();
        }
        
        // Stage 2: Temporal averaging
        temporal_filter_.addSample(last_consensus_.consensus_distance_mm);
        
        return temporal_filter_.getAverage();
    }
    
    const ConsensusResult& getLastConsensus() const {
        return last_consensus_;
    }
    
    void reset() {
        temporal_filter_.reset();
        last_consensus_ = {0, 0, 0, false};
    }
    
    bool isTemporalFilterFull() const {
        return temporal_filter_.isFull();
    }

private:
    MockMovingAverageFilter temporal_filter_;
    ConsensusResult last_consensus_;
};

// ============================================
// Test Fixtures
// ============================================

void initMockData(MockSensorData& data, uint16_t distance = 850, uint8_t status = 5) {
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        data.distance_mm[i] = distance;
        data.target_status[i] = status;
    }
}

void setUp(void) {
}

void tearDown(void) {
}

// ============================================
// Basic Pipeline Tests
// ============================================

/**
 * @test Single frame through pipeline produces reasonable output
 */
void test_two_stage_single_frame(void) {
    TwoStageFilter filter;
    MockSensorData data;
    initMockData(data, 850, 5);
    
    uint16_t result = filter.process(data);
    
    TEST_ASSERT_EQUAL_UINT16(850, result);
    TEST_ASSERT_TRUE(filter.getLastConsensus().is_reliable);
}

/**
 * @test Five frames fill temporal filter window
 */
void test_two_stage_window_filling(void) {
    TwoStageFilter filter;
    MockSensorData data;
    initMockData(data, 850, 5);
    
    // Process 5 frames
    for (int i = 0; i < 5; i++) {
        filter.process(data);
    }
    
    TEST_ASSERT_TRUE(filter.isTemporalFilterFull());
    TEST_ASSERT_EQUAL_UINT16(850, filter.process(data));
}

/**
 * @test Temporal filter smooths slight spatial variations
 */
void test_two_stage_smoothing(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Alternate between 845 and 855 (10mm variation)
    for (int i = 0; i < 10; i++) {
        initMockData(data, (i % 2 == 0) ? 845 : 855, 5);
        filter.process(data);
    }
    
    // Temporal average should be around 850
    uint16_t result = filter.process(data);
    TEST_ASSERT_UINT16_WITHIN(3, 850, result);
}

// ============================================
// Gradual Drift Tests (Temperature Effects)
// ============================================

/**
 * @test Gradual drift: all zones shift +1mm per cycle
 * Simulates temperature-induced drift over time
 */
void test_two_stage_gradual_drift_1mm_per_cycle(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Start at 850mm, drift up by 1mm each cycle for 20 cycles
    for (int cycle = 0; cycle < 20; cycle++) {
        uint16_t current = 850 + cycle;
        initMockData(data, current, 5);
        filter.process(data);
    }
    
    // After 20 cycles at 850→869, temporal average should track
    // Last 5 values: 865, 866, 867, 868, 869 → avg ≈ 867
    MockSensorData final_data;
    initMockData(final_data, 870, 5);
    uint16_t result = filter.process(final_data);
    
    // Should be tracking the drift, not stuck at initial value
    TEST_ASSERT_UINT16_WITHIN(5, 868, result);
}

/**
 * @test Gradual drift: +10mm over 10 cycles (spec requirement)
 * Per T016 in tasks.md: "gradual drift scenario"
 */
void test_two_stage_gradual_drift_10mm_over_10_cycles(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Start at 850mm, drift up to 860mm over 10 cycles
    for (int cycle = 0; cycle <= 10; cycle++) {
        uint16_t current = 850 + cycle;
        initMockData(data, current, 5);
        filter.process(data);
    }
    
    // Last 5 values in window: 856, 857, 858, 859, 860 → avg ≈ 858
    TEST_ASSERT_UINT16_WITHIN(3, 858, filter.process(data));
}

/**
 * @test Gradual drift with noise: drift + ±3mm random noise
 */
void test_two_stage_drift_with_noise(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Simulated noise pattern (deterministic for test repeatability)
    int8_t noise[] = {0, 2, -1, 3, -2, 1, -3, 2, 0, -1, 1, -2, 3, 0, -1};
    
    // Drift from 850 to 865 over 15 cycles with noise
    for (int cycle = 0; cycle < 15; cycle++) {
        uint16_t base = 850 + cycle;
        uint16_t current = base + noise[cycle];
        initMockData(data, current, 5);
        filter.process(data);
    }
    
    // Final reading should be tracking drift despite noise
    MockSensorData final_data;
    initMockData(final_data, 865, 5);
    uint16_t result = filter.process(final_data);
    
    // Should be near 862-865 (tracking drift)
    TEST_ASSERT_UINT16_WITHIN(5, 863, result);
}

// ============================================
// Noise Spike Tests
// ============================================

/**
 * @test Single noisy frame is smoothed by temporal filter
 */
void test_two_stage_single_spike_smoothed(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Fill window with stable readings
    for (int i = 0; i < 5; i++) {
        initMockData(data, 850, 5);
        filter.process(data);
    }
    
    // Introduce a spike (all zones jump to 880)
    initMockData(data, 880, 5);
    uint16_t result_with_spike = filter.process(data);
    
    // Temporal average: (850, 850, 850, 850, 880) / 5 = 856
    // Should be smoothed, not showing full spike
    TEST_ASSERT_UINT16_WITHIN(3, 856, result_with_spike);
}

/**
 * @test Noise spike with outliers (some zones spike, others don't)
 */
void test_two_stage_partial_spike(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Fill window with stable readings
    for (int i = 0; i < 5; i++) {
        initMockData(data, 850, 5);
        filter.process(data);
    }
    
    // Partial spike: 12 zones at 850, 4 zones at 920 (outliers)
    initMockData(data, 850, 5);
    data.distance_mm[0] = 920;
    data.distance_mm[1] = 920;
    data.distance_mm[2] = 920;
    data.distance_mm[3] = 920;
    
    uint16_t result = filter.process(data);
    
    // Spatial: 4 outliers filtered, consensus = 850
    // Temporal: all 5 readings are 850
    TEST_ASSERT_EQUAL_UINT16(850, result);
    TEST_ASSERT_EQUAL_UINT8(4, filter.getLastConsensus().outlier_count);
}

/**
 * @test Consecutive noise frames don't dominate output
 */
void test_two_stage_consecutive_noise(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Fill window with stable readings
    for (int i = 0; i < 5; i++) {
        initMockData(data, 850, 5);
        filter.process(data);
    }
    
    // Two consecutive noisy frames
    initMockData(data, 870, 5);
    filter.process(data);
    filter.process(data);
    
    // Window now: [850, 850, 850, 870, 870] → avg = 858
    // But we call process again which adds another 870: [850, 850, 870, 870, 870] → avg = 862
    TEST_ASSERT_UINT16_WITHIN(5, 862, filter.process(data));
}

// ============================================
// Reliability Tests
// ============================================

/**
 * @test Unreliable consensus doesn't update temporal filter
 */
void test_two_stage_unreliable_skipped(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Fill window with stable readings
    for (int i = 0; i < 5; i++) {
        initMockData(data, 850, 5);
        filter.process(data);
    }
    
    uint16_t stable_avg = filter.process(data);
    TEST_ASSERT_EQUAL_UINT16(850, stable_avg);
    
    // Send unreliable data (only 3 valid zones)
    initMockData(data, 0, 0);  // All invalid
    data.distance_mm[0] = 900;
    data.distance_mm[1] = 905;
    data.distance_mm[2] = 895;
    data.target_status[0] = 5;
    data.target_status[1] = 5;
    data.target_status[2] = 5;
    
    uint16_t result = filter.process(data);
    
    // Should return previous average since new data is unreliable
    TEST_ASSERT_EQUAL_UINT16(850, result);
    TEST_ASSERT_FALSE(filter.getLastConsensus().is_reliable);
}

/**
 * @test Recovery from unreliable period
 */
void test_two_stage_recovery_from_unreliable(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Establish baseline
    for (int i = 0; i < 5; i++) {
        initMockData(data, 850, 5);
        filter.process(data);
    }
    
    // Unreliable period (sensor partially blocked)
    initMockData(data, 0, 0);
    data.distance_mm[0] = 850;
    data.target_status[0] = 5;
    for (int i = 0; i < 3; i++) {
        filter.process(data);
    }
    
    // Recovery with new valid data at 860mm
    initMockData(data, 860, 5);
    filter.process(data);
    
    // Should be transitioning toward new value
    // Window still mostly 850s plus one 860
    uint16_t result = filter.process(data);
    TEST_ASSERT_UINT16_WITHIN(5, 852, result);
}

// ============================================
// Movement Scenario Tests
// ============================================

/**
 * @test Desk moving up (850→900mm) tracked correctly
 */
void test_two_stage_desk_moving_up(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Start at 850mm
    initMockData(data, 850, 5);
    for (int i = 0; i < 5; i++) {
        filter.process(data);
    }
    
    // Move to 900mm over 10 cycles (5mm/cycle)
    for (int i = 0; i < 10; i++) {
        initMockData(data, 850 + (i + 1) * 5, 5);
        filter.process(data);
    }
    
    // Should be tracking near final position
    uint16_t result = filter.process(data);
    TEST_ASSERT_UINT16_WITHIN(10, 895, result);
}

/**
 * @test Desk moving down (900→850mm) tracked correctly
 */
void test_two_stage_desk_moving_down(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Start at 900mm
    initMockData(data, 900, 5);
    for (int i = 0; i < 5; i++) {
        filter.process(data);
    }
    
    // Move to 850mm over 10 cycles
    for (int i = 0; i < 10; i++) {
        initMockData(data, 900 - (i + 1) * 5, 5);
        filter.process(data);
    }
    
    // Should be tracking near final position
    uint16_t result = filter.process(data);
    TEST_ASSERT_UINT16_WITHIN(10, 855, result);
}

/**
 * @test Stabilization at target height
 */
void test_two_stage_stabilization(void) {
    TwoStageFilter filter;
    MockSensorData data;
    
    // Simulate approach and stop at 850mm
    // Approach: 840, 843, 846, 848, 849
    uint16_t approach[] = {840, 843, 846, 848, 849};
    for (int i = 0; i < 5; i++) {
        initMockData(data, approach[i], 5);
        filter.process(data);
    }
    
    // Stabilized at 850mm for several cycles
    initMockData(data, 850, 5);
    for (int i = 0; i < 10; i++) {
        filter.process(data);
    }
    
    // Should be very close to 850mm now
    uint16_t result = filter.process(data);
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

// Arduino framework entry points
#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Basic pipeline tests
    RUN_TEST(test_two_stage_single_frame);
    RUN_TEST(test_two_stage_window_filling);
    RUN_TEST(test_two_stage_smoothing);
    
    // Gradual drift tests
    RUN_TEST(test_two_stage_gradual_drift_1mm_per_cycle);
    RUN_TEST(test_two_stage_gradual_drift_10mm_over_10_cycles);
    RUN_TEST(test_two_stage_drift_with_noise);
    
    // Noise spike tests
    RUN_TEST(test_two_stage_single_spike_smoothed);
    RUN_TEST(test_two_stage_partial_spike);
    RUN_TEST(test_two_stage_consecutive_noise);
    
    // Reliability tests
    RUN_TEST(test_two_stage_unreliable_skipped);
    RUN_TEST(test_two_stage_recovery_from_unreliable);
    
    // Movement scenario tests
    RUN_TEST(test_two_stage_desk_moving_up);
    RUN_TEST(test_two_stage_desk_moving_down);
    RUN_TEST(test_two_stage_stabilization);
    
    return UNITY_END();
}
#else
void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    // Basic pipeline tests
    RUN_TEST(test_two_stage_single_frame);
    RUN_TEST(test_two_stage_window_filling);
    RUN_TEST(test_two_stage_smoothing);
    
    // Gradual drift tests
    RUN_TEST(test_two_stage_gradual_drift_1mm_per_cycle);
    RUN_TEST(test_two_stage_gradual_drift_10mm_over_10_cycles);
    RUN_TEST(test_two_stage_drift_with_noise);
    
    // Noise spike tests
    RUN_TEST(test_two_stage_single_spike_smoothed);
    RUN_TEST(test_two_stage_partial_spike);
    RUN_TEST(test_two_stage_consecutive_noise);
    
    // Reliability tests
    RUN_TEST(test_two_stage_unreliable_skipped);
    RUN_TEST(test_two_stage_recovery_from_unreliable);
    
    // Movement scenario tests
    RUN_TEST(test_two_stage_desk_moving_up);
    RUN_TEST(test_two_stage_desk_moving_down);
    RUN_TEST(test_two_stage_stabilization);
    
    UNITY_END();
}

void loop() {
}
#endif
