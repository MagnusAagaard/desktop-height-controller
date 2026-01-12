/**
 * @file test_zone_validation.cpp
 * @brief Unit tests for isZoneValid() function
 * 
 * TDD Phase: These tests must FAIL initially, then PASS after implementation.
 * Per Constitution Principle I: Test-First Development
 * 
 * Zone validation checks:
 * 1. Status codes: 5, 6, 9 = valid; 0, 255 = invalid; others = reject (conservative)
 * 2. Range: SENSOR_MIN_VALID_MM (10) to SENSOR_MAX_RANGE_MM (4000)
 */

#ifdef NATIVE_TEST
#include <ArduinoFake.h>
using namespace fakeit;
#else
#include <Arduino.h>
#endif
#include <unity.h>

// Local constants matching Config.h
constexpr uint16_t SENSOR_MIN_VALID_MM = 10;
constexpr uint16_t SENSOR_MAX_RANGE_MM = 4000;

/**
 * @brief Zone validation function (mirrors HeightController::isZoneValid)
 * 
 * @param status Target status from VL53L5CX
 * @param distance Distance reading in mm
 * @return true if zone measurement is valid
 */
bool isZoneValid(uint8_t status, uint16_t distance) {
    // Reject explicit invalid codes
    if (status == 0 || status == 255) return false;
    
    // Accept only documented high-confidence codes
    if (status != 5 && status != 6 && status != 9) return false;
    
    // Range validation
    if (distance < SENSOR_MIN_VALID_MM) return false;
    if (distance > SENSOR_MAX_RANGE_MM) return false;
    
    return true;
}

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// ============================================
// Status Code Tests
// ============================================

/**
 * @test Status 5 (100% confidence) is valid
 */
void test_status_5_valid(void) {
    TEST_ASSERT_TRUE(isZoneValid(5, 850));
}

/**
 * @test Status 6 (50% confidence, wrap around) is valid
 */
void test_status_6_valid(void) {
    TEST_ASSERT_TRUE(isZoneValid(6, 850));
}

/**
 * @test Status 9 (sigma failure but valid) is valid
 */
void test_status_9_valid(void) {
    TEST_ASSERT_TRUE(isZoneValid(9, 850));
}

/**
 * @test Status 0 (no target detected) is invalid
 */
void test_status_0_invalid(void) {
    TEST_ASSERT_FALSE(isZoneValid(0, 850));
}

/**
 * @test Status 255 (explicit invalid) is invalid
 */
void test_status_255_invalid(void) {
    TEST_ASSERT_FALSE(isZoneValid(255, 850));
}

/**
 * @test Status 1 (undefined) is rejected (conservative approach)
 */
void test_status_1_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(1, 850));
}

/**
 * @test Status 2 (undefined) is rejected
 */
void test_status_2_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(2, 850));
}

/**
 * @test Status 3 (undefined) is rejected
 */
void test_status_3_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(3, 850));
}

/**
 * @test Status 4 (undefined) is rejected
 */
void test_status_4_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(4, 850));
}

/**
 * @test Status 7 (undefined) is rejected
 */
void test_status_7_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(7, 850));
}

/**
 * @test Status 8 (undefined) is rejected
 */
void test_status_8_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(8, 850));
}

/**
 * @test Status 10 (undefined) is rejected
 */
void test_status_10_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(10, 850));
}

/**
 * @test High undefined status (100) is rejected
 */
void test_status_100_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(100, 850));
}

/**
 * @test Status 254 (just below 255) is rejected
 */
void test_status_254_rejected(void) {
    TEST_ASSERT_FALSE(isZoneValid(254, 850));
}

// ============================================
// Range Validation Tests
// ============================================

/**
 * @test Distance at minimum (10mm) is valid
 */
void test_distance_at_minimum_valid(void) {
    TEST_ASSERT_TRUE(isZoneValid(5, SENSOR_MIN_VALID_MM));
}

/**
 * @test Distance below minimum (9mm) is invalid
 */
void test_distance_below_minimum_invalid(void) {
    TEST_ASSERT_FALSE(isZoneValid(5, SENSOR_MIN_VALID_MM - 1));
}

/**
 * @test Distance zero is invalid
 */
void test_distance_zero_invalid(void) {
    TEST_ASSERT_FALSE(isZoneValid(5, 0));
}

/**
 * @test Distance at maximum (4000mm) is valid
 */
void test_distance_at_maximum_valid(void) {
    TEST_ASSERT_TRUE(isZoneValid(5, SENSOR_MAX_RANGE_MM));
}

/**
 * @test Distance above maximum (4001mm) is invalid
 */
void test_distance_above_maximum_invalid(void) {
    TEST_ASSERT_FALSE(isZoneValid(5, SENSOR_MAX_RANGE_MM + 1));
}

/**
 * @test Very high distance (65535) is invalid
 */
void test_distance_max_uint16_invalid(void) {
    TEST_ASSERT_FALSE(isZoneValid(5, 65535));
}

// ============================================
// Combined Tests
// ============================================

/**
 * @test Valid status with invalid range (too low) is invalid
 */
void test_valid_status_invalid_range_low(void) {
    TEST_ASSERT_FALSE(isZoneValid(5, 5));
    TEST_ASSERT_FALSE(isZoneValid(6, 5));
    TEST_ASSERT_FALSE(isZoneValid(9, 5));
}

/**
 * @test Valid status with invalid range (too high) is invalid
 */
void test_valid_status_invalid_range_high(void) {
    TEST_ASSERT_FALSE(isZoneValid(5, 5000));
    TEST_ASSERT_FALSE(isZoneValid(6, 5000));
    TEST_ASSERT_FALSE(isZoneValid(9, 5000));
}

/**
 * @test Invalid status with valid range is still invalid
 */
void test_invalid_status_valid_range(void) {
    TEST_ASSERT_FALSE(isZoneValid(0, 850));
    TEST_ASSERT_FALSE(isZoneValid(255, 850));
    TEST_ASSERT_FALSE(isZoneValid(1, 850));
}

/**
 * @test Typical desk height ranges with valid status
 */
void test_typical_desk_heights(void) {
    // Standing: ~110-120cm = 1100-1200mm
    TEST_ASSERT_TRUE(isZoneValid(5, 1100));
    TEST_ASSERT_TRUE(isZoneValid(5, 1200));
    
    // Sitting: ~70-80cm = 700-800mm
    TEST_ASSERT_TRUE(isZoneValid(5, 700));
    TEST_ASSERT_TRUE(isZoneValid(5, 800));
    
    // Low position: ~60cm = 600mm
    TEST_ASSERT_TRUE(isZoneValid(5, 600));
}

/**
 * @test All valid statuses work across valid range
 */
void test_all_valid_statuses_all_ranges(void) {
    // Test at various points in the valid range
    uint16_t test_distances[] = {10, 100, 500, 850, 1000, 2000, 3000, 4000};
    uint8_t valid_statuses[] = {5, 6, 9};
    
    for (int s = 0; s < 3; s++) {
        for (int d = 0; d < 8; d++) {
            TEST_ASSERT_TRUE(isZoneValid(valid_statuses[s], test_distances[d]));
        }
    }
}

/**
 * @test Edge case: just inside minimum range
 */
void test_just_inside_min_range(void) {
    TEST_ASSERT_TRUE(isZoneValid(5, 10));   // At minimum
    TEST_ASSERT_TRUE(isZoneValid(5, 11));   // Just above
    TEST_ASSERT_FALSE(isZoneValid(5, 9));   // Just below
}

/**
 * @test Edge case: just inside maximum range
 */
void test_just_inside_max_range(void) {
    TEST_ASSERT_TRUE(isZoneValid(5, 4000));  // At maximum
    TEST_ASSERT_TRUE(isZoneValid(5, 3999));  // Just below
    TEST_ASSERT_FALSE(isZoneValid(5, 4001)); // Just above
}

// Arduino framework entry points
#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Status code tests
    RUN_TEST(test_status_5_valid);
    RUN_TEST(test_status_6_valid);
    RUN_TEST(test_status_9_valid);
    RUN_TEST(test_status_0_invalid);
    RUN_TEST(test_status_255_invalid);
    RUN_TEST(test_status_1_rejected);
    RUN_TEST(test_status_2_rejected);
    RUN_TEST(test_status_3_rejected);
    RUN_TEST(test_status_4_rejected);
    RUN_TEST(test_status_7_rejected);
    RUN_TEST(test_status_8_rejected);
    RUN_TEST(test_status_10_rejected);
    RUN_TEST(test_status_100_rejected);
    RUN_TEST(test_status_254_rejected);
    
    // Range validation tests
    RUN_TEST(test_distance_at_minimum_valid);
    RUN_TEST(test_distance_below_minimum_invalid);
    RUN_TEST(test_distance_zero_invalid);
    RUN_TEST(test_distance_at_maximum_valid);
    RUN_TEST(test_distance_above_maximum_invalid);
    RUN_TEST(test_distance_max_uint16_invalid);
    
    // Combined tests
    RUN_TEST(test_valid_status_invalid_range_low);
    RUN_TEST(test_valid_status_invalid_range_high);
    RUN_TEST(test_invalid_status_valid_range);
    RUN_TEST(test_typical_desk_heights);
    RUN_TEST(test_all_valid_statuses_all_ranges);
    RUN_TEST(test_just_inside_min_range);
    RUN_TEST(test_just_inside_max_range);
    
    return UNITY_END();
}
#else
void setup() {
    // Wait for Serial to be ready
    delay(2000);
    
    UNITY_BEGIN();
    
    // Status code tests
    RUN_TEST(test_status_5_valid);
    RUN_TEST(test_status_6_valid);
    RUN_TEST(test_status_9_valid);
    RUN_TEST(test_status_0_invalid);
    RUN_TEST(test_status_255_invalid);
    RUN_TEST(test_status_1_rejected);
    RUN_TEST(test_status_2_rejected);
    RUN_TEST(test_status_3_rejected);
    RUN_TEST(test_status_4_rejected);
    RUN_TEST(test_status_7_rejected);
    RUN_TEST(test_status_8_rejected);
    RUN_TEST(test_status_10_rejected);
    RUN_TEST(test_status_100_rejected);
    RUN_TEST(test_status_254_rejected);
    
    // Range validation tests
    RUN_TEST(test_distance_at_minimum_valid);
    RUN_TEST(test_distance_below_minimum_invalid);
    RUN_TEST(test_distance_zero_invalid);
    RUN_TEST(test_distance_at_maximum_valid);
    RUN_TEST(test_distance_above_maximum_invalid);
    RUN_TEST(test_distance_max_uint16_invalid);
    
    // Combined tests
    RUN_TEST(test_valid_status_invalid_range_low);
    RUN_TEST(test_valid_status_invalid_range_high);
    RUN_TEST(test_invalid_status_valid_range);
    RUN_TEST(test_typical_desk_heights);
    RUN_TEST(test_all_valid_statuses_all_ranges);
    RUN_TEST(test_just_inside_min_range);
    RUN_TEST(test_just_inside_max_range);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
#endif
