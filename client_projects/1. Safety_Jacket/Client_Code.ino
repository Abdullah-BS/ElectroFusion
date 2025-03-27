#include <SoftwareSerial.h>

SoftwareSerial A9G(10, 11); // RX, TX

String incomingData = "";

const String phoneNumber = "+966506578868"; // Replace with the recipient's phone number

void setup() {
  Serial.begin(9600);
  A9G.begin(9600);

  Serial.println("Initializing A9G Module...");

  // Give time for the module to initialize
  delay(5000);

  // Check if the module responds
  A9G.println("AT");
  if (waitForResponse("OK", 2000)) {
    Serial.println("A9G Module is ready.");
  } else {
    Serial.println("Failed to communicate with A9G Module.");
  }

  // Set SMS text mode
  A9G.println("AT+CMGF=1");
  waitForResponse("OK", 1000);

  // Configure GPS
  A9G.println("AT+GPS=1"); // Turn on GPS
  waitForResponse("OK", 1000);
}

void loop() {
  // Check for user input
  if (Serial.available()) {
    char command = Serial.read();

    switch (command) {
      case 's':
        sendSMS("Hello");
        break;
      case 'c':
        makeCall();
        break;
      case 'p':
        printCoordinates();
        break;
      case 'd':
        sendCoordinatesSMS();
        break;
      default:
        Serial.println("Unknown command. Use 's', 'c', 'p', or 'd'.");
    }
  }

  // Read data from A9G Module
  if (A9G.available()) {
    char c = A9G.read();
    incomingData += c;
    if (c == '\n') {
      Serial.print("A9G: ");
      Serial.print(incomingData);
      incomingData = "";
    }
  }
}

void sendSMS(String message) {
  Serial.println("Sending SMS...");

  A9G.println("AT+CMGF=1"); // Ensure SMS text mode is enabled
  waitForResponse("OK", 1000);

  A9G.print("AT+CMGS=\"");
  A9G.print(phoneNumber);
  A9G.println("\"");
  waitForResponse(">", 5000);

  A9G.print(message);
  A9G.write(26); // Ctrl+Z ASCII code to send the message

  if (waitForResponse("OK", 10000)) {
    Serial.println("SMS sent successfully.");
  } else {
    Serial.println("Failed to send SMS.");
  }
}

void makeCall() {
  Serial.println("Making a call...");

  A9G.print("ATD");
  A9G.print(phoneNumber);
  A9G.println(";"); // Semicolon initiates a voice call

  if (waitForResponse("OK", 1000)) {
    Serial.println("Call initiated. Waiting 20 seconds before hanging up...");
    delay(20000); // Wait for 20 seconds
    A9G.println("ATH"); // Hang up the call
    Serial.println("Call ended.");
  } else {
    Serial.println("Failed to make a call.");
  }
}

void printCoordinates() {
  Serial.println("Retrieving GPS coordinates...");

  A9G.println("AT+GPSRD=1"); // Request GPS data every 1 second
  delay(2000); // Wait for data to accumulate

  // Parse GPS data from NMEA sentences
  String nmeaData = readNMEAData();
  if (nmeaData != "") {
    parseAndPrintCoordinates(nmeaData);
  } else {
    Serial.println("Failed to retrieve GPS data.");
  }

  A9G.println("AT+GPSRD=0"); // Stop GPS data output
}

void sendCoordinatesSMS() {
  Serial.println("Sending GPS coordinates via SMS...");

  A9G.println("AT+GPSRD=1"); // Request GPS data every 1 second
  delay(2000); // Wait for data to accumulate

  String nmeaData = readNMEAData();
  String coordinates = "";

  if (nmeaData != "") {
    coordinates = parseCoordinates(nmeaData);
  } else {
    Serial.println("Failed to retrieve GPS data.");
    return;
  }

  A9G.println("AT+GPSRD=0"); // Stop GPS data output

  if (coordinates != "") {
    A9G.println("AT+CMGF=1"); // Ensure SMS text mode is enabled
    waitForResponse("OK", 1000);

    A9G.print("AT+CMGS=\"");
    A9G.print(phoneNumber);
    A9G.println("\"");
    waitForResponse(">", 5000);

    A9G.print("Current Coordinates: ");
    A9G.print(coordinates);
    A9G.write(26); // Ctrl+Z ASCII code to send the message

    if (waitForResponse("OK", 10000)) {
      Serial.println("Coordinates sent via SMS successfully.");
    } else {
      Serial.println("Failed to send coordinates via SMS.");
    }
  } else {
    Serial.println("Failed to parse coordinates.");
  }
}
