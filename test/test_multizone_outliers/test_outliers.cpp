/**
 * @file test_outliers.cpp
 * @brief Unit tests for filterOutliers() statistical utility
 * 
 * TDD Phase: These tests must FAIL initially, then PASS after implementation.
 * Per Constitution Principle I: Test-First Development
 * 
 * Outlier detection uses 30mm threshold from median per FR-003 and spec clarification.
 */

#ifdef NATIVE_TEST
#include <ArduinoFake.h>
using namespace fakeit;
#else
#include <Arduino.h>
#endif
#include <unity.h>
#include <cstring>
#include "multi_zone_filtering.h"

// Threshold constant (matches multi_zone_filtering.h and Config.h)
// constexpr uint16_t OUTLIER_THRESHOLD_MM = 30;  // Already in header

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

/**
 * @test No outliers when all values within 30mm of median
 */
void test_outliers_none_all_within_threshold(void) {
    uint16_t values[] = {840, 845, 850, 855, 860};  // All within ±30mm of ~850
    bool keep_flags[5];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 5, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(5, kept_count);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(keep_flags[i]);
    }
}

/**
 * @test Single outlier detected and excluded
 */
void test_outliers_single_outlier(void) {
    uint16_t values[] = {850, 845, 855, 840, 1000};  // 1000 is outlier (150mm > 30mm)
    bool keep_flags[5];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 5, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(4, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);   // 850 - within threshold
    TEST_ASSERT_TRUE(keep_flags[1]);   // 845 - within threshold
    TEST_ASSERT_TRUE(keep_flags[2]);   // 855 - within threshold
    TEST_ASSERT_TRUE(keep_flags[3]);   // 840 - within threshold
    TEST_ASSERT_FALSE(keep_flags[4]);  // 1000 - outlier
}

/**
 * @test Multiple outliers detected
 */
void test_outliers_multiple_outliers(void) {
    uint16_t values[] = {850, 700, 855, 1000, 845};  // 700 and 1000 are outliers
    bool keep_flags[5];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 5, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(3, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);   // 850 - within threshold
    TEST_ASSERT_FALSE(keep_flags[1]);  // 700 - outlier (150mm below)
    TEST_ASSERT_TRUE(keep_flags[2]);   // 855 - within threshold
    TEST_ASSERT_FALSE(keep_flags[3]);  // 1000 - outlier (150mm above)
    TEST_ASSERT_TRUE(keep_flags[4]);   // 845 - within threshold
}

/**
 * @test Exactly at threshold boundary (30mm) - should be INCLUDED
 */
void test_outliers_exactly_at_threshold_included(void) {
    uint16_t values[] = {850, 820, 880};  // 820 and 880 are exactly 30mm from 850
    bool keep_flags[3];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 3, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(3, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);  // 850 - at median
    TEST_ASSERT_TRUE(keep_flags[1]);  // 820 - exactly 30mm, included
    TEST_ASSERT_TRUE(keep_flags[2]);  // 880 - exactly 30mm, included
}

/**
 * @test Just beyond threshold (31mm) - should be EXCLUDED
 */
void test_outliers_just_beyond_threshold_excluded(void) {
    uint16_t values[] = {850, 819, 881};  // 819 and 881 are 31mm from 850
    bool keep_flags[3];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 3, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(1, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);   // 850 - at median
    TEST_ASSERT_FALSE(keep_flags[1]);  // 819 - 31mm, outlier
    TEST_ASSERT_FALSE(keep_flags[2]);  // 881 - 31mm, outlier
}

/**
 * @test Bimodal distribution - typical scenario per FR-009
 * When separation < 60mm (2× threshold), all should be included
 */
void test_outliers_bimodal_within_tolerance(void) {
    // 8 zones at 820mm, 8 zones at 860mm, median ~840mm
    uint16_t values[] = {820, 820, 820, 820, 860, 860, 860, 860};
    bool keep_flags[8];
    uint8_t kept_count = 0;
    
    // With median 840: 820 is 20mm below, 860 is 20mm above (both < 30mm)
    filterOutliers(values, 8, 840, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(8, kept_count);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(keep_flags[i]);
    }
}

/**
 * @test Bimodal distribution - wider separation
 * When individual values within threshold of median, all included per FR-009
 */
void test_outliers_bimodal_wide_separation_still_valid(void) {
    // 8 zones at 800mm, 8 zones at 900mm, median ~850mm
    // 800 is 50mm below median - OUTLIER
    // 900 is 50mm above median - OUTLIER
    // This scenario results in most values being outliers!
    uint16_t values[] = {800, 800, 800, 800, 900, 900, 900, 900};
    bool keep_flags[8];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 8, 850, keep_flags, kept_count);
    
    // Both clusters are 50mm from median (>30mm), so all are outliers
    TEST_ASSERT_EQUAL_UINT8(0, kept_count);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_FALSE(keep_flags[i]);
    }
}

/**
 * @test 16 zones typical scenario - realistic sensor data
 */
void test_outliers_16_zones_typical(void) {
    // 14 zones around 850mm, 1 low outlier, 1 high outlier
    uint16_t values[] = {845, 850, 848, 852, 847, 853, 849, 851,
                         846, 854, 848, 852, 847, 853, 700, 1000};
    bool keep_flags[16];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 16, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(14, kept_count);
    // First 14 should be kept
    for (int i = 0; i < 14; i++) {
        TEST_ASSERT_TRUE(keep_flags[i]);
    }
    // Last 2 are outliers
    TEST_ASSERT_FALSE(keep_flags[14]);  // 700 - outlier
    TEST_ASSERT_FALSE(keep_flags[15]);  // 1000 - outlier
}

/**
 * @test Single element - always kept (no outliers possible)
 */
void test_outliers_single_element(void) {
    uint16_t values[] = {850};
    bool keep_flags[1];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 1, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(1, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);
}

/**
 * @test Empty array (edge case)
 */
void test_outliers_empty_array(void) {
    uint16_t values[] = {0};  // Dummy
    bool keep_flags[1] = {true};  // Pre-fill to detect modification
    uint8_t kept_count = 99;  // Pre-fill to detect modification
    
    filterOutliers(values, 0, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(0, kept_count);
}

/**
 * @test Low outliers only (below median)
 */
void test_outliers_low_only(void) {
    uint16_t values[] = {850, 855, 845, 600, 650};  // 600 and 650 are low outliers
    bool keep_flags[5];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 5, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(3, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);   // 850
    TEST_ASSERT_TRUE(keep_flags[1]);   // 855
    TEST_ASSERT_TRUE(keep_flags[2]);   // 845
    TEST_ASSERT_FALSE(keep_flags[3]);  // 600 - low outlier
    TEST_ASSERT_FALSE(keep_flags[4]);  // 650 - low outlier
}

/**
 * @test High outliers only (above median)
 */
void test_outliers_high_only(void) {
    uint16_t values[] = {850, 855, 845, 1000, 1050};  // 1000 and 1050 are high outliers
    bool keep_flags[5];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 5, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(3, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);   // 850
    TEST_ASSERT_TRUE(keep_flags[1]);   // 855
    TEST_ASSERT_TRUE(keep_flags[2]);   // 845
    TEST_ASSERT_FALSE(keep_flags[3]);  // 1000 - high outlier
    TEST_ASSERT_FALSE(keep_flags[4]);  // 1050 - high outlier
}

/**
 * @test Median value always kept
 */
void test_outliers_median_always_kept(void) {
    uint16_t values[] = {850};  // Single value that is the median
    bool keep_flags[1];
    uint8_t kept_count = 0;
    
    filterOutliers(values, 1, 850, keep_flags, kept_count);
    
    TEST_ASSERT_EQUAL_UINT8(1, kept_count);
    TEST_ASSERT_TRUE(keep_flags[0]);
}

// Arduino framework entry points
#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_outliers_none_all_within_threshold);
    RUN_TEST(test_outliers_single_outlier);
    RUN_TEST(test_outliers_multiple_outliers);
    RUN_TEST(test_outliers_exactly_at_threshold_included);
    RUN_TEST(test_outliers_just_beyond_threshold_excluded);
    RUN_TEST(test_outliers_bimodal_within_tolerance);
    RUN_TEST(test_outliers_bimodal_wide_separation_still_valid);
    RUN_TEST(test_outliers_16_zones_typical);
    RUN_TEST(test_outliers_single_element);
    RUN_TEST(test_outliers_empty_array);
    RUN_TEST(test_outliers_low_only);
    RUN_TEST(test_outliers_high_only);
    RUN_TEST(test_outliers_median_always_kept);
    
    return UNITY_END();
}
#else
void setup() {
    // Wait for Serial to be ready
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_outliers_none_all_within_threshold);
    RUN_TEST(test_outliers_single_outlier);
    RUN_TEST(test_outliers_multiple_outliers);
    RUN_TEST(test_outliers_exactly_at_threshold_included);
    RUN_TEST(test_outliers_just_beyond_threshold_excluded);
    RUN_TEST(test_outliers_bimodal_within_tolerance);
    RUN_TEST(test_outliers_bimodal_wide_separation_still_valid);
    RUN_TEST(test_outliers_16_zones_typical);
    RUN_TEST(test_outliers_single_element);
    RUN_TEST(test_outliers_empty_array);
    RUN_TEST(test_outliers_low_only);
    RUN_TEST(test_outliers_high_only);
    RUN_TEST(test_outliers_median_always_kept);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
#endif
