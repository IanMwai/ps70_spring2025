// Include required libraries for the OLED display and NeoPixel LEDs
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// OLED display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin definitions
#define NUM_PIXELS_LEFT 7
#define NUM_PIXELS_RIGHT 8
#define LEFT_PIN   18             // Left NeoPixel strip data pin
#define RIGHT_PIN  19             // Right NeoPixel strip data pin
#define BUTTON_LEFT 26            // Left blinker button
#define BUTTON_RIGHT 25           // Right blinker button
#define RESET_BUTTON 33           // Reset trip button
#define LDR_PIN    34             // Light sensor (LDR) pin

// Create NeoPixel objects
Adafruit_NeoPixel stripLeft(NUM_PIXELS_LEFT, LEFT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripRight(NUM_PIXELS_RIGHT, RIGHT_PIN, NEO_GRB + NEO_KHZ800);

// Speed sensor config
#define FRONT_PIN 35              // Front speed sensor analog pin
#define BACK_PIN 32               // Back speed sensor analog pin
#define WHEEL_RADIUS 0.058        // In meters
#define CIRCUMFERENCE (2 * PI * WHEEL_RADIUS)
#define THRESHOLD_DROP 200        // Drop in analog value to detect magnet
#define MIN_INTERVAL 300          // ms between magnet detections
#define INACTIVE_TIMEOUT 2000     // ms to consider ride stopped

// State variables for lights
bool leftOn = false, rightOn = false, warningOn = false;
bool lastLeftState = HIGH, lastRightState = HIGH;
unsigned long blinkTimer = 0;
bool blinkState = false;
int brightness = 50;
unsigned long lightTimer = 0;
unsigned long blinkInterval = 500;  // Blink speed (ms)

// Reset button state tracking
unsigned long resetPressTime = 0;
const unsigned long RESET_HOLD_TIME = 2000;
bool resetMessageShowing = false;
unsigned long resetMessageTime = 0;

// Struct to hold wheel sensor info
struct Wheel {
  int pin;
  int baseline;
  unsigned long lastPulseTime;
  float speed;
};

// Create wheel sensor objects
Wheel front = {FRONT_PIN, 0, 0, 0};
Wheel back = {BACK_PIN, 0, 0, 0};

// Ride tracking variables
bool rideActive = false;
unsigned long lastActiveTime = 0;
float avgSpeed = 0;
float maxSpeed = 0;
float totalDistance = 0;

void setup() {
  // Configure input pins
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  // Initialize NeoPixels
  stripLeft.begin();
  stripRight.begin();
  updateBrightness();  // Set brightness based on LDR

  Serial.begin(115200);  // For debug output

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Display failed"));
    while (1);  // Halt if display init fails
  }

  // Calibrate light sensors for speed tracking
  calibrateSensor(&front);
  calibrateSensor(&back);

  // Show splash message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("System Ready");
  display.display();
  delay(1000);
}

void loop() {
  unsigned long now = millis();

  updateBrightness();
  handleLightButtons();        // Check and handle blinker buttons
  handleResetButton(now);      // Handle long-press reset button

  checkSensor(&front, now);    // Detect speed from front wheel
  checkSensor(&back, now);     // Detect speed from back wheel
  updateSpeedMetrics(now);     // Compute average speed and update status

  updateLights();              // Control LED blinking behavior

  // Refresh OLED display periodically
  static unsigned long lastDisplayUpdate = 0;
  if (now - lastDisplayUpdate >= 200) {
    lastDisplayUpdate = now;
    updateDisplay();
  }

  // Recalibrate sensors every 30 seconds
  static unsigned long lastCalibration = 0;
  if (now - lastCalibration > 30000) {
    calibrateSensor(&front);
    calibrateSensor(&back);
    lastCalibration = now;
  }

  // Hide reset message after 2 seconds
  if (resetMessageShowing && (now - resetMessageTime > 2000)) {
    resetMessageShowing = false;
  }
}

// Reads ambient light from LDR and sets NeoPixel brightness accordingly
void updateBrightness() {
  int ldrValue = analogRead(LDR_PIN);
  brightness = map(ldrValue, 0, 4095, 255, 20);  // Darker = brighter
  brightness = constrain(brightness, 20, 255);
  stripLeft.setBrightness(brightness);
  stripRight.setBrightness(brightness);
}

// Handles left, right, and simultaneous button presses
void handleLightButtons() {
  bool leftState = digitalRead(BUTTON_LEFT);
  bool rightState = digitalRead(BUTTON_RIGHT);
  static bool lastBothPressed = false;
  bool bothPressed = (leftState == LOW && rightState == LOW);

  if (bothPressed && !lastBothPressed) {
    delay(50);
    if (digitalRead(BUTTON_LEFT) == LOW && digitalRead(BUTTON_RIGHT) == LOW) {
      // Toggle safety mode
      warningOn = !warningOn;
      leftOn = rightOn = false;
      blinkInterval = warningOn ? 200 : 500;
      updateDisplay();
    }
  }

  // Left button pressed alone
  if (!bothPressed && leftState == LOW && lastLeftState == HIGH) {
    delay(50);
    if (digitalRead(BUTTON_LEFT) == LOW) {
      leftOn = !leftOn;
      rightOn = warningOn = false;
      blinkInterval = 500;
      updateDisplay();
    }
  }

  // Right button pressed alone
  if (!bothPressed && rightState == LOW && lastRightState == HIGH) {
    delay(50);
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      rightOn = !rightOn;
      leftOn = warningOn = false;
      blinkInterval = 500;
      updateDisplay();
    }
  }

  // Save previous state
  lastBothPressed = bothPressed;
  lastLeftState = leftState;
  lastRightState = rightState;
}

// Detects long press on reset button and resets trip data
void handleResetButton(unsigned long now) {
  static bool resetButtonActive = false;

  if (digitalRead(RESET_BUTTON) == LOW) {
    if (!resetButtonActive) {
      resetButtonActive = true;
      resetPressTime = now;
    } else if (now - resetPressTime > RESET_HOLD_TIME) {
      totalDistance = 0;
      maxSpeed = 0;
      resetMessageShowing = true;
      resetMessageTime = now;
      Serial.println("Trip reset.");
      delay(500);
    }
  } else {
    resetButtonActive = false;
  }
}

// Controls the blinking animation of the LED strips
void updateLights() {
  if (millis() - blinkTimer >= blinkInterval) {
    blinkTimer = millis();
    blinkState = !blinkState;

    if (leftOn) {
      setStripColor(stripLeft, blinkState ? stripLeft.Color(255, 0, 0) : 0, NUM_PIXELS_LEFT);
      setStripColor(stripRight, 0, NUM_PIXELS_RIGHT);
    } else if (rightOn) {
      setStripColor(stripRight, blinkState ? stripRight.Color(255, 0, 0) : 0, NUM_PIXELS_RIGHT);
      setStripColor(stripLeft, 0, NUM_PIXELS_LEFT);
    } else if (warningOn) {
      setStripColor(stripLeft, blinkState ? stripLeft.Color(255, 0, 0) : 0, NUM_PIXELS_LEFT);
      setStripColor(stripRight, blinkState ? stripRight.Color(255, 0, 0) : 0, NUM_PIXELS_RIGHT);
    } else {
      setStripColor(stripLeft, 0, NUM_PIXELS_LEFT);
      setStripColor(stripRight, 0, NUM_PIXELS_RIGHT);
    }
  }
}

// Utility to set a given color across a NeoPixel strip
void setStripColor(Adafruit_NeoPixel &strip, uint32_t color, int pixelCount) {
  for (int i = 0; i < pixelCount; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// Monitors wheel sensor to detect magnet pass (indicating rotation)
void checkSensor(Wheel* wheel, unsigned long now) {
  int val = analogRead(wheel->pin);
  if (wheel->baseline - val > THRESHOLD_DROP) {
    if (now - wheel->lastPulseTime > MIN_INTERVAL) {
      if (wheel->lastPulseTime > 0) {
        wheel->speed = CIRCUMFERENCE / ((now - wheel->lastPulseTime) / 1000.0);
        totalDistance += CIRCUMFERENCE;
      }
      wheel->lastPulseTime = now;
    }
  }

  // Reset speed if no pulse for over 1.5s
  if (now - wheel->lastPulseTime > 1500) {
    wheel->speed = 0;
  }
}

// Calculates average speed, updates max speed, and tracks ride activity
void updateSpeedMetrics(unsigned long now) {
  int sources = 0;
  float sum = 0;
  if (front.speed > 0) { sum += front.speed; sources++; }
  if (back.speed > 0)  { sum += back.speed;  sources++; }
  avgSpeed = (sources > 0) ? (sum / sources) : 0;

  if (avgSpeed > maxSpeed) {
    maxSpeed = avgSpeed;
  }

  rideActive = (avgSpeed > 0);
  if (rideActive) lastActiveTime = now;

  // If idle for too long, mark as stopped
  if (now - lastActiveTime > INACTIVE_TIMEOUT) {
    front.speed = 0;
    back.speed = 0;
    avgSpeed = 0;
    rideActive = false;
  }
}

// Simple calibration for analog baseline of wheel sensor
void calibrateSensor(Wheel* wheel) {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(wheel->pin);
    delay(10);
  }
  wheel->baseline = sum / 10;
}

// Draws current status info to the OLED display
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (resetMessageShowing) {
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.println("TRIP RESET");
    display.setTextSize(1);
    display.display();
    return;
  }

  display.setCursor(0, 0);
  display.print("Ride: ");
  display.println(rideActive ? "Active" : "Stopped");

  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(avgSpeed, 1);
  display.println(" m/s");

  display.setTextSize(1);
  display.setCursor(0, 38);
  if (warningOn) {
    display.print("Safety Lights");
  } else if (leftOn) {
    display.print("Left Blinker");
  } else if (rightOn) {
    display.print("Right Blinker");
  } else {
    display.print("Lights Off");
  }

  display.setCursor(0, 52);
  display.print("D:");
  display.print(totalDistance / 1000.0, 2);
  display.print(" km Max:");
  display.print(maxSpeed, 1);
  display.print(" m/s");
  display.display();
}