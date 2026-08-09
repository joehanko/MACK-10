#include <Arduino.h>

// =====================================================
// MACK-10 TMC2208 / NEMA17 TEST
// =====================================================

constexpr int ENABLE_PIN = 5;
constexpr int STEP_PIN = 18;
constexpr int DIR_PIN = 19;

// Number of STEP pulses in each direction.
// Start conservatively.
constexpr int TEST_STEPS = 200;

// Larger delay = slower motor.
// 2000 us HIGH + 2000 us LOW = slow first test.
constexpr unsigned int STEP_DELAY_US = 2000;

// =====================================================
// Move motor
// =====================================================

void moveSteps(int steps, bool direction)
{
    digitalWrite(DIR_PIN, direction);

    // Give DIR a moment to settle.
    delay(10);

    for (int i = 0; i < steps; i++)
    {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(STEP_DELAY_US);

        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(STEP_DELAY_US);
    }
}

// =====================================================
// Setup
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================");
    Serial.println(" MACK-10 NEMA17 MOTOR TEST");
    Serial.println("==============================");

    pinMode(ENABLE_PIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);

    // Known safe startup states.
    digitalWrite(STEP_PIN, LOW);
    digitalWrite(DIR_PIN, LOW);

    // TMC2208 ENABLE is active LOW.
    // Keep disabled initially.
    digitalWrite(ENABLE_PIN, HIGH);

    Serial.println("Driver disabled.");
    Serial.println("Motor test begins in 3 seconds...");

    delay(3000);

    // ---------------------------------------------------
    // Enable driver
    // ---------------------------------------------------

    Serial.println("Enabling TMC2208...");

    digitalWrite(ENABLE_PIN, LOW);

    delay(500);

    // ---------------------------------------------------
    // Direction 1
    // ---------------------------------------------------

    Serial.println("Moving direction 1...");

    moveSteps(
        TEST_STEPS,
        HIGH);

    Serial.println("Pause.");

    delay(1500);

    // ---------------------------------------------------
    // Direction 2
    // ---------------------------------------------------

    Serial.println("Moving direction 2...");

    moveSteps(
        TEST_STEPS,
        LOW);

    Serial.println("Pause.");

    delay(500);

    // ---------------------------------------------------
    // Disable
    // ---------------------------------------------------

    digitalWrite(ENABLE_PIN, HIGH);

    Serial.println("Driver disabled.");
    Serial.println("Motor test complete.");
}

// =====================================================
// Loop
// =====================================================

void loop()
{
    // One-time startup test only.
}