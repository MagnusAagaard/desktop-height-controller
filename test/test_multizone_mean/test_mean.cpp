/**
 * @file test_mean.cpp
 * @brief Unit tests for computeMean() statistical utility
 * 
 * TDD Phase: These tests must FAIL initially, then PASS after implementation.
 * Per Constitution Principle I: Test-First Development
 */

#ifdef NATIVE_TEST
#include <ArduinoFake.h>
using namespace fakeit;
#else
#include <Arduino.h>
#endif
#include <unity.h>
#include "multi_zone_filtering.h"

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

/**
 * @test Mean of typical distance values
 */
void test_mean_typical_values(void) {
    uint16_t values[] = {800, 850, 900};
    uint16_t result = computeMean(values, 3);
    // (800 + 850 + 900) / 3 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

/**
 * @test Mean of single element returns that element
 */
void test_mean_single_element(void) {
    uint16_t values[] = {750};
    uint16_t result = computeMean(values, 1);
    TEST_ASSERT_EQUAL_UINT16(750, result);
}

/**
 * @test Mean with two elements
 */
void test_mean_two_elements(void) {
    uint16_t values[] = {800, 900};
    uint16_t result = computeMean(values, 2);
    // (800 + 900) / 2 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

/**
 * @test Mean with 16 elements (typical multi-zone scenario)
 */
void test_mean_16_elements(void) {
    uint16_t values[] = {840, 842, 844, 846, 848, 850, 852, 854,
                         856, 858, 860, 862, 864, 866, 868, 870};
    uint16_t result = computeMean(values, 16);
    // Sum = 13680, mean = 855
    TEST_ASSERT_EQUAL_UINT16(855, result);
}

/**
 * @test Mean handles rounding (truncation toward zero)
 */
void test_mean_rounding(void) {
    uint16_t values[] = {800, 801, 802};
    uint16_t result = computeMean(values, 3);
    // (800 + 801 + 802) / 3 = 801
    TEST_ASSERT_EQUAL_UINT16(801, result);
}

/**
 * @test Mean handles fractional result (integer truncation)
 */
void test_mean_fractional_truncation(void) {
    uint16_t values[] = {100, 101};
    uint16_t result = computeMean(values, 2);
    // (100 + 101) / 2 = 100.5 -> 100 (integer truncation)
    TEST_ASSERT_EQUAL_UINT16(100, result);
}

/**
 * @test Mean with all same values
 */
void test_mean_all_same(void) {
    uint16_t values[] = {850, 850, 850, 850, 850};
    uint16_t result = computeMean(values, 5);
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

/**
 * @test Mean overflow safety - large values within sensor range
 * Maximum: 16 zones × 4000mm = 64000 (fits in uint32_t accumulator)
 */
void test_mean_large_values_no_overflow(void) {
    uint16_t values[] = {4000, 4000, 4000, 4000};
    uint16_t result = computeMean(values, 4);
    // (4000 * 4) / 4 = 4000
    TEST_ASSERT_EQUAL_UINT16(4000, result);
}

/**
 * @test Mean overflow safety - max 16 zones at max range
 */
void test_mean_max_zones_max_range(void) {
    uint16_t values[] = {4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000,
                         4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000};
    uint16_t result = computeMean(values, 16);
    // (4000 * 16) / 16 = 4000, sum = 64000 (fits in uint32_t)
    TEST_ASSERT_EQUAL_UINT16(4000, result);
}

/**
 * @test Mean with extreme overflow scenario 
 * Testing with values that would overflow uint16_t accumulator
 * but should work with uint32_t accumulator
 */
void test_mean_accumulator_overflow_safety(void) {
    // 16 values of 60000 would sum to 960000, well within uint32_t
    uint16_t values[] = {60000, 60000, 60000, 60000, 60000, 60000, 60000, 60000,
                         60000, 60000, 60000, 60000, 60000, 60000, 60000, 60000};
    uint16_t result = computeMean(values, 16);
    TEST_ASSERT_EQUAL_UINT16(60000, result);
}

/**
 * @test Mean with zero count returns 0 (edge case - division by zero prevention)
 */
void test_mean_zero_count(void) {
    uint16_t values[] = {800, 850};
    uint16_t result = computeMean(values, 0);
    TEST_ASSERT_EQUAL_UINT16(0, result);
}

/**
 * @test Mean with minimum valid distances
 */
void test_mean_minimum_distances(void) {
    uint16_t values[] = {10, 10, 10, 10};  // SENSOR_MIN_VALID_MM
    uint16_t result = computeMean(values, 4);
    TEST_ASSERT_EQUAL_UINT16(10, result);
}

/**
 * @test Mean with mixed typical distances (real-world scenario)
 */
void test_mean_mixed_realistic(void) {
    // Simulating 4 valid zones with slight variation (after outlier filtering)
    uint16_t values[] = {847, 853, 849, 851};
    uint16_t result = computeMean(values, 4);
    // (847 + 853 + 849 + 851) / 4 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

// Arduino framework entry points
#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_mean_typical_values);
    RUN_TEST(test_mean_single_element);
    RUN_TEST(test_mean_two_elements);
    RUN_TEST(test_mean_16_elements);
    RUN_TEST(test_mean_rounding);
    RUN_TEST(test_mean_fractional_truncation);
    RUN_TEST(test_mean_all_same);
    RUN_TEST(test_mean_large_values_no_overflow);
    RUN_TEST(test_mean_max_zones_max_range);
    RUN_TEST(test_mean_accumulator_overflow_safety);
    RUN_TEST(test_mean_zero_count);
    RUN_TEST(test_mean_minimum_distances);
    RUN_TEST(test_mean_mixed_realistic);
    
    return UNITY_END();
}
#else
void setup() {
    // Wait for Serial to be ready
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_mean_typical_values);
    RUN_TEST(test_mean_single_element);
    RUN_TEST(test_mean_two_elements);
    RUN_TEST(test_mean_16_elements);
    RUN_TEST(test_mean_rounding);
    RUN_TEST(test_mean_fractional_truncation);
    RUN_TEST(test_mean_all_same);
    RUN_TEST(test_mean_large_values_no_overflow);
    RUN_TEST(test_mean_max_zones_max_range);
    RUN_TEST(test_mean_accumulator_overflow_safety);
    RUN_TEST(test_mean_zero_count);
    RUN_TEST(test_mean_minimum_distances);
    RUN_TEST(test_mean_mixed_realistic);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
#endif
