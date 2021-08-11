#include <Arduino.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include "DHT.h"
#include "credentials.h"
#include "PubSubClient.h"


const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWD;
const char *uname = U_NAME;
const char *upass = U_PASS;

const int monitor_speed = 115200;
const int delay_five_sec = 5000;
const int delay_one_sec = 1000;

bool guard = false;

//MQTT
const char *mqtt_server = MQTT_BROKER;
const char *mqtt_username = MQTT_USER_NAME;
const char *mqtt_password = MQTT_USER_PASSWORD;
const char *humidity_topic = "/sensor1/humidity";
const char *temperature_topic = "/sensor1/temperature";
const char *clientID = "sensor1";
const char *inTopic = "sensor1/in";

long interval = MQTT_INTERVAL;
long currentTime, lastTime;
//const int message_length = 50;
char messages[50];
float temp, hum;
const int MQTT_PORT = 1883;
const int http_ok = 200;

WiFiClient espClient;
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print(topic);
  for (int i = 0; i < length; i++)
  {
    Serial.print(" ");
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

PubSubClient client(mqtt_server, MQTT_PORT, espClient);

// Set LED GPIO
const int ledPin = 2;
String ledState;
// temp humity sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Create AsyncWebServer object on port 80
const int ASYNC_PORT = 80;
AsyncWebServer server(ASYNC_PORT);

String readDHTTemperature()
{

  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //float t = dht.readTemperature(true);
  //Check if any reads failed and exit early (to try again).
  if (isnan(t))
  {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else
  {
    //Serial.println(t);
    return String(t);
  }
}


String readDHTHumidity()
{
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  if (isnan(h))
  {
    Serial.println("Failed to read from DHT sensor!");
    return "--";
  }
  else
  {
    //  Serial.println(h);
    return String(h);
  }
}

void mqtt_temp_tx()
{ 

 // sprintf(messages, "Temperature %", readDHTTemperature());
  //Serial.println(messages);
  //Serial.print(readDHTTemperature().c_str());
  //Serial.print("mqtt temp");
  client.publish(temperature_topic, readDHTTemperature().c_str());
  //client.publish(temperature_topic, messages);
}

void mqtt_humidity_tx()
{
  
  //sprintf(messages, "Humidity %", readDHTHumidity());
  // Serial.println(messages);
  // client.publish(humidity_topic, messages);
 // Serial.println(readDHTHumidity().c_str());
  //Serial.print("mqq_humidity");
  client.publish(humidity_topic, readDHTHumidity().c_str());
}

// Replaces placeholder with LED state value
String processor(const String &var)
{
  Serial.println(var);
  if (var == "STATE")
  {
    if (digitalRead(ledPin))
    {
      ledState = "ON";
    }
    else
    {
      ledState = "OFF";
    }
    Serial.print(ledState);
    return ledState;
  }
  return String();
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("\n connecting to .....");
    Serial.print(mqtt_server);
    if (client.connect(clientID, mqtt_username, mqtt_password))
    {
      Serial.print("\n connected to ...");
      Serial.println(mqtt_server);
      client.subscribe(inTopic);
    }
    else
    {
      Serial.print("\n trying to connect again");
      delay(delay_five_sec);
    }
  }
}

void setup()
{
  // Serial port for debugging purposes

  Serial.begin(monitor_speed);
  pinMode(ledPin, OUTPUT);
  dht.begin();

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(delay_one_sec);
    Serial.println(" WiFi  connecting..");
  }
  client.setServer(mqtt_server, MQTT_PORT);
  client.setCallback(callback);

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/style.css", "text/css"); });

  // Route to set GPIO to HIGH
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              digitalWrite(ledPin, HIGH);
              request->send(SPIFFS, "/index.html", String(), false, processor);
            });

  // Route to set GPIO to LOW
  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              digitalWrite(ledPin, LOW);
              request->send(SPIFFS, "/index.html", String(), false, processor);
            });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
            {
             
              request->send_P(http_ok, "text/plain", readDHTTemperature().c_str());
              //request->send(SPIFFS, "/index.html", c);
            });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              request->send_P(http_ok, "text/plain", readDHTHumidity().c_str());
              //request->send(SPIFFS, "/index.html", readDHTHumidity().c_str());
            });

  // Start server
  AsyncElegantOTA.begin(&server, uname, upass); // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  currentTime = millis();
  if (currentTime - lastTime > interval)
  {
    Serial.println(" time " + currentTime);
    mqtt_temp_tx();
    mqtt_humidity_tx();
    lastTime = millis();
  }
  delay(delay_one_sec);
}