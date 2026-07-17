#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "HX711.h"

// =====================================================
// Pins
// =====================================================

constexpr int HX711_DT_PIN = 4;
constexpr int HX711_SCK_PIN = 5;

constexpr int OLED_SDA_PIN = 21;
constexpr int OLED_SCL_PIN = 22;

constexpr int BUTTON_PIN = 18;

// =====================================================
// OLED
// =====================================================

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================================================
// HX711
// =====================================================

HX711 scale;

// Temporary demo conversion.
//
// Example:
// 1000 HX711 counts = approximately 1 displayed newton.
//
// This is NOT a real calibration. Adjust it if the displayed
// force rises too quickly or too slowly when you press the cell.
constexpr float DEMO_COUNTS_PER_NEWTON = 500.0f;

// Force at which the demo declares that the specimen broke.
constexpr float BREAK_THRESHOLD_N = 20.0f;

// Ignore tiny unloaded fluctuations.
constexpr float ZERO_DEADBAND_N = 0.15f;

unsigned long lastSerialPrintMs = 0;
constexpr unsigned long SERIAL_PRINT_INTERVAL_MS = 100;

// =====================================================
// Test state
// =====================================================

enum TestState { TEST_RUNNING, BREAK_DETECTED };

TestState testState = TEST_RUNNING;

long zeroOffset = 0;

float forceN = 0.0f;
float peakForceN = 0.0f;
float breakForceN = 0.0f;

// =====================================================
// Button debounce
// =====================================================

bool previousRawButton = HIGH;
bool stableButtonState = HIGH;

unsigned long lastButtonChangeMs = 0;
constexpr unsigned long DEBOUNCE_MS = 40;

// Returns true once for each completed button press.
bool buttonPressed() {
  bool rawButton = digitalRead(BUTTON_PIN);

  if (rawButton != previousRawButton) {
    previousRawButton = rawButton;
    lastButtonChangeMs = millis();
  }

  if (millis() - lastButtonChangeMs >= DEBOUNCE_MS) {
    if (rawButton != stableButtonState) {
      stableButtonState = rawButton;

      if (stableButtonState == LOW) {
        return true;
      }
    }
  }

  return false;
}

// =====================================================
// Display functions
// =====================================================

void drawLoadBar(float currentForce) {
  constexpr int BAR_X = 4;
  constexpr int BAR_Y = 43;
  constexpr int BAR_WIDTH = 120;
  constexpr int BAR_HEIGHT = 14;

  display.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, SSD1306_WHITE);

  float fraction = currentForce / BREAK_THRESHOLD_N;
  fraction = constrain(fraction, 0.0f, 1.0f);

  int fillWidth = static_cast<int>(fraction * (BAR_WIDTH - 4));

  if (fillWidth > 0) {
    display.fillRect(BAR_X + 2, BAR_Y + 2, fillWidth, BAR_HEIGHT - 4,
                     SSD1306_WHITE);
  }
}

void drawRunningScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("TEST RUNNING");

  display.setCursor(88, 0);
  display.print("20N");

  display.setTextSize(2);
  display.setCursor(0, 13);
  display.print(forceN, 1);
  display.print(" N");

  display.setTextSize(1);
  display.setCursor(80, 20);
  display.print("Peak");
  display.setCursor(80, 30);
  display.print(peakForceN, 1);
  display.print("N");

  drawLoadBar(forceN);

  display.display();
}

void drawBreakScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("BREAK DETECTED");

  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(10, 17);
  display.print(breakForceN, 1);
  display.println(" N");

  display.setTextSize(1);
  display.setCursor(19, 40);
  display.println("Peak force");

  display.setCursor(11, 54);
  display.println("Press to restart");

  display.display();
}

void drawTaringScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("REMOVE ALL LOAD");

  display.setCursor(43, 38);
  display.println("TARING");

  display.display();
}

// =====================================================
// Test control
// =====================================================

void startNewTest() {
  drawTaringScreen();

  delay(750);

  zeroOffset = scale.read_average(20);

  forceN = 0.0f;
  peakForceN = 0.0f;
  breakForceN = 0.0f;

  testState = TEST_RUNNING;
}

void triggerBreak() {
  breakForceN = peakForceN;
  testState = BREAK_DETECTED;

  drawBreakScreen();
}

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("MACK-10 starting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(16, 20);
  display.println("TENSILE TESTER");

  display.setCursor(34, 38);
  display.println("STARTING");
  display.display();

  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);

  while (!scale.wait_ready_timeout(1000)) {
    // Wait silently for the HX711.
  }

  startNewTest();
}

// =====================================================
// Main loop
// =====================================================

void loop() {
  bool pressed = buttonPressed();

  // After a simulated break, one button press starts over.
  if (testState == BREAK_DETECTED) {
    if (pressed) {
      startNewTest();
    }

    return;
  }

  if (!scale.is_ready()) {
    return;
  }

  long rawReading = scale.read();
  long relativeCounts = rawReading - zeroOffset;

  // Absolute value allows either compression or tension
  // to trigger the demo.
  forceN = abs(static_cast<float>(relativeCounts)) / DEMO_COUNTS_PER_NEWTON;

  if (forceN < ZERO_DEADBAND_N) {
    forceN = 0.0f;
  }

  if (forceN > peakForceN) {
    peakForceN = forceN;
  }

  if (millis() - lastSerialPrintMs >= SERIAL_PRINT_INTERVAL_MS) {
    lastSerialPrintMs = millis();

    Serial.print("Force:");
    Serial.print(forceN, 2);
    Serial.print(",Peak:");
    Serial.println(peakForceN, 2);
  }

  if (forceN >= BREAK_THRESHOLD_N) {
    triggerBreak();
    return;
  }

  drawRunningScreen();
}