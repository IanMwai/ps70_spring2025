#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define NUM_PIXELS_LEFT 7
#define NUM_PIXELS_RIGHT 8
#define LEFT_PIN   18
#define RIGHT_PIN  19
#define BUTTON_LEFT 26
#define BUTTON_RIGHT 25
#define LDR_PIN    34

Adafruit_NeoPixel stripLeft(NUM_PIXELS_LEFT, LEFT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripRight(NUM_PIXELS_RIGHT, RIGHT_PIN, NEO_GRB + NEO_KHZ800);

bool leftOn = false, rightOn = false, warningOn = false;
bool lastLeftState = HIGH, lastRightState = HIGH;
unsigned long blinkTimer = 0;
bool blinkState = false;
int brightness = 50; // Default brightness (will be overwritten by LDR)

void setup() {
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  stripLeft.begin();
  stripRight.begin();
  stripLeft.setBrightness(brightness);
  stripRight.setBrightness(brightness);
  stripLeft.show();
  stripRight.show();

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  // Read LDR and adjust brightness (0-100 dark to light)
  int ldrValue = analogRead(LDR_PIN);
  brightness = map(ldrValue, 0, 4095, 255, 20); // Adjust these values based on your LDR readings
  brightness = constrain(brightness, 20, 255); // Keep within safe bounds
  
  stripLeft.setBrightness(brightness);
  stripRight.setBrightness(brightness);

  bool leftState = digitalRead(BUTTON_LEFT);
  bool rightState = digitalRead(BUTTON_RIGHT);

  // Left button pressed (with debounce)
  if (leftState == LOW && lastLeftState == HIGH) {
    delay(50); // simple debounce
    if (digitalRead(BUTTON_LEFT) == HIGH) return;
    
    leftOn = !leftOn;
    rightOn = false;
    warningOn = false;
    updateDisplay();
  }

  // Right button pressed (with debounce)
  if (rightState == LOW && lastRightState == HIGH) {
    delay(50); // simple debounce
    if (digitalRead(BUTTON_RIGHT)) return; // if button bounced back
    
    rightOn = !rightOn;
    leftOn = false;
    warningOn = false;
    updateDisplay();
  }

  // Both buttons pressed simultaneously
  if (leftState == LOW && rightState == LOW) {
    delay(50); // simple debounce
    if (digitalRead(BUTTON_LEFT) || digitalRead(BUTTON_RIGHT)) return; // if buttons bounced back
    
    warningOn = true;
    leftOn = false;
    rightOn = false;
    updateDisplay();
  }

  lastLeftState = leftState;
  lastRightState = rightState;

  // Blinking logic
  if (millis() - blinkTimer >= 500) {
    blinkTimer = millis();
    blinkState = !blinkState;

    if (leftOn) {
      blink(stripLeft, blinkState, NUM_PIXELS_LEFT);
      stripRight.clear();
      stripRight.show();
    } else if (rightOn) {
      blink(stripRight, blinkState, NUM_PIXELS_RIGHT);
      stripLeft.clear();
      stripLeft.show();
    } else if (warningOn) {
      blink(stripLeft, blinkState, NUM_PIXELS_LEFT);
      blink(stripRight, blinkState, NUM_PIXELS_RIGHT);
      stripLeft.show();
      stripRight.show();
    } else {
    display.setCursor(35, 25);
    display.print("LIGHTS OFF");
  }
  }
}

void blink(Adafruit_NeoPixel &strip, bool state, int pixelCount) {
  uint32_t color = state ? strip.Color(255, 0, 0) : 0; // Red when on
  for (int i = 0; i < pixelCount; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);  // Smaller text
  display.setTextColor(SSD1306_WHITE);

  if (warningOn) {
    display.setCursor(20, 20);
    display.print("WARNING LIGHTS");
    display.setCursor(35, 35);
    display.print("ACTIVE");
  } else if (leftOn) {
    display.setCursor(40, 20);
    display.print("LEFT");
    display.setCursor(30, 35);
    display.print("BLINKER ON");
  } else if (rightOn) {
    display.setCursor(40, 20);
    display.print("RIGHT");
    display.setCursor(30, 35);
    display.print("BLINKER ON");
  }

  display.display();
}