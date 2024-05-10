#include "time.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HTS221.h>
#include <WiFi.h>

#include <PubSubClient.h>

// WiFi Details
const char* ssid = "IoT5";
const char* WIFI_PASSWORD = "iotgroup5";
const char* ntpServer = "pool.ntp.org";

#define UPDATEDATA   "@msg/data/3"

WiFiClient client;
PubSubClient mqtt(client);

Adafruit_BMP280 bmp;
Adafruit_HTS221 hts;

QueueHandle_t sensorDataQueue;

const char* mqtt_server = "raspi.lan";
const int mqtt_port = 1883;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  Serial.println(mqtt.connected());
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(mqtt_server)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setupNetwork() {
  Serial.print(F("Connecting to network: "));
  Serial.println(ssid);
  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, WIFI_PASSWORD);

  // while (WiFi.localIP().toString() == "0.0.0.0") {
  //   delay(500);
  //   Serial.println(WiFi.status());
  // }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(WiFi.status());
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  // Show IP-Address
}

void setupHardware() {
  Wire.begin(41, 40, 100000);
  if (bmp.begin(0x76)) {  // prepare BMP280 sensor
    Serial.println("BMP280 sensor ready");
  }
  if (hts.begin_I2C()) {  // prepare HTS221 sensor
    Serial.println("HTS221 sensor ready");
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void taskDisplayTime(void* parameter) {
  for(;;) {
    printLocalTime();
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void taskUpdateTime(void* parameter) {
  for(;;) {
    configTime(7*3600, 0, ntpServer); // +7 UTC Thailand Time
    Serial.println("NTP updated");
    vTaskDelay(60000/portTICK_PERIOD_MS);
  }
}

void sendSensorData(void *parameter) {
  for (;;) {
    sensors_event_t temp, humidity;
    hts.getEvent(&humidity, &temp);
    
    float temp_hts = temp.temperature;
    float humid_hts = humidity.relative_humidity;
      
    float temp_bmp = bmp.readTemperature();
    float pressure_bmp = bmp.readPressure();


    char json_body[200];
    const char json_tmpl[] = "{\"temperature\": %.2f,"
                             "\"humidity\": %.2f," 
                             "\"pressure\": %.2f}";
    sprintf(json_body, json_tmpl, temp_hts, humid_hts, pressure_bmp);

    Serial.println(json_body);
        
    // Queue JSON message to the back of the FIFO queue
    xQueueSendToBack(sensorDataQueue, &json_body, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(60000)); // Read sensor data every minute
  }
}

void publishDataFromQueue(void *parameter) {
  char json_body[200];

  for (;;) {
    // Check if the queue has any data
    if (uxQueueMessagesWaiting(sensorDataQueue) > 0) {
      // Receive data from the queue
      if (xQueueReceive(sensorDataQueue, &json_body, portMAX_DELAY) == pdPASS) {
        // Publish data to Netpie
        if (mqtt.connected()) {
          mqtt.publish(UPDATEDATA, json_body);
        } else {
          // If MQTT connection is not established, attempt to reconnect
          reconnect();
        }
      }
    }

    // Delay for a short time before checking the queue again
    vTaskDelay(pdMS_TO_TICKS(60000)); // Publish every minute
  }
}


void setup() {
  Serial.begin(115200);
  setupNetwork();
  setupHardware();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);
  sensorDataQueue = xQueueCreate(10, sizeof(char[200]));
  xTaskCreatePinnedToCore(
    taskDisplayTime,
    "Display Time",
    10000,
    NULL,
    1,
    NULL,
    0);
  xTaskCreatePinnedToCore(
    taskUpdateTime,
    "Update Time",
    10000,
    NULL,
    2, // Higher
    NULL,
    0);
  xTaskCreate(
    sendSensorData,     // Task function
    "sendSensorData",   // Task name
    10000,              // Stack size (bytes)
    NULL,               // Parameter to pass to the task
    3,                  // Task priority
    NULL                // Task handle
  );
  xTaskCreate(
    publishDataFromQueue,     // Task function
    "publishDataFromQueue",   // Task name
    10000,                    // Stack size (bytes)
    NULL,                     // Parameter to pass to the task
    4,                        // Task priority
    NULL                      // Task handle
  );
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
}