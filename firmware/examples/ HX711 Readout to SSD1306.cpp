#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "HX711.h"

// ---------------- Pins ----------------

constexpr int HX711_DT_PIN  = 4;
constexpr int HX711_SCK_PIN = 5;

constexpr int OLED_SDA_PIN = 21;
constexpr int OLED_SCL_PIN = 22;

constexpr int MODE_BUTTON_PIN = 18;

// ---------------- OLED ----------------

constexpr int SCREEN_WIDTH  = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS = 0x3C;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// ---------------- Load cell ----------------

HX711 scale;

// Replace this after calibration.
// This example assumes counts per pound-force.
constexpr float CALIBRATION_FACTOR = -5423.781f;

float forceLbf = 0.0f;
float forceN = 0.0f;
float peakN = 0.0f;

// ---------------- Display mode ----------------

enum DisplayMode {
  NUMERIC_MODE,
  GRAPH_MODE
};

DisplayMode displayMode = NUMERIC_MODE;

// ---------------- Button debounce ----------------

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastButtonChangeMs = 0;
constexpr unsigned long DEBOUNCE_MS = 40;

// ---------------- Graph data ----------------

constexpr int GRAPH_LEFT   = 0;
constexpr int GRAPH_TOP    = 12;
constexpr int GRAPH_WIDTH  = 128;
constexpr int GRAPH_HEIGHT = 52;

float graphData[GRAPH_WIDTH] = {0};

void updateButton() {
  bool reading = digitalRead(MODE_BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastButtonChangeMs = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastButtonChangeMs >= DEBOUNCE_MS) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      // Button was just pressed.
      if (stableButtonState == LOW) {
        displayMode =
          (displayMode == NUMERIC_MODE)
            ? GRAPH_MODE
            : NUMERIC_MODE;
      }
    }
  }
}

void addGraphPoint(float value) {
  // Shift all points one pixel to the left.
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    graphData[i] = graphData[i + 1];
  }

  graphData[GRAPH_WIDTH - 1] = value;
}

void drawNumericView() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TENSILE TESTER");

  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(forceN, 2);
  display.println(" N");

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(forceLbf, 2);
  display.println(" lbf");

  display.setCursor(0, 54);
  display.print("Peak: ");
  display.print(peakN, 2);
  display.print(" N");

  display.display();
}

void drawGraphView() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Force: ");
  display.print(forceN, 1);
  display.print(" N");

  // Determine graph scale from the visible data.
  float minimum = graphData[0];
  float maximum = graphData[0];

  for (int i = 1; i < GRAPH_WIDTH; i++) {
    if (graphData[i] < minimum) minimum = graphData[i];
    if (graphData[i] > maximum) maximum = graphData[i];
  }

  // Always include zero in the graph range.
  if (minimum > 0.0f) minimum = 0.0f;
  if (maximum < 0.0f) maximum = 0.0f;

  // Prevent division by zero when the signal is flat.
  if (maximum - minimum < 1.0f) {
    maximum = minimum + 1.0f;
  }

  // Draw horizontal zero line, when zero is visible.
  int zeroY = GRAPH_TOP + GRAPH_HEIGHT - 1 -
    static_cast<int>(
      ((0.0f - minimum) / (maximum - minimum))
      * (GRAPH_HEIGHT - 1)
    );

  if (zeroY >= GRAPH_TOP &&
      zeroY < GRAPH_TOP + GRAPH_HEIGHT) {
    display.drawFastHLine(
      GRAPH_LEFT,
      zeroY,
      GRAPH_WIDTH,
      SSD1306_WHITE
    );
  }

  // Draw force trace.
  for (int x = 1; x < GRAPH_WIDTH; x++) {
    int previousY = GRAPH_TOP + GRAPH_HEIGHT - 1 -
      static_cast<int>(
        ((graphData[x - 1] - minimum) /
         (maximum - minimum))
        * (GRAPH_HEIGHT - 1)
      );

    int currentY = GRAPH_TOP + GRAPH_HEIGHT - 1 -
      static_cast<int>(
        ((graphData[x] - minimum) /
         (maximum - minimum))
        * (GRAPH_HEIGHT - 1)
      );

    previousY = constrain(
      previousY,
      GRAPH_TOP,
      GRAPH_TOP + GRAPH_HEIGHT - 1
    );

    currentY = constrain(
      currentY,
      GRAPH_TOP,
      GRAPH_TOP + GRAPH_HEIGHT - 1
    );

    display.drawLine(
      x - 1,
      previousY,
      x,
      currentY,
      SSD1306_WHITE
    );
  }

  display.display();
}

void setup() {
  Serial.begin(115200);

  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);

  while (!scale.wait_ready_timeout(1000)) {
    // Wait silently for HX711.
  }

  scale.set_scale(CALIBRATION_FACTOR);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Remove all load");
  display.println("Taring...");
  display.display();

  delay(2000);
  scale.tare(30);
}

void loop() {
  updateButton();

  if (scale.is_ready()) {
    forceLbf = scale.get_units(3);
    forceN = forceLbf * 4.44822f;

    if (forceN > peakN) {
      peakN = forceN;
    }

    addGraphPoint(forceN);

    if (displayMode == NUMERIC_MODE) {
      drawNumericView();
    } else {
      drawGraphView();
    }
  }
}