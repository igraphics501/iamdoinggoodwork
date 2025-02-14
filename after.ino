#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266SAM.h>
#include "AudioOutputI2SNoDAC.h"

#define BUTTON_PIN 14  // GPIO pin for the button (D4 on NodeMCU)

AudioOutputI2SNoDAC* out;

const char* ssid = "Beast 4G Router";
const char* password = "3377775149";
const char* serverURL = "http://192.168.29.201:3000/api/process";

volatile int buttonPressCount = 0;
bool buttonPressed = false;

unsigned long lastPressTime = 0;
const unsigned long debounceDelay = 50;

void IRAM_ATTR handleButtonPress() {
  if (millis() - lastPressTime > debounceDelay) {
    buttonPressCount++;
    buttonPressed = true;
    lastPressTime = millis();
  }
}

void terminateTasks() {
  Serial.println("Terminating all tasks...");

  WiFi.disconnect();
  WiFi.forceSleepBegin();
  delay(200);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi Disabled");

  HTTPClient http;
  WiFiClient client;
  http.end();
  client.stop();

  if (out) {
    out->stop();
    delete out;
    out = nullptr;
  }

  delay(300);
  Serial.println("All tasks terminated.");
}

void speakSegments(String response) {
  ESP8266SAM* sam = new ESP8266SAM;

  // **Split response at '>' and store in an array**
  const int maxSegments = 10; // Limit to avoid overflow
  String segments[maxSegments];
  int segmentCount = 0;

  while (response.length() > 0 && segmentCount < maxSegments) {
    int index = response.indexOf('>');
    if (index != -1) {
      segments[segmentCount] = response.substring(0, index);
      response = response.substring(index + 1);
    } else {
      segments[segmentCount] = response;
      response = "";
    }
    segmentCount++;
  }

  // **Speak each segment one by one**
  for (int i = 0; i < segmentCount; i++) {
    Serial.print("Speaking: ");
    Serial.println(segments[i]);
    sam->Say(out, segments[i].c_str());
    delay(500); // Small pause between phrases
  }

  delete sam;
}

void setup() {
  Serial.begin(9600);
  Serial.println("\nStarting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

  Serial.println("Waiting for button presses...");
  while (buttonPressCount == 0) {
    yield();
  }

  unsigned long idleStart = millis();
  unsigned long idleTimeout = 2000;
  while (millis() - idleStart < idleTimeout) {
    if (buttonPressed) {
      buttonPressed = false;
      idleStart = millis();
      Serial.println("Button pressed!");
    }
    yield();
  }

  Serial.print("Total button presses: ");
  Serial.println(buttonPressCount);

  // **Connect to WiFi**
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    yield();
  }
  Serial.println("\nConnected to WiFi");

  String response = "";  

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, serverURL)) {
      http.addHeader("Content-Type", "application/json");
      String payload = "{\"count\": " + String(buttonPressCount) + "}";
      Serial.print("Sending to: ");
      Serial.println(serverURL);
      Serial.print("Payload: ");
      Serial.println(payload);

      http.setTimeout(5000);  
      int httpResponseCode = http.POST(payload);

      if (httpResponseCode > 0) {
        response = http.getString();
        Serial.print("Server response: ");
        Serial.println(response);
      } else {
        Serial.print("Error sending data: ");
        Serial.println(httpResponseCode);
      }
      http.end();
      client.stop();
    } else {
      Serial.println("Failed to connect to server.");
    }
  }

  // **Terminate all tasks before initializing ESP8266SAM**
  terminateTasks();

  // **Reinitialize ESP8266SAM**
  Serial.println("Reinitializing ESP8266SAM...");
  out = new AudioOutputI2SNoDAC();
  out->SetOutputModeMono(true);
  out->begin();

  // **Speak the response in parts**
  if (response.length() > 0) {
    Serial.println("Speaking response...");
    speakSegments(response);
  }

  // **Stop Audio**
  out->stop();
  delete out;
  out = nullptr;

  // **Go to deep sleep**
  Serial.println("Going to deep sleep...");
  ESP.deepSleep(0);

  delay(100);
}

void loop() {}
