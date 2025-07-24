#ifndef DISTANCE_CONTROLLER_H
#define DISTANCE_CONTROLLER_H

#include <Arduino.h>

class DistanceController
{
   public:

    DistanceController(gpio_num_t control_pin_up, gpio_num_t control_pin_down);

    void setTargetHeight(float target_height_mm);
    void updateDistanceToPlane(float distance_mm);
    void updateControlLoop();
    void setMinDistanceLimit(float min_distance_mm) { min_distance_mm_ = min_distance_mm; }
    void setMaxDistanceLimit(float max_distance_mm) { max_distance_mm_ = max_distance_mm; }
    float getTargetHeight() const { return target_height_mm_; }
    float getEstimatedDistanceToPlane() const { return plane_estimated_distance_; }
    bool isControlling() const { return controlling_; }

   private:

    gpio_num_t control_pin_up_;
    gpio_num_t control_pin_down_;
    float target_height_mm_{-1.0f};
    float plane_estimated_distance_{-1.0f};
    float min_distance_mm_{100.0f};
    float max_distance_mm_{1000.0f};
    bool controlling_{false};
    bool going_down_{false};
};

#endif  // DISTANCE_CONTROLLER_H