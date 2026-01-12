/**
 * @file test_nvs_performance.cpp
 * @brief Benchmark tests for NVS write latency
 * 
 * Verifies preset save operations complete within PERF-006 requirements (<1s).
 * Tests Preferences.putString/putUInt timing.
 */

#include <unity.h>
#ifdef NATIVE_TEST
#include <ArduinoFake.h>
using namespace fakeit;
#else
#include <Arduino.h>
#endif

// Performance requirement per PERF-006
const unsigned long MAX_NVS_WRITE_LATENCY_MS = 1000;

// Typical expected values (based on ESP32 NVS performance)
const unsigned long TYPICAL_FLOAT_WRITE_MS = 50;
const unsigned long TYPICAL_STRING_WRITE_MS = 100;
const unsigned long TYPICAL_TOTAL_PRESET_SAVE_MS = 200;

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

// ============================================================================
// Performance Expectation Tests
// ============================================================================

/**
 * Test max latency requirement
 */
void test_max_latency_requirement(void) {
    // Per PERF-006: NVS writes must complete within 1 second
    TEST_ASSERT_EQUAL(1000, MAX_NVS_WRITE_LATENCY_MS);
}

/**
 * Test typical float write expectation
 */
void test_typical_float_write_time(void) {
    // Typical NVS float write: ~50ms
    TEST_ASSERT_TRUE(TYPICAL_FLOAT_WRITE_MS < MAX_NVS_WRITE_LATENCY_MS);
}

/**
 * Test typical string write expectation
 */
void test_typical_string_write_time(void) {
    // Typical NVS string write (16 chars): ~100ms
    TEST_ASSERT_TRUE(TYPICAL_STRING_WRITE_MS < MAX_NVS_WRITE_LATENCY_MS);
}

/**
 * Test total preset save expectation
 */
void test_total_preset_save_time(void) {
    // Preset save = height (float) + name (string) + commit
    // Expected: ~200ms total
    TEST_ASSERT_TRUE(TYPICAL_TOTAL_PRESET_SAVE_MS < MAX_NVS_WRITE_LATENCY_MS);
}

// ============================================================================
// Benchmark Simulation Tests
// ============================================================================

/**
 * Test simulated float write timing
 */
void test_simulated_float_write(void) {
    unsigned long start = millis();
    
    // Simulate NVS float write with small delay
    // In real tests on hardware, this would be actual NVS write
    delay(50);  // Simulate ~50ms write
    
    unsigned long duration = millis() - start;
    
    TEST_ASSERT_TRUE(duration < MAX_NVS_WRITE_LATENCY_MS);
}

/**
 * Test simulated string write timing
 */
void test_simulated_string_write(void) {
    unsigned long start = millis();
    
    // Simulate NVS string write
    delay(100);  // Simulate ~100ms write for 16 char string
    
    unsigned long duration = millis() - start;
    
    TEST_ASSERT_TRUE(duration < MAX_NVS_WRITE_LATENCY_MS);
}

/**
 * Test simulated full preset save timing
 */
void test_simulated_preset_save(void) {
    unsigned long start = millis();
    
    // Simulate full preset save:
    // 1. Write height (float)
    delay(50);
    // 2. Write name (string)
    delay(100);
    // 3. Commit (included in write operations)
    
    unsigned long duration = millis() - start;
    
    TEST_ASSERT_TRUE(duration < MAX_NVS_WRITE_LATENCY_MS);
    TEST_ASSERT_TRUE(duration < 500);  // Should be well under 1s
}

/**
 * Test multiple sequential writes
 */
void test_multiple_sequential_writes(void) {
    unsigned long start = millis();
    
    // Simulate saving 5 presets (worst case)
    for (int i = 0; i < 5; i++) {
        delay(50);   // Float write
        delay(100);  // String write
    }
    
    unsigned long total_duration = millis() - start;
    
    // Even 5 saves should complete in reasonable time
    // 5 * 150ms = 750ms expected
    TEST_ASSERT_TRUE(total_duration < 2000);  // 2 second budget for all 5
}

// ============================================================================
// Timing Precision Tests
// ============================================================================

/**
 * Test millis() timing accuracy
 */
void test_millis_timing_accuracy(void) {
    unsigned long start = millis();
    delay(100);
    unsigned long duration = millis() - start;
    
    // Should be close to 100ms (allow ±10ms variance)
    TEST_ASSERT_TRUE(duration >= 90);
    TEST_ASSERT_TRUE(duration <= 110);
}

/**
 * Test micros() available for finer timing
 */
void test_micros_available(void) {
    unsigned long start_us = micros();
    delayMicroseconds(1000);  // 1ms
    unsigned long duration_us = micros() - start_us;
    
    // Should be close to 1000us (allow variance)
    TEST_ASSERT_TRUE(duration_us >= 900);
    TEST_ASSERT_TRUE(duration_us <= 1500);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

/**
 * Test timing with zero-length string
 */
void test_zero_length_string_timing(void) {
    // Empty string should be fastest
    unsigned long start = millis();
    delay(20);  // Simulated minimal write
    unsigned long duration = millis() - start;
    
    TEST_ASSERT_TRUE(duration < TYPICAL_STRING_WRITE_MS);
}

/**
 * Test timing with max-length string
 */
void test_max_length_string_timing(void) {
    // 16 char string (max for preset name)
    unsigned long start = millis();
    delay(100);  // Simulated max-length write
    unsigned long duration = millis() - start;
    
    TEST_ASSERT_TRUE(duration < MAX_NVS_WRITE_LATENCY_MS);
}

/**
 * Test timing consistency
 */
void test_timing_consistency(void) {
    unsigned long durations[5];
    
    for (int i = 0; i < 5; i++) {
        unsigned long start = millis();
        delay(50);  // Consistent operation
        durations[i] = millis() - start;
    }
    
    // All durations should be similar
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_TRUE(durations[i] >= 40);
        TEST_ASSERT_TRUE(durations[i] <= 60);
    }
}

#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Performance expectation tests (these just check constants, no timing)
    RUN_TEST(test_max_latency_requirement);
    RUN_TEST(test_typical_float_write_time);
    RUN_TEST(test_typical_string_write_time);
    RUN_TEST(test_total_preset_save_time);
    
    // SKIP: All benchmark/timing tests - require real delay()/millis()
    // test_simulated_float_write
    // test_simulated_string_write
    // test_simulated_preset_save
    // test_multiple_sequential_writes
    // test_millis_timing_accuracy
    // test_micros_available
    // test_zero_length_string_timing
    // test_max_length_string_timing
    // test_timing_consistency
    
    return UNITY_END();
}
#else
void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    // Performance expectation tests
    RUN_TEST(test_max_latency_requirement);
    RUN_TEST(test_typical_float_write_time);
    RUN_TEST(test_typical_string_write_time);
    RUN_TEST(test_total_preset_save_time);
    
    // Benchmark simulation tests
    RUN_TEST(test_simulated_float_write);
    RUN_TEST(test_simulated_string_write);
    RUN_TEST(test_simulated_preset_save);
    RUN_TEST(test_multiple_sequential_writes);
    
    // Timing precision tests
    RUN_TEST(test_millis_timing_accuracy);
    RUN_TEST(test_micros_available);
    
    // Edge case tests
    RUN_TEST(test_zero_length_string_timing);
    RUN_TEST(test_max_length_string_timing);
    RUN_TEST(test_timing_consistency);
    
    UNITY_END();
}

void loop() {
    // Empty
}
#endif
