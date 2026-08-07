#include <Arduino.h>
#include <Wire.h>

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Wire.begin(32, 33);

    Serial.println("\nScanning...");

    for (uint8_t address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);

        if (Wire.endTransmission() == 0)
        {
            Serial.print("I2C device found at 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }

    Serial.println("Done.");
}

void loop() {}