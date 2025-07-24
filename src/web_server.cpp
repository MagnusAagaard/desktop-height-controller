#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>

#include "wifi_credentials.h"

// Replace with your network credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 10000;

// Add variables to store the current height and preset heights
int currentHeight = 0; // Example height value
std::vector<int> presetHeights = {100, 200, 300}; // Example preset heights

void setup() {
  Serial.begin(115200);
  // Initialize the output variables as outputs
  pinMode(LED_BUILTIN, OUTPUT);
  // Set outputs to LOW
  digitalWrite(LED_BUILTIN, LOW);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Handle adding a new preset height
            if (header.indexOf("GET /add?height=") >= 0) {
              int startIndex = header.indexOf("/add?height=") + 12; // Adjusted to match the query parameter
              int endIndex = header.indexOf('&', startIndex); // Find the end of the value or use space as fallback
              if (endIndex == -1) {
                endIndex = header.indexOf(' ', startIndex);
              }
              if (endIndex > startIndex) {
                String heightValue = header.substring(startIndex, endIndex);
                int newHeight = heightValue.toInt();
                if (newHeight > 0) { // Ensure valid height
                  presetHeights.push_back(newHeight);
                  Serial.println("Added new preset height: " + String(newHeight));
                } else {
                  Serial.println("Invalid height value received: " + heightValue);
                }
              }
            }

            // Handle removing a preset height
            if (header.indexOf("GET /remove/") >= 0) {
              int removeHeight = header.substring(header.indexOf("/remove/") + 8).toInt();
              presetHeights.erase(std::remove(presetHeights.begin(), presetHeights.end(), removeHeight), presetHeights.end());
              Serial.println("Removed preset height: " + String(removeHeight));
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; text-align: center;}</style></head>");
            client.println("<body><h1>Height Controller</h1>");

            // Display current height
            client.println("<p>Current Height: " + String(currentHeight) + " mm</p>");

            // Display preset heights
            client.println("<h2>Preset Heights</h2>");
            for (int height : presetHeights) {
              client.println("<p>" + String(height) + " mm <a href=\"/remove/" + String(height) + "\">Remove</a></p>");
            }

            // Add new preset height
            client.println("<h2>Add New Preset Height</h2>");
            client.println("<form action=\"/add\" method=\"get\">");
            client.println("<input type=\"number\" name=\"height\" placeholder=\"Enter height in mm\" required>");
            client.println("<button type=\"submit\">Add</button>");
            client.println("</form>");

            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}