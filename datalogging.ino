#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>
#include "secrets.h"
#define TIME_ZONE +8

#define DHTTYPE DHT22 // DHT22
#define DHTPIN 13 // D7 which is GPIO13
// Initiate DHT sensor
DHT dht(DHTPIN, DHTTYPE);
float h;
float t;
String fullness;
long duration;
float fullnessint;
float height = 12.55; //Height of the inside of the safe, we will take the first measurement value to determine height, assuming safe is empty at the beginning

unsigned long lastMillis = 0;
unsigned long previousMillis = 0;
const long interval = 5000;
const int trigPin = D0; // Trigger pin (GPIO12)
const int echoPin = D1; // Echo pin (GPIO14)
 
#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/pub"
 
WiFiClientSecure net;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient client(net);
 
time_t now;
time_t nowish = 1672531200; // January 1, 2023, 00:00:00 UTC
 
 
void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
 
 
void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

 
void connectAWS()
{
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
 
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);
 
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}
 
 
void publishMessage()
{
  StaticJsonDocument<200> doc;
  struct tm timeinfo;
  char timeBuffer[30]; // Buffer for ISO 8601 format

  // Get local time
  localtime_r(&now, &timeinfo);

  // Format time as ISO 8601 (YYYY-MM-DDTHH:MM:SS+08:00)
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%dT%H:%M:%S+08:00", &timeinfo);
  doc["temperature"] = t;
  doc["humidity"] = h;
  doc["fullness"] = fullness;
  doc["time"] = timeBuffer;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

float ultrasonic(){
  float distance;
  // Clear the trigPin by setting it LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Set the trigPin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echoPin, and calculate the duration in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance in cm (speed of sound = 343 m/s)
  // Divide by 58 to convert microseconds to centimeters
  distance = duration * 0.034 / 2;

  // Print the distance to the serial monitor
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  fullnessint = roundf(100-((distance/height)*100));
  return fullnessint;
}
 
 
void setup()
{
  Serial.begin(115200);
  // Connect to AWS IoT
  connectAWS();
  // Start DHT22
  dht.begin();
  // Set trigPin as OUTPUT and echoPin as INPUT
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // // Get Initial Value of Ultrasonic Sensor
  // float distance;
  // // Clear the trigPin by setting it LOW
  // digitalWrite(trigPin, LOW);
  // delayMicroseconds(2);

  // // Set the trigPin HIGH for 10 microseconds
  // digitalWrite(trigPin, HIGH);
  // delayMicroseconds(10);
  // digitalWrite(trigPin, LOW);

  // // Read the echoPin, and calculate the duration in microseconds
  // duration = pulseIn(echoPin, HIGH);

  // Calculate the distance in cm (speed of sound = 343 m/s)
  // Divide by 58 to convert microseconds to centimeters
  // height = duration * 0.034 / 2;
  // Serial.print("Height: ");
  // Serial.println(height);
  // delay(1000);
}
 
 
void loop()
{
  h = dht.readHumidity();
  t = dht.readTemperature();
  fullnessint = ultrasonic();
  fullness = String(fullnessint) + "%";
  Serial.println(fullness);
 
  if (isnan(h) || isnan(t) )  // Check if any reads failed and exit early (to try again).
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
 
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));
  delay(2000);
 
  now = time(nullptr);
 
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis > 30000)
    {
      lastMillis = millis();
      publishMessage();
    }
  }
}