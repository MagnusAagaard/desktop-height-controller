#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

class WebServer
{
   public:

    enum class CommandType
    {
        NONE,
        SET_HEIGHT,
        GET_DISTANCE
    };
    WebServer() = default;
    WebServer(const char* ssid, const char* password);
    void begin();
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    WiFiClient newClientConnection();
    CommandType handleClient(WiFiClient& client, float current_height);
    float getNewHeight() const { return commanded_height_; }

   private:

    WiFiServer server_{80};
    String current_line_;
    String header_;
    float commanded_height_{-1.0f};
};

#endif  // WEB_SERVER_H