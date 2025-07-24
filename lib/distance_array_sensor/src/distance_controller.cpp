#include "distance_controller.h"

DistanceController::DistanceController(gpio_num_t control_pin_up, gpio_num_t control_pin_down)
    : control_pin_up_(control_pin_up), control_pin_down_(control_pin_down)
{
    pinMode(control_pin_up_, OUTPUT);
    pinMode(control_pin_down_, OUTPUT);
    digitalWrite(control_pin_up_, LOW);
    digitalWrite(control_pin_down_, LOW);
}

void DistanceController::setTargetHeight(float target_height_mm)
{
    if (target_height_mm < min_distance_mm_ || target_height_mm > max_distance_mm_)
    {
        return;
    }
    if (target_height_mm < plane_estimated_distance_)
    {
        going_down_ = true;
    }
    else
    {
        going_down_ = false;
    }
    target_height_mm_ = target_height_mm;
    controlling_ = true;
}

void DistanceController::updateDistanceToPlane(float distance_mm)
{
    plane_estimated_distance_ = distance_mm;
}

void DistanceController::updateControlLoop()
{
    if (!controlling_ || plane_estimated_distance_ < 0.0f)
    {
        digitalWrite(control_pin_up_, LOW);
        digitalWrite(control_pin_down_, LOW);
        return;
    }

    if (going_down_)
    {
        digitalWrite(control_pin_down_, HIGH);
        if (plane_estimated_distance_ <= target_height_mm_)
        {
            digitalWrite(control_pin_down_, LOW);
            controlling_ = false;
        }
    }
    else
    {
        digitalWrite(control_pin_up_, HIGH);
        if (plane_estimated_distance_ >= target_height_mm_)
        {
            digitalWrite(control_pin_up_, LOW);
            controlling_ = false;
        }
    }
}
