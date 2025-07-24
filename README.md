# Desktop height controller
NOTE: Currently contains a bug for the WiFi server version
This project uses an ESP32 combined wth a VL53L5CX TOF sensor to compute an estimated plane from the distances measurements. This is used to control two output pins which drives the desktop either up or down. The desired distance to the ground be input in a simple local webserver running on the ESP32.

The project is structued as real-time tasks using FreeRTOS.

## Setup
Your wifi credentials has to be defined and included in `./include/wifi_credentials.h` as:
```
#define WIFI_SSID "YOUR-WIFI-SSID"
#define WIFI_PASSWORD "YOUR-WIFI-PASSWORD"
```

### Connection diagram
TODO

## Testing
TODO

## Data visualizaton
A processing script can be found in the `./scripts/` directory that visualizes the sensor output. The `./src/3dmap.cpp` should be build and uploaded in order to show this correctly.
