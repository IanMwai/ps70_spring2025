#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Firebase credentials
#define FIREBASE_HOST "week-9-a4ee0-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY "AIzaSyDrExaIlGlpGHFJ3jJMCcBgWRuFb2GJjUI"
#define WIFI_SSID "MAKERSPACE"
#define WIFI_PASSWORD "12345678"

// Firebase objects
FirebaseData fbdo;
FirebaseData statusFbdo; // for writing status updates separately
FirebaseAuth auth;
FirebaseConfig config;

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Pin definitions
#define RED_LED_PIN 4
#define GREEN_LED_PIN 18
#define BUZZER_PIN 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 55
#define OLED_RESET -1

// OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// System variables
const String correctCode = "1234";
String receivedCode = "";
int wrongAttempts = 0;
unsigned long lastActivityTime = 0;

bool signupOK = false;
bool stateChanging = false;
unsigned long stateStartTime = 0;
const unsigned long stateDuration = 7000;        // display duration
const unsigned long inactivityTimeout = 10000;   // reset after inactivity

// System state enum
enum State {
  STATE_DEFAULT,
  STATE_GRANTED,
  STATE_DENIED
};
State currentState = STATE_DEFAULT;

void setup() {
  Serial.begin(115200);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Ensure all indicators are off at startup
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // OLED init
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true);
  }

  // Connect WiFi
  displayMessage("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  displayMessage("WiFi Connected");
  delay(1000);

  // Firebase setup
  config.api_key = FIREBASE_API_KEY;
  config.database_url = "https://" + String(FIREBASE_HOST);
  displayMessage("Connecting Firebase...");

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signup ok");
    signupOK = true;
  } else {
    Serial.printf("Firebase signup failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (signupOK) {
    displayMessage("Firebase Connected");
    Firebase.RTDB.setString(&fbdo, "/password", "");
    Firebase.RTDB.setString(&statusFbdo, "/status", "");
  } else {
    displayMessage("Firebase Failed!");
  }

  resetToDefaultStage();
  lastActivityTime = millis();
}

void loop() {
  unsigned long now = millis();

  // Reset to default state after timeout (for both granted and denied)
  if (stateChanging && now - stateStartTime >= stateDuration) {
    resetToDefaultStage();
    Firebase.RTDB.setString(&statusFbdo, "/status", "");
  }

  // Auto-reset after user is idle
  if (now - lastActivityTime >= inactivityTimeout) {
    resetToDefaultStage();
    wrongAttempts = 0;
    lastActivityTime = now;
    Firebase.RTDB.setString(&statusFbdo, "/status", "");
  }

  // Stop buzzer after short delay
  if (currentState == STATE_DENIED && now - stateStartTime >= 1500) {
    noTone(BUZZER_PIN);
  }

  // Process password
  if (signupOK && Firebase.ready()) {
    if (Firebase.RTDB.getString(&fbdo, "/password")) {
      receivedCode = fbdo.stringData();

      if (receivedCode.length() > 0) {
        lastActivityTime = millis();
        Firebase.RTDB.setString(&fbdo, "/password", "");
        Serial.print("Received: ");
        Serial.println(receivedCode);

        // Check correctness and update state
        if (receivedCode == correctCode) {
          Firebase.RTDB.setString(&statusFbdo, "/status", "granted");
          accessGranted();
          wrongAttempts = 0;
        } else {
          Firebase.RTDB.setString(&statusFbdo, "/status", "denied");
          wrongAttempts++;
          displayMessage("Wrong Code! Attempt " + String(wrongAttempts) + "/3");
          accessDenied();
        }

        // Log attempt
        Firebase.RTDB.pushString(&fbdo, "/log", "Attempt: " + receivedCode +
                                " Result: " + (receivedCode == correctCode ? "GRANTED" : "DENIED") +
                                " at " + String(millis()));
      }
    }
  }
}

void accessGranted() {
  Serial.println("Access Granted");
  digitalWrite(RED_LED_PIN, LOW);
  noTone(BUZZER_PIN);
  digitalWrite(GREEN_LED_PIN, HIGH);
  displayMessage("Access Granted");

  currentState = STATE_GRANTED;
  stateChanging = true;
  stateStartTime = millis();
}

void accessDenied() {
  Serial.println("Access Denied");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  tone(BUZZER_PIN, 2000);

  currentState = STATE_DENIED;
  stateChanging = true;
  stateStartTime = millis();
}

void resetToDefaultStage() {
  Serial.println("Reset to Default Stage");
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  noTone(BUZZER_PIN);
  displayMessage("Enter Access Code");

  currentState = STATE_DEFAULT;
  stateChanging = false;
}

void displayMessage(String msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(msg);
  display.display();
}