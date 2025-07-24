#include "plane_estimator.h"
#include <algorithm>
#include <cmath>
#include <numeric>

PlaneEstimator::PlaneEstimator(int width, int height) : width_(width), height_(height)
{
    xs_.reserve(width * height);
    ys_.reserve(width * height);
    zs_.reserve(width * height);
}

bool PlaneEstimator::setDistances(const int16_t* distances, const uint8_t size)
{
    if (size != width_ * height_)
        return false;

    xs_.clear();
    ys_.clear();
    zs_.clear();
    for (int16_t y = 0; y < height_; ++y)
    {
        for (int16_t x = 0; x < width_; ++x)
        {
            int16_t index = y * width_ + x;
            if (distances[index] > 0)
            {
                xs_.push_back(x);
                ys_.push_back(y);
                zs_.push_back(distances[index]);
            }
        }
    }
    return true;
}

// 1. Simple least-squares fit for plane: z = ax + by + c
// 2. Least squares objective: minimize sum of (z - (ax + by + c))^2 over all points
// 3. Normal equations: A^T A [a, b, c]^T = A^T z

// Derivation
bool PlaneEstimator::estimatePlane()
{
    int N = xs_.size();
    if (N < 3)
    {
        return false;
    }
    // Coefficients needed
    float sum_x = std::accumulate(xs_.begin(), xs_.end(), 0.0f);
    float sum_y = std::accumulate(ys_.begin(), ys_.end(), 0.0f);
    float sum_z = std::accumulate(zs_.begin(), zs_.end(), 0.0f);
    float sum_xx = std::inner_product(xs_.begin(), xs_.end(), xs_.begin(), 0.0f);
    float sum_yy = std::inner_product(ys_.begin(), ys_.end(), ys_.begin(), 0.0f);
    float sum_xy = std::inner_product(xs_.begin(), xs_.end(), ys_.begin(), 0.0f);
    float sum_xz = std::inner_product(xs_.begin(), xs_.end(), zs_.begin(), 0.0f);
    float sum_yz = std::inner_product(ys_.begin(), ys_.end(), zs_.begin(), 0.0f);

    // Formula derivation:
    // Set up the normal equations
    // A^T A [a, b, c]^T = A^T z
    // And take the partial derivatives with respect to a, b, c and set to zero
    // to get the system of equations
    // sum_x * sum_z = a * sum_xx + b * sum_xy + c * sum_x
    // sum_y * sum_z = a * sum_xy + b * sum_yy + c * sum_y
    // sum_z = a * sum_x + b * sum_y + c * N
    // Written in matrix form
    // [sum_xx, sum_xy, sum_x] [a]   [sum_xz]
    // [sum_xy, sum_yy, sum_y] [b]   [sum_yz]
    // [sum_x, sum_y, N]       [c] = [sum_z]
    // Which is a 3x3 linear system. This can be solved using Cramer's rule
    // to obtain the equations for a, b, c
    // a = det(A_a) / det(A)
    // b = det(A_b) / det(A)
    // c = det(A_c) / det(A)

    float denom = N * sum_xx * sum_yy + 2 * sum_x * sum_y * sum_xy - sum_xx * sum_y * sum_y - sum_yy * sum_x * sum_x -
                  N * sum_xy * sum_xy;
    if (fabs(denom) < 1e-6)
    {
        return false;
    }
    a_ = (N * sum_xz * sum_yy + sum_x * sum_y * sum_z + sum_xy * sum_y * sum_z - sum_xz * sum_y * sum_y -
          sum_yy * sum_x * sum_z - N * sum_xy * sum_z) /
         denom;
    b_ = (N * sum_xx * sum_yz + sum_x * sum_y * sum_z + sum_xy * sum_x * sum_z - sum_xx * sum_y * sum_z -
          sum_yz * sum_x * sum_x - N * sum_xy * sum_z) /
         denom;
    c_ = (sum_z - a_ * sum_x - b_ * sum_y) / N;
    estimated_ = true;
    return true;
}

float PlaneEstimator::estimatedDistanceToPlane() const
{
    if (!estimated_)
        return -1.0f;
    float centerX = (width_ - 1) / 2.0f;
    float centerY = (height_ - 1) / 2.0f;
    return a_ * centerX + b_ * centerY + c_;
}
