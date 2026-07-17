# MACK-10 Tensile Tester

> An open-source desktop tensile tester for material characterization, designed to make mechanical testing affordable, accessible, and educational.

<p align="center">
  <img src="images/prototype.jpg" width="700" alt="MACK-10 Prototype">
</p>

MACK-10 is a DIY tensile testing machine designed to evaluate the mechanical properties of small material specimens including 3D printed plastics, polymers, composites, and other engineering materials.

The project combines embedded systems, mechanical design, and materials engineering into a low-cost testing platform capable of measuring force and displacement while generating professional-quality stress-strain data.

---

## Current Status

🚧 **Actively under development**

### Electronics

- [x] ESP32 controller
- [x] HX711 load cell interface
- [x] 10 kg S-type load cell
- [x] 128×64 OLED user interface
- [x] Automatic taring
- [x] Peak force tracking
- [x] Break detection simulation
- [x] Live serial plotting
- [x] PlatformIO development environment

### Mechanical

- [ ] Linear motion system
- [ ] Stepper motor drive
- [ ] Lead screw assembly
- [ ] Limit switches
- [ ] Custom specimen grips
- [ ] Structural frame

### Software

- [ ] Load cell calibration
- [ ] Closed-loop motor control
- [ ] Crosshead speed control
- [ ] Force vs. displacement logging
- [ ] Stress-strain calculation
- [ ] CSV export
- [ ] SD card support
- [ ] Material database

---

## Planned Features

- Live force measurement
- Real-time force graph
- Peak force detection
- Automatic specimen taring
- Adjustable crosshead speed
- Break detection
- Force vs. displacement plotting
- Stress-strain curve generation
- CSV export
- Calibration mode
- OLED graphical interface
- Emergency stop
- Modular firmware architecture

---

## Hardware

- ESP32 Dev Module
- HX711 Load Cell Amplifier
- 10 kg S-Type Load Cell
- 128×64 I²C OLED Display
- NEMA Stepper Motor
- Stepper Driver
- Linear Motion Components
- Limit Switches
- Custom 3D Printed Parts

---

## Software

- PlatformIO
- Arduino Framework
- C++
- Git + GitHub
- VS Code

---

## Project Goals

- Build an affordable tensile tester for hobbyists, students, and makers.
- Learn embedded systems and motion control.
- Generate repeatable force-displacement and stress-strain data.
- Design a modular, open-source platform that others can improve.

---

## Why?

Commercial tensile testers often cost several thousand dollars, making them inaccessible to many students, hobbyists, and small workshops.

MACK-10 explores how modern maker hardware—including microcontrollers, affordable sensors, 3D printing, and open-source software—can be combined to build a capable educational testing machine at a fraction of the cost.

---

## Disclaimer

This project is intended for educational and hobby use only and is **not** intended to replace calibrated or certified laboratory testing equipment.

---
