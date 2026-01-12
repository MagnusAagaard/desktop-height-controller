/**
 * @file test_median.cpp
 * @brief Unit tests for computeMedian() statistical utility
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
#include <cstring>
#include "multi_zone_filtering.h"

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

/**
 * @test Median of odd count array returns middle value
 */
void test_median_odd_count_5_elements(void) {
    uint16_t values[] = {800, 850, 840, 860, 845};
    uint16_t result = computeMedian(values, 5);
    // Sorted: 800, 840, 845, 850, 860 -> middle is 845
    TEST_ASSERT_EQUAL_UINT16(845, result);
}

/**
 * @test Median of odd count array with 3 elements
 */
void test_median_odd_count_3_elements(void) {
    uint16_t values[] = {900, 850, 870};
    uint16_t result = computeMedian(values, 3);
    // Sorted: 850, 870, 900 -> middle is 870
    TEST_ASSERT_EQUAL_UINT16(870, result);
}

/**
 * @test Median of even count returns lower middle value
 * Per research.md: For even counts, return lower of two middle values
 */
void test_median_even_count_4_elements(void) {
    uint16_t values[] = {800, 850, 840, 860};
    uint16_t result = computeMedian(values, 4);
    // Sorted: 800, 840, 850, 860 -> lower middle is 840 (index 1)
    TEST_ASSERT_EQUAL_UINT16(840, result);
}

/**
 * @test Median of even count with 6 elements
 */
void test_median_even_count_6_elements(void) {
    uint16_t values[] = {810, 820, 830, 840, 850, 860};
    uint16_t result = computeMedian(values, 6);
    // Already sorted, lower middle is index 2 = 830
    TEST_ASSERT_EQUAL_UINT16(830, result);
}

/**
 * @test Median of single element returns that element
 */
void test_median_single_element(void) {
    uint16_t values[] = {750};
    uint16_t result = computeMedian(values, 1);
    TEST_ASSERT_EQUAL_UINT16(750, result);
}

/**
 * @test Median with duplicate values
 */
void test_median_duplicates(void) {
    uint16_t values[] = {800, 800, 850, 850};
    uint16_t result = computeMedian(values, 4);
    // Sorted: 800, 800, 850, 850 -> lower middle is 800 (index 1)
    TEST_ASSERT_EQUAL_UINT16(800, result);
}

/**
 * @test Median with all same values
 */
void test_median_all_same(void) {
    uint16_t values[] = {850, 850, 850, 850, 850};
    uint16_t result = computeMedian(values, 5);
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

/**
 * @test Median with maximum 16 elements (typical multi-zone scenario)
 */
void test_median_16_elements(void) {
    uint16_t values[] = {800, 810, 820, 830, 840, 850, 860, 870,
                         880, 890, 900, 910, 920, 930, 940, 950};
    uint16_t result = computeMedian(values, 16);
    // Even count, lower middle at index 7 = 870
    TEST_ASSERT_EQUAL_UINT16(870, result);
}

/**
 * @test Median handles reverse-sorted input
 */
void test_median_reverse_sorted(void) {
    uint16_t values[] = {900, 850, 800, 750, 700};
    uint16_t result = computeMedian(values, 5);
    // Sorted: 700, 750, 800, 850, 900 -> middle is 800
    TEST_ASSERT_EQUAL_UINT16(800, result);
}

/**
 * @test Median with outlier value (edge case for filtering context)
 */
void test_median_with_outlier(void) {
    uint16_t values[] = {850, 845, 855, 840, 1200};  // 1200 is outlier
    uint16_t result = computeMedian(values, 5);
    // Sorted: 840, 845, 850, 855, 1200 -> middle is 850
    TEST_ASSERT_EQUAL_UINT16(850, result);
}

/**
 * @test Median with zero count returns 0 (edge case)
 */
void test_median_zero_count(void) {
    uint16_t values[] = {800, 850};
    uint16_t result = computeMedian(values, 0);
    TEST_ASSERT_EQUAL_UINT16(0, result);
}

/**
 * @test Median with two elements
 */
void test_median_two_elements(void) {
    uint16_t values[] = {900, 800};
    uint16_t result = computeMedian(values, 2);
    // Sorted: 800, 900 -> lower middle is 800 (index 0)
    TEST_ASSERT_EQUAL_UINT16(800, result);
}

// Arduino framework entry points
#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_median_odd_count_5_elements);
    RUN_TEST(test_median_odd_count_3_elements);
    RUN_TEST(test_median_even_count_4_elements);
    RUN_TEST(test_median_even_count_6_elements);
    RUN_TEST(test_median_single_element);
    RUN_TEST(test_median_duplicates);
    RUN_TEST(test_median_all_same);
    RUN_TEST(test_median_16_elements);
    RUN_TEST(test_median_reverse_sorted);
    RUN_TEST(test_median_with_outlier);
    RUN_TEST(test_median_zero_count);
    RUN_TEST(test_median_two_elements);
    
    return UNITY_END();
}
#else
void setup() {
    // Wait for Serial to be ready
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_median_odd_count_5_elements);
    RUN_TEST(test_median_odd_count_3_elements);
    RUN_TEST(test_median_even_count_4_elements);
    RUN_TEST(test_median_even_count_6_elements);
    RUN_TEST(test_median_single_element);
    RUN_TEST(test_median_duplicates);
    RUN_TEST(test_median_all_same);
    RUN_TEST(test_median_16_elements);
    RUN_TEST(test_median_reverse_sorted);
    RUN_TEST(test_median_with_outlier);
    RUN_TEST(test_median_zero_count);
    RUN_TEST(test_median_two_elements);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
#endif
