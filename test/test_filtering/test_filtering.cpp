/**
 * @file test_filtering.cpp
 * @brief Unit tests for HeightController sensor filtering
 * 
 * Tests written FIRST per Constitution Principle I (Test-First Development)
 * Verifies moving average filter is applied to raw sensor readings before height calculation.
 * 
 * Per FR-001a: "Moving average filter applied to smooth sensor noise"
 * Per data-model.md: First N-1 samples use partial average (N = filter window size)
 */

#include <Arduino.h>
#include <unity.h>
#include "utils/MovingAverageFilter.h"

void setUp() {}
void tearDown() {}

/**
 * Test: Verify filter is applied before height calculation
 * 
 * Raw readings should go through MovingAverageFilter before being used
 * in the height formula.
 */
void test_filtering_applied_to_raw_readings() {
    MovingAverageFilter filter(5);
    
    // Simulate noisy sensor readings (1000 ± 50mm)
    filter.addSample(1050);  // Noisy high
    filter.addSample(950);   // Noisy low
    filter.addSample(1020);  // Noisy high
    filter.addSample(980);   // Noisy low
    filter.addSample(1000);  // Target
    
    // Average should smooth out noise: (1050+950+1020+980+1000)/5 = 1000
    uint16_t filtered = filter.getAverage();
    TEST_ASSERT_EQUAL(1000, filtered);
}

/**
 * Test: First reading should use single sample (no filtering yet)
 */
void test_filtering_first_sample() {
    MovingAverageFilter filter(5);
    
    filter.addSample(1000);
    
    // With only 1 sample, filtered value should equal raw value
    TEST_ASSERT_EQUAL(1000, filter.getAverage());
}

/**
 * Test: Partial window uses available samples
 * 
 * Per data-model.md: "First N-1 samples use partial average"
 */
void test_filtering_partial_window() {
    MovingAverageFilter filter(5);
    
    filter.addSample(1000);
    filter.addSample(1100);
    filter.addSample(1200);
    
    // With 3 samples in 5-sample window, average all 3
    // (1000 + 1100 + 1200) / 3 = 1100
    TEST_ASSERT_EQUAL(1100, filter.getAverage());
}

/**
 * Test: Filter handles gradual changes smoothly
 * 
 * When desk moves, readings change gradually. Filter should track this.
 */
void test_filtering_gradual_change() {
    MovingAverageFilter filter(5);
    
    // Simulate desk rising (distance decreasing)
    filter.addSample(1500);  // Starting position
    filter.addSample(1450);
    filter.addSample(1400);
    filter.addSample(1350);
    filter.addSample(1300);  // Final position
    
    // Average: (1500+1450+1400+1350+1300)/5 = 1400
    TEST_ASSERT_EQUAL(1400, filter.getAverage());
    
    // Add one more sample to see window slide
    filter.addSample(1250);
    // Window now: 1450, 1400, 1350, 1300, 1250
    // Average: 1350
    TEST_ASSERT_EQUAL(1350, filter.getAverage());
}

/**
 * Test: Filter rejects sudden spikes (demonstrates smoothing)
 */
void test_filtering_spike_rejection() {
    MovingAverageFilter filter(5);
    
    // Establish baseline
    filter.addSample(1000);
    filter.addSample(1000);
    filter.addSample(1000);
    filter.addSample(1000);
    
    // Introduce spike (maybe interference or measurement error)
    filter.addSample(2000);  // Spike!
    
    // Average: (1000+1000+1000+1000+2000)/5 = 1200
    // Spike effect is reduced from 100% deviation to 20%
    TEST_ASSERT_EQUAL(1200, filter.getAverage());
    
    // After more normal readings, spike effect diminishes
    filter.addSample(1000);  // Window: 1000,1000,1000,2000,1000 = 1200
    filter.addSample(1000);  // Window: 1000,1000,2000,1000,1000 = 1200
    filter.addSample(1000);  // Window: 1000,2000,1000,1000,1000 = 1200
    filter.addSample(1000);  // Window: 2000,1000,1000,1000,1000 = 1200
    filter.addSample(1000);  // Window: 1000,1000,1000,1000,1000 = 1000 (spike gone!)
    
    TEST_ASSERT_EQUAL(1000, filter.getAverage());
}

/**
 * Test: Filter maintains accuracy at steady state
 */
void test_filtering_steady_state_accuracy() {
    MovingAverageFilter filter(5);
    
    // All readings the same = average equals that value
    for (int i = 0; i < 10; i++) {
        filter.addSample(1234);
    }
    
    TEST_ASSERT_EQUAL(1234, filter.getAverage());
}

/**
 * Test: Different window sizes affect smoothing
 */
void test_filtering_window_size_effect() {
    // Small window (3) - faster response, less smoothing
    MovingAverageFilter filterSmall(3);
    filterSmall.addSample(1000);
    filterSmall.addSample(1000);
    filterSmall.addSample(2000);  // Spike
    // Average: (1000+1000+2000)/3 = 1333 (33% spike effect)
    TEST_ASSERT_EQUAL(1333, filterSmall.getAverage());
    
    // Large window (5) - slower response, more smoothing
    MovingAverageFilter filterLarge(5);
    filterLarge.addSample(1000);
    filterLarge.addSample(1000);
    filterLarge.addSample(1000);
    filterLarge.addSample(1000);
    filterLarge.addSample(2000);  // Spike
    // Average: (1000*4+2000)/5 = 1200 (20% spike effect)
    TEST_ASSERT_EQUAL(1200, filterLarge.getAverage());
}

/**
 * Test: Filter reset clears history
 * 
 * Used when sensor is recalibrated or error recovery
 */
void test_filtering_reset() {
    MovingAverageFilter filter(5);
    
    // Fill with old readings
    filter.addSample(500);
    filter.addSample(500);
    filter.addSample(500);
    filter.addSample(500);
    filter.addSample(500);
    
    TEST_ASSERT_EQUAL(500, filter.getAverage());
    
    // Reset filter
    filter.reset();
    
    // Start fresh with new readings
    filter.addSample(1000);
    
    // Should not be influenced by old readings
    TEST_ASSERT_EQUAL(1000, filter.getAverage());
}

/**
 * Test: Filtered height calculation end-to-end
 * 
 * Demonstrates the complete pipeline: raw → filtered → height
 */
void test_filtering_to_height_calculation() {
    MovingAverageFilter filter(5);
    uint16_t calibration_constant_cm = 200;
    
    // Noisy sensor readings around 1000mm
    filter.addSample(1010);
    filter.addSample(990);
    filter.addSample(1005);
    filter.addSample(995);
    filter.addSample(1000);
    
    // Get filtered value
    uint16_t filtered_mm = filter.getAverage();  // = 1000
    TEST_ASSERT_EQUAL(1000, filtered_mm);
    
    // Calculate height using filtered value
    uint16_t height_cm = calibration_constant_cm - (filtered_mm / 10);
    TEST_ASSERT_EQUAL(100, height_cm);  // 200 - 100 = 100cm
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    RUN_TEST(test_filtering_applied_to_raw_readings);
    RUN_TEST(test_filtering_first_sample);
    RUN_TEST(test_filtering_partial_window);
    RUN_TEST(test_filtering_gradual_change);
    RUN_TEST(test_filtering_spike_rejection);
    RUN_TEST(test_filtering_steady_state_accuracy);
    RUN_TEST(test_filtering_window_size_effect);
    RUN_TEST(test_filtering_reset);
    RUN_TEST(test_filtering_to_height_calculation);
    
    UNITY_END();
}

void loop() {}
