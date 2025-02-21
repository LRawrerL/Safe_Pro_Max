#include <Keypad.h>
#include <lcd_i2c.h>  
#include <ESP32Servo.h> 
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define ULTRASONIC_PIN 5       // Pin to turn on LCD and ask for RFID when ultrasonic (pin 5)
#define WRONG_RFID_PIN 23      // Pin to indicate wrong RFID (pin 23)
#define CORRECT_RFID_PIN 16    // Pin to indicate correct RFID and prompt for fingerprint (pin 16)
#define WRONG_FINGERPRINT_PIN 15 // Pin to indicate wrong fingerprint (pin 15)
#define BEGIN_OTP_PIN 2        // Pin to indicate correct fingerprint and begin OTP (pin 2)

#define AWS_IOT_PUBLISH_DOOR_OTP   "door_otp/pub"
#define AWS_IOT_SUBSCRIBE_DOOR_OTP "door_otp/sub"
#define AWS_IOT_PUBLISH_DOOR_STATUS   "door_status/pub"
#define AWS_IOT_SUBSCRIBE_DOOR_STATUS "door_status/pub"

WiFiClientSecure net;
PubSubClient client(net);

unsigned long lastMillis = 0;
String doorStatus;

time_t now;
time_t nowish = 1672531200; // January 1, 2023, 00:00:00 UTC

int OTPYES;

// OTP variables
int otpNumber;                   // Generated OTP as a number
char generatedOTP[7] = {'\0'};     // Generated OTP as a C-string (null-terminated)
char userOTP[7] = {'\0'};          // User-entered OTP from the keypad
char adminPassword[7] = "444444"; // Admin Password for resetting the Machine
char userPassword[7] = {'\0'}; // User-entered Admin Password from the keypad
int otpIndex = 0;                // Index into userOTP while entering digits
unsigned long otpGeneratedTime = 0; // Time when OTP was generated
const unsigned long otpValidityPeriod = 60000; // 60 seconds (in milliseconds)

// LCD address 0x3E, with 16 columns and 2 rows
lcd_i2c lcd(0x3E, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;

// Define the key map for the 4x4 keypad
char keyMap[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Define the pins for the rows and columns
uint8_t colPins[COLS] = {14, 27, 26, 25};
uint8_t rowPins[ROWS] = {33, 32, 18, 19};

// Initialise the keypad
Keypad keypad = Keypad(makeKeymap(keyMap), rowPins, colPins, ROWS, COLS);

// Initialise Servo
Servo servo;
uint8_t servoPin = 13;
int pos = 0;

int tries;

void NTPConnect(void) {
  Serial.print("Setting time using SNTP");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish) {
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

void messageReceived(char *topic, byte *payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

String generateOTP() {
  otpNumber = random(100000, 999999);
  Serial.print("Generated OTP: ");
  Serial.println(otpNumber);
  otpGeneratedTime = millis(); // Store the time when OTP was generated
  String otpString = String(otpNumber);
  otpString.toCharArray(generatedOTP, 7); // Save as a C-string in generatedOTP
  return otpString;
}

void connectAWS() {
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  NTPConnect();

  net.setCACert(cacert);
  net.setCertificate(client_cert);
  net.setPrivateKey(privkey);

  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);

  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  while(!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_DOOR_OTP);

  Serial.println("AWS IoT Connected!");
}

void publishStatus(const String& status) {
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["Door Status"] = status;

  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);
  Serial.println("Checking Client Connection");

  client.disconnect();

  if(!client.connect(THINGNAME)){
    Serial.println("Not Connected. Connecting to AWS IoT");
    connectAWS();
  }

  if (client.publish(AWS_IOT_PUBLISH_DOOR_STATUS, jsonBuffer)) {
    Serial.println("Door Status Published to IoT Core");
  } else {
    Serial.println("Failed to publish Door Status");

  }
}

void publishOTP() {
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["otp"] = otpNumber;  // Publishing the OTP number

  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);
  
  if(!client.connect(THINGNAME)){
    Serial.println("Not Connected. Connecting to AWS IoT");
    connectAWS();
  }

  if (client.publish(AWS_IOT_PUBLISH_DOOR_OTP, jsonBuffer)) {
    Serial.println("OTP published to AWS IoT Core");
  } else {
    Serial.println("Failed to publish OTP");
  }
}

// Function to authenticate the OTP
bool AUTHENTICATE_OTP() {
  return strcmp(userOTP, generatedOTP) == 0;
}

bool AUTHENTICATE_PASSWORD(){
  return strcmp(userPassword, adminPassword) == 0;
}

// Open the servo (unlock the door)
void SERVO_OPEN() {
  for (pos = 90; pos <= 180; pos += 10) {
    servo.write(pos);
    delay(20);
  }
  Serial.println("Door Unlocked!");
  doorStatus = "open";
}

// Close the servo (lock the door)
void SERVO_CLOSE() {
  for (pos = 180; pos >= 90; pos -= 10) {
    servo.write(pos);
    delay(20);
  }
  Serial.println("Door Locked!");
  doorStatus = "closed";
}

void setup() {
  Serial.begin(115200);
  pinMode(ULTRASONIC_PIN, INPUT);
  pinMode(WRONG_RFID_PIN, INPUT);
  pinMode(CORRECT_RFID_PIN, INPUT);
  pinMode(WRONG_FINGERPRINT_PIN, INPUT);
  pinMode(BEGIN_OTP_PIN, INPUT);
  
  // Set up the LCD
  lcd.begin();
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("SAFE PRO MAX");

  // Connect to AWS IoT
  connectAWS();

  // Set initial door status and publish it
  doorStatus = "closed";
  publishStatus(doorStatus);
  Serial.println("SENT LED!! CHECK AWS");

  // Set up the Servo Motor
  servo.attach(servoPin, 500, 2500);
  Serial.println("Servo attached");

  delay(1000);
}

void loop() {
  // Step 1: Check for Ultrasonic/Person Detected
  if (digitalRead(ULTRASONIC_PIN)) {
    Serial.println("PERSON DETECTED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please Scan");
    lcd.setCursor(0, 1);
    lcd.print("Your RFID Tag");
    delay(3000);
    while (!digitalRead(WRONG_RFID_PIN) && !digitalRead(CORRECT_RFID_PIN)) // Wait until a valid RFID is detected
  }

  // IF WRONG RFID 
  if (digitalRead(WRONG_RFID_PIN)) {
    Serial.println("WRONG RFID!");
    tries++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Denied");
    delay(3000);
    lcd.clear();
    if (tries != 3) {
      int triesleft = 3 - tries;
      lcd.setCursor(0, 0);
      lcd.print("Tries Left:");
      lcd.setCursor(0, 1);
      char triesStr[2];  // Buffer to hold the converted integer
      sprintf(triesStr, "%d", triesleft);  // Convert integer to string
      lcd.print(triesStr);  // Print the converted string
    }
    else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Too many tries!");
      delay(3000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Please Reset");
      lcd.setCursor(0, 1);
      lcd.print("With Password");
      int reset = 1;
      while (reset) {
        char key = keypad.getKey();
        if (key) {
          // Append digit if a number key is pressed and we haven't reached 6 digits yet
          if (key >= '0' && key <= '9' && otpIndex < 6) {
            userPassword[otpIndex] = key;
            otpIndex++;
            userPassword[otpIndex] = '\0';  // Ensure the string is null-terminated
          }
          // BACKSPACE functionality (using '*' key)
          if (key == '*') {
            if (otpIndex > 0) {
              otpIndex--;
              userPassword[otpIndex] = '\0';
            }
          }
          // Update LCD display with the current user-entered OTP
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter Password: ");
          lcd.setCursor(0, 1);
          lcd.print(userPassword);
        }
        // When 6 digits have been entered, attempt OTP validation
        if (otpIndex == 6) {
          delay(500);
          Serial.println("Password entered:");
          Serial.println(userPassword);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Authenticating...");
          delay(1000);
          if (AUTHENTICATE_PASSWORD()) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Password Correct");
            lcd.setCursor(0, 1);
            lcd.print("Safe Reset!");
            reset = 0;
            tries = 0;
            delay(3000);
          } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Access Denied");
            while(1){
              lcd.clear();
              lcd.print("cooked!");
              delay(5000);
            };
          }
          // Reset user OTP input for the next attempt
          for (int i = 0; i < 7; i++) {
            userOTP[i] = '\0';
          }
          otpIndex = 0;
        }
      }
    }
    delay(3000);
  }
  
  // Step 2: Validate RFID Tag
  if (digitalRead(CORRECT_RFID_PIN)) {
    Serial.println("CORRECT RFID TAG!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Valid Tag");
    delay(3000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please Scan");
    lcd.setCursor(0, 1);
    lcd.print("Your Fingerprint");
    delay(1000);
    // Wait until either a wrong fingerprint or begin OTP is triggered
    while (!digitalRead(WRONG_FINGERPRINT_PIN) && !digitalRead(BEGIN_OTP_PIN));
  }
  
  // IF WRONG FINGERPRINT
  if (digitalRead(WRONG_FINGERPRINT_PIN)) {
    Serial.println("WRONG FINGERPRINT!");
    tries++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Denied");
    delay(3000);
    lcd.clear();
    if (tries != 3) {
      int triesleft = 3 - tries;
      lcd.setCursor(0, 0);
      lcd.print("Tries Left:");
      lcd.setCursor(0, 1);
      char triesStr[2];  // Buffer to hold the converted integer
      sprintf(triesStr, "%d", triesleft);  // Convert integer to string
      lcd.print(triesStr);  // Print the converted string
    }
    else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Too many");
      lcd.setCursor(0, 1);
      lcd.print("tries!");
      delay(3000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Please Reset");
      lcd.setCursor(0, 1);
      lcd.print("With Password");
      delay(3000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Password:");
      int reset = 1;
      while (reset) {
        char key = keypad.getKey();
        if (key) {
          // Append digit if a number key is pressed and we haven't reached 6 digits yet
          if (key >= '0' && key <= '9' && otpIndex < 6) {
            userPassword[otpIndex] = key;
            otpIndex++;
            userPassword[otpIndex] = '\0';  // Ensure the string is null-terminated
          }
          // BACKSPACE functionality (using '*' key)
          if (key == '*') {
            if (otpIndex > 0) {
              otpIndex--;
              userPassword[otpIndex] = '\0';
            }
          }
          // Update LCD display with the current user-entered OTP
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter Password:");
          lcd.setCursor(0, 1);
          lcd.print(userPassword);
        }
        // When 6 digits have been entered, attempt OTP validation
        if (otpIndex == 6) {
          delay(500);
          Serial.println("Password entered:");
          Serial.println(userPassword);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Authenticating...");
          delay(1000);
          if (AUTHENTICATE_PASSWORD()) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Password Correct");
            lcd.setCursor(0, 1);
            lcd.print("Safe Reset!");
            reset = 0;
            tries = 0;
            delay(3000);
          } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Access Denied");
            while(1){
              lcd.clear();
              lcd.print("cooked");
              delay(5000);
            };
          }
          // Reset user OTP input for the next attempt
          for (int i = 0; i < 7; i++) {
            userOTP[i] = '\0';
          }
          otpIndex = 0;
        }
      }
    }
    delay(3000);
  }
  
  if (digitalRead(BEGIN_OTP_PIN)) {
    OTPYES = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sending OTP");
    // Generate OTP and publish it
    randomSeed(analogRead(0));
    String otpGenerated = generateOTP();
    Serial.println("Generated OTP: " + otpGenerated);
    publishOTP();
    Serial.println("SENT OTP!! CHECK AWS");
    Serial.println("Key in the OTP: ");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OTP SENT!");
    delay(3000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Please Check");
    lcd.setCursor(0, 1);
    lcd.print("Your Email");
    delay(3000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter OTP: ");
    lcd.setCursor(0, 1);
    
    while (OTPYES) {
      char key = keypad.getKey();
      if (key) {
        // Append digit if a number key is pressed and we haven't reached 6 digits yet
        if (key >= '0' && key <= '9' && otpIndex < 6) {
          userOTP[otpIndex] = key;
          otpIndex++;
          userOTP[otpIndex] = '\0';  // Ensure the string is null-terminated
        }
        // BACKSPACE functionality (using '*' key)
        if (key == '*') {
          if (otpIndex > 0) {
            otpIndex--;
            userOTP[otpIndex] = '\0';
          }
        }
        // If '#' is pressed, close the door
        if (key == '#') {
          Serial.println("CLOSING DOOR");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("# Pressed!");
          lcd.setCursor(0, 1);
          lcd.print("Locking Door");
          SERVO_CLOSE();  // Lock door
          publishStatus(doorStatus);
          OTPYES = 0;
          delay(2000);
        }
        // Update LCD display with the current user-entered OTP
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter OTP: ");
        lcd.setCursor(0, 1);
        lcd.print(userOTP);
      }
      // When 6 digits have been entered, attempt OTP validation
      if (otpIndex == 6) {
        delay(500);
        Serial.println("OTP entered:");
        Serial.println(userOTP);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Authenticating...");
        Serial.print("Correct OTP: ");
        Serial.println(generatedOTP);
        delay(1000);
        if (AUTHENTICATE_OTP()) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access Granted");
          SERVO_OPEN();  // Unlock door
          publishStatus(doorStatus);
          tries = 0;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access Denied");
          OTPYES = 0;
          delay(2000);
        }
        // Reset user OTP input for the next attempt
        for (int i = 0; i < 7; i++) {
          userOTP[i] = '\0';
        }
        otpIndex = 0;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Press #");
        lcd.setCursor(0, 1);
        lcd.print("To close door");
      }
    }
  }
  else {
    lcd.clear();
  }
}