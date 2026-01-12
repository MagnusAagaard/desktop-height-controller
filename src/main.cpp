/**
 * @file main.cpp
 * @brief Entry point for Desktop Height Controller
 * 
 * Initializes all subsystems and runs the main loop.
 * 
 * Boot sequence:
 * 1. Serial initialization
 * 2. Logger setup
 * 3. SPIFFS mount
 * 4. NVS/SystemConfiguration init
 * 5. WiFi connection
 * 6. Sensor initialization
 * 7. Movement controller initialization
 * 8. Web server start
 * 9. Main loop (sensor sampling, state machine)
 */

// Exclude from test builds (tests provide their own setup/loop)
#ifndef UNIT_TEST

#include <Arduino.h>
#include <SPIFFS.h>

#include "Config.h"
#include "SystemConfiguration.h"
#include "WiFiManager.h"
#include "HeightController.h"
#include "MovementController.h"
#include "PresetManager.h"
#include "WebServer.h"
#include "utils/Logger.h"

// Optional: Include secrets file if it exists (WiFi credentials)
#if __has_include("secrets.h")
#include "secrets.h"
#define HAS_SECRETS 1
#else
#define HAS_SECRETS 0
#endif

// ============================================================================
// Global Objects
// ============================================================================

WiFiManager wifiManager;
HeightController heightController;
MovementController movementController(heightController);
PresetManager presetManager;
DeskWebServer webServer(heightController, movementController);

// ============================================================================
// Forward Declarations
// ============================================================================

void initSPIFFS();
void initWiFi();
void onWiFiStatusChange(WiFiState state, const String& message);
void onMovementStatusChange(MovementState state, const String& message);

// ============================================================================
// Arduino Setup
// ============================================================================

void setup() {
    // 1. Serial initialization
    Serial.begin(SERIAL_BAUD_RATE);
    delay(100);  // Give serial time to initialize
    
    Serial.println();
    Serial.println("================================");
    Serial.println("  Desktop Height Controller");
    Serial.println("================================");
    Serial.println();
    
    // 2. Logger setup
    Logger::init(LogLevel::INFO);
    Logger::info("Main", "Starting initialization...");
    
    // 3. SPIFFS mount
    initSPIFFS();
    
    // 4. SystemConfiguration init (NVS)
    if (!SystemConfig.init()) {
        Logger::error("Main", "Failed to init SystemConfiguration, using defaults");
    }
    
    // Check calibration status
    if (!SystemConfig.isCalibrated()) {
        Logger::warn("Main", "System not calibrated! Please run calibration.");
    }
    
    // 5. WiFi initialization
    initWiFi();
    
    // 6. Sensor initialization
    if (!heightController.init()) {
        Logger::error("Main", "Failed to initialize height sensor!");
    }
    
    // 7. Movement controller initialization
    movementController.init();
    movementController.setStatusCallback(onMovementStatusChange);
    
    // 8. Preset manager initialization
    if (!presetManager.init()) {
        Logger::error("Main", "Failed to initialize PresetManager");
    }
    
    // 9. Web server initialization
    webServer.setPresetManager(&presetManager);
    webServer.begin();
    Logger::info("Main", "Web server started on port 80");
    
    Logger::info("Main", "Initialization complete!");
    Serial.println();
    Serial.println("Ready.");
    Serial.println();
}

// ============================================================================
// Arduino Loop
// ============================================================================

void loop() {
    static unsigned long lastSensorUpdate = 0;
    unsigned long now = millis();
    
    // WiFi state management
    wifiManager.update();
    
    // Sensor sampling at 5Hz (200ms intervals) per PERF-002
    if (now - lastSensorUpdate >= SENSOR_SAMPLE_INTERVAL_MS) {
        lastSensorUpdate = now;
        
        // Update height reading
        heightController.update();
        
        // Update movement state machine
        movementController.update();
        
        // Push SSE height updates to connected clients
        // Always send updates so clients can see raw sensor data even if invalid/uncalibrated
        webServer.sendHeightUpdate();
    }
    
    // Web server update (handles async events)
    // Note: ESPAsyncWebServer handles requests asynchronously, minimal loop work needed
    
    // Small delay to prevent tight loop
    delay(1);
}

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * @brief Initialize SPIFFS filesystem
 * 
 * SPIFFS stores the web interface files (HTML, CSS, JS).
 * If mount fails, web interface will not be available.
 */
void initSPIFFS() {
    Logger::info("Main", "Mounting SPIFFS...");
    
    if (!SPIFFS.begin(true)) {  // true = format if mount fails
        Logger::error("Main", "SPIFFS mount failed!");
        return;
    }
    
    // List files for debugging
    Logger::info("Main", "SPIFFS mounted successfully");
    
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    
    size_t totalSize = 0;
    int fileCount = 0;
    
    while (file) {
        totalSize += file.size();
        fileCount++;
        Logger::debug("Main", "  File: %s (%d bytes)", file.name(), file.size());
        file = root.openNextFile();
    }
    
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    
    Logger::info("Main", "SPIFFS: %d files, %d/%d bytes used", 
                 fileCount, usedBytes, totalBytes);
}

/**
 * @brief Initialize WiFi connection
 * 
 * WiFi credentials are loaded from secrets.h (compile-time)
 * Falls back to AP mode if secrets.h is not configured
 */
void initWiFi() {
    Logger::info("Main", "Initializing WiFi...");
    
    wifiManager.setStatusCallback(onWiFiStatusChange);
    
#if HAS_SECRETS && defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    if (strlen(WIFI_SSID) > 0) {
        Logger::info("Main", "Connecting to: %s", WIFI_SSID);
        wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
        return;
    }
#endif
    
    Logger::warn("Main", "No WiFi configured in secrets.h, starting AP mode");
    wifiManager.beginAPMode();
}

/**
 * @brief Callback for WiFi status changes
 */
void onWiFiStatusChange(WiFiState state, const String& message) {
    Logger::info("WiFi", "%s - %s", wifiManager.getStateString(), message.c_str());
    
    if (state == WiFiState::CONNECTED || state == WiFiState::AP_MODE) {
        Logger::info("WiFi", "Access web interface at: http://%s", 
                     wifiManager.getIPAddress().toString().c_str());
    }
}

/**
 * @brief Callback for movement status changes
 */
void onMovementStatusChange(MovementState state, const String& message) {
    Logger::info("Movement", "%s - %s", movementController.getStateString(), message.c_str());
    
    // Send SSE status_change event via WebServer
    webServer.sendStatusChange(state, message);
}

#endif // UNIT_TEST
