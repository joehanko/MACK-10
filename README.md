# MACK-10 Tensile Tester

MACK-10 is a DIY desktop tensile testing machine designed for testing small material specimens and learning about materials engineering.

The goal of this project is to build an affordable, open-source tensile tester capable of producing repeatable force and displacement data for plastics, polymers, and other small specimens.

## Current Status

Project is under active development.

Current progress:
- [x] ESP32 controller
- [x] HX711 load cell amplifier
- [x] 10 kg S-type load cell
- [x] OLED user interface
- [ ] Stepper motor drive
- [ ] Load cell calibration
- [ ] Custom grips
- [ ] Frame assembly
- [ ] Stress-strain data logging

## Features (Planned)

- Live force readout
- Peak force detection
- Break detection
- Real-time force graph
- Stress-strain curve generation
- CSV export
- Automatic tare
- Calibration mode
- Adjustable crosshead speed
- Emergency stop

## Hardware

- ESP32
- HX711 Load Cell Amplifier
- 10 kg S-Type Load Cell
- 128x64 I2C OLED Display
- Stepper Motor
- Stepper Driver
- Limit Switches

## Software

- Arduino Framework
- PlatformIO / Arduino IDE
- C++

## Why?

Commercial tensile testers can cost several thousand dollars. This project is an attempt to build a capable bench-top alternative while learning more about embedded systems, controls, mechanics, and materials testing.

This project is intended for educational and hobby use and is not intended to replace certified testing equipment.
