#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "HX711.h"

// =====================================================
// Pins
// =====================================================

constexpr int HX711_DT_PIN = 21;
constexpr int HX711_SCK_PIN = 22;

constexpr int OLED_SDA_PIN = 32;
constexpr int OLED_SCL_PIN = 33;

// J6 pin 4 -> SELECT -> GPIO14
constexpr int SELECT_BUTTON_PIN = 14;

constexpr int STATUS_LED_PIN = 4;

// =====================================================
// Demo timing
// =====================================================

constexpr unsigned long AUTO_START_DELAY_MS = 5000;
constexpr unsigned long BREAK_SCREEN_DELAY_MS = 5000;

unsigned long menuStartMs = 0;
unsigned long breakDetectedMs = 0;

// =====================================================
// Dog animation timing
// =====================================================

unsigned long lastDogFrameMs = 0;
constexpr unsigned long DOG_FRAME_INTERVAL_MS = 250;

bool dogFrame = false;

// =====================================================
// Status LED pulse
// =====================================================

int statusBrightness = 5;
int statusFadeAmount = 2;

unsigned long lastStatusFadeMs = 0;

constexpr unsigned long STATUS_FADE_INTERVAL_MS = 15;

// =====================================================
// OLED
// =====================================================

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;

constexpr uint8_t OLED_ADDRESS = 0x3C;

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1);

// =====================================================
// HX711
// =====================================================

HX711 scale;

// TEMPORARY demo calibration.
// Replace this after real load-cell calibration.
constexpr float DEMO_COUNTS_PER_NEWTON = 500.0f;

// Demo break threshold in either direction.
constexpr float BREAK_THRESHOLD_N = 50.0f;

// Ignore tiny unloaded fluctuations around zero.
constexpr float ZERO_DEADBAND_N = 0.15f;

// =====================================================
// General timing
// =====================================================

unsigned long lastSerialPrintMs = 0;

constexpr unsigned long SERIAL_PRINT_INTERVAL_MS = 100;

unsigned long lastDisplayUpdateMs = 0;

constexpr unsigned long DISPLAY_UPDATE_INTERVAL_MS = 100;

// =====================================================
// Machine state
// =====================================================

enum MachineState
{
  MENU,
  TEST_RUNNING,
  BREAK_DETECTED
};

MachineState machineState = MENU;

// =====================================================
// Test display mode
// =====================================================

enum TestDisplayMode
{
  GAUGE_VIEW,
  GRAPH_VIEW
};

TestDisplayMode testDisplayMode = GAUGE_VIEW;

// =====================================================
// Force data
// =====================================================

long zeroOffset = 0;

float forceN = 0.0f;
float peakForceN = 0.0f;
float breakForceN = 0.0f;

// =====================================================
// Graph history
// =====================================================

// 120 samples x 100 ms = about 12 seconds on screen.
constexpr int GRAPH_HISTORY_SIZE = 120;

float forceHistory[GRAPH_HISTORY_SIZE];

// =====================================================
// Graph smoothing
// =====================================================

// 5 samples x 100 ms = about 0.5 sec moving average.
constexpr int GRAPH_SMOOTHING_SAMPLES = 5;

float smoothingBuffer[GRAPH_SMOOTHING_SAMPLES] = {0};

int smoothingIndex = 0;
int smoothingCount = 0;

// =====================================================
// Button debounce
// =====================================================

bool previousRawButton = HIGH;
bool stableButtonState = HIGH;

unsigned long lastButtonChangeMs = 0;

constexpr unsigned long DEBOUNCE_MS = 40;

// =====================================================
// Status LED
// =====================================================

void statusLedOn()
{
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void statusLedOff()
{
  digitalWrite(STATUS_LED_PIN, LOW);
}

void statusLedBlink(int times, int delayMs)
{
  for (int i = 0; i < times; i++)
  {
    statusLedOn();
    delay(delayMs);

    statusLedOff();
    delay(delayMs);
  }
}

void resetStatusPulse()
{
  statusBrightness = 5;
  statusFadeAmount = 2;

  lastStatusFadeMs = millis();

  analogWrite(
      STATUS_LED_PIN,
      statusBrightness);
}

void updateStatusPulse()
{
  if (millis() - lastStatusFadeMs <
      STATUS_FADE_INTERVAL_MS)
  {
    return;
  }

  lastStatusFadeMs = millis();

  statusBrightness += statusFadeAmount;

  if (statusBrightness >= 100)
  {
    statusBrightness = 100;
    statusFadeAmount = -2;
  }
  else if (statusBrightness <= 5)
  {
    statusBrightness = 5;
    statusFadeAmount = 2;
  }

  analogWrite(
      STATUS_LED_PIN,
      statusBrightness);
}

// =====================================================
// Button handling
// =====================================================

bool selectPressed()
{
  const bool rawButton =
      digitalRead(SELECT_BUTTON_PIN);

  if (rawButton != previousRawButton)
  {
    previousRawButton = rawButton;
    lastButtonChangeMs = millis();
  }

  if (millis() - lastButtonChangeMs >=
      DEBOUNCE_MS)
  {
    if (rawButton != stableButtonState)
    {
      stableButtonState = rawButton;

      if (stableButtonState == LOW)
      {
        return true;
      }
    }
  }

  return false;
}

// =====================================================
// Graph history
// =====================================================

void clearForceHistory()
{
  for (int i = 0;
       i < GRAPH_HISTORY_SIZE;
       i++)
  {
    forceHistory[i] = 0.0f;
  }
}

void addForceHistory(float value)
{
  // Shift everything left one pixel/sample.
  for (int i = 0;
       i < GRAPH_HISTORY_SIZE - 1;
       i++)
  {
    forceHistory[i] =
        forceHistory[i + 1];
  }

  // Newest sample enters from the right.
  forceHistory[GRAPH_HISTORY_SIZE - 1] =
      value;
}

// =====================================================
// Graph smoothing
// =====================================================

float smoothGraphForce(float newValue)
{
  smoothingBuffer[smoothingIndex] =
      newValue;

  smoothingIndex =
      (smoothingIndex + 1) %
      GRAPH_SMOOTHING_SAMPLES;

  if (smoothingCount <
      GRAPH_SMOOTHING_SAMPLES)
  {
    smoothingCount++;
  }

  float total = 0.0f;

  for (int i = 0;
       i < smoothingCount;
       i++)
  {
    total += smoothingBuffer[i];
  }

  return total / smoothingCount;
}

void resetGraphSmoothing()
{
  for (int i = 0;
       i < GRAPH_SMOOTHING_SAMPLES;
       i++)
  {
    smoothingBuffer[i] = 0.0f;
  }

  smoothingIndex = 0;
  smoothingCount = 0;
}

// =====================================================
// Dancing dog
// =====================================================

void drawDog(int x, int y, bool frame)
{
  // ===================================================
  // Side-profile dog
  // ===================================================

  // Body
  display.fillRect(
      x + 5,
      y + 9,
      13,
      7,
      SSD1306_WHITE);

  // Chest / neck
  display.fillRect(
      x + 16,
      y + 6,
      4,
      8,
      SSD1306_WHITE);

  // Head
  display.fillRect(
      x + 18,
      y + 3,
      7,
      7,
      SSD1306_WHITE);

  // Muzzle
  display.fillRect(
      x + 24,
      y + 6,
      4,
      3,
      SSD1306_WHITE);

  // Nose
  display.drawPixel(
      x + 28,
      y + 7,
      SSD1306_WHITE);

  // Eye
  display.drawPixel(
      x + 22,
      y + 5,
      SSD1306_BLACK);

  // Floppy ear
  display.fillRect(
      x + 18,
      y + 1,
      3,
      5,
      SSD1306_WHITE);

  // Belly cutout
  display.drawFastHLine(
      x + 8,
      y + 15,
      7,
      SSD1306_BLACK);

  // ===================================================
  // Dancing legs
  // ===================================================

  if (frame)
  {
    // Front leg forward
    display.drawLine(
        x + 17,
        y + 15,
        x + 20,
        y + 21,
        SSD1306_WHITE);

    display.drawFastHLine(
        x + 20,
        y + 21,
        3,
        SSD1306_WHITE);

    // Rear leg backward
    display.drawLine(
        x + 8,
        y + 15,
        x + 4,
        y + 20,
        SSD1306_WHITE);

    display.drawFastHLine(
        x + 2,
        y + 20,
        3,
        SSD1306_WHITE);
  }
  else
  {
    // Front leg backward
    display.drawLine(
        x + 17,
        y + 15,
        x + 14,
        y + 21,
        SSD1306_WHITE);

    display.drawFastHLine(
        x + 12,
        y + 21,
        3,
        SSD1306_WHITE);

    // Rear leg forward
    display.drawLine(
        x + 8,
        y + 15,
        x + 11,
        y + 21,
        SSD1306_WHITE);

    display.drawFastHLine(
        x + 11,
        y + 21,
        3,
        SSD1306_WHITE);
  }

  // ===================================================
  // Wagging tail
  // ===================================================

  if (frame)
  {
    display.drawLine(
        x + 5,
        y + 10,
        x + 1,
        y + 6,
        SSD1306_WHITE);

    display.drawLine(
        x + 1,
        y + 6,
        x,
        y + 3,
        SSD1306_WHITE);
  }
  else
  {
    display.drawLine(
        x + 5,
        y + 10,
        x + 1,
        y + 11,
        SSD1306_WHITE);

    display.drawLine(
        x + 1,
        y + 11,
        x,
        y + 14,
        SSD1306_WHITE);
  }
}

// =====================================================
// Menu screen
// =====================================================

void drawMenuScreen()
{
  display.clearDisplay();

  display.setTextColor(
      SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 1);
  display.print("MACK-10");

  display.drawFastHLine(
      0,
      11,
      SCREEN_WIDTH,
      SSD1306_WHITE);

  display.setCursor(4, 19);
  display.print("> START TEST");

  display.setCursor(4, 33);
  display.print("Press SELECT");

  // Countdown
  unsigned long elapsed =
      millis() - menuStartMs;

  int remainingSeconds =
      5 -
      static_cast<int>(
          elapsed / 1000);

  if (remainingSeconds < 0)
  {
    remainingSeconds = 0;
  }

  display.setCursor(4, 48);

  display.print("Auto in ");
  display.print(remainingSeconds);
  display.print("s");

  // Animated dog
  drawDog(
      97,
      18,
      dogFrame);

  display.display();
}

void updateMenuAnimation()
{
  if (millis() - lastDogFrameMs >=
      DOG_FRAME_INTERVAL_MS)
  {
    lastDogFrameMs = millis();

    dogFrame = !dogFrame;

    drawMenuScreen();
  }
}

// =====================================================
// Gauge view
// =====================================================

void drawLoadBar(float currentForce)
{
  constexpr int BAR_X = 4;
  constexpr int BAR_Y = 43;

  constexpr int BAR_WIDTH = 120;
  constexpr int BAR_HEIGHT = 14;

  display.drawRect(
      BAR_X,
      BAR_Y,
      BAR_WIDTH,
      BAR_HEIGHT,
      SSD1306_WHITE);

  // Use magnitude for bar length.
  float fraction =
      fabs(currentForce) /
      BREAK_THRESHOLD_N;

  fraction =
      constrain(
          fraction,
          0.0f,
          1.0f);

  const int fillWidth =
      static_cast<int>(
          fraction *
          (BAR_WIDTH - 4));

  if (fillWidth > 0)
  {
    display.fillRect(
        BAR_X + 2,
        BAR_Y + 2,
        fillWidth,
        BAR_HEIGHT - 4,
        SSD1306_WHITE);
  }
}

void drawGaugeView()
{
  display.clearDisplay();

  display.setTextColor(
      SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("TEST RUNNING");

  display.setCursor(88, 0);
  display.print("+/-50N");

  display.setTextSize(2);

  display.setCursor(0, 13);

  display.print(
      forceN,
      1);

  display.print(" N");

  display.setTextSize(1);

  display.setCursor(80, 20);
  display.print("Peak");

  display.setCursor(80, 30);

  display.print(
      peakForceN,
      1);

  display.print("N");

  drawLoadBar(forceN);

  display.display();
}

// =====================================================
// Graph view
// =====================================================

void drawGraphView()
{
  display.clearDisplay();

  display.setTextColor(
      SSD1306_WHITE);

  display.setTextSize(1);

  // ===================================================
  // Header
  // ===================================================

  display.setCursor(0, 0);
  display.print("LOAD");

  display.setCursor(31, 0);

  display.print(
      forceN,
      1);

  display.print("N");

  display.setCursor(82, 0);

  display.print("P:");

  display.print(
      peakForceN,
      1);

  // ===================================================
  // Graph geometry
  // ===================================================

  constexpr int GRAPH_X = 7;
  constexpr int GRAPH_Y = 13;

  constexpr int GRAPH_WIDTH = 120;
  constexpr int GRAPH_HEIGHT = 48;

  constexpr int GRAPH_TOP =
      GRAPH_Y;

  constexpr int GRAPH_BOTTOM =
      GRAPH_Y +
      GRAPH_HEIGHT -
      1;

  // Zero is always centered.
  constexpr int ZERO_Y =
      GRAPH_Y +
      GRAPH_HEIGHT / 2;

  constexpr int HALF_GRAPH_HEIGHT =
      (GRAPH_HEIGHT / 2) - 2;

  // Y axis.
  display.drawFastVLine(
      GRAPH_X,
      GRAPH_Y,
      GRAPH_HEIGHT,
      SSD1306_WHITE);

  // Dotted zero line.
  for (int x = GRAPH_X;
       x < SCREEN_WIDTH;
       x += 4)
  {
    display.drawFastHLine(
        x,
        ZERO_Y,
        2,
        SSD1306_WHITE);
  }

  // Small zero marker.
  display.setCursor(0, ZERO_Y - 3);
  display.print("0");

  // ===================================================
  // Symmetric auto scaling
  // ===================================================

  // Minimum +/-5 N range keeps small noise reasonable.
  float graphMagnitude = 5.0f;

  for (int i = 0;
       i < GRAPH_HISTORY_SIZE;
       i++)
  {
    const float magnitude =
        fabs(forceHistory[i]);

    if (magnitude >
        graphMagnitude)
    {
      graphMagnitude =
          magnitude;
    }
  }

  // Add 20% headroom in both directions.
  graphMagnitude *= 1.20f;

  // ===================================================
  // Draw scrolling trace
  // ===================================================

  for (int i = 1;
       i < GRAPH_HISTORY_SIZE;
       i++)
  {
    const float previousForce =
        forceHistory[i - 1];

    const float currentForce =
        forceHistory[i];

    // Positive goes upward.
    // Negative goes downward.
    int y1 =
        ZERO_Y -
        static_cast<int>(
            (previousForce /
             graphMagnitude) *
            HALF_GRAPH_HEIGHT);

    int y2 =
        ZERO_Y -
        static_cast<int>(
            (currentForce /
             graphMagnitude) *
            HALF_GRAPH_HEIGHT);

    y1 =
        constrain(
            y1,
            GRAPH_TOP,
            GRAPH_BOTTOM);

    y2 =
        constrain(
            y2,
            GRAPH_TOP,
            GRAPH_BOTTOM);

    const int x1 =
        GRAPH_X +
        i -
        1;

    const int x2 =
        GRAPH_X +
        i;

    // Primary line
    display.drawLine(
        x1,
        y1,
        x2,
        y2,
        SSD1306_WHITE);

    // Thicker second pixel.
    if (y1 < ZERO_Y &&
        y2 < ZERO_Y)
    {
      if (y1 - 1 >= GRAPH_TOP &&
          y2 - 1 >= GRAPH_TOP)
      {
        display.drawLine(
            x1,
            y1 - 1,
            x2,
            y2 - 1,
            SSD1306_WHITE);
      }
    }
    else if (y1 > ZERO_Y &&
             y2 > ZERO_Y)
    {
      if (y1 + 1 <= GRAPH_BOTTOM &&
          y2 + 1 <= GRAPH_BOTTOM)
      {
        display.drawLine(
            x1,
            y1 + 1,
            x2,
            y2 + 1,
            SSD1306_WHITE);
      }
    }
    else
    {
      if (y1 + 1 <= GRAPH_BOTTOM &&
          y2 + 1 <= GRAPH_BOTTOM)
      {
        display.drawLine(
            x1,
            y1 + 1,
            x2,
            y2 + 1,
            SSD1306_WHITE);
      }
    }
  }

  // Newest-sample cursor.
  display.drawFastVLine(
      126,
      ZERO_Y - 2,
      5,
      SSD1306_WHITE);

  display.display();
}

// =====================================================
// Test view dispatcher
// =====================================================

void drawCurrentTestView()
{
  if (testDisplayMode ==
      GAUGE_VIEW)
  {
    drawGaugeView();
  }
  else
  {
    drawGraphView();
  }
}

// =====================================================
// Break screen
// =====================================================

void drawBreakScreen()
{
  display.clearDisplay();

  display.setTextColor(
      SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(20, 0);
  display.println("BREAK DETECTED");

  display.drawFastHLine(
      0,
      10,
      SCREEN_WIDTH,
      SSD1306_WHITE);

  display.setTextSize(2);

  display.setCursor(10, 17);

  display.print(
      breakForceN,
      1);

  display.println(" N");

  display.setTextSize(1);

  display.setCursor(19, 40);
  display.println("Peak force");

  display.setCursor(2, 54);
  display.println("SELECT or auto 5s");

  display.display();
}

// =====================================================
// Taring screen
// =====================================================

void drawTaringScreen()
{
  display.clearDisplay();

  display.setTextColor(
      SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(20, 20);
  display.println("REMOVE ALL LOAD");

  display.setCursor(43, 38);
  display.println("TARING");

  display.display();
}

// =====================================================
// Menu control
// =====================================================

void enterMenu()
{
  machineState = MENU;

  menuStartMs = millis();

  lastDogFrameMs = millis();

  dogFrame = false;

  statusLedOff();

  drawMenuScreen();

  Serial.println();
  Serial.println("START TEST screen.");
  Serial.println(
      "Press SELECT or wait 5 seconds.");
}

// =====================================================
// Start test
// =====================================================

void startNewTest()
{
  Serial.println();
  Serial.println("Starting new test...");
  Serial.println("Taring load cell...");

  machineState =
      TEST_RUNNING;

  testDisplayMode =
      GAUGE_VIEW;

  clearForceHistory();
  resetGraphSmoothing();

  statusLedOff();

  drawTaringScreen();

  statusLedBlink(
      2,
      150);

  delay(300);

  zeroOffset =
      scale.read_average(20);

  forceN = 0.0f;
  peakForceN = 0.0f;
  breakForceN = 0.0f;

  resetStatusPulse();

  lastDisplayUpdateMs =
      millis();

  lastSerialPrintMs =
      millis();

  drawCurrentTestView();

  Serial.println("Test ready.");
}

// =====================================================
// Break detection
// =====================================================

void triggerBreak()
{
  breakForceN =
      peakForceN;

  machineState =
      BREAK_DETECTED;

  breakDetectedMs =
      millis();

  statusLedOff();

  Serial.println();

  Serial.print(
      "Break detected at ");

  Serial.print(
      breakForceN,
      2);

  Serial.println(" N");

  Serial.println(
      "Press SELECT to restart immediately.");

  drawBreakScreen();
}

// =====================================================
// Setup
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(300);

  // ---------------------------------------------------
  // Status LED
  // ---------------------------------------------------

  pinMode(
      STATUS_LED_PIN,
      OUTPUT);

  statusLedOff();

  statusLedBlink(
      3,
      150);

  Serial.println(
      "MACK-10 starting...");

  // ---------------------------------------------------
  // SELECT button
  // ---------------------------------------------------

  pinMode(
      SELECT_BUTTON_PIN,
      INPUT_PULLUP);

  previousRawButton =
      digitalRead(
          SELECT_BUTTON_PIN);

  stableButtonState =
      previousRawButton;

  lastButtonChangeMs =
      millis();

  // ---------------------------------------------------
  // OLED
  // ---------------------------------------------------

  Wire.begin(
      OLED_SDA_PIN,
      OLED_SCL_PIN);

  Wire.setClock(
      100000);

  if (!display.begin(
          SSD1306_SWITCHCAPVCC,
          OLED_ADDRESS))
  {
    Serial.println(
        "OLED initialization failed.");

    while (true)
    {
      statusLedOn();
      delay(300);

      statusLedOff();
      delay(300);
    }
  }

  display.ssd1306_command(
      SSD1306_SETCONTRAST);

  display.ssd1306_command(
      0xFF);

  // ---------------------------------------------------
  // Startup splash
  // ---------------------------------------------------

  display.clearDisplay();

  display.setTextColor(
      SSD1306_WHITE);

  display.setTextSize(1);

  display.setCursor(
      16,
      20);

  display.println(
      "TENSILE TESTER");

  display.setCursor(
      34,
      38);

  display.println(
      "STARTING");

  display.display();

  // ---------------------------------------------------
  // HX711
  // ---------------------------------------------------

  scale.begin(
      HX711_DT_PIN,
      HX711_SCK_PIN);

  while (!scale.wait_ready_timeout(
      1000))
  {
    Serial.println(
        "Waiting for HX711...");

    statusLedOn();
    delay(100);

    statusLedOff();
    delay(100);
  }

  Serial.println(
      "HX711 detected.");

  enterMenu();
}

// =====================================================
// Main loop
// =====================================================

void loop()
{
  // ===================================================
  // SELECT input
  // ===================================================

  if (selectPressed())
  {
    Serial.println(
        "SELECT pressed.");

    // Menu -> start immediately.
    if (machineState == MENU)
    {
      startNewTest();
      return;
    }

    // Running -> toggle gauge / graph.
    if (machineState ==
        TEST_RUNNING)
    {
      if (testDisplayMode ==
          GAUGE_VIEW)
      {
        testDisplayMode =
            GRAPH_VIEW;

        Serial.println(
            "Display: GRAPH");
      }
      else
      {
        testDisplayMode =
            GAUGE_VIEW;

        Serial.println(
            "Display: GAUGE");
      }

      drawCurrentTestView();

      return;
    }

    // Break -> restart immediately.
    if (machineState ==
        BREAK_DETECTED)
    {
      Serial.println(
          "Manual restart.");

      startNewTest();

      return;
    }
  }

  // ===================================================
  // Menu
  // ===================================================

  if (machineState == MENU)
  {
    statusLedOff();

    updateMenuAnimation();

    if (millis() -
            menuStartMs >=
        AUTO_START_DELAY_MS)
    {
      Serial.println(
          "Auto-start timeout.");

      startNewTest();
    }

    return;
  }

  // ===================================================
  // Break screen
  // ===================================================

  if (machineState ==
      BREAK_DETECTED)
  {
    statusLedOff();

    if (millis() -
            breakDetectedMs >=
        BREAK_SCREEN_DELAY_MS)
    {
      enterMenu();
    }

    return;
  }

  // ===================================================
  // Active test
  // ===================================================

  updateStatusPulse();

  if (!scale.is_ready())
  {
    return;
  }

  const long rawReading =
      scale.read();

  const long relativeCounts =
      rawReading -
      zeroOffset;

  // Signed force.
  forceN =
      static_cast<float>(
          relativeCounts) /
      DEMO_COUNTS_PER_NEWTON;

  // ---------------------------------------------------
  // Deadband
  // ---------------------------------------------------

  if (fabs(forceN) <
      ZERO_DEADBAND_N)
  {
    forceN = 0.0f;
  }

  // ---------------------------------------------------
  // Signed peak
  // ---------------------------------------------------

  if (fabs(forceN) >
      fabs(peakForceN))
  {
    peakForceN =
        forceN;
  }

  // ---------------------------------------------------
  // Serial
  // ---------------------------------------------------

  if (millis() -
          lastSerialPrintMs >=
      SERIAL_PRINT_INTERVAL_MS)
  {
    lastSerialPrintMs =
        millis();

    Serial.print("Force:");

    Serial.print(
        forceN,
        2);

    Serial.print(",Peak:");

    Serial.println(
        peakForceN,
        2);
  }

  // ---------------------------------------------------
  // Break detection in either direction
  // ---------------------------------------------------

  if (fabs(forceN) >=
      BREAK_THRESHOLD_N)
  {
    triggerBreak();

    return;
  }

  // ===================================================
  // Graph sampling + OLED update
  // ===================================================

  if (millis() -
          lastDisplayUpdateMs >=
      DISPLAY_UPDATE_INTERVAL_MS)
  {
    lastDisplayUpdateMs =
        millis();

    // Smoothing applies only to graph trace.
    const float smoothedForce =
        smoothGraphForce(
            forceN);

    // Always shift graph history.
    addForceHistory(
        smoothedForce);

    drawCurrentTestView();
  }
}