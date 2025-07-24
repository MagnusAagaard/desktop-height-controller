#ifndef DISTANCE_ARRAY_SENSOR_H
#define DISTANCE_ARRAY_SENSOR_H

#include <SparkFun_VL53L5CX_Library.h>

class DistanceArraySensor
{
   public:

    bool begin();
    void update();
    const int16_t* getDistances();
    int getResolution() const { return image_resolution_; };
    int getWidth() const { return image_width_; };
    bool isDataReady() const { return data_ready_; }
    bool initialized() const { return initialized_; }

   private:

    SparkFun_VL53L5CX my_imager_;
    VL53L5CX_ResultsData measurement_data_;
    bool initialized_{false};
    uint8_t image_resolution_{0};
    uint8_t image_width_{0};
    bool data_ready_{false};
};

#endif  // DISTANCE_ARRAY_SENSOR_H
