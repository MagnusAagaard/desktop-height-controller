#include "web_server.h"

WebServer::WebServer(const char* ssid, const char* password)
{
    WiFi.begin(ssid, password);
}

void WebServer::begin()
{
    server_.begin();
    Serial.println("Web server started.");
}

WiFiClient WebServer::newClientConnection()
{
    current_line_ = "";
    header_ = "";
    return server_.available();
}

WebServer::CommandType WebServer::handleClient(WiFiClient& client, float current_height)
{
    CommandType command = CommandType::NONE;  // Move outside and initialize

    while (client.available())
    {
        char c = client.read();  // Read a byte from the client
        Serial.write(c);
        header_ += c;
        if (c == '\n')
        {  // If the byte is a newline character
            if (current_line_.length() == 0)
            {
                // HTTP response header
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println("Connection: close");  // Add this line
                client.println();

                // Handle the request
                if (header_.indexOf("GET /set?height=") >= 0)
                {
                    int startIndex = header_.indexOf("height=") + 7;
                    int endIndex = header_.indexOf(' ', startIndex);
                    if (endIndex == -1)
                        endIndex =
                            header_.indexOf('&', startIndex);  // Handle case where there might be additional parameters
                    if (endIndex == -1)
                        endIndex = header_.length();  // Handle case where it's at the end

                    String heightValue = header_.substring(startIndex, endIndex);
                    float newHeight = heightValue.toFloat();
                    if (newHeight > 0.0)
                    {
                        Serial.println("Setting height to: " + String(newHeight));
                        command = CommandType::SET_HEIGHT;
                        commanded_height_ = newHeight;
                    }
                    else
                    {
                        Serial.println("Invalid height value received: " + heightValue);
                    }
                }

                // HTML content
                client.println("<!DOCTYPE html><html>");
                client.println("<head><title>Distance Sensor Control</title></head>");
                client.println("<body><h1>Control Panel</h1>");
                client.println("<p>Current Height: " + String(current_height) + " mm</p>");

                // Set height form
                client.println("<h2>Set Height</h2>");
                client.println("<form action=\"/set\" method=\"get\">");
                client.println("<input type=\"number\" name=\"height\" placeholder=\"Enter height in mm\" required>");
                client.println("<button type=\"submit\">Set Height</button>");
                client.println("</form>");

                client.println("</body></html>");

                // Close the connection
                client.stop();
                break;  // Exit the while loop after processing the request
            }
            else
            {  // If the current line is not empty, clear it
                current_line_ = "";
            }
        }
        else if (c != '\r')
        {  // If the byte is not a carriage return, add it to the current line
            current_line_ += c;
        }
    }
    return command;
}
