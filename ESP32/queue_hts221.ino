#include <Adafruit_NeoPixel.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HTS221.h>
#include "time.h"
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>


// WiFi Details
const char* ssid = "IoT5";
const char* WIFI_PASSWORD = "iotgroup5";
const char* ntpServer = "pool.ntp.org";

// MQTT topics
#define topicToPublish "@msg/data/1"
#define topicToSubscribe "@msg/cc"

// MQTT-Connection
WiFiClient client;
PubSubClient mqtt(client);

// On-board sensors
Adafruit_BMP280 bmp;
Adafruit_HTS221 hts;

// Storing data in a queue
QueueHandle_t sensorDataQueue;

// IPAddress mqtt_server(192,168,1,200);
const char* mqtt_server = "raspi.lan";
const int mqtt_port = 1883;

// Built-in RGB LED
#define PIN 18       // not sure
#define NUMPIXELS 1  // only 1 RGB LED is on Cucumber board
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void reconnect() {
  // Loop until we're reconnected
  Serial.println(mqtt.connected());
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String clientId = "ESP32-";
    clientId += String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      mqtt.subscribe(topicToSubscribe);
      Serial.print("Subscribed to ");
      Serial.println(topicToSubscribe);
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
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void taskDisplayTime(void* parameter) {
  for (;;) {
    printLocalTime();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void taskUpdateTime(void* parameter) {
  for (;;) {
    configTime(7 * 3600, 0, ntpServer);  // +7 UTC Thailand Time
    Serial.println("NTP updated");
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

void sendSensorData(void* parameter) {
  for (;;) {
    sensors_event_t temp, humidity;
    hts.getEvent(&humidity, &temp);

    float temp_hts = temp.temperature;
    float humid_hts = humidity.relative_humidity;
    float pressure_bmp = bmp.readPressure();

    char json_body[200];

    const char json_tmpl[] = "{\"temperature\": %.2f,"
                             "\"humidity\": %.2f,"
                             "\"pressure\": %.2f}";
    // sprintf(var, format, arg);  %.2f = round the float to two decimal places
    sprintf(json_body, json_tmpl, temp_hts, humid_hts, pressure_bmp);

    Serial.println(json_body);  //json message is a string

    // Queue JSON message to the back of the FIFO queue
    xQueueSendToBack(sensorDataQueue, &json_body, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(60000));  // Read sensor data every minute
  }
}

void publishDataFromQueue(void* parameter) {
  char json_body[200];

  for (;;) {
    // Check if the queue has any data
    if (uxQueueMessagesWaiting(sensorDataQueue) > 0) {
      // Receive data from the queue
      if (xQueueReceive(sensorDataQueue, &json_body, portMAX_DELAY) == pdPASS) {
        // Publish data to Netpie
        if (mqtt.connected()) {
          mqtt.publish(topicToPublish, json_body);
        } else {
          // If MQTT connection is not established, attempt to reconnect
          reconnect();
        }
      }
    }

    // Delay for a short time before checking the queue again
    vTaskDelay(pdMS_TO_TICKS(60000));  // Publish every minute
  }
}

// When the JSON messages arrive:
// {"sensor": 1, "predictedTemp": 28.252, "predictedHumidity": 79.338}
// {"sensor": 2, "predictedTemp": 37.816, "predictedHumidity": 49.733}
// {"sensor": 3, "predictedTemp": 36.209, "predictedHumidity": 62.834}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: [");
  Serial.print(topic);
  Serial.print("] ");

  char jsonMessage[200];

  for (int i = 0; i < length; i++) {
    // Serial.print((char)payload[i]);
    jsonMessage[i] = (char)payload[i];
  }

  Serial.println(jsonMessage);

  //Extract the numbers from JSON message
  String str = String(jsonMessage);
  String numStr = "";
  float valToCompare[3];
  int k = 0;
  for (int i = 0; i < str.length(); i++) {
    char ch = str.charAt(i);
    if (isdigit(ch) || ch == '-' || ch == '.') {
      numStr += ch;
    } else if (numStr != "") {
      float num = numStr.toFloat();
      Serial.println(num);
      valToCompare[k] = num;
      k += 1;
      numStr = "";
    }
  }
  // Get current temp & humidity value to check the model accuracy
  sensors_event_t temp, humidity;
  hts.getEvent(&humidity, &temp);

  float temp_hts = temp.temperature;
  float humid_hts = humidity.relative_humidity;

  // Check sensor No.
  // Convert the C-style string to a String object
  String messageString = String(topicToPublish);

  // Find the position of the last '/'
  int lastSlashIndex = messageString.lastIndexOf('/');

  // Extract the substring containing the number
  String numberString = messageString.substring(lastSlashIndex + 1);
  if (int(valToCompare[0]) == numberString.toInt()) {
    Serial.println("Yep, it's mine");
    // Check predicted temperature & humidity accuracy
    // 00
    //! if temp & humid are both inaccurate => red
    // 01
    //? if humid is inaccurate => blue
    // 10
    //^ if temp is inaccurate => yellow
    // 11
    //* if temp & humid are both accurate => green

    float tempDiff = abs(valToCompare[1] - temp_hts);
    float humidDiff = abs(valToCompare[2] - humid_hts);
    // LED turns...
    if ((tempDiff > 10) && (humidDiff > 10)) {
      // red
      pixels.setPixelColor(0, pixels.Color(150, 0, 0));

    } else if ((tempDiff <= 10) && (humidDiff > 10)) {
      // blue
      pixels.setPixelColor(0, pixels.Color(0, 0, 150));

    } else if ((tempDiff > 10) && (humidDiff <= 10)) {
      // yellow
      pixels.setPixelColor(0, pixels.Color(150, 150, 0));

    } else {
      // green
      pixels.setPixelColor(0, pixels.Color(0, 150, 0));
    }
    pixels.show();  // Send the updated pixel colors to the hardware.
  } else {
    Serial.println("Nope, discard the message pls");
  }
}

void setup() {
  Serial.begin(115200);
  pixels.begin();  // RGB LED
  pixels.clear();  // Set all pixel colors to 'off'
  setupNetwork();
  setupHardware();
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);  //set a callback function name "callback" to be called when a message arrives from a subscribed topic
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
    2,  // Higher
    NULL,
    0);
  xTaskCreate(
    sendSensorData,    // Task function
    "sendSensorData",  // Task name
    10000,             // Stack size (bytes)
    NULL,              // Parameter to pass to the task
    3,                 // Task priority
    NULL               // Task handle
  );
  xTaskCreate(
    publishDataFromQueue,    // Task function
    "publishDataFromQueue",  // Task name
    10000,                   // Stack size (bytes)
    NULL,                    // Parameter to pass to the task
    4,                       // Task priority
    NULL                     // Task handle
  );
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
}