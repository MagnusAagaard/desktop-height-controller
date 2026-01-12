/**
 * @file Config.h
 * @brief Hardware pin definitions, constants, and default configuration values
 * 
 * Web-Based Desk Height Controller
 * ESP32 with VL53L5CX ToF Sensor
 * 
 * Modify the WiFi credentials and pin assignments for your specific hardware setup.
 * See quickstart.md for wiring diagrams and calibration instructions.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// Hardware Pin Definitions
// =============================================================================

/**
 * I2C pins for VL53L5CX ToF sensor
 * Default ESP32 I2C pins: SDA=21, SCL=22
 */
constexpr uint8_t PIN_I2C_SDA = 21;
constexpr uint8_t PIN_I2C_SCL = 22;

/**
 * MOSFET control pins for desk motor
 * These pins control N-channel MOSFETs that switch the motor power
 * HIGH = motor active, LOW = motor stopped
 * WARNING: Never set both pins HIGH simultaneously!
 */
constexpr uint8_t PIN_MOTOR_UP = 25;
constexpr uint8_t PIN_MOTOR_DOWN = 26;

// =============================================================================
// VL53L5CX Sensor Configuration
// =============================================================================

/**
 * VL53L5CX I2C address (default 0x29)
 */
constexpr uint8_t VL53L5CX_I2C_ADDRESS = 0x29;

/**
 * I2C clock frequency (400kHz max for VL53L5CX)
 */
constexpr uint32_t I2C_FREQUENCY = 400000;

/**
 * Sensor sampling interval in milliseconds
 * 200ms = 5 Hz sampling rate per PERF-002
 */
constexpr uint16_t SENSOR_SAMPLE_INTERVAL_MS = 200;

// =============================================================================
// Height Calculation Defaults
// =============================================================================

/**
 * Default calibration constant in centimeters
 * Formula: height_cm = calibration_constant_cm - (sensor_reading_mm / 10)
 * Value 0 indicates uncalibrated system - calibration required before use
 */
constexpr uint16_t DEFAULT_CALIBRATION_CONSTANT_CM = 0;

/**
 * Minimum allowed target height in centimeters (per FR-014)
 */
constexpr uint16_t DEFAULT_MIN_HEIGHT_CM = 50;

/**
 * Maximum allowed target height in centimeters (per FR-014)
 */
constexpr uint16_t DEFAULT_MAX_HEIGHT_CM = 125;

/**
 * Target height tolerance in millimeters
 * Movement stops when within ±tolerance of target
 */
constexpr uint16_t DEFAULT_TOLERANCE_MM = 10;

// =============================================================================
// Movement Control Defaults
// =============================================================================

/**
 * Stabilization duration in milliseconds
 * Time desk must remain within tolerance before confirming target reached
 * Prevents oscillation from momentum/mechanical delay
 */
constexpr uint16_t DEFAULT_STABILIZATION_DURATION_MS = 2000;

/**
 * Movement timeout in milliseconds
 * Safety cutoff - movement stops if target not reached within this time
 * Prevents runaway motor conditions
 */
constexpr uint16_t DEFAULT_MOVEMENT_TIMEOUT_MS = 30000;

// =============================================================================
// Sensor Filtering Defaults
// =============================================================================

/**
 * Moving average filter window size
 * Number of samples to average for noise reduction
 * Larger = smoother but slower response
 */
constexpr uint8_t DEFAULT_FILTER_WINDOW_SIZE = 5;

/**
 * Maximum allowed filter window size
 */
constexpr uint8_t MAX_FILTER_WINDOW_SIZE = 10;

/**
 * Minimum allowed filter window size
 */
constexpr uint8_t MIN_FILTER_WINDOW_SIZE = 3;

// =============================================================================
// WiFi Configuration
// =============================================================================

/**
 * Default WiFi credentials - CHANGE THESE for your network
 * These are loaded from NVS if previously saved, otherwise use these defaults
 */
constexpr const char* DEFAULT_WIFI_SSID = "";
constexpr const char* DEFAULT_WIFI_PASSWORD = "";

/**
 * WiFi connection timeout in milliseconds
 */
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

/**
 * WiFi reconnection delay in milliseconds
 */
constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 5000;

/**
 * WiFi Access Point mode SSID prefix (per FR-020)
 * Full SSID format: "DeskController-[CHIP_ID]"
 */
constexpr const char* AP_SSID_PREFIX = "DeskController-";

/**
 * WiFi Access Point password (empty for open network, or set a password)
 */
constexpr const char* AP_PASSWORD = "desk1337";

// =============================================================================
// Web Server Configuration
// =============================================================================

/**
 * HTTP server port
 */
constexpr uint16_t WEB_SERVER_PORT = 80;

/**
 * Maximum concurrent web connections (per PERF-005)
 */
constexpr uint8_t MAX_WEB_CONNECTIONS = 5;

/**
 * SSE keepalive interval in milliseconds
 */
constexpr uint32_t SSE_KEEPALIVE_INTERVAL_MS = 30000;

// =============================================================================
// Preset Configuration
// =============================================================================

/**
 * Number of preset slots available (per FR-010)
 */
constexpr uint8_t NUM_PRESET_SLOTS = 5;

/**
 * Maximum length of preset label string
 */
constexpr uint8_t MAX_PRESET_LABEL_LENGTH = 20;

// =============================================================================
// NVS Storage Namespaces
// =============================================================================

/**
 * NVS namespace for system configuration
 */
constexpr const char* NVS_NAMESPACE_CONFIG = "config";

/**
 * NVS namespace for preset storage
 */
constexpr const char* NVS_NAMESPACE_PRESETS = "presets";

// =============================================================================
// Sensor Value Limits
// =============================================================================

/**
 * Maximum valid sensor reading in millimeters
 * VL53L5CX can read up to 4000mm
 */
constexpr uint16_t SENSOR_MAX_RANGE_MM = 4000;

/**
 * Minimum valid sensor reading in millimeters
 * Readings at or below this indicate sensor error
 */
constexpr uint16_t SENSOR_MIN_VALID_MM = 10;

/**
 * Reading stale timeout in milliseconds
 * If no new reading within this time, consider data stale
 */
constexpr uint16_t READING_STALE_TIMEOUT_MS = 1000;

// =============================================================================
// Debug and Logging Configuration
// =============================================================================

/**
 * Serial baud rate for debug output
 */
constexpr uint32_t SERIAL_BAUD_RATE = 115200;

/**
 * Enable verbose debug logging (comment out for production)
 */
#define DEBUG_LOGGING_ENABLED

#ifdef DEBUG_LOGGING_ENABLED
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

// =============================================================================
// Safety Configuration
// =============================================================================

/**
 * Minimum height difference required for confirmation dialog (per UX-009)
 * Changes larger than this require user confirmation
 */
constexpr uint16_t LARGE_CHANGE_THRESHOLD_CM = 30;

/**
 * Emergency stop debounce time in milliseconds
 */
constexpr uint16_t EMERGENCY_STOP_DEBOUNCE_MS = 100;

#endif // CONFIG_H
