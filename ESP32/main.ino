#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HTS221.h>
#include <WiFi.h>

#include <PubSubClient.h>

#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#define PIN 18
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

Adafruit_BMP280 bmp;
Adafruit_HTS221 hts;

// WiFi-Connection
#define WIFI_SSID "IoT5"
// WiFi password
#define WIFI_PASSWORD "iotgroup5"
unsigned long previousMillis = 0;
unsigned long interval = 30000;  //WiFi health-check


// MQTT-Connection
WiFiClient client;
PubSubClient mqtt(client);

// IPAddress mqtt_server(192,168,1,200);
#define mqtt_server "raspi.lan"
#define mqtt_port 1883

const char* mqtt_topic = "@msg/data/3";

// NTP-Server
int timezone = 7 * 3600;  //ตั้งค่า TimeZone ตามเวลาประเทศไทย
int dst = 0;              //กำหนดค่า Date Swing Time

// MQTT-Callback Function
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setupHardware() {
  Wire.begin(41, 40, 100000);
  if (bmp.begin(0x76)) {  // prepare BMP280 sensor
    Serial.println("BMP280 sensor ready");
  }
  if (hts.begin_I2C()) {  // prepare HTS221 sensor
    Serial.println("HTS221 sensor ready");
  }
  pinMode(2, OUTPUT);     // prepare LED
  digitalWrite(2, HIGH);  // Set board LED to HIGH
}

void setupNetwork() {
  Serial.print(F("Connecting to network: "));
  
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  // Show IP-Address
}

void setupTime() {
  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");  //ดึงเวลาจาก Server
  Serial.println("\nLoading time");
  while (!time(nullptr)) {
    Serial.print("*");
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  setupHardware();
  setupNetwork();
  setupTime();
  // Disable on-board WLED
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();
  //MQTT-Setup
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);
}

void reconnect() {
  // Loop until we're reconnected
  Serial.println(mqtt.connected());
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      mqtt.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  static uint32_t prev_millis = 0;
  sensors_event_t temp, humidity;
  char json_body[200];
  const char json_tmpl[] = "{\"pressure\": %.2f,"
                           "\"temperature\": %.2f,"
                           "\"humidity\": %.2f}";
  hts.getEvent(&humidity, &temp);
  Serial.print("Pressure (Pa): ");
  Serial.println(bmp.readPressure());
  Serial.print("Temperature (C): ");
  Serial.println(temp.temperature);
  Serial.print("Humidity (RH%): ");
  Serial.println(humidity.relative_humidity);

  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  if (!mqtt.connected()) {
    reconnect();
  }
  if (millis() - prev_millis > 15000) {
    prev_millis = millis();
    float p = bmp.readPressure();
    float t = temp.temperature;
    float h = humidity.relative_humidity;
    sprintf(json_body, json_tmpl, p, t, h);
    Serial.println(json_body);
    mqtt.publish(mqtt_topic, json_body);
  }
  mqtt.loop();
  delay(1000);
}