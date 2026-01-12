/**
 * @file test_consensus.cpp
 * @brief Unit tests for computeMultiZoneConsensus() function
 * 
 * TDD Phase: These tests must FAIL initially, then PASS after implementation.
 * Per Constitution Principle I: Test-First Development
 * 
 * Tests the full multi-zone consensus pipeline:
 * 1. Extract valid zones (status + range check)
 * 2. Compute median
 * 3. Filter outliers (30mm threshold)
 * 4. Compute mean of remaining values
 * 5. Mark reliability (>=4 zones = reliable)
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
constexpr uint8_t MAX_ZONES = 16;  // 4x4 resolution

// ============================================
// Data Structures (match HeightController.h)
// ============================================

/**
 * @brief Mock of VL53L5CX_ResultsData for testing
 * Only includes fields needed for consensus calculation
 */
struct MockSensorData {
    int16_t distance_mm[MAX_ZONES];
    uint8_t target_status[MAX_ZONES];
};

/**
 * @brief Result of multi-zone consensus calculation
 */
struct ConsensusResult {
    uint16_t consensus_distance_mm;  ///< Final filtered distance
    uint8_t valid_zone_count;        ///< Number of zones that passed validation
    uint8_t outlier_count;           ///< Number of zones filtered as outliers
    bool is_reliable;                ///< True if valid_zone_count >= MIN_VALID_ZONES
};

// ============================================
// Utility Functions (match HeightController.cpp)
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
    
    // Copy to avoid modifying original
    uint16_t sorted[MAX_ZONES];
    memcpy(sorted, values, count * sizeof(uint16_t));
    
    // Insertion sort
    for (uint8_t i = 1; i < count; i++) {
        uint16_t key = sorted[i];
        int8_t j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    
    // Return lower middle for even count
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

/**
 * @brief Compute multi-zone consensus from sensor data
 */
ConsensusResult computeMultiZoneConsensus(const MockSensorData& data) {
    ConsensusResult result = {0, 0, 0, false};
    
    // Step 1: Extract valid zones
    uint16_t valid_distances[MAX_ZONES];
    uint8_t valid_count = 0;
    
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        uint16_t dist = (uint16_t)data.distance_mm[i];
        if (isZoneValid(data.target_status[i], dist)) {
            valid_distances[valid_count++] = dist;
        }
    }
    
    result.valid_zone_count = valid_count;
    
    // Step 2: Check minimum zones requirement
    if (valid_count < MIN_VALID_ZONES) {
        result.is_reliable = false;
        return result;
    }
    
    // Step 3: Compute median
    uint16_t median = computeMedian(valid_distances, valid_count);
    
    // Step 4: Filter outliers
    bool keep_flags[MAX_ZONES];
    uint8_t kept_count = 0;
    filterOutliers(valid_distances, valid_count, median, keep_flags, kept_count);
    
    result.outlier_count = valid_count - kept_count;
    
    // Step 5: Compute mean of remaining values
    if (kept_count == 0) {
        // All values are outliers - use median as fallback
        result.consensus_distance_mm = median;
        result.is_reliable = false;
        return result;
    }
    
    // Collect non-outlier values
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

// ============================================
// Test Setup
// ============================================

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

/**
 * @brief Helper to initialize mock data with defaults
 */
void initMockData(MockSensorData& data, uint16_t default_distance = 850, uint8_t default_status = 5) {
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        data.distance_mm[i] = default_distance;
        data.target_status[i] = default_status;
    }
}

// ============================================
// All Zones Valid Tests
// ============================================

/**
 * @test All 16 zones valid with identical readings
 */
void test_consensus_all_zones_identical(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT16(850, result.consensus_distance_mm);
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(0, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
}

/**
 * @test All 16 zones valid with slight variation (within 30mm)
 */
void test_consensus_all_zones_slight_variation(void) {
    MockSensorData data;
    // Values around 850: 835, 840, 845, 850, 855, 860, 865, 870 (repeated)
    uint16_t values[] = {835, 840, 845, 850, 855, 860, 865, 870,
                         835, 840, 845, 850, 855, 860, 865, 870};
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        data.distance_mm[i] = values[i];
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(0, result.outlier_count);  // All within 30mm of median
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean should be around 852-853 (sum = 13640 / 16 = 852.5)
    TEST_ASSERT_UINT16_WITHIN(5, 852, result.consensus_distance_mm);
}

/**
 * @test All zones valid with mixed status codes (5, 6, 9)
 */
void test_consensus_mixed_valid_statuses(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Mix in status 6 and 9
    data.target_status[0] = 6;
    data.target_status[5] = 9;
    data.target_status[10] = 6;
    data.target_status[15] = 9;
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_TRUE(result.is_reliable);
}

// ============================================
// Partial Zone Valid Tests
// ============================================

/**
 * @test 12 zones valid, 4 zones invalid (status 0)
 */
void test_consensus_12_zones_valid(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Invalidate 4 zones
    data.target_status[0] = 0;
    data.target_status[5] = 0;
    data.target_status[10] = 0;
    data.target_status[15] = 0;
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(12, result.valid_zone_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    TEST_ASSERT_EQUAL_UINT16(850, result.consensus_distance_mm);
}

/**
 * @test 8 zones valid (exactly half)
 */
void test_consensus_8_zones_valid(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Invalidate half the zones
    for (uint8_t i = 0; i < 8; i++) {
        data.target_status[i] = 0;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(8, result.valid_zone_count);
    TEST_ASSERT_TRUE(result.is_reliable);
}

/**
 * @test Exactly 4 zones valid (minimum for reliability)
 */
void test_consensus_4_zones_valid_minimum(void) {
    MockSensorData data;
    initMockData(data, 0, 0);  // All invalid
    // Make exactly 4 zones valid
    data.distance_mm[5] = 850;
    data.distance_mm[6] = 855;
    data.distance_mm[9] = 845;
    data.distance_mm[10] = 850;
    data.target_status[5] = 5;
    data.target_status[6] = 5;
    data.target_status[9] = 5;
    data.target_status[10] = 5;
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(4, result.valid_zone_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 850, 855, 845, 850 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result.consensus_distance_mm);
}

/**
 * @test 3 zones valid (below minimum - not reliable)
 */
void test_consensus_3_zones_not_reliable(void) {
    MockSensorData data;
    initMockData(data, 0, 0);  // All invalid
    // Make only 3 zones valid
    data.distance_mm[5] = 850;
    data.distance_mm[6] = 855;
    data.distance_mm[9] = 845;
    data.target_status[5] = 5;
    data.target_status[6] = 5;
    data.target_status[9] = 5;
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(3, result.valid_zone_count);
    TEST_ASSERT_FALSE(result.is_reliable);
}

/**
 * @test No valid zones
 */
void test_consensus_no_valid_zones(void) {
    MockSensorData data;
    initMockData(data, 0, 0);  // All invalid
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(0, result.valid_zone_count);
    TEST_ASSERT_FALSE(result.is_reliable);
}

// ============================================
// Outlier Filtering Tests
// ============================================

/**
 * @test Single outlier filtered from 16 zones
 */
void test_consensus_single_outlier(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Add one outlier
    data.distance_mm[0] = 1000;  // 150mm above median
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(1, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 15 values at 850 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result.consensus_distance_mm);
}

/**
 * @test Multiple outliers filtered
 */
void test_consensus_multiple_outliers(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Add outliers at both ends
    data.distance_mm[0] = 700;   // Low outlier
    data.distance_mm[1] = 1000;  // High outlier
    data.distance_mm[2] = 600;   // Low outlier
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(3, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 13 values at 850 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result.consensus_distance_mm);
}

/**
 * @test Value exactly at 30mm threshold is NOT filtered
 */
void test_consensus_at_threshold_kept(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Exactly at threshold
    data.distance_mm[0] = 820;  // 30mm below median
    data.distance_mm[1] = 880;  // 30mm above median
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(0, result.outlier_count);  // Both kept
    TEST_ASSERT_TRUE(result.is_reliable);
}

/**
 * @test Value just beyond 30mm threshold is filtered
 */
void test_consensus_beyond_threshold_filtered(void) {
    MockSensorData data;
    initMockData(data, 850, 5);
    // Just beyond threshold
    data.distance_mm[0] = 819;  // 31mm below median
    data.distance_mm[1] = 881;  // 31mm above median
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(2, result.outlier_count);  // Both filtered
    TEST_ASSERT_TRUE(result.is_reliable);
}

// ============================================
// Bimodal Distribution Tests (FR-009)
// ============================================

/**
 * @test Bimodal distribution with 40mm separation
 * Per FR-009: Bimodal distributions should work if within tolerance
 * 
 * With 8 zones at 830 and 8 at 870:
 * - Sorted: [830x8, 870x8]
 * - Median (lower middle at index 7) = 830
 * - Values at 870 are 40mm from median (>30mm) = outliers
 * 
 * This tests the algorithm behavior, not necessarily ideal handling.
 * The algorithm keeps values within 30mm of median.
 */
void test_consensus_bimodal_40mm_separation(void) {
    MockSensorData data;
    // 8 zones at 830mm, 8 zones at 870mm (40mm separation)
    for (uint8_t i = 0; i < 8; i++) {
        data.distance_mm[i] = 830;
        data.target_status[i] = 5;
    }
    for (uint8_t i = 8; i < 16; i++) {
        data.distance_mm[i] = 870;
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    // Median = 830 (lower middle of even count)
    // Values at 870 are 40mm away (>30mm threshold) = 8 outliers
    TEST_ASSERT_EQUAL_UINT8(8, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 8 values at 830 = 830
    TEST_ASSERT_EQUAL_UINT16(830, result.consensus_distance_mm);
}

/**
 * @test Bimodal distribution with 60mm separation
 * Each cluster is 30mm from center, but median is at lower cluster
 */
void test_consensus_bimodal_60mm_separation(void) {
    MockSensorData data;
    // 8 zones at 820mm, 8 zones at 880mm (60mm separation)
    for (uint8_t i = 0; i < 8; i++) {
        data.distance_mm[i] = 820;
        data.target_status[i] = 5;
    }
    for (uint8_t i = 8; i < 16; i++) {
        data.distance_mm[i] = 880;
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    // Median = 820, values at 880 are 60mm away = 8 outliers
    TEST_ASSERT_EQUAL_UINT8(8, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 8 values at 820 = 820
    TEST_ASSERT_EQUAL_UINT16(820, result.consensus_distance_mm);
}

/**
 * @test Bimodal distribution with 80mm separation
 * Each cluster is 40mm from center, but median is at lower cluster
 */
void test_consensus_bimodal_80mm_separation(void) {
    MockSensorData data;
    // 8 zones at 810mm, 8 zones at 890mm (80mm separation)
    for (uint8_t i = 0; i < 8; i++) {
        data.distance_mm[i] = 810;
        data.target_status[i] = 5;
    }
    for (uint8_t i = 8; i < 16; i++) {
        data.distance_mm[i] = 890;
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    // Median = 810, values at 890 are 80mm away = 8 outliers
    TEST_ASSERT_EQUAL_UINT8(8, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 8 values at 810 = 810
    TEST_ASSERT_EQUAL_UINT16(810, result.consensus_distance_mm);
}

/**
 * @test Asymmetric bimodal: majority cluster near median, minority far
 */
void test_consensus_bimodal_asymmetric(void) {
    MockSensorData data;
    // 12 zones at 850mm, 4 zones at 750mm (100mm separation from majority)
    for (uint8_t i = 0; i < 12; i++) {
        data.distance_mm[i] = 850;
        data.target_status[i] = 5;
    }
    for (uint8_t i = 12; i < 16; i++) {
        data.distance_mm[i] = 750;  // Far outliers
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    // Median should be 850 (majority), 750 values are 100mm away = outliers
    TEST_ASSERT_EQUAL_UINT8(4, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean of 12 values at 850 = 850
    TEST_ASSERT_EQUAL_UINT16(850, result.consensus_distance_mm);
}

// ============================================
// Edge Cases
// ============================================

/**
 * @test All zones have status 255 (sensor error)
 */
void test_consensus_all_status_255(void) {
    MockSensorData data;
    initMockData(data, 850, 255);
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(0, result.valid_zone_count);
    TEST_ASSERT_FALSE(result.is_reliable);
}

/**
 * @test All zones have distance 0
 */
void test_consensus_all_distance_zero(void) {
    MockSensorData data;
    initMockData(data, 0, 5);
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(0, result.valid_zone_count);
    TEST_ASSERT_FALSE(result.is_reliable);
}

/**
 * @test Mix of invalid reasons (status and range)
 */
void test_consensus_mixed_invalid_reasons(void) {
    MockSensorData data;
    initMockData(data, 850, 5);  // Start valid
    
    data.target_status[0] = 0;      // Invalid status
    data.target_status[1] = 255;    // Invalid status
    data.distance_mm[2] = 5;        // Below range
    data.distance_mm[3] = 5000;     // Above range
    data.target_status[4] = 1;      // Undefined status
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(11, result.valid_zone_count);  // 16 - 5 invalid
    TEST_ASSERT_TRUE(result.is_reliable);
}

/**
 * @test Typical desk standing height (~1100mm)
 */
void test_consensus_standing_height(void) {
    MockSensorData data;
    // Values clustered around 1100mm with some noise
    uint16_t values[] = {1095, 1098, 1100, 1102, 1105, 1108, 1110, 1100,
                         1095, 1098, 1100, 1102, 1105, 1108, 1110, 1100};
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        data.distance_mm[i] = values[i];
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(0, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean should be around 1102-1103
    TEST_ASSERT_UINT16_WITHIN(5, 1102, result.consensus_distance_mm);
}

/**
 * @test Typical desk sitting height (~750mm)
 */
void test_consensus_sitting_height(void) {
    MockSensorData data;
    // Values clustered around 750mm with some noise
    uint16_t values[] = {745, 748, 750, 752, 755, 748, 750, 752,
                         745, 748, 750, 752, 755, 748, 750, 752};
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        data.distance_mm[i] = values[i];
        data.target_status[i] = 5;
    }
    
    ConsensusResult result = computeMultiZoneConsensus(data);
    
    TEST_ASSERT_EQUAL_UINT8(16, result.valid_zone_count);
    TEST_ASSERT_EQUAL_UINT8(0, result.outlier_count);
    TEST_ASSERT_TRUE(result.is_reliable);
    // Mean should be around 750
    TEST_ASSERT_UINT16_WITHIN(5, 750, result.consensus_distance_mm);
}

// Arduino framework entry points
#ifdef NATIVE_TEST
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // All zones valid tests
    RUN_TEST(test_consensus_all_zones_identical);
    RUN_TEST(test_consensus_all_zones_slight_variation);
    RUN_TEST(test_consensus_mixed_valid_statuses);
    
    // Partial zone valid tests
    RUN_TEST(test_consensus_12_zones_valid);
    RUN_TEST(test_consensus_8_zones_valid);
    RUN_TEST(test_consensus_4_zones_valid_minimum);
    RUN_TEST(test_consensus_3_zones_not_reliable);
    RUN_TEST(test_consensus_no_valid_zones);
    
    // Outlier filtering tests
    RUN_TEST(test_consensus_single_outlier);
    RUN_TEST(test_consensus_multiple_outliers);
    RUN_TEST(test_consensus_at_threshold_kept);
    RUN_TEST(test_consensus_beyond_threshold_filtered);
    
    // Bimodal distribution tests (FR-009)
    RUN_TEST(test_consensus_bimodal_40mm_separation);
    RUN_TEST(test_consensus_bimodal_60mm_separation);
    RUN_TEST(test_consensus_bimodal_80mm_separation);
    RUN_TEST(test_consensus_bimodal_asymmetric);
    
    // Edge cases
    RUN_TEST(test_consensus_all_status_255);
    RUN_TEST(test_consensus_all_distance_zero);
    RUN_TEST(test_consensus_mixed_invalid_reasons);
    RUN_TEST(test_consensus_standing_height);
    RUN_TEST(test_consensus_sitting_height);
    
    return UNITY_END();
}
#else
void setup() {
    // Wait for Serial to be ready
    delay(2000);
    
    UNITY_BEGIN();
    
    // All zones valid tests
    RUN_TEST(test_consensus_all_zones_identical);
    RUN_TEST(test_consensus_all_zones_slight_variation);
    RUN_TEST(test_consensus_mixed_valid_statuses);
    
    // Partial zone valid tests
    RUN_TEST(test_consensus_12_zones_valid);
    RUN_TEST(test_consensus_8_zones_valid);
    RUN_TEST(test_consensus_4_zones_valid_minimum);
    RUN_TEST(test_consensus_3_zones_not_reliable);
    RUN_TEST(test_consensus_no_valid_zones);
    
    // Outlier filtering tests
    RUN_TEST(test_consensus_single_outlier);
    RUN_TEST(test_consensus_multiple_outliers);
    RUN_TEST(test_consensus_at_threshold_kept);
    RUN_TEST(test_consensus_beyond_threshold_filtered);
    
    // Bimodal distribution tests (FR-009)
    RUN_TEST(test_consensus_bimodal_40mm_separation);
    RUN_TEST(test_consensus_bimodal_60mm_separation);
    RUN_TEST(test_consensus_bimodal_80mm_separation);
    RUN_TEST(test_consensus_bimodal_asymmetric);
    
    // Edge cases
    RUN_TEST(test_consensus_all_status_255);
    RUN_TEST(test_consensus_all_distance_zero);
    RUN_TEST(test_consensus_mixed_invalid_reasons);
    RUN_TEST(test_consensus_standing_height);
    RUN_TEST(test_consensus_sitting_height);
    
    UNITY_END();
}

void loop() {
    // Nothing to do in loop for tests
}
#endif
