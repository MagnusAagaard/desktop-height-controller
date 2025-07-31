#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "distance_array_sensor.h"
#include "distance_controller.h"
#include "plane_estimator.h"
#include "web_server.h"
#include "wifi_credentials.h"

#define CONTROL_PIN_UP GPIO_NUM_13    // Define GPIO pin for control up
#define CONTROL_PIN_DOWN GPIO_NUM_12  // Define GPIO pin for control down

static const bool debug = false;  // Set to true for debugging output
static const bool use_serial = false;

static TimerHandle_t poll_distances_timer = NULL;
static QueueHandle_t wifi_event_queue;
static SemaphoreHandle_t control_semaphore = NULL;

static DistanceArraySensor distance_sensor;
static PlaneEstimator plane_estimator(8, 8);
static DistanceController distance_controller(CONTROL_PIN_UP, CONTROL_PIN_DOWN);

static const int CMD_BUF_LEN = 255;
static const int USER_INTERFACE_LOOP_DELAY = 20;  // Delay for user interface loop in milliseconds


char XML[2048];
char buf[32];
static WebServer server(80);

void printDebug(const String& message)
{
    if (debug)
    {
        Serial.println(message);
    }
}

void pollDistancesCallback(TimerHandle_t xTimer)
{
    distance_sensor.update();
    if (distance_sensor.isDataReady())
    {
        const int16_t* distances = distance_sensor.getDistances();
        plane_estimator.setDistances(distances, distance_sensor.getResolution());
        plane_estimator.estimatePlane();
        const float estimated_distance = plane_estimator.estimatedDistanceToPlane();
        if (estimated_distance > 0)
        {
            distance_controller.updateDistanceToPlane(estimated_distance);
            xSemaphoreGive(control_semaphore);
        }
        printDebug("Estimated distance to plane: " + String(estimated_distance));
    }
    else
    {
        printDebug("No new data available.");
    }
}

void controlTask(void* pvParameters)
{
    while (true)
    {
        // Wait for the semaphore to be given by the timer callback
        if (xSemaphoreTake(control_semaphore, portMAX_DELAY) == pdTRUE)
        {
            distance_controller.updateControlLoop();
            // if (distance_controller.isControlling())
            // {
            //     digitalWrite(LED_BUILTIN, HIGH);
            // }
            // else
            // {
            //     digitalWrite(LED_BUILTIN, LOW);
            // }
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // Control loop delay
    }
}

void serialControlTask(void* pvParameters)
{
    char c;
    char cmd_buf[CMD_BUF_LEN];
    uint8_t idx = 0;

    // Clear whole buffer
    memset(cmd_buf, 0, CMD_BUF_LEN);
    while (true)
    {
        if (Serial.available())
        {
            c = Serial.read();

            // Store received character to buffer if not over buffer limit
            if (idx < CMD_BUF_LEN - 1)
            {
                cmd_buf[idx] = c;
                idx++;
            }

            // Print newline and check input on 'enter'
            if ((c == '\n') || (c == '\r'))
            {
                // Print average value if command given is "avg"
                cmd_buf[idx - 1] = '\0';
                if (strcmp(cmd_buf, "dist") == 0)
                {
                    Serial.print("\r\n");
                    Serial.print("Estimated distance to plane: ");
                    Serial.println(distance_controller.getEstimatedDistanceToPlane());
                }
                else if (strncmp(cmd_buf, "set ", 4) == 0)
                {
                    Serial.print("\r\n");
                    // Extract height value from command
                    const float height_mm = atof(cmd_buf + 4);
                    distance_controller.setTargetHeight(height_mm);
                }

                // Reset receive buffer and index counter
                memset(cmd_buf, 0, CMD_BUF_LEN);
                idx = 0;

                // Otherwise, echo character back to serial terminal
            }
            else
            {
                Serial.print(c);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(USER_INTERFACE_LOOP_DELAY));
    }
}

// void webServerTask(void* pvParameters)
// {
//     WiFiClient client;
//     while (true)
//     {
//         if (client)
//         {
//             Serial.println("Client connected.");
//             while (client.connected())
//             {
//                 WebServer::CommandType command =
//                     web_server.handleClient(client, distance_controller.getEstimatedDistanceToPlane());
//                 if (command == WebServer::CommandType::SET_HEIGHT)
//                 {
//                     Serial.println("Setting new height from web server.");
//                     float new_height = web_server.getNewHeight();
//                     if (new_height > 0.0)
//                     {
//                         distance_controller.setTargetHeight(200);
//                         Serial.print("New height set: ");
//                         Serial.println(new_height);
//                     }
//                 }
//                 vTaskDelay(pdMS_TO_TICKS(USER_INTERFACE_LOOP_DELAY));
//             }
//             Serial.println("Client disconnected.");
//         }
//         else
//         {
//             client = web_server.newClientConnection();
//             if (client)
//             {
//                 Serial.println("New client connection.");
//             }
//         }
//         vTaskDelay(pdMS_TO_TICKS(USER_INTERFACE_LOOP_DELAY));  // Handle client every 100 ms
//     }
// }

void connectToWifi()
{
    const TickType_t xTicksToWait = pdMS_TO_TICKS(4000);

    Serial.println("GFV - Connecting to WiFi");
    Serial.printf("GFV - SSID: %s\n", WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        // web_server = WebServer(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("GFV - Waiting on wifi connection");
        vTaskDelay(xTicksToWait);
    }
    // web_server.begin();
}

void onWifiEvent(WiFiEvent_t event)
{
    xQueueSendToBackFromISR(wifi_event_queue, &event, 0);
}

void handleWifiEvent(void* pvParameters)
{
    WiFiEvent_t event;
    IPAddress ip;

    for (;;)
    {
        xQueueReceive(wifi_event_queue, &event, portMAX_DELAY);
        switch (event)
        {
            case SYSTEM_EVENT_STA_DISCONNECTED:
                Serial.println("GFV - WiFi Disconnected");
                connectToWifi();
                break;

            case SYSTEM_EVENT_STA_CONNECTED:
                Serial.println("GFV - WiFi Connected");
                break;

            case SYSTEM_EVENT_STA_GOT_IP:
                ip = WiFi.localIP();
                Serial.printf("GFV - Got IP Address %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
                break;

            default:
                Serial.printf("GFV - WiFi Event: %d\n", (uint16_t)event);
                break;
        }
    }
}

// TODO: Move these XML handlers to web_server class
// TODO: Change buttons to control the desktop height
void ButtonPress0Start() {
    digitalWrite(BUILTIN_LED, HIGH);
    Serial.println("Button 0 pressed");
}

void ButtonPress0Stop() {
    digitalWrite(BUILTIN_LED, LOW);
    Serial.println("Button 0 released");
}

void ButtonPress1Start() {
    digitalWrite(BUILTIN_LED, HIGH);
    Serial.println("Button 1 pressed");
}

void ButtonPress1Stop() {
    digitalWrite(BUILTIN_LED, LOW);
    Serial.println("Button 1 released");
}

void SendWebsite() {
  Serial.println("sending web page");
  server.send(200, "text/html", PAGE_MAIN);
}

void SendXML() {
  strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");

  strcat(XML, "<CORE0_STATUS>");
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && uxTaskGetNumberOfTasks() > 0) {
    strcat(XML, "1");
  } else {
    strcat(XML, "0");
  }
  strcat(XML, "</CORE0_STATUS>\n");

  strcat(XML, "<CORE1_STATUS>");
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING && uxTaskGetNumberOfTasks() > 1) {
    strcat(XML, "1");
  } else {
    strcat(XML, "0");
  }
  strcat(XML, "</CORE1_STATUS>\n");
  // Append height value in <HEIGHT> tag for web UI compatibility
  strcat(XML, "<HEIGHT>");
  sprintf(buf, "%.2f", distance_controller.getEstimatedDistanceToPlane());
  strcat(XML, buf);
  strcat(XML, "</HEIGHT>\n");

  strcat(XML, "</Data>\n");
//   Serial.println(XML);
  server.send(200, "text/xml", XML);
}

void ServerTask(void *pvParameters) {
  while (true) {
    // Update the server task core indicators
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    printDebug("Starting desktop height controller...");
    control_semaphore = xSemaphoreCreateBinary();
    if (control_semaphore == NULL)
    {
        printDebug("Failed to create control semaphore.");
        exit(1);
    }
    if (!use_serial)
    {
        wifi_event_queue = xQueueCreate(5, sizeof(WiFiEvent_t));
        xTaskCreatePinnedToCore(handleWifiEvent, "WiFi Event Handler", 5 * 1024, NULL, 2, NULL, 0);
        WiFi.onEvent(onWifiEvent);
        connectToWifi();
    }
    if (!distance_sensor.begin())
    {
        printDebug("Failed to initialize distance sensor.");
        exit(1);
    }

    poll_distances_timer = xTimerCreate("Poll Distances", pdMS_TO_TICKS(100), pdTRUE, NULL, pollDistancesCallback);
    xTimerStart(poll_distances_timer, 0);
    printDebug("Started timer for polling distances");

    if (use_serial)
    {
        xTaskCreate(serialControlTask, "Serial Control Task", 2048, NULL, 2, NULL);
        printDebug("Started serial control task");
    }
    else
    {
        server.on("/", SendWebsite);
        server.on("/xml", SendXML);
        server.on("/BUTTON_0_START", ButtonPress0Start);
        server.on("/BUTTON_0_STOP", ButtonPress0Stop);
        server.on("/BUTTON_1_START", ButtonPress1Start);
        server.on("/BUTTON_1_STOP", ButtonPress1Stop);
        server.begin();
        // xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 5 * 1024, NULL, 2, NULL, 0);
        xTaskCreatePinnedToCore(ServerTask, "serverTask", 4096, NULL, 3, NULL, 1);
        printDebug("Started web server task");
    }

    xTaskCreatePinnedToCore(controlTask, "Control Task", 2048, NULL, 3, NULL, 0);

    // Delete setup and loop tasks
    vTaskDelete(NULL);
}

void loop()
{
    // Execution will not reach here as the setup function deletes the task
}