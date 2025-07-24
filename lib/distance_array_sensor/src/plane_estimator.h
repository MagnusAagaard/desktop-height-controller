#ifndef PLANE_ESTIMATOR_H
#define PLANE_ESTIMATOR_H

#include <cstdint>
#include <vector>

class PlaneEstimator
{
   public:

    PlaneEstimator(int width, int height);

    bool setDistances(const int16_t* distances, const uint8_t size);
    bool estimatePlane();
    // Returns the estimated distance to the plane at the center
    float estimatedDistanceToPlane() const;

   private:

    std::vector<int16_t> xs_, ys_, zs_;
    int width_{0};
    int height_{0};
    float a_{0};
    float b_{0};
    float c_{0};
    bool estimated_{false};
};

#endif  // PLANE_ESTIMATOR_H