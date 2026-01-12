/**
 * @file WebServer.h
 * @brief Async web server with SSE support for real-time updates
 * 
 * Implements:
 * - Static file serving from SPIFFS
 * - REST API endpoints for height control
 * - Server-Sent Events for real-time updates
 * 
 * Per FR-003: Host web server on local network
 * Per FR-004: SSE for real-time height updates
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "Config.h"
#include "HeightController.h"
#include "MovementController.h"
#include "PresetManager.h"

// Forward declaration for PresetManager (for optional dependency)
// class PresetManager;

/**
 * @class DeskWebServer
 * @brief Async web server with SSE for desk height controller
 * 
 * Usage:
 *   DeskWebServer server(heightController, movementController);
 *   server.begin();
 *   // In loop:
 *   server.sendHeightUpdate();  // Push SSE events
 */
class DeskWebServer {
public:
    /**
     * @brief Construct web server
     * @param heightController Reference to height sensor
     * @param movementController Reference to movement control
     */
    DeskWebServer(HeightController& heightController, 
                  MovementController& movementController);
    
    /**
     * @brief Start the web server
     */
    void begin();
    
    /**
     * @brief Set preset manager reference (optional, for Phase 5)
     * @param presetManager Pointer to PresetManager
     */
    void setPresetManager(PresetManager* presetManager);
    
    /**
     * @brief Send height update SSE event to all connected clients
     * 
     * Per FR-004: Push height updates via SSE
     * Should be called every 200ms from main loop.
     */
    void sendHeightUpdate();
    
    /**
     * @brief Send status change SSE event
     * @param state New movement state
     * @param message Status message
     */
    void sendStatusChange(MovementState state, const String& message);
    
    /**
     * @brief Send error SSE event
     * @param errorCode Error code
     * @param message Error message
     */
    void sendError(const String& errorCode, const String& message);
    
    /**
     * @brief Send preset updated SSE event
     * @param slot Preset slot that was updated
     */
    void sendPresetUpdated(uint8_t slot);
    
    /**
     * @brief Get number of connected SSE clients
     * @return size_t Number of clients
     */
    size_t getClientCount() const;

private:
    AsyncWebServer server_;
    AsyncEventSource events_;
    
    HeightController& heightController_;
    MovementController& movementController_;
    PresetManager* presetManager_;
    
    /**
     * @brief Setup all route handlers
     */
    void setupRoutes();
    
    /**
     * @brief Setup SSE event source
     */
    void setupSSE();
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handlePostTarget(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePostStop(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleGetPresets(AsyncWebServerRequest* request);
    void handlePostPreset(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePostPresetSave(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handlePostCalibrate(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    
    /**
     * @brief Send JSON error response
     */
    void sendJsonError(AsyncWebServerRequest* request, int code, const String& message);
    
    /**
     * @brief Parse JSON body and extract field
     */
    bool parseJsonField(const String& json, const String& field, String& value);
    bool parseJsonField(const String& json, const String& field, int& value);
};

#endif // WEB_SERVER_H
