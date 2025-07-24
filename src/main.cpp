#include <Arduino.h>
#include "distance_array_sensor.h"
#include "distance_controller.h"
#include "plane_estimator.h"
#include "web_server.h"
#include "wifi_credentials.h"

#define CONTROL_PIN_UP GPIO_NUM_13    // Define GPIO pin for control up
#define CONTROL_PIN_DOWN GPIO_NUM_12  // Define GPIO pin for control down

static const bool debug = false;  // Set to true for debugging output
static const bool use_serial = true;
static const uint64_t control_timer_period = 100000;  // Control timer period in microseconds

static TimerHandle_t poll_distances_timer = NULL;
static hw_timer_t* control_timer = NULL;  // Timer for control task
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t wifi_event_queue;

static DistanceArraySensor distance_sensor;
static PlaneEstimator plane_estimator(8, 8);
static DistanceController distance_controller(CONTROL_PIN_UP, CONTROL_PIN_DOWN);
static WebServer web_server;

static const int CMD_BUF_LEN = 255;
static const int USER_INTERFACE_LOOP_DELAY = 20;  // Delay for user interface loop in milliseconds

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
            taskENTER_CRITICAL(&spinlock);
            distance_controller.updateDistanceToPlane(estimated_distance);
            taskEXIT_CRITICAL(&spinlock);
        }
        printDebug("Estimated distance to plane: " + String(estimated_distance));
    }
    else
    {
        printDebug("No new data available.");
    }
}

void IRAM_ATTR controlCallback()
{
    distance_controller.updateControlLoop();
    if (distance_controller.isControlling())
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
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
                    portENTER_CRITICAL(&spinlock);
                    distance_controller.setTargetHeight(height_mm);
                    portEXIT_CRITICAL(&spinlock);
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

void webServerTask(void* pvParameters)
{
    WiFiClient client;
    while (true)
    {
        if (client)
        {
            Serial.println("Client connected.");
            while (client.connected())
            {
                WebServer::CommandType command =
                    web_server.handleClient(client, distance_controller.getEstimatedDistanceToPlane());
                if (command == WebServer::CommandType::SET_HEIGHT)
                {
                    Serial.println("Setting new height from web server.");
                    float new_height = web_server.getNewHeight();
                    if (new_height > 0.0)
                    {
                        portENTER_CRITICAL(&spinlock);
                        distance_controller.setTargetHeight(200);
                        portEXIT_CRITICAL(&spinlock);
                        Serial.print("New height set: ");
                        Serial.println(new_height);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(USER_INTERFACE_LOOP_DELAY));
            }
            Serial.println("Client disconnected.");
        }
        else
        {
            client = web_server.newClientConnection();
            if (client)
            {
                Serial.println("New client connection.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(USER_INTERFACE_LOOP_DELAY));  // Handle client every 100 ms
    }
}

void connectToWifi()
{
    const TickType_t xTicksToWait = pdMS_TO_TICKS(4000);

    Serial.println("GFV - Connecting to WiFi");
    Serial.printf("GFV - SSID: %s\n", WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.disconnect();
        web_server = WebServer(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("GFV - Waiting on wifi connection");
        vTaskDelay(xTicksToWait);
    }
    web_server.begin();
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

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    printDebug("Starting desktop height controller...");
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
        xTaskCreatePinnedToCore(webServerTask, "Web Server Task", 5 * 1024, NULL, 2, NULL, 0);
        printDebug("Started web server task");
    }

    control_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(control_timer, controlCallback, true);
    timerAlarmWrite(control_timer, control_timer_period, true);
    timerAlarmEnable(control_timer);
    printDebug("Control timer started");

    // Delete setup and loop tasks
    vTaskDelete(NULL);
}

void loop()
{
    // Execution will not reach here as the setup function deletes the task
}