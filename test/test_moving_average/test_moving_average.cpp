/**
 * @file test_moving_average.cpp
 * @brief Unit tests for MovingAverageFilter class
 * 
 * Tests written FIRST per Constitution Principle I (Test-First Development)
 * These tests should FAIL until MovingAverageFilter is implemented.
 */

#include <Arduino.h>
#include <unity.h>
#include "utils/MovingAverageFilter.h"

// Forward declarations
void test_filter_initial_state();
void test_filter_single_sample();
void test_filter_partial_window();
void test_filter_full_window_averaging();
void test_filter_window_sliding();
void test_filter_reset();
void test_filter_window_size_minimum();
void test_filter_window_size_maximum();
void test_filter_empty_average();
void test_filter_overflow_protection();

void setUp() {
    // Called before each test
}

void tearDown() {
    // Called after each test
}

/**
 * Test: Newly created filter should report empty/no samples
 */
void test_filter_initial_state() {
    MovingAverageFilter filter(5);
    
    TEST_ASSERT_EQUAL(0, filter.getSampleCount());
    TEST_ASSERT_TRUE(filter.isEmpty());
    TEST_ASSERT_EQUAL(5, filter.getWindowSize());
}

/**
 * Test: Adding a single sample should return that value as average
 */
void test_filter_single_sample() {
    MovingAverageFilter filter(5);
    
    filter.addSample(100);
    
    TEST_ASSERT_EQUAL(1, filter.getSampleCount());
    TEST_ASSERT_FALSE(filter.isEmpty());
    TEST_ASSERT_EQUAL(100, filter.getAverage());
}

/**
 * Test: With fewer samples than window size, average all available samples
 */
void test_filter_partial_window() {
    MovingAverageFilter filter(5);
    
    filter.addSample(100);  // avg = 100
    filter.addSample(200);  // avg = 150
    filter.addSample(300);  // avg = 200
    
    TEST_ASSERT_EQUAL(3, filter.getSampleCount());
    TEST_ASSERT_EQUAL(200, filter.getAverage());  // (100+200+300)/3 = 200
}

/**
 * Test: With full window, average exactly window_size samples
 */
void test_filter_full_window_averaging() {
    MovingAverageFilter filter(5);
    
    filter.addSample(100);  // 100
    filter.addSample(200);  // 150
    filter.addSample(300);  // 200
    filter.addSample(400);  // 250
    filter.addSample(500);  // 300
    
    TEST_ASSERT_EQUAL(5, filter.getSampleCount());
    TEST_ASSERT_EQUAL(300, filter.getAverage());  // (100+200+300+400+500)/5 = 300
}

/**
 * Test: Adding more samples than window slides the window (drops oldest)
 */
void test_filter_window_sliding() {
    MovingAverageFilter filter(3);
    
    filter.addSample(100);  // [100] avg=100
    filter.addSample(200);  // [100,200] avg=150
    filter.addSample(300);  // [100,200,300] avg=200
    filter.addSample(400);  // [200,300,400] avg=300 (100 dropped)
    filter.addSample(500);  // [300,400,500] avg=400 (200 dropped)
    
    TEST_ASSERT_EQUAL(3, filter.getSampleCount());  // Still 3, not 5
    TEST_ASSERT_EQUAL(400, filter.getAverage());  // (300+400+500)/3 = 400
}

/**
 * Test: Reset clears all samples
 */
void test_filter_reset() {
    MovingAverageFilter filter(5);
    
    filter.addSample(100);
    filter.addSample(200);
    filter.addSample(300);
    
    filter.reset();
    
    TEST_ASSERT_EQUAL(0, filter.getSampleCount());
    TEST_ASSERT_TRUE(filter.isEmpty());
}

/**
 * Test: Window size clamped to minimum (3)
 */
void test_filter_window_size_minimum() {
    MovingAverageFilter filter(1);  // Requested 1, should clamp to 3
    
    TEST_ASSERT_EQUAL(3, filter.getWindowSize());
}

/**
 * Test: Window size clamped to maximum (10)
 */
void test_filter_window_size_maximum() {
    MovingAverageFilter filter(20);  // Requested 20, should clamp to 10
    
    TEST_ASSERT_EQUAL(10, filter.getWindowSize());
}

/**
 * Test: Empty filter returns 0 for average (safe default)
 */
void test_filter_empty_average() {
    MovingAverageFilter filter(5);
    
    TEST_ASSERT_EQUAL(0, filter.getAverage());
}

/**
 * Test: Filter handles large values without overflow (uint16_t max = 65535)
 */
void test_filter_overflow_protection() {
    MovingAverageFilter filter(5);
    
    // Add 5 samples near uint16_t max
    filter.addSample(60000);
    filter.addSample(60000);
    filter.addSample(60000);
    filter.addSample(60000);
    filter.addSample(60000);
    
    // Sum would be 300000 which exceeds uint16_t, but uint32_t sum should handle it
    TEST_ASSERT_EQUAL(60000, filter.getAverage());
}

/**
 * Test: isFull returns true only when window is full
 */
void test_filter_is_full() {
    MovingAverageFilter filter(3);
    
    TEST_ASSERT_FALSE(filter.isFull());
    
    filter.addSample(100);
    TEST_ASSERT_FALSE(filter.isFull());
    
    filter.addSample(200);
    TEST_ASSERT_FALSE(filter.isFull());
    
    filter.addSample(300);
    TEST_ASSERT_TRUE(filter.isFull());
    
    filter.addSample(400);  // Still full after more samples
    TEST_ASSERT_TRUE(filter.isFull());
}

/**
 * Test: getLastSample returns most recently added value
 */
void test_filter_get_last_sample() {
    MovingAverageFilter filter(5);
    
    filter.addSample(100);
    TEST_ASSERT_EQUAL(100, filter.getLastSample());
    
    filter.addSample(200);
    TEST_ASSERT_EQUAL(200, filter.getLastSample());
    
    filter.addSample(300);
    TEST_ASSERT_EQUAL(300, filter.getLastSample());
}

// Arduino framework entry points
void setup() {
    // Wait for Serial to be ready
    delay(2000);
    
    UNITY_BEGIN();
    
    // Run all tests
    RUN_TEST(test_filter_initial_state);
    RUN_TEST(test_filter_single_sample);
    RUN_TEST(test_filter_partial_window);
    RUN_TEST(test_filter_full_window_averaging);
    RUN_TEST(test_filter_window_sliding);
    RUN_TEST(test_filter_reset);
    RUN_TEST(test_filter_window_size_minimum);
    RUN_TEST(test_filter_window_size_maximum);
    RUN_TEST(test_filter_empty_average);
    RUN_TEST(test_filter_overflow_protection);
    RUN_TEST(test_filter_is_full);
    RUN_TEST(test_filter_get_last_sample);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
