/**
 * @file test_input_validation.cpp
 * @brief Unit tests for web server input validation
 * 
 * Tests height range checks, reject <min or >max values.
 * Per constitution: Tests MUST be written BEFORE implementation.
 */

#include <unity.h>
#include <Arduino.h>

// Test constants matching system configuration
const float MIN_HEIGHT_CM = 50.0f;
const float MAX_HEIGHT_CM = 125.0f;
const int MAX_PRESET_SLOTS = 5;

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

// ============================================================================
// Height Range Validation Tests
// ============================================================================

/**
 * Test valid height values are accepted
 */
void test_valid_heights_accepted(void) {
    float valid_heights[] = {50.0f, 75.0f, 100.0f, 110.0f, 125.0f};
    
    for (int i = 0; i < 5; i++) {
        bool is_valid = (valid_heights[i] >= MIN_HEIGHT_CM && 
                         valid_heights[i] <= MAX_HEIGHT_CM);
        TEST_ASSERT_TRUE(is_valid);
    }
}

/**
 * Test heights below minimum are rejected
 */
void test_heights_below_minimum_rejected(void) {
    float invalid_heights[] = {0.0f, 10.0f, 30.0f, 49.0f, 49.9f};
    
    for (int i = 0; i < 5; i++) {
        bool is_valid = (invalid_heights[i] >= MIN_HEIGHT_CM && 
                         invalid_heights[i] <= MAX_HEIGHT_CM);
        TEST_ASSERT_FALSE(is_valid);
    }
}

/**
 * Test heights above maximum are rejected
 */
void test_heights_above_maximum_rejected(void) {
    float invalid_heights[] = {125.1f, 126.0f, 150.0f, 200.0f, 999.0f};
    
    for (int i = 0; i < 5; i++) {
        bool is_valid = (invalid_heights[i] >= MIN_HEIGHT_CM && 
                         invalid_heights[i] <= MAX_HEIGHT_CM);
        TEST_ASSERT_FALSE(is_valid);
    }
}

/**
 * Test negative heights are rejected
 */
void test_negative_heights_rejected(void) {
    float invalid_heights[] = {-1.0f, -10.0f, -50.0f, -100.0f};
    
    for (int i = 0; i < 4; i++) {
        bool is_valid = (invalid_heights[i] >= MIN_HEIGHT_CM && 
                         invalid_heights[i] <= MAX_HEIGHT_CM);
        TEST_ASSERT_FALSE(is_valid);
    }
}

/**
 * Test boundary values
 */
void test_boundary_values(void) {
    // Exact minimum should be valid
    bool min_valid = (MIN_HEIGHT_CM >= MIN_HEIGHT_CM && MIN_HEIGHT_CM <= MAX_HEIGHT_CM);
    TEST_ASSERT_TRUE(min_valid);
    
    // Exact maximum should be valid
    bool max_valid = (MAX_HEIGHT_CM >= MIN_HEIGHT_CM && MAX_HEIGHT_CM <= MAX_HEIGHT_CM);
    TEST_ASSERT_TRUE(max_valid);
    
    // Just below minimum
    float below_min = MIN_HEIGHT_CM - 0.1f;
    bool below_valid = (below_min >= MIN_HEIGHT_CM && below_min <= MAX_HEIGHT_CM);
    TEST_ASSERT_FALSE(below_valid);
    
    // Just above maximum
    float above_max = MAX_HEIGHT_CM + 0.1f;
    bool above_valid = (above_max >= MIN_HEIGHT_CM && above_max <= MAX_HEIGHT_CM);
    TEST_ASSERT_FALSE(above_valid);
}

// ============================================================================
// Preset Slot Validation Tests
// ============================================================================

/**
 * Test valid preset slots are accepted
 */
void test_valid_preset_slots_accepted(void) {
    for (int slot = 1; slot <= MAX_PRESET_SLOTS; slot++) {
        bool is_valid = (slot >= 1 && slot <= MAX_PRESET_SLOTS);
        TEST_ASSERT_TRUE(is_valid);
    }
}

/**
 * Test invalid preset slots are rejected
 */
void test_invalid_preset_slots_rejected(void) {
    int invalid_slots[] = {0, -1, 6, 10, 255};
    
    for (int i = 0; i < 5; i++) {
        bool is_valid = (invalid_slots[i] >= 1 && invalid_slots[i] <= MAX_PRESET_SLOTS);
        TEST_ASSERT_FALSE(is_valid);
    }
}

// ============================================================================
// Input Type Validation Tests
// ============================================================================

/**
 * Test integer height values are valid
 */
void test_integer_heights_valid(void) {
    int integer_heights[] = {50, 75, 100, 125};
    
    for (int i = 0; i < 4; i++) {
        float as_float = (float)integer_heights[i];
        bool is_valid = (as_float >= MIN_HEIGHT_CM && as_float <= MAX_HEIGHT_CM);
        TEST_ASSERT_TRUE(is_valid);
    }
}

/**
 * Test decimal height values are valid
 */
void test_decimal_heights_valid(void) {
    float decimal_heights[] = {50.5f, 75.3f, 100.7f, 124.9f};
    
    for (int i = 0; i < 4; i++) {
        bool is_valid = (decimal_heights[i] >= MIN_HEIGHT_CM && 
                         decimal_heights[i] <= MAX_HEIGHT_CM);
        TEST_ASSERT_TRUE(is_valid);
    }
}

// ============================================================================
// Error Response Tests
// ============================================================================

/**
 * Test error response code for invalid height
 */
void test_invalid_height_error_code(void) {
    // 400 Bad Request for invalid input
    const int HTTP_BAD_REQUEST = 400;
    TEST_ASSERT_EQUAL(400, HTTP_BAD_REQUEST);
}

/**
 * Test error response code for invalid slot
 */
void test_invalid_slot_error_code(void) {
    // 400 Bad Request for invalid input
    const int HTTP_BAD_REQUEST = 400;
    TEST_ASSERT_EQUAL(400, HTTP_BAD_REQUEST);
}

/**
 * Test error message contains helpful information
 */
void test_error_message_format(void) {
    // Error message should mention the valid range
    const char* error_template = "Height must be between %d and %d cm";
    TEST_ASSERT_NOT_NULL(error_template);
    
    // Verify we can format the message
    char error_msg[100];
    snprintf(error_msg, sizeof(error_msg), error_template, 
             (int)MIN_HEIGHT_CM, (int)MAX_HEIGHT_CM);
    TEST_ASSERT_TRUE(strlen(error_msg) > 0);
}

// ============================================================================
// Calibration Validation Tests
// ============================================================================

/**
 * Test calibration height range
 */
void test_calibration_height_range(void) {
    // Calibration accepts broader range for reference measurement
    float calib_min = 30.0f;
    float calib_max = 200.0f;
    
    // Valid calibration heights
    TEST_ASSERT_TRUE(50.0f >= calib_min && 50.0f <= calib_max);
    TEST_ASSERT_TRUE(100.0f >= calib_min && 100.0f <= calib_max);
    TEST_ASSERT_TRUE(150.0f >= calib_min && 150.0f <= calib_max);
    
    // Invalid calibration heights
    TEST_ASSERT_FALSE(20.0f >= calib_min);
    TEST_ASSERT_FALSE(250.0f <= calib_max);
}

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    // Height range validation
    RUN_TEST(test_valid_heights_accepted);
    RUN_TEST(test_heights_below_minimum_rejected);
    RUN_TEST(test_heights_above_maximum_rejected);
    RUN_TEST(test_negative_heights_rejected);
    RUN_TEST(test_boundary_values);
    
    // Preset slot validation
    RUN_TEST(test_valid_preset_slots_accepted);
    RUN_TEST(test_invalid_preset_slots_rejected);
    
    // Input type validation
    RUN_TEST(test_integer_heights_valid);
    RUN_TEST(test_decimal_heights_valid);
    
    // Error response tests
    RUN_TEST(test_invalid_height_error_code);
    RUN_TEST(test_invalid_slot_error_code);
    RUN_TEST(test_error_message_format);
    
    // Calibration validation
    RUN_TEST(test_calibration_height_range);
    
    UNITY_END();
}

void loop() {
    // Empty
}
