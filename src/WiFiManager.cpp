/**
 * @file WiFiManager.cpp
 * @brief Implementation of WiFi connection management
 */

#include "WiFiManager.h"
#include "utils/Logger.h"

static const char* TAG = "WiFiManager";

// Static instance pointer for event callback
static WiFiManager* wifiManagerInstance = nullptr;

WiFiManager::WiFiManager()
    : state_(WiFiState::DISCONNECTED)
    , statusCallback_(nullptr)
    , connectStartTime_(0)
    , lastReconnectAttempt_(0)
    , reconnectAttempts_(0)
    , apSSID_("")  // Will be generated in begin()
{
    wifiManagerInstance = this;
    // Note: Don't call generateAPSSID() here - ESP functions not available during static init
}

void WiFiManager::setStatusCallback(WiFiStatusCallback callback) {
    statusCallback_ = callback;
}

bool WiFiManager::begin(const String& ssid, const String& password) {
    // Generate AP SSID if not already done (deferred from constructor)
    if (apSSID_.length() == 0) {
        apSSID_ = generateAPSSID();
    }
    
    if (ssid.length() == 0) {
        Logger::warn(TAG, "No SSID provided, starting AP mode");
        return beginAPMode();
    }
    
    ssid_ = ssid;
    password_ = password;
    
    // Register event handler
    WiFi.onEvent(onWiFiEvent);
    
    // Set mode to station
    WiFi.mode(WIFI_STA);
    
    Logger::info(TAG, "Connecting to: %s", ssid.c_str());
    startConnection();
    
    return true;
}

bool WiFiManager::beginAPMode() {
    // Generate AP SSID if not already done
    if (apSSID_.length() == 0) {
        apSSID_ = generateAPSSID();
    }
    
    Logger::info(TAG, "Starting AP mode: %s", apSSID_.c_str());
    
    WiFi.mode(WIFI_AP);
    
    bool success;
    if (strlen(AP_PASSWORD) > 0) {
        success = WiFi.softAP(apSSID_.c_str(), AP_PASSWORD);
    } else {
        success = WiFi.softAP(apSSID_.c_str());
    }
    
    if (success) {
        IPAddress ip = WiFi.softAPIP();
        String message = "AP started at " + ip.toString();
        setState(WiFiState::AP_MODE, message);
        Logger::info(TAG, "AP IP: %s", ip.toString().c_str());
        return true;
    } else {
        setState(WiFiState::ERROR, "Failed to start AP");
        Logger::error(TAG, "Failed to start AP mode");
        return false;
    }
}

void WiFiManager::startConnection() {
    setState(WiFiState::CONNECTING, "Connecting to " + ssid_);
    connectStartTime_ = millis();
    
    WiFi.begin(ssid_.c_str(), password_.c_str());
}

void WiFiManager::update() {
    switch (state_) {
        case WiFiState::CONNECTING:
            checkConnection();
            break;
            
        case WiFiState::CONNECTED:
            // Check if still connected
            if (WiFi.status() != WL_CONNECTED) {
                Logger::warn(TAG, "Connection lost");
                setState(WiFiState::DISCONNECTED, "Connection lost");
                reconnectAttempts_ = 0;
            }
            break;
            
        case WiFiState::DISCONNECTED:
            // Attempt reconnection
            if (ssid_.length() > 0) {
                unsigned long now = millis();
                if (now - lastReconnectAttempt_ > WIFI_RECONNECT_DELAY_MS) {
                    if (reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
                        Logger::info(TAG, "Reconnect attempt %d/%d", 
                                     reconnectAttempts_ + 1, MAX_RECONNECT_ATTEMPTS);
                        reconnectAttempts_++;
                        lastReconnectAttempt_ = now;
                        startConnection();
                    } else {
                        // Too many attempts, fall back to AP mode
                        Logger::warn(TAG, "Max reconnect attempts reached, starting AP mode");
                        beginAPMode();
                    }
                }
            }
            break;
            
        case WiFiState::AP_MODE:
        case WiFiState::ERROR:
            // Nothing to do in these states
            break;
    }
}

void WiFiManager::checkConnection() {
    wl_status_t status = WiFi.status();
    
    if (status == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        String message = "Connected: " + ip.toString();
        setState(WiFiState::CONNECTED, message);
        Logger::info(TAG, "Connected! IP: %s, RSSI: %d dBm", 
                     ip.toString().c_str(), WiFi.RSSI());
        reconnectAttempts_ = 0;
        return;
    }
    
    // Check for timeout
    if (millis() - connectStartTime_ > WIFI_CONNECT_TIMEOUT_MS) {
        Logger::warn(TAG, "Connection timeout after %d ms", WIFI_CONNECT_TIMEOUT_MS);
        setState(WiFiState::DISCONNECTED, "Connection timeout");
        
        if (reconnectAttempts_ >= MAX_RECONNECT_ATTEMPTS) {
            Logger::warn(TAG, "Falling back to AP mode");
            beginAPMode();
        }
    }
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    setState(WiFiState::DISCONNECTED, "Disconnected");
}

WiFiState WiFiManager::getState() const {
    return state_;
}

const char* WiFiManager::getStateString() const {
    switch (state_) {
        case WiFiState::DISCONNECTED: return "Disconnected";
        case WiFiState::CONNECTING:   return "Connecting";
        case WiFiState::CONNECTED:    return "Connected";
        case WiFiState::AP_MODE:      return "AP Mode";
        case WiFiState::ERROR:        return "Error";
        default:                      return "Unknown";
    }
}

bool WiFiManager::isConnected() const {
    return state_ == WiFiState::CONNECTED;
}

bool WiFiManager::isAPMode() const {
    return state_ == WiFiState::AP_MODE;
}

IPAddress WiFiManager::getIPAddress() const {
    if (state_ == WiFiState::CONNECTED) {
        return WiFi.localIP();
    } else if (state_ == WiFiState::AP_MODE) {
        return WiFi.softAPIP();
    }
    return IPAddress(0, 0, 0, 0);
}

int8_t WiFiManager::getRSSI() const {
    if (state_ == WiFiState::CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

String WiFiManager::getConnectedSSID() const {
    if (state_ == WiFiState::CONNECTED) {
        return WiFi.SSID();
    }
    return "";
}

String WiFiManager::getAPSSID() const {
    return apSSID_;
}

String WiFiManager::generateAPSSID() {
    // Get last 4 hex digits of chip ID
    uint64_t chipId = ESP.getEfuseMac();
    uint16_t shortId = (uint16_t)(chipId >> 32);
    
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%04X", AP_SSID_PREFIX, shortId);
    
    return String(ssid);
}

void WiFiManager::setState(WiFiState newState, const String& message) {
    if (state_ != newState) {
        Logger::debug(TAG, "State: %s -> %s", getStateString(), 
                      (newState == WiFiState::DISCONNECTED) ? "Disconnected" :
                      (newState == WiFiState::CONNECTING) ? "Connecting" :
                      (newState == WiFiState::CONNECTED) ? "Connected" :
                      (newState == WiFiState::AP_MODE) ? "AP Mode" : "Error");
        state_ = newState;
        
        if (statusCallback_ != nullptr) {
            statusCallback_(newState, message);
        }
    }
}

void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    if (wifiManagerInstance == nullptr) return;
    
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Logger::debug(TAG, "Event: Got IP");
            break;
            
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Logger::debug(TAG, "Event: Disconnected");
            if (wifiManagerInstance->state_ == WiFiState::CONNECTED) {
                wifiManagerInstance->setState(WiFiState::DISCONNECTED, "Disconnected");
            }
            break;
            
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Logger::debug(TAG, "Event: Connected to AP");
            break;
            
        default:
            break;
    }
}

String WiFiManager::toJson() const {
    String json = "{";
    json += "\"state\":\"" + String(getStateString()) + "\",";
    json += "\"ip\":\"" + getIPAddress().toString() + "\",";
    
    if (state_ == WiFiState::CONNECTED) {
        json += "\"ssid\":\"" + getConnectedSSID() + "\",";
        json += "\"rssi\":" + String(getRSSI());
    } else if (state_ == WiFiState::AP_MODE) {
        json += "\"ssid\":\"" + apSSID_ + "\",";
        json += "\"rssi\":0";
    } else {
        json += "\"ssid\":\"\",";
        json += "\"rssi\":0";
    }
    
    json += "}";
    return json;
}
