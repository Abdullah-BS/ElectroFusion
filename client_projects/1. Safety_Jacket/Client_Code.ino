// Include necessary libraries
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ultrasonic.h>  // Include the Ultrasonic library

// **** Pin Definitions ****

// Sensors and Actuators
const int waterSensorPin = 2;    // Water sensor connected to digital pin 2
const int trigPin = 3;           // Ultrasonic sensor TRIG pin connected to digital pin 3
const int echoPin = 4;           // Ultrasonic sensor ECHO pin connected to digital pin 4
const int activeBuzzerPin = 5;   // Active buzzer connected to digital pin 5
const int passiveBuzzerPin = 6;  // Passive buzzer connected to digital pin 6
const int pushButtonPin = 7;     // Push button connected to digital pin 7
const int tempSensorPin = 8;     // DS18B20 data pin connected to digital pin 8
const int A9GTXPin = 9;          // Arduino's TX pin (connects to A9G's RXD)
const int A9GRXPin = 10;         // Arduino's RX pin (connects to A9G's TXD)
const int thirdBuzzerPin = 11;   // Third passive buzzer connected to digital pin 11
const int ledPin = 13;           // LED connected to digital pin 13

// **** Global Variables ****

// Function 1 Variables
bool waterDetected = false;
unsigned long waterDetectionTime = 0;
const unsigned long operationDuration = 180000;  // 3 minutes in milliseconds

// Obstacle Detection Variables
int distance;
unsigned long previousBeepTime = 0;
int beepInterval = 0;
bool activeBuzzerState = false;

// Passive Buzzer (Animal Deterrent) Variables
unsigned long passiveBuzzerPreviousTime = 0;
const int passiveBuzzerOnTime = 500;   // Time the buzzer is ON in milliseconds
const int passiveBuzzerOffTime = 100;  // Time the buzzer is OFF in milliseconds
bool passiveBuzzerState = false;

// Function 2 Variables
bool function2Active = false;
unsigned long buttonPressTime = 0;
bool buttonPressed = false;

// Third Buzzer (Siren) Variables
unsigned long sirenPreviousTime = 0;
int sirenFrequency = 1000;
bool sirenIncreasing = true;

// GPS and Temperature Variables
double currentLatitude = 0.0;
double currentLongitude = 0.0;
bool gpsFix = false;

// Preconfigured Coordinates (used when GPS fix is not available)
const double predefinedLatitude = 21.486099633614717;
const double predefinedLongitude = 39.24415539952562;

// SMS Sending Variables
bool smsSent = false;
unsigned long smsStartTime = 0;

// **** Initialize Sensors and Modules ****

// Temperature Sensor
OneWire oneWire(tempSensorPin);
DallasTemperature sensors(&oneWire);

// A9G GSM/GPRS/GPS Module
SoftwareSerial A9GSerial(A9GRXPin, A9GTXPin);  // RX, TX

// Ultrasonic Sensor
Ultrasonic ultrasonic(trigPin, echoPin);  // Initialize ultrasonic sensor with trig and echo pins

// **** Function Prototypes ****
void initializeA9GModule();
void function2Tasks();
void readA9GData();
void handleWaterDetection();
void handleFunction2();
void parseNMEA(String nmeaSentence);
String getField(String data, char separator, int index);
int getFieldIndex(String data, char separator, int field);
double convertToDecimalDegrees(String nmeaCoordinate);
float getTemperature();
bool sendSMS(String message);
bool sendATCommand(String command, String expectedResponse, unsigned long timeout);
bool waitForResponse(String target, unsigned long timeout);
void sendSMSAlert();
void blinkLED();
void activateThirdBuzzer();
void passiveBuzzerSoftSound();
int getDistance();
void obstacleAlert(int distance);
String getGPSData();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  A9GSerial.begin(9600);

  // Initialize pins
  pinMode(waterSensorPin, INPUT);
  pinMode(activeBuzzerPin, OUTPUT);
  pinMode(passiveBuzzerPin, OUTPUT);
  pinMode(pushButtonPin, INPUT_PULLUP);  // Use internal pull-up resistor
  pinMode(ledPin, OUTPUT);
  pinMode(thirdBuzzerPin, OUTPUT);

  // Start with buzzers and LED off
  digitalWrite(activeBuzzerPin, LOW);
  digitalWrite(passiveBuzzerPin, LOW);
  digitalWrite(ledPin, LOW);
  noTone(thirdBuzzerPin);

  // Initialize temperature sensor
  sensors.begin();

  // Initialize A9G module
  initializeA9GModule();
}

void loop() {
  // Handle water detection for Function 1
  handleWaterDetection();

  // Handle Function 2 activation and deactivation
  handleFunction2();

  // If Function 2 is active, perform its tasks
  if (function2Active) {
    function2Tasks();
  }

  // Read data from A9G Module (AT command responses and NMEA sentences)
  readA9GData();
}

/////////////////////////////
// Function 1: Water Detection and Obstacle Alert System
/////////////////////////////

void handleWaterDetection() {
  int waterSensorValue = digitalRead(waterSensorPin);

  if (waterSensorValue == LOW && !waterDetected) {
    // Water detected for the first time
    waterDetected = true;
    waterDetectionTime = millis();  // Record the time when water was detected
    Serial.println("Water detected! Activating system.");
  }

  if (waterDetected) {
    // Check if 3 minutes have passed since water was detected
    if (millis() - waterDetectionTime <= operationDuration) {
      // Continue operating the system
      // Activate passive buzzer (animal deterrent) with a very soft sound
      passiveBuzzerSoftSound();

      // Obstacle detection
      distance = getDistance();

      // Output distance to serial monitor (optional)
      Serial.print("Distance in CM: ");
      Serial.println(distance);

      // Control active buzzer based on distance
      obstacleAlert(distance);
    } else {
      // 3 minutes have passed, reset water detection
      waterDetected = false;
      Serial.println("3 minutes elapsed. Checking water sensor again.");

      // Deactivate buzzers
      digitalWrite(activeBuzzerPin, LOW);
      digitalWrite(passiveBuzzerPin, LOW);
    }
  } else {
    // No water detected, ensure system is inactive
    waterDetected = false;
    // Deactivate buzzers
    digitalWrite(activeBuzzerPin, LOW);
    digitalWrite(passiveBuzzerPin, LOW);
  }
}

// Function to get distance using Ultrasonic library
int getDistance() {
  // Read distance from ultrasonic sensor in centimeters
  int distanceCm = ultrasonic.read();

  // Check if a valid reading was obtained
  if (distanceCm == 0 || distanceCm > 400) {
    // Invalid reading, return a large distance
    return 999;
  }

  return distanceCm;
}

// Function to control active buzzer based on obstacle distance
void obstacleAlert(int distance) {
  unsigned long currentMillis = millis();

  // Set interval between beeps based on distance (in milliseconds)
  if (distance <= 10) {
    beepInterval = 200;  // Very close, fast beeping
  } else if (distance <= 20) {
    beepInterval = 400;
  } else if (distance <= 30) {
    beepInterval = 600;
  } else if (distance <= 50) {
    beepInterval = 800;
  } else {
    beepInterval = 0;  // No beeping if distance is greater than 50 cm
  }

  if (beepInterval > 0) {
    if (currentMillis - previousBeepTime >= beepInterval) {
      previousBeepTime = currentMillis;
      // Toggle buzzer state
      activeBuzzerState = !activeBuzzerState;
      digitalWrite(activeBuzzerPin, activeBuzzerState);
    }
  } else {
    // Ensure buzzer is off
    digitalWrite(activeBuzzerPin, LOW);
  }
}

// Function to create a soft sound with the passive buzzer using PWM
void passiveBuzzerSoftSound() {
  unsigned long currentMillis = millis();

  if (passiveBuzzerState) {
    // Buzzer is ON
    if (currentMillis - passiveBuzzerPreviousTime >= passiveBuzzerOnTime) {
      passiveBuzzerPreviousTime = currentMillis;
      passiveBuzzerState = false;
      //digitalWrite(passiveBuzzerPin, LOW);
      tone(passiveBuzzerPin, 30);
    }
  } else {
    // Buzzer is OFF
    if (currentMillis - passiveBuzzerPreviousTime >= passiveBuzzerOffTime) {
      passiveBuzzerPreviousTime = currentMillis;
      passiveBuzzerState = true;
      //digitalWrite(passiveBuzzerPin, HIGH);
      noTone(passiveBuzzerPin);
    }
  }
}

/////////////////////////////
// Function 2: Emergency Alert System
/////////////////////////////

void handleFunction2() {
  int buttonState = digitalRead(pushButtonPin);

  if (buttonState == LOW && !buttonPressed) {
    // Button pressed
    buttonPressed = true;
    buttonPressTime = millis();
  } else if (buttonState == HIGH && buttonPressed) {
    // Button released
    unsigned long pressDuration = millis() - buttonPressTime;
    buttonPressed = false;

    if (!function2Active) {
      // Activate Function 2 on any button press
      function2Active = true;
      smsSent = false;  // Reset SMS sent flag
      Serial.println("Function 2 activated.");
    } else if (pressDuration >= 3000) {
      // Deactivate Function 2 if button was pressed for 3 seconds
      function2Active = false;
      Serial.println("Function 2 deactivated.");

      // Turn off LED and third buzzer
      digitalWrite(ledPin, LOW);
      noTone(thirdBuzzerPin);
    }
  }
}

void function2Tasks() {
  // Blink LED continuously
  blinkLED();

  // Activate third buzzer to alert nearby people
  activateThirdBuzzer();

  // Send SMS with GPS coordinates and temperature (once when Function 2 is activated)
  if (!smsSent) {
    sendSMSAlert();
  }
}

// Function to blink the LED
void blinkLED() {
  static unsigned long previousLEDTime = 0;
  static bool ledState = false;
  unsigned long currentMillis = millis();

  if (currentMillis - previousLEDTime >= 500) {  // Blink every 500ms
    previousLEDTime = currentMillis;
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
  }
}

// Function to produce a siren-like sound with the third buzzer
void activateThirdBuzzer() {
  unsigned long currentMillis = millis();

  // Change frequency every 50ms
  if (currentMillis - sirenPreviousTime >= 50) {
    sirenPreviousTime = currentMillis;

    if (sirenIncreasing) {
      sirenFrequency += 50;
      if (sirenFrequency >= 2000) {
        sirenIncreasing = false;
      }
    } else {
      sirenFrequency -= 50;
      if (sirenFrequency <= 1000) {
        sirenIncreasing = true;
      }
    }

    tone(thirdBuzzerPin, sirenFrequency);
  }
}

/////////////////////////////
// A9G Module Communication
/////////////////////////////

void initializeA9GModule() {
  Serial.println("Initializing A9G Module...");
  unsigned long initStartTime = millis();

  // Non-blocking initialization
  bool initCompleted = false;

  while (!initCompleted && (millis() - initStartTime < 10000)) {
    if (sendATCommand("AT", "OK", 2000)) {
      Serial.println("A9G Module is ready.");
      initCompleted = true;
    } else {
      Serial.println("Failed to communicate with A9G Module. Retrying...");
    }
  }

  if (!initCompleted) {
    Serial.println("A9G Module initialization failed.");
  } else {
    // Set SMS text mode
    sendATCommand("AT+CMGF=1", "OK", 1000);

    // Turn on GPS
    sendATCommand("AT+GPS=1", "OK", 1000);

    // Start GPS data output over the AT command serial port
    sendATCommand("AT+GPSRD=1", "OK", 1000);
  }
}

void readA9GData() {
  while (A9GSerial.available()) {
    String line = A9GSerial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      if (line.startsWith("$")) {
        // It's an NMEA sentence
        parseNMEA(line);
      } else {
        // It's an AT command response or other info
        // Serial.println("A9G: " + line);
      }
    }
  }
}

void parseNMEA(String nmeaSentence) {
  // Check for GNGGA or GPGGA sentences
  if (nmeaSentence.startsWith("$GNGGA") || nmeaSentence.startsWith("$GPGGA")) {
    // Example NMEA sentence: $GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
    int fixIndex = getFieldIndex(nmeaSentence, ',', 6);
    char fixStatus = nmeaSentence.charAt(fixIndex);

    if (fixStatus != '0') {
      gpsFix = true;

      String latitudeStr = getField(nmeaSentence, ',', 2);
      String nsIndicator = getField(nmeaSentence, ',', 3);
      String longitudeStr = getField(nmeaSentence, ',', 4);
      String ewIndicator = getField(nmeaSentence, ',', 5);

      double lat = convertToDecimalDegrees(latitudeStr);
      if (nsIndicator == "S") lat = -lat;

      double lon = convertToDecimalDegrees(longitudeStr);
      if (ewIndicator == "W") lon = -lon;

      currentLatitude = lat;
      currentLongitude = lon;
    } else {
      gpsFix = false;
    }
  }
}

/////////////////////////////
// SMS Functions
/////////////////////////////

void sendSMSAlert() {
  static int smsState = 0;
  static unsigned long smsTimer = 0;

  switch (smsState) {
    case 0:
      Serial.println("Preparing to send SMS...");
      smsState = 1;
      smsTimer = millis();
      break;

    case 1:
      if (sendATCommand("AT+CMGF=1", "OK", 1000)) {
        smsState = 2;
      } else if (millis() - smsTimer > 5000) {
        Serial.println("Failed to set SMS text mode.");
        smsState = 0;
        smsSent = true;  // Prevent further attempts
      }
      break;

    case 2:
      A9GSerial.print("AT+CMGS=\"+966544848680\"\r");
      smsState = 3;
      smsTimer = millis();
      break;

    case 3:
      if (waitForResponse(">", 5000)) {
        smsState = 4;
      } else if (millis() - smsTimer > 5000) {
        Serial.println("No prompt after AT+CMGS. Failed to send SMS.");
        smsState = 0;
        smsSent = true;  // Prevent further attempts
      }
      break;

    case 4:
      {
        String gpsData = getGPSData();
        float temperature = getTemperature();

        String message = "HELP! SOS!\n";
        message += "Location: " + gpsData + "\n";
        message += "Water Temp: " + String(temperature, 1) + " C";

        Serial.println("Sending SMS: ");
        Serial.println(message);

        A9GSerial.print(message);
        A9GSerial.write(26);  // Ctrl+Z ASCII code to send the message
        smsState = 5;
        smsTimer = millis();
        break;
      }

    case 5:
      if (waitForResponse("OK", 10000)) {
        Serial.println("SMS sent successfully.");
        smsState = 0;
        smsSent = true;
      } else if (millis() - smsTimer > 15000) {
        Serial.println("Failed to send SMS.");
        smsState = 0;
        smsSent = true;  // Prevent further attempts
      }
      break;
  }
}

// Function to send AT commands to A9G module and wait for a response
bool sendATCommand(String command, String expectedResponse, unsigned long timeout) {
  A9GSerial.println(command);
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (A9GSerial.available()) {
      String response = A9GSerial.readStringUntil('\n');
      response.trim();
      if (response.indexOf(expectedResponse) != -1) {
        return true;
      }
    }
  }
  return false;
}

// Function to wait for a specific response from A9G module
bool waitForResponse(String target, unsigned long timeout) {
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (A9GSerial.available()) {
      String response = A9GSerial.readStringUntil('\n');
      response.trim();
      if (response.indexOf(target) != -1) {
        return true;
      }
    }
  }
  return false;
}

/////////////////////////////
// Helper Functions
/////////////////////////////

// Function to get GPS data or predefined location if GPS fix is unavailable
String getGPSData() {
  String latitudeStr;
  String longitudeStr;

  if (gpsFix) {
    latitudeStr = String(currentLatitude, 6);
    longitudeStr = String(currentLongitude, 6);
  } else {
    // Use predefined coordinates
    latitudeStr = String(predefinedLatitude, 6);
    longitudeStr = String(predefinedLongitude, 6);
  }

  String googleMapsLink = "https://maps.google.com/?q=" + latitudeStr + "," + longitudeStr;
  return googleMapsLink;
}

// Function to extract fields from NMEA sentences
String getField(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  if (found > index) {
    return data.substring(strIndex[0], strIndex[1]);
  } else {
    return "";
  }
}

// Function to get the index of a specific field in NMEA sentences
int getFieldIndex(String data, char separator, int field) {
  int found = 0;
  int index = -1;
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex; i++) {
    if (data.charAt(i) == separator) {
      found++;
      if (found == field) {
        index = i + 1;
        break;
      }
    }
  }

  return index;
}

// Function to convert NMEA coordinates to decimal degrees
double convertToDecimalDegrees(String nmeaCoordinate) {
  if (nmeaCoordinate == "") return 0;

  double coordinate = nmeaCoordinate.toFloat();
  int degrees = (int)(coordinate / 100);
  double minutes = coordinate - (degrees * 100);

  return degrees + (minutes / 60);
}

// Function to get temperature from DS18B20 sensor
float getTemperature() {
  sensors.requestTemperatures();  // Send command to get temperatures
  float tempC = sensors.getTempCByIndex(0);
  return tempC;
}
