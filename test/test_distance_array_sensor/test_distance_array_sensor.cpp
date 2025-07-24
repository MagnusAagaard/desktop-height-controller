#include <unity.h>
#include "distance_array_sensor.h"

static DistanceArraySensor sensor;

void test_distance_array_sensor_begin()
{
    TEST_ASSERT_TRUE(sensor.initialized());
}

void test_distance_array_sensor_update()
{
    delay(1000);  // Allow time for sensor to initialize
    sensor.update();
    TEST_ASSERT_TRUE(sensor.isDataReady());
}

void test_distance_array_sensor_getDistances()
{
    sensor.update();
    const int16_t* distances = sensor.getDistances();
    TEST_ASSERT_NOT_NULL(distances);
    TEST_ASSERT_EQUAL(sensor.getResolution(), 64);  // Assuming 8x8 resolution
}

void setUp()
{
    // This function is called before each test
    sensor.begin();
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_distance_array_sensor_begin);
    RUN_TEST(test_distance_array_sensor_update);
    RUN_TEST(test_distance_array_sensor_getDistances);
    UNITY_END();
}

void loop() {}