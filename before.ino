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

  // Stop WiFi and network connections
  WiFi.disconnect();
  WiFi.forceSleepBegin();
  delay(200);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi Disabled");

  // Stop any HTTP or TCP connections
  HTTPClient http;
  WiFiClient client;
  http.end();
  client.stop();

  // Stop Audio Processing
  if (out) {
    out->stop();
    delete out;
    out = nullptr;
  }

  // Small delay to ensure everything stops
  delay(300);
  Serial.println("All tasks terminated.");
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

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    yield();
  }
  Serial.println("\nConnected to WiFi");

  String response = "";  // Store response here

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

      http.setTimeout(5000);  // 5 seconds timeout
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
      client.stop();  // Free up memory
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

  // **Speak the response**
  if (response.length() > 0) {
    Serial.println("Speaking response...");
    ESP8266SAM* sam = new ESP8266SAM;
    sam->Say(out, response.c_str());
  }

  // **Stop Audio to prevent interference**
  out->stop();
  delete out;
  out = nullptr;

  // **Go to deep sleep**
  Serial.println("Going to deep sleep...");
  ESP.deepSleep(0);

  delay(100);
}

void loop() {}
