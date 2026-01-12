/**
 * @file WiFiManager.h
 * @brief WiFi connection management with auto-reconnect and AP fallback
 * 
 * Handles:
 * - Station mode: Connect to configured WiFi network
 * - AP fallback mode: Create access point "DeskController-[ID]" per FR-020
 * - Auto-reconnection on disconnect
 * - Status reporting via callback
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"

/**
 * @enum WiFiState
 * @brief Current WiFi connection state
 */
enum class WiFiState : uint8_t {
    DISCONNECTED,    ///< Not connected, not trying
    CONNECTING,      ///< Connection attempt in progress
    CONNECTED,       ///< Connected to WiFi network
    AP_MODE,         ///< Running in Access Point mode
    ERROR            ///< Connection failed, in error state
};

/**
 * @typedef WiFiStatusCallback
 * @brief Callback for WiFi status changes
 * @param state New WiFi state
 * @param message Human-readable status message
 */
typedef void (*WiFiStatusCallback)(WiFiState state, const String& message);

/**
 * @class WiFiManager
 * @brief Manages WiFi connectivity with reconnection and AP fallback
 * 
 * Usage:
 *   WiFiManager wifi;
 *   wifi.setStatusCallback(onWiFiStatusChange);
 *   wifi.begin("MyNetwork", "password");
 *   // In loop:
 *   wifi.update();
 */
class WiFiManager {
public:
    /**
     * @brief Construct a new WiFi Manager
     */
    WiFiManager();
    
    /**
     * @brief Set callback for status changes
     * @param callback Function to call on state change
     */
    void setStatusCallback(WiFiStatusCallback callback);
    
    /**
     * @brief Begin WiFi connection with given credentials
     * 
     * If credentials are empty or connection fails after timeout,
     * falls back to AP mode per FR-020.
     * 
     * @param ssid Network SSID
     * @param password Network password (empty for open networks)
     * @return true if connection attempt started
     */
    bool begin(const String& ssid, const String& password);
    
    /**
     * @brief Begin in AP mode only (skip station connection)
     * 
     * Creates access point with SSID "DeskController-[CHIP_ID]"
     * 
     * @return true if AP started successfully
     */
    bool beginAPMode();
    
    /**
     * @brief Update WiFi state machine (call from loop)
     * 
     * Handles connection monitoring, reconnection attempts,
     * and state transitions.
     */
    void update();
    
    /**
     * @brief Disconnect from WiFi
     */
    void disconnect();
    
    /**
     * @brief Get current WiFi state
     * @return WiFiState Current state
     */
    WiFiState getState() const;
    
    /**
     * @brief Get state as human-readable string
     * @return const char* State string
     */
    const char* getStateString() const;
    
    /**
     * @brief Check if connected to a network
     * @return true if in CONNECTED state
     */
    bool isConnected() const;
    
    /**
     * @brief Check if in AP mode
     * @return true if in AP_MODE state
     */
    bool isAPMode() const;
    
    /**
     * @brief Get local IP address
     * @return IPAddress IP (0.0.0.0 if not connected)
     */
    IPAddress getIPAddress() const;
    
    /**
     * @brief Get WiFi signal strength
     * @return int8_t RSSI in dBm (-30 to -90, or 0 if not connected)
     */
    int8_t getRSSI() const;
    
    /**
     * @brief Get connected SSID
     * @return String SSID or empty if not connected
     */
    String getConnectedSSID() const;
    
    /**
     * @brief Get AP mode SSID
     * @return String AP SSID (includes chip ID)
     */
    String getAPSSID() const;
    
    /**
     * @brief Get status as JSON string (for API)
     * @return String JSON status
     */
    String toJson() const;

private:
    WiFiState state_;
    WiFiStatusCallback statusCallback_;
    
    String ssid_;
    String password_;
    String apSSID_;
    
    unsigned long connectStartTime_;
    unsigned long lastReconnectAttempt_;
    uint8_t reconnectAttempts_;
    
    static const uint8_t MAX_RECONNECT_ATTEMPTS = 3;
    
    /**
     * @brief Set new state and notify callback
     * @param newState New state
     * @param message Status message
     */
    void setState(WiFiState newState, const String& message);
    
    /**
     * @brief Generate AP SSID with chip ID
     * @return String SSID in format "DeskController-XXXX"
     */
    String generateAPSSID();
    
    /**
     * @brief Start connection attempt
     */
    void startConnection();
    
    /**
     * @brief Check connection status and handle timeout
     */
    void checkConnection();
    
    /**
     * @brief Start AP mode
     */
    void startAPMode();
    
    /**
     * @brief Handle WiFi events
     * @param event WiFi event type
     */
    static void onWiFiEvent(WiFiEvent_t event);
};

#endif // WIFI_MANAGER_H
