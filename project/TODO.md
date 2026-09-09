## MACK-10 TODO - V1

### Schematic

- [x] Finish ESP32 wiring
- [x] Finish limit switch circuitry
- [x] Verify all power nets
- [x] Add remaining test points
- [x] Run ERC with zero errors
- [x] Assign footprints

### PCB Layout

- [x] Update PCB from schematic
- [x] Connect duplicated ESP32 breakout pads (socket ↔ breakout header)
- [x] Place all components
- [x] Route power traces
- [x] Route signal traces
- [x] Add ground pours
- [x] Run DRC
- [x] Review silkscreen

### Prototype

- [x] Order Rev A PCBs
- [x] Assemble board
- [x] Test power rails
- [x] Program ESP32
- [x] Verify HX711
- [x] Verify stepper driver

## MACK-10 TODO - V2

### PCB Layout

- [ ] Move ESP32 Right Header 2.54mm x+, to ensure proper seat of ESP32
- [ ] Add test pins for current readings (FUSE)
- [ ] Integrate Buck Converter into board
- [ ] 24 and 9V Toggle Jumper ?
- [ ] I2C Interface Expansion Pins (4x16 GND, VCC, SCL, SDA)
- [ ] M3 Through-hole reinforcements / grounding
- [0] TMC2208 / TMC2209 90deg Header rotation for fan / cross ventilate
- [0] Buck Converter Header
- [0] Adjust Right ESP32 Header by 2.54mm
- [x] DC Jack Pin Correction (ground pin 2-3 swap)
- [x] Move DC and USB connections to same location
- [x] Swap out Fuse JST connector for jumper
- [x] Reduce board size (1/2)
- [x] Integrate HX711 into board
- [x] Fuse Pads (board perimeter) for solder jumper or pad connection
- [x] Voltage Testing Pins
- [x] Move TMC2208 Pin 1 Arrow to EN
- [x] Change Stepper Pin 1 Location (Enable)
- [x] Add 3 separate Status LEDs, multicolor? - RGB I2C Addressable
- [x] External Status LED
- [x] Inline Current Pins
- [x] Onboard Current Sensor IC
- [x] Add stepper Test Pins (Voltage and pin location)
- [x] Add flyback diode to prevent motor EMF
- [x] Add rotary encoder input pins
- [x] Switch to 2x5 ribbon cable male header for inputs
- [x] Change input pins to joystick (active, not signal)
- [x] Add more ground test pins
- [x] Add test pins to back of board (?)
- [x] Connect Unused Pins as Male Headers

## MACK-10 TODO - V3 Notes

- [ ] Ensure all GND are connected to GND plane / fill (2 unconnected GNDs)
- [ ] Some footprints clipped (too close to edge of board)
- [ ] VIN and GND test pins too close (risks short)
- [ ] Bypass pins could be a 2x3 vertical stack to save space (specialized pin header)
- [ ] Thermal relief checks
- [ ] Avoid via-in-pad wherever possible
- [ ]--- Make note of difficult to solder pins (thermal relief issues)
- [ ] Integrade PCF8575 / I/O expander to save space
- [ ] 4-layer board
- [ ] Pick-and-place small SMDs / resistors
- [ ] Pogo pins over test pads? Or external testing Pins to test board?
- [ ] INA226 Footprint wrong orientation, rotate 90º as board is too wide
- [ ] Move XIAO underneath EN Pad / hard to solder

### Mechanical

- [ ] Design moving carriage
- [ ] Design uppper tensile gripper
- [ ] Design bottom stationary tensile gripper
- [ ] Design encoder mount

### Scripts

- [ ] Add multithreading I2C addressing
- [ ] Add Pin Expansion (reconfigurable)
- [ ] Startup test sequence
- [ ] Calibration sequence
- [ ] Rotary encoder input
- [ ] Estop shutdown signal and restart

### Instructions

- [ ] Quickstart Guide for board
  - [ ] Bring-up sequence: SMDs, Components, Headers, Buck trimpot test and glue, TMC2208 Vref Adjust:

1. Solder SMD components
2. Solder board components
3. Solder pin headers
4. Inspect solder joints
5. Check power shorts
6. Verify barrel input
7. Set buck 5V
8. Verify 3.3V rail
9. Install ESP32
10. Confirm serial boot
11. Install TMC2208
12. Set TMC2208 VREF
13. Verify OLED display
14. Verify HX711 readings
15. Connect load cell
16. Test control buttons
17. Test limit switches
18. Connect stepper motor
19. Test motor motion
20. Run full system
