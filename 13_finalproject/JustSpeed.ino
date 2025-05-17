#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Analog sensor pins
#define FRONT_PIN 35
#define BACK_PIN 32

// Reset button
#define RESET_BUTTON_PIN 33

// Detection config
#define THRESHOLD_DROP 200
#define MIN_INTERVAL 300           // ms debounce
#define INACTIVE_TIMEOUT 2000     // ms to detect stop

#define WHEEL_RADIUS 0.058
#define CIRCUMFERENCE (2 * PI * WHEEL_RADIUS)  // ~0.364m

struct Wheel {
  int pin;
  int baseline;
  unsigned long lastPulseTime;
  float speed;
};

Wheel front = {FRONT_PIN, 0, 0, 0};
Wheel back = {BACK_PIN, 0, 0, 0};

bool rideActive = false;
unsigned long lastActiveTime = 0;
float avgSpeed = 0;
float maxSpeed = 0;
float totalDistance = 0;  // meters

void setup() {
  Serial.begin(115200);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
    while (1);
  }

  calibrateSensor(&front);
  calibrateSensor(&back);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Bicycle Speed Tracker");
  display.display();
  delay(1000);
}

void loop() {
  unsigned long now = millis();

  // Handle reset button
  static bool lastResetState = HIGH;
  bool currentResetState = digitalRead(RESET_BUTTON_PIN);

  if (lastResetState == HIGH && currentResetState == LOW) {
    // Button just pressed
    totalDistance = 0;
    maxSpeed = 0;
    Serial.println("Trip reset");
  }
  lastResetState = currentResetState;

  // Check sensors
  checkSensor(&front, now);
  checkSensor(&back, now);

  // Average speed logic
  int sources = 0;
  float sum = 0;
  if (front.speed > 0) { sum += front.speed; sources++; }
  if (back.speed > 0)  { sum += back.speed;  sources++; }
  avgSpeed = (sources > 0) ? (sum / sources) : 0;

  if (avgSpeed > maxSpeed) {
    maxSpeed = avgSpeed;
  }

  // Ride active logic
  rideActive = (avgSpeed > 0);
  if (rideActive) lastActiveTime = now;

  if (now - lastActiveTime > INACTIVE_TIMEOUT) {
    front.speed = 0;
    back.speed = 0;
    avgSpeed = 0;
    rideActive = false;
  }

  // Refresh display
  static unsigned long lastDisplay = 0;
  if (now - lastDisplay > 200) {
    lastDisplay = now;
    updateDisplay();
  }

  // Recalibrate every 30 seconds
  static unsigned long lastCal = 0;
  if (now - lastCal > 30000) {
    calibrateSensor(&front);
    calibrateSensor(&back);
    lastCal = now;
  }
}

void checkSensor(Wheel* wheel, unsigned long now) {
  int val = analogRead(wheel->pin);
  if (wheel->baseline - val > THRESHOLD_DROP) {
    if (now - wheel->lastPulseTime > MIN_INTERVAL) {
      if (wheel->lastPulseTime > 0) {
        float speed = CIRCUMFERENCE / ((now - wheel->lastPulseTime) / 1000.0);
        wheel->speed = speed;
        totalDistance += CIRCUMFERENCE;
      }
      wheel->lastPulseTime = now;
    }
  }

  if (now - wheel->lastPulseTime > 1500) {
    wheel->speed = 0;
  }
}

void calibrateSensor(Wheel* wheel) {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(wheel->pin);
    delay(10);
  }
  wheel->baseline = sum / 10;
}

void updateDisplay() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("RIDE: ");
  display.println(rideActive ? "ACTIVE" : "STOPPED");

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print("Speed: ");
  display.print(avgSpeed, 1);
  display.println(" m/s");

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("Dist: ");
  display.print(totalDistance / 1000.0, 2);  // km
  display.print(" km");

  display.setCursor(80, 50);
  display.print("Max: ");
  display.print(maxSpeed, 1);

  display.display();
}