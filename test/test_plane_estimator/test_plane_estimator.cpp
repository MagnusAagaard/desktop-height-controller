#include <unity.h>
#include "plane_estimator.h"

void test_setting_distances()
{
    PlaneEstimator estimator(8, 8);
    int16_t distances[64] = {0};
    for (int i = 0; i < 64; ++i)
    {
        distances[i] = i / 8 + 1;
    }
    int16_t wrong_size_distances[8] = {0};
    TEST_ASSERT_FALSE(estimator.setDistances(wrong_size_distances, 8));
    TEST_ASSERT_TRUE(estimator.setDistances(distances, 64));
}

void test_plane_estimation()
{
    PlaneEstimator estimator(8, 8);
    int16_t distances[64] = {0};
    for (int i = 0; i < 64; ++i)
    {
        distances[i] = i / 8 + 1;
    }
    TEST_ASSERT_FALSE(estimator.estimatePlane());
    TEST_ASSERT_EQUAL_FLOAT(-1.0, estimator.estimatedDistanceToPlane());
    TEST_ASSERT_TRUE(estimator.setDistances(distances, 64));
    TEST_ASSERT_TRUE(estimator.estimatePlane());
    TEST_ASSERT_EQUAL_FLOAT(4.5, estimator.estimatedDistanceToPlane());
}

void setup()
{
    UNITY_BEGIN();

    RUN_TEST(test_setting_distances);
    RUN_TEST(test_plane_estimation);

    UNITY_END();
}

void loop() {}