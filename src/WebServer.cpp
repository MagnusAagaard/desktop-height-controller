/**
 * @file WebServer.cpp
 * @brief Implementation of async web server with SSE
 */

#include "WebServer.h"
#include "SystemConfiguration.h"
#include "utils/Logger.h"
#include <SPIFFS.h>

static const char* TAG = "WebServer";

DeskWebServer::DeskWebServer(HeightController& heightController, 
                             MovementController& movementController)
    : server_(WEB_SERVER_PORT)
    , events_("/events")
    , heightController_(heightController)
    , movementController_(movementController)
    , presetManager_(nullptr)
{
}

void DeskWebServer::begin() {
    Logger::info(TAG, "Starting web server on port %d", WEB_SERVER_PORT);
    
    setupSSE();
    setupRoutes();
    
    server_.begin();
    Logger::info(TAG, "Web server started");
}

void DeskWebServer::setPresetManager(PresetManager* presetManager) {
    presetManager_ = presetManager;
}

void DeskWebServer::setupSSE() {
    // Configure SSE event source
    events_.onConnect([](AsyncEventSourceClient* client) {
        if (client->lastId()) {
            Logger::debug(TAG, "SSE client reconnected, last ID: %u", client->lastId());
        } else {
            Logger::info(TAG, "SSE client connected");
        }
        // Send initial connection event
        client->send("connected", "connection", millis());
    });
    
    server_.addHandler(&events_);
    Logger::debug(TAG, "SSE handler registered at /events");
}

void DeskWebServer::setupRoutes() {
    // Serve static files from SPIFFS
    server_.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    
    // API Routes
    
    // GET /status - Current system status
    server_.on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });
    
    // POST /target - Set target height
    server_.on("/target", HTTP_POST, 
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostTarget(request, data, len);
        }
    );
    
    // POST /stop - Emergency stop
    server_.on("/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handlePostStop(request);
    });
    
    // GET /config - Get system configuration
    server_.on("/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetConfig(request);
    });
    
    // POST /config - Update system configuration
    server_.on("/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostConfig(request, data, len);
        }
    );
    
    // GET /presets - Get all presets
    server_.on("/presets", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetPresets(request);
    });
    
    // POST /preset/save - Save a preset (register before /preset to avoid prefix matching)
    server_.on("/preset/save", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostPresetSave(request, data, len);
        }
    );
    
    // POST /preset - Activate a preset
    server_.on("/preset", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostPreset(request, data, len);
        }
    );
    
    // POST /calibrate - Run calibration
    server_.on("/calibrate", HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            handlePostCalibrate(request, data, len);
        }
    );
    
    // 404 handler
    server_.onNotFound([this](AsyncWebServerRequest* request) {
        sendJsonError(request, 404, "Not found");
    });
    
    Logger::debug(TAG, "Routes configured");
}

void DeskWebServer::sendHeightUpdate() {
    if (events_.count() == 0) return;
    
    const HeightReading& reading = heightController_.getReading();
    const TargetHeight& target = movementController_.getTarget();
    
    String json = "{";
    json += "\"height\":" + String(reading.calculated_height_cm) + ",";
    json += "\"rawDistance\":" + String(reading.raw_distance_mm) + ",";
    json += "\"filteredDistance\":" + String(reading.filtered_distance_mm) + ",";
    json += "\"valid\":" + String(reading.validity == ReadingValidity::VALID ? "true" : "false") + ",";
    json += "\"timestamp\":" + String(reading.timestamp_ms) + ",";
    // Include target height
    json += "\"targetHeight\":" + String(target.active ? target.target_height_cm : 0) + ",";
    json += "\"targetActive\":" + String(target.active ? "true" : "false") + ",";
    // Include system diagnostics for live updates
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"sseClients\":" + String(events_.count());
    json += "}";
    
    events_.send(json.c_str(), "height_update", millis());
}

void DeskWebServer::sendStatusChange(MovementState state, const String& message) {
    if (events_.count() == 0) return;
    
    const char* stateStr;
    switch (state) {
        case MovementState::IDLE:        stateStr = "idle"; break;
        case MovementState::MOVING_UP:   stateStr = "moving_up"; break;
        case MovementState::MOVING_DOWN: stateStr = "moving_down"; break;
        case MovementState::STABILIZING: stateStr = "stabilizing"; break;
        case MovementState::ERROR:       stateStr = "error"; break;
        default:                         stateStr = "unknown"; break;
    }
    
    String json = "{";
    json += "\"state\":\"" + String(stateStr) + "\",";
    json += "\"message\":\"" + message + "\",";
    json += "\"timestamp\":" + String(millis());
    json += "}";
    
    events_.send(json.c_str(), "status_change", millis());
}

void DeskWebServer::sendError(const String& errorCode, const String& message) {
    if (events_.count() == 0) return;
    
    String json = "{";
    json += "\"code\":\"" + errorCode + "\",";
    json += "\"message\":\"" + message + "\",";
    json += "\"timestamp\":" + String(millis());
    json += "}";
    
    events_.send(json.c_str(), "error", millis());
}

void DeskWebServer::sendPresetUpdated(uint8_t slot) {
    if (events_.count() == 0) return;
    
    String json = "{";
    json += "\"slot\":" + String(slot) + ",";
    json += "\"timestamp\":" + String(millis());
    json += "}";
    
    events_.send(json.c_str(), "preset_updated", millis());
}

size_t DeskWebServer::getClientCount() const {
    return events_.count();
}

// Route Handlers

void DeskWebServer::handleGetStatus(AsyncWebServerRequest* request) {
    String json = "{";
    json += "\"height\":" + heightController_.toJson() + ",";
    json += "\"movement\":" + movementController_.toJson() + ",";
    json += "\"config\":" + SystemConfig.toJson() + ",";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"sseClients\":" + String(events_.count());
    json += "}";
    
    request->send(200, "application/json", json);
}

void DeskWebServer::handlePostTarget(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    String body = String((char*)data).substring(0, len);
    Logger::debug(TAG, "POST /target: %s", body.c_str());
    
    int targetHeight;
    if (!parseJsonField(body, "height", targetHeight)) {
        sendJsonError(request, 400, "Missing 'height' field");
        return;
    }
    
    // Validate height range
    if (!SystemConfig.isValidHeight(targetHeight)) {
        String msg = "Height must be between " + String(SystemConfig.getMinHeight()) + 
                     " and " + String(SystemConfig.getMaxHeight()) + " cm";
        sendJsonError(request, 400, msg);
        return;
    }
    
    // Check if calibrated
    if (!SystemConfig.isCalibrated()) {
        sendJsonError(request, 400, "System not calibrated. Please calibrate first.");
        return;
    }
    
    // Set target
    if (!movementController_.setTargetHeight(targetHeight)) {
        sendJsonError(request, 500, "Failed to set target height");
        return;
    }
    
    String json = "{\"success\":true,\"target\":" + String(targetHeight) + "}";
    request->send(200, "application/json", json);
}

void DeskWebServer::handlePostStop(AsyncWebServerRequest* request) {
    Logger::info(TAG, "Emergency stop requested via web");
    
    movementController_.emergencyStop();
    
    String json = "{\"success\":true,\"message\":\"Emergency stop activated\"}";
    request->send(200, "application/json", json);
}

void DeskWebServer::handleGetConfig(AsyncWebServerRequest* request) {
    request->send(200, "application/json", SystemConfig.toJson());
}

void DeskWebServer::handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    String body = String((char*)data).substring(0, len);
    Logger::debug(TAG, "POST /config: %s", body.c_str());
    
    int value;
    bool updated = false;
    
    if (parseJsonField(body, "minHeight", value)) {
        if (SystemConfig.setMinHeight(value)) updated = true;
    }
    if (parseJsonField(body, "maxHeight", value)) {
        if (SystemConfig.setMaxHeight(value)) updated = true;
    }
    if (parseJsonField(body, "tolerance", value)) {
        if (SystemConfig.setTolerance(value)) updated = true;
    }
    if (parseJsonField(body, "stabilizationDuration", value)) {
        if (SystemConfig.setStabilizationDuration(value)) updated = true;
    }
    if (parseJsonField(body, "movementTimeout", value)) {
        if (SystemConfig.setMovementTimeout(value)) updated = true;
    }
    
    if (updated) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        sendJsonError(request, 400, "No valid configuration fields provided");
    }
}

void DeskWebServer::handleGetPresets(AsyncWebServerRequest* request) {
    String json = "[";
    
    if (presetManager_ != nullptr) {
        const Preset* presets = presetManager_->getAllPresets();
        for (int i = 0; i < MAX_PRESETS; i++) {
            if (i > 0) json += ",";
            json += "{\"slot\":" + String(presets[i].slot);
            json += ",\"name\":\"" + String(presets[i].name) + "\"";
            json += ",\"height_cm\":" + String(presets[i].height_cm, 1);
            json += ",\"enabled\":" + String(presets[i].isEnabled() ? "true" : "false");
            json += "}";
        }
    } else {
        // Return empty presets if PresetManager not set
        for (int i = 0; i < NUM_PRESET_SLOTS; i++) {
            if (i > 0) json += ",";
            json += "{\"slot\":" + String(i + 1) + ",\"name\":\"\",\"height_cm\":0,\"enabled\":false}";
        }
    }
    
    json += "]";
    request->send(200, "application/json", json);
}

void DeskWebServer::handlePostPreset(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (presetManager_ == nullptr) {
        sendJsonError(request, 500, "PresetManager not initialized");
        return;
    }
    
    String body = String((char*)data).substring(0, len);
    Logger::debug(TAG, "POST /preset: %s", body.c_str());
    
    int slot;
    if (!parseJsonField(body, "slot", slot)) {
        sendJsonError(request, 400, "Missing 'slot' field");
        return;
    }
    
    if (!PresetManager::isValidSlot(slot)) {
        sendJsonError(request, 400, "Invalid slot (must be 1-5)");
        return;
    }
    
    const Preset* preset = presetManager_->getPreset(slot);
    if (preset == nullptr || !preset->isEnabled()) {
        sendJsonError(request, 400, "Preset slot " + String(slot) + " is not configured");
        return;
    }
    
    // Check if calibrated
    if (!SystemConfig.isCalibrated()) {
        sendJsonError(request, 400, "System not calibrated. Please calibrate first.");
        return;
    }
    
    // Set target to preset height
    if (!movementController_.setTargetHeight(preset->height_cm)) {
        sendJsonError(request, 500, "Failed to activate preset");
        return;
    }
    
    Logger::info(TAG, "Activated preset %d: '%s' -> %.1f cm", 
                 slot, preset->name, preset->height_cm);
    
    String json = "{\"success\":true,\"slot\":" + String(slot) + 
                  ",\"target\":" + String(preset->height_cm, 1) + "}";
    request->send(200, "application/json", json);
}

void DeskWebServer::handlePostPresetSave(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (presetManager_ == nullptr) {
        sendJsonError(request, 500, "PresetManager not initialized");
        return;
    }
    
    String body = String((char*)data).substring(0, len);
    Logger::debug(TAG, "POST /preset/save: %s", body.c_str());
    
    int slot;
    if (!parseJsonField(body, "slot", slot)) {
        sendJsonError(request, 400, "Missing 'slot' field");
        return;
    }
    
    String name;
    parseJsonField(body, "name", name);  // Optional
    
    int height;
    if (!parseJsonField(body, "height", height)) {
        sendJsonError(request, 400, "Missing 'height' field");
        return;
    }
    
    if (!presetManager_->savePreset(slot, name.c_str(), (float)height)) {
        sendJsonError(request, 400, "Failed to save preset (invalid slot or height)");
        return;
    }
    
    // Notify connected clients
    sendPresetUpdated(slot);
    
    String json = "{\"success\":true,\"slot\":" + String(slot) + "}";
    request->send(200, "application/json", json);
}

void DeskWebServer::handlePostCalibrate(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    String body = String((char*)data).substring(0, len);
    Logger::debug(TAG, "POST /calibrate: %s", body.c_str());
    
    int knownHeight;
    if (!parseJsonField(body, "height", knownHeight)) {
        sendJsonError(request, 400, "Missing 'height' field");
        return;
    }
    
    if (knownHeight < 30 || knownHeight > 200) {
        sendJsonError(request, 400, "Known height must be between 30 and 200 cm");
        return;
    }
    
    if (!heightController_.calibrate(knownHeight)) {
        sendJsonError(request, 500, "Calibration failed - check sensor");
        return;
    }
    
    String json = "{\"success\":true,\"calibrationConstant\":" + 
                  String(SystemConfig.getCalibrationConstant()) + "}";
    request->send(200, "application/json", json);
}

void DeskWebServer::sendJsonError(AsyncWebServerRequest* request, int code, const String& message) {
    String json = "{\"error\":true,\"message\":\"" + message + "\"}";
    request->send(code, "application/json", json);
}

bool DeskWebServer::parseJsonField(const String& json, const String& field, String& value) {
    // Simple JSON parser - find "field":"value" or "field":value
    String searchKey = "\"" + field + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return false;
    
    int valueStart = keyPos + searchKey.length();
    
    // Check if string value (starts with ")
    if (json.charAt(valueStart) == '"') {
        valueStart++;
        int valueEnd = json.indexOf('"', valueStart);
        if (valueEnd < 0) return false;
        value = json.substring(valueStart, valueEnd);
    } else {
        // Numeric or boolean - find end
        int valueEnd = valueStart;
        while (valueEnd < json.length() && 
               json.charAt(valueEnd) != ',' && 
               json.charAt(valueEnd) != '}') {
            valueEnd++;
        }
        value = json.substring(valueStart, valueEnd);
        value.trim();
    }
    
    return value.length() > 0;
}

bool DeskWebServer::parseJsonField(const String& json, const String& field, int& value) {
    String strValue;
    if (!parseJsonField(json, field, strValue)) return false;
    value = strValue.toInt();
    return true;
}
