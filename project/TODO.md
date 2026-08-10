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
- [ ] Swap out Fuse connector
- [ ] Reduce board size (1/3)
- [ ] Adjust Right ESP32 Header by 2.54mm
- [ ] Integrate HX711 into board
- [ ] Change Stepper Pin 1 Location (Enable)
- [ ] Add stepper Test Pins (Voltage and pin location)
- [ ] Add flyback diode to prevent motor EMF
- [ ] Add encoder input pins
- [ ] Switch to 2x5 ribbon cable male header for inputs
- [ ] Change input pins to joystick (active, not signal)
- [ ] Add more ground test pins
- [ ] Add test pins to back of board?

### Mechanical

- [ ] Design moving carriage
- [ ] Design uppper tensile gripper
- [ ] Design bottom stationary tensile gripper
- [ ] Design encoder mount
