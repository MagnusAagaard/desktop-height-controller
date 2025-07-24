#include "distance_array_sensor.h"
#include <Arduino.h>
#include <Wire.h>

bool DistanceArraySensor::begin()
{
    bool success = true;
    success &= Wire.begin();
    success &= Wire.setClock(400000);
    if (!my_imager_.begin())
    {
        Serial.println(F("Sensor not found - check your wiring."));
        return false;
    }
    success &= my_imager_.setResolution(8 * 8);
    image_resolution_ = my_imager_.getResolution();
    image_width_ = sqrt(image_resolution_);
    success &= my_imager_.setRangingFrequency(15);  // At 8x8 resolution, max frequency is 15Hz
    success &= my_imager_.startRanging();
    initialized_ = success;

    return success;
}

void DistanceArraySensor::update()
{
    if (my_imager_.isDataReady())
    {
        if (my_imager_.getRangingData(&measurement_data_))
        {
            data_ready_ = true;
        }
    }
}

const int16_t* DistanceArraySensor::getDistances()
{
    if (!data_ready_)
    {
        return nullptr;
    }
    data_ready_ = false;
    return measurement_data_.distance_mm;
}