#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <Adafruit_Fingerprint.h>

// Define RFID module pins
#define RST_PIN 22   
#define SS_PIN 5     

// Define Fingerprint module pins
#define FINGER_RX 16  
#define FINGER_TX 17  

// Define Ultrasonic Sensor Pins
#define TRIG_PIN 2   
#define ECHO_PIN 4   

// Define GPIO signals for ESP32B
#define SIGNAL_US_DETECTED   25  // Ultrasonic Sensor Close
#define SIGNAL_WRONG_RFID    21  // Wrong RFID
#define SIGNAL_CORRECT_RFID  27  // Correct RFID → Ask for Fingerprint
#define SIGNAL_WRONG_FINGER  14  // Wrong Fingerprint
#define SIGNAL_CORRECT_FINGER 12 // Correct Fingerprint → Start OTP

// Initialize RFID module
MFRC522DriverPinSimple ss_pin(SS_PIN);
MFRC522DriverSPI driver{ss_pin};
MFRC522 mfrc522{driver}; 

// Initialize Fingerprint module
HardwareSerial mySerial(2);  
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// List of authorized RFID cards
String authorizedRFID[] = {"f1 6c 42 1b"};
int totalAuthorizedRFID = sizeof(authorizedRFID) / sizeof(authorizedRFID[0]);

void setup() {
    Serial.begin(115200);
    SPI.begin();
    mfrc522.PCD_Init();

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Initialize GPIO signals
    pinMode(SIGNAL_US_DETECTED, OUTPUT);
    pinMode(SIGNAL_WRONG_RFID, OUTPUT);
    pinMode(SIGNAL_CORRECT_RFID, OUTPUT);
    pinMode(SIGNAL_WRONG_FINGER, OUTPUT);
    pinMode(SIGNAL_CORRECT_FINGER, OUTPUT);

    // Ensure all signals start LOW
    digitalWrite(SIGNAL_US_DETECTED, LOW);
    digitalWrite(SIGNAL_WRONG_RFID, LOW);
    digitalWrite(SIGNAL_CORRECT_RFID, LOW);
    digitalWrite(SIGNAL_WRONG_FINGER, LOW);
    digitalWrite(SIGNAL_CORRECT_FINGER, LOW);

    Serial.println("🔹 System Initialized");

    // Initialize Fingerprint Sensor
    mySerial.begin(57600, SERIAL_8N1, FINGER_RX, FINGER_TX);
    if (finger.verifyPassword()) {
        Serial.println("✅ Fingerprint sensor detected!");
    } else {
        Serial.println("❌ Fingerprint sensor NOT detected! Check wiring.");
        while (1);
    }
}

void loop() {
    // Step 1: Wait for user to be close using the Ultrasonic Sensor
    if (isUserClose()) {
        Serial.println("✅ Close enough! Sending signal for RFID scan...");
        // Reset any previous signals
        digitalWrite(SIGNAL_WRONG_RFID, LOW);
        digitalWrite(SIGNAL_CORRECT_RFID, LOW);
        digitalWrite(SIGNAL_WRONG_FINGER, LOW);
        digitalWrite(SIGNAL_CORRECT_FINGER, LOW);
        digitalWrite(SIGNAL_US_DETECTED, HIGH);

        // Step 2: Wait for an RFID card scan (with a 10 second timeout)
        if (waitForRFID(10)) {  
            Serial.println("✅ RFID Matched! Asking for Fingerprint...");
            // Prepare signals for fingerprint scan
            digitalWrite(SIGNAL_US_DETECTED, LOW);
            digitalWrite(SIGNAL_WRONG_RFID, LOW);
            digitalWrite(SIGNAL_WRONG_FINGER, LOW);
            digitalWrite(SIGNAL_CORRECT_FINGER, LOW);
            digitalWrite(SIGNAL_CORRECT_RFID, HIGH);

            // Step 3: Wait for a valid fingerprint scan
            if (getFingerprintID()) {
                Serial.println("✅ Fingerprint Matched! Beginning OTP process...");
                digitalWrite(SIGNAL_US_DETECTED, LOW);
                digitalWrite(SIGNAL_WRONG_RFID, LOW);
                digitalWrite(SIGNAL_CORRECT_RFID, LOW);
                digitalWrite(SIGNAL_WRONG_FINGER, LOW);
                digitalWrite(SIGNAL_CORRECT_FINGER, HIGH);
                delay(3000);
                digitalWrite(SIGNAL_CORRECT_FINGER, LOW);
            } else {
                Serial.println("❌ Wrong Fingerprint! Sending signal...");
                digitalWrite(SIGNAL_US_DETECTED, LOW);
                digitalWrite(SIGNAL_WRONG_RFID, LOW);
                digitalWrite(SIGNAL_CORRECT_RFID, LOW);
                digitalWrite(SIGNAL_CORRECT_FINGER, LOW);
                digitalWrite(SIGNAL_WRONG_FINGER, HIGH);
                delay(3000);
                digitalWrite(SIGNAL_WRONG_FINGER, LOW);
            }
        }
        // No additional else block is needed because waitForRFID() already
        // handles the wrong RFID signal when an unauthorized RFID is scanned
        // or when the timeout is reached.
    }
}

// Function to check if user is close using the Ultrasonic Sensor
bool isUserClose() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // 30ms timeout
    float distance = (duration * 0.0343) / 2; // Convert duration to cm

    if (distance > 0 && distance <= 3) {  
        Serial.println("🔹 Detected within 3cm range.");
        return true;
    }
    delay(500);
    return false;
}

// Function to wait for an RFID card scan within a given timeout (in seconds)
// If a card is scanned:
//   - If it is authorized, return true.
//   - If it is not authorized, immediately send the "wrong RFID" signal and return false.
// If no card is scanned within the timeout, also send the "wrong RFID" signal and return false.
bool waitForRFID(int timeoutSeconds) {
    unsigned long startTime = millis();
    while (millis() - startTime < timeoutSeconds * 1000UL) {
        String rfidUID = readRFID();
        if (rfidUID != "") {
            if (isAuthorizedRFID(rfidUID)) {
                return true;  // Successfully scanned an authorized RFID card
            } else {
                // Wrong RFID scanned—immediately send the signal:
                Serial.println("❌ Wrong RFID! Sending signal...");
                digitalWrite(SIGNAL_US_DETECTED, LOW);
                digitalWrite(SIGNAL_CORRECT_RFID, LOW);
                digitalWrite(SIGNAL_WRONG_FINGER, LOW);
                digitalWrite(SIGNAL_CORRECT_FINGER, LOW);
                digitalWrite(SIGNAL_WRONG_RFID, HIGH);
                delay(3000);
                digitalWrite(SIGNAL_WRONG_RFID, LOW);
                return false;
            }
        }
    }
    // Timeout reached without any card scan—send wrong RFID signal.
    Serial.println("❌ No RFID card scanned within timeout. Sending wrong RFID signal...");
    digitalWrite(SIGNAL_US_DETECTED, LOW);
    digitalWrite(SIGNAL_CORRECT_RFID, LOW);
    digitalWrite(SIGNAL_WRONG_FINGER, LOW);
    digitalWrite(SIGNAL_CORRECT_FINGER, LOW);
    digitalWrite(SIGNAL_WRONG_RFID, HIGH);
    delay(3000);
    digitalWrite(SIGNAL_WRONG_RFID, LOW);
    return false;
}

// Function to read the RFID card UID
String readRFID() {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return "";
    }

    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) {
            uid += " 0";  // Formatting
        } else {
            uid += " ";
        }
        uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.trim(); // Remove extra spaces

    Serial.print("🔹 RFID Card Detected:");
    Serial.println(uid);

    // Halt RFID communication
    mfrc522.PICC_HaltA();

    return uid;
}

// Function to check if the scanned RFID card is authorized
bool isAuthorizedRFID(String uid) {
    for (int i = 0; i < totalAuthorizedRFID; i++) {
        if (authorizedRFID[i].equalsIgnoreCase(uid)) {
            return true;
        }
    }
    return false;
}

// Function to check for a fingerprint match
bool getFingerprintID() {
    Serial.println("👉 Place your finger on the scanner...");

    // Try up to 5 times before failing
    for (int attempt = 0; attempt < 5; attempt++) {  
        uint8_t result = finger.getImage();
        
        if (result == FINGERPRINT_OK) {
            Serial.println("✅ Fingerprint detected, processing...");
            result = finger.image2Tz();
            if (result == FINGERPRINT_OK) {
                result = finger.fingerFastSearch();
                if (result == FINGERPRINT_OK) {
                    Serial.print("✅ Fingerprint Matched! ID: ");
                    Serial.println(finger.fingerID);
                    return true; // Successful fingerprint match
                }
            }
        }
        delay(3000); // Wait before retrying
    }

    Serial.println("❌ Fingerprint not recognized.");
    return false;
}
