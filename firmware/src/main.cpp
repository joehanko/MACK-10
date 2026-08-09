#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

// =====================================================
// MACK-10 SMOOTH MOTOR + OLED TEST
// ESP32 Arduino Core 2.x
// TMC2208 hardware STEP generation with LEDC
// =====================================================

// =====================================================
// Pins
// =====================================================

constexpr int ENABLE_PIN = 5;
constexpr int STEP_PIN = 18;
constexpr int DIR_PIN = 19;

constexpr int OLED_SDA_PIN = 32;
constexpr int OLED_SCL_PIN = 33;

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
// Motion settings
// =====================================================

constexpr int START_STEP_HZ = 250;
constexpr int MAX_STEP_HZ = 2000;

constexpr unsigned long ACCEL_TIME_MS = 2500;
constexpr unsigned long CRUISE_TIME_MS = 3000;
constexpr unsigned long DECEL_TIME_MS = 2500;

constexpr unsigned long DIRECTION_PAUSE_MS = 1500;

// OLED refresh
constexpr unsigned long DISPLAY_INTERVAL_MS = 100;

// Update commanded motor frequency every 20 ms
constexpr unsigned long RAMP_UPDATE_INTERVAL_MS = 20;

// =====================================================
// ESP32 LEDC hardware STEP generator
// =====================================================

constexpr int STEP_CHANNEL = 0;
constexpr int STEP_RESOLUTION_BITS = 8;

// =====================================================
// Motion state
// =====================================================

enum MotionPhase
{
    WAITING,
    ACCELERATING,
    CRUISING,
    DECELERATING,
    PAUSED,
    COMPLETE
};

MotionPhase motionPhase = WAITING;

bool currentDirection = HIGH;

int currentStepHz = 0;

unsigned long phaseStartMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lastRampUpdateMs = 0;

// =====================================================
// Phase text
// =====================================================

const char *phaseName(MotionPhase phase)
{
    switch (phase)
    {
    case WAITING:
        return "WAITING";

    case ACCELERATING:
        return "ACCELERATING";

    case CRUISING:
        return "FULL SPEED";

    case DECELERATING:
        return "DECELERATING";

    case PAUSED:
        return "PAUSED";

    case COMPLETE:
        return "COMPLETE";
    }

    return "";
}

// =====================================================
// Hardware STEP control
// =====================================================

void startStepGenerator(int frequencyHz)
{
    if (frequencyHz < 1)
    {
        frequencyHz = 1;
    }

    currentStepHz = frequencyHz;

    // Hardware-generated square wave.
    ledcWriteTone(
        STEP_CHANNEL,
        frequencyHz);
}

void changeStepFrequency(int frequencyHz)
{
    if (frequencyHz < 1)
    {
        frequencyHz = 1;
    }

    currentStepHz = frequencyHz;

    ledcWriteTone(
        STEP_CHANNEL,
        frequencyHz);
}

void stopStepGenerator()
{
    currentStepHz = 0;

    ledcWriteTone(
        STEP_CHANNEL,
        0);

    ledcWrite(
        STEP_CHANNEL,
        0);
}

// =====================================================
// Progress bar
// =====================================================

void drawProgressBar(float progress)
{
    constexpr int BAR_X = 4;
    constexpr int BAR_Y = 50;
    constexpr int BAR_W = 120;
    constexpr int BAR_H = 10;

    progress =
        constrain(
            progress,
            0.0f,
            1.0f);

    display.drawRect(
        BAR_X,
        BAR_Y,
        BAR_W,
        BAR_H,
        SSD1306_WHITE);

    const int fillWidth =
        static_cast<int>(
            progress *
            (BAR_W - 4));

    if (fillWidth > 0)
    {
        display.fillRect(
            BAR_X + 2,
            BAR_Y + 2,
            fillWidth,
            BAR_H - 4,
            SSD1306_WHITE);
    }
}

// =====================================================
// Motor status screen
// =====================================================

void drawMotorScreen()
{
    display.clearDisplay();

    display.setTextColor(
        SSD1306_WHITE);

    display.setTextSize(1);

    // Header
    display.setCursor(0, 0);
    display.print("MACK-10 MOTOR TEST");

    display.drawFastHLine(
        0,
        10,
        SCREEN_WIDTH,
        SSD1306_WHITE);

    // Direction
    display.setCursor(0, 15);
    display.print("DIR: ");

    if (currentDirection == HIGH)
    {
        display.print("FORWARD");
    }
    else
    {
        display.print("REVERSE");
    }

    // Phase
    display.setCursor(0, 26);
    display.print(
        phaseName(motionPhase));

    // Step rate
    display.setCursor(0, 37);

    display.print(
        currentStepHz);

    display.print(" steps/s");

    // -----------------------------------------------
    // Progress through current phase
    // -----------------------------------------------

    float progress = 0.0f;

    const unsigned long elapsed =
        millis() - phaseStartMs;

    switch (motionPhase)
    {
    case ACCELERATING:
        progress =
            static_cast<float>(elapsed) /
            ACCEL_TIME_MS;
        break;

    case CRUISING:
        progress =
            static_cast<float>(elapsed) /
            CRUISE_TIME_MS;
        break;

    case DECELERATING:
        progress =
            static_cast<float>(elapsed) /
            DECEL_TIME_MS;
        break;

    case PAUSED:
        progress =
            static_cast<float>(elapsed) /
            DIRECTION_PAUSE_MS;
        break;

    case COMPLETE:
        progress = 1.0f;
        break;

    default:
        progress = 0.0f;
        break;
    }

    drawProgressBar(progress);

    display.display();
}

// =====================================================
// Startup display
// =====================================================

void drawStartupScreen(int seconds)
{
    display.clearDisplay();

    display.setTextColor(
        SSD1306_WHITE);

    display.setTextSize(1);

    display.setCursor(40, 8);
    display.println("MACK-10");

    display.setCursor(12, 25);
    display.println("SMOOTH MOTOR TEST");

    display.setTextSize(2);

    display.setCursor(58, 43);
    display.print(seconds);

    display.display();
}

// =====================================================
// Motion phases
// =====================================================

void beginAcceleration()
{
    motionPhase =
        ACCELERATING;

    phaseStartMs =
        millis();

    lastRampUpdateMs =
        millis();

    startStepGenerator(
        START_STEP_HZ);

    Serial.println(
        "Accelerating...");
}

void beginCruise()
{
    motionPhase =
        CRUISING;

    phaseStartMs =
        millis();

    changeStepFrequency(
        MAX_STEP_HZ);

    Serial.println(
        "Full speed.");
}

void beginDeceleration()
{
    motionPhase =
        DECELERATING;

    phaseStartMs =
        millis();

    lastRampUpdateMs =
        millis();

    Serial.println(
        "Decelerating...");
}

void beginPause()
{
    stopStepGenerator();

    motionPhase =
        PAUSED;

    phaseStartMs =
        millis();

    Serial.println(
        "Paused.");
}

// =====================================================
// Motion update
// =====================================================

void updateMotion()
{
    const unsigned long now =
        millis();

    const unsigned long elapsed =
        now - phaseStartMs;

    // ===================================================
    // Acceleration
    // ===================================================

    if (motionPhase ==
        ACCELERATING)
    {
        if (elapsed >=
            ACCEL_TIME_MS)
        {
            beginCruise();
            return;
        }

        if (now -
                lastRampUpdateMs >=
            RAMP_UPDATE_INTERVAL_MS)
        {
            lastRampUpdateMs =
                now;

            const float progress =
                static_cast<float>(elapsed) /
                ACCEL_TIME_MS;

            const int frequency =
                START_STEP_HZ +
                static_cast<int>(
                    progress *
                    (MAX_STEP_HZ -
                     START_STEP_HZ));

            changeStepFrequency(
                frequency);
        }

        return;
    }

    // ===================================================
    // Cruise
    // ===================================================

    if (motionPhase ==
        CRUISING)
    {
        if (elapsed >=
            CRUISE_TIME_MS)
        {
            beginDeceleration();
        }

        return;
    }

    // ===================================================
    // Deceleration
    // ===================================================

    if (motionPhase ==
        DECELERATING)
    {
        if (elapsed >=
            DECEL_TIME_MS)
        {
            beginPause();
            return;
        }

        if (now -
                lastRampUpdateMs >=
            RAMP_UPDATE_INTERVAL_MS)
        {
            lastRampUpdateMs =
                now;

            const float progress =
                static_cast<float>(elapsed) /
                DECEL_TIME_MS;

            const int frequency =
                MAX_STEP_HZ -
                static_cast<int>(
                    progress *
                    (MAX_STEP_HZ -
                     START_STEP_HZ));

            changeStepFrequency(
                frequency);
        }

        return;
    }
}

// =====================================================
// Run one direction
// =====================================================

void runDirection(bool direction)
{
    currentDirection =
        direction;

    // Stop STEP first before changing direction.
    stopStepGenerator();

    digitalWrite(
        DIR_PIN,
        direction);

    // Generous DIR setup time.
    delay(20);

    beginAcceleration();

    while (motionPhase != PAUSED)
    {
        updateMotion();

        // OLED is completely independent
        // of the STEP waveform now.
        if (millis() -
                lastDisplayMs >=
            DISPLAY_INTERVAL_MS)
        {
            lastDisplayMs =
                millis();

            drawMotorScreen();
        }

        // Give background ESP32 tasks time.
        delay(1);
    }

    drawMotorScreen();

    delay(
        DIRECTION_PAUSE_MS);
}

// =====================================================
// Setup
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(300);

    Serial.println();
    Serial.println("==============================");
    Serial.println(" MACK-10 SMOOTH MOTOR TEST");
    Serial.println("==============================");

    // ===================================================
    // GPIO
    // ===================================================

    pinMode(
        ENABLE_PIN,
        OUTPUT);

    pinMode(
        DIR_PIN,
        OUTPUT);

    pinMode(
        STEP_PIN,
        OUTPUT);

    // Safe startup states.
    digitalWrite(
        ENABLE_PIN,
        HIGH);

    digitalWrite(
        DIR_PIN,
        LOW);

    digitalWrite(
        STEP_PIN,
        LOW);

    // ===================================================
    // Hardware PWM / STEP generator
    // Arduino-ESP32 2.x API
    // ===================================================

    ledcSetup(
        STEP_CHANNEL,
        START_STEP_HZ,
        STEP_RESOLUTION_BITS);

    ledcAttachPin(
        STEP_PIN,
        STEP_CHANNEL);

    stopStepGenerator();

    // ===================================================
    // OLED
    // ===================================================

    Wire.begin(
        OLED_SDA_PIN,
        OLED_SCL_PIN);

    // SSD1306 generally supports 400 kHz I2C.
    Wire.setClock(
        400000);

    if (!display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDRESS))
    {
        Serial.println(
            "OLED initialization failed.");

        while (true)
        {
            delay(1000);
        }
    }

    display.ssd1306_command(
        SSD1306_SETCONTRAST);

    display.ssd1306_command(
        0xFF);

    // ===================================================
    // Startup countdown
    // ===================================================

    for (int seconds = 3;
         seconds > 0;
         seconds--)
    {
        drawStartupScreen(
            seconds);

        delay(1000);
    }

    // ===================================================
    // Enable TMC2208
    // ===================================================

    digitalWrite(
        ENABLE_PIN,
        LOW);

    Serial.println(
        "TMC2208 enabled.");

    delay(500);

    // ===================================================
    // Forward
    // ===================================================

    Serial.println();
    Serial.println(
        "FORWARD");

    runDirection(
        HIGH);

    // ===================================================
    // Reverse
    // ===================================================

    Serial.println();
    Serial.println(
        "REVERSE");

    runDirection(
        LOW);

    // ===================================================
    // Complete
    // ===================================================

    stopStepGenerator();

    digitalWrite(
        ENABLE_PIN,
        HIGH);

    motionPhase =
        COMPLETE;

    phaseStartMs =
        millis();

    drawMotorScreen();

    Serial.println();
    Serial.println(
        "TEST COMPLETE");
}

// =====================================================
// Loop
// =====================================================

void loop()
{
    // One-shot test.
}