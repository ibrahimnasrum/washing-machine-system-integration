# Smart Washing Machine Controller (RFID Access + Automatic Cycle) — Arduino Mega
**RFID Safety Enable • Finite State Machine (FSM) • Relays • Stepper Drum • LCD UI • Keypad Start**

This project implements a **smart washing machine controller** on **Arduino Mega** with:
- **RFID access control** (authorized card required to enable washer)
- **Automatic washing cycle** using a **Finite State Machine (FSM)**:
  `IDLE → FILLING → WASHING → RINSING → SPINNING → DRAINING → FINISHED`
- **20x4 I2C LCD** for UI
- **4×4 button matrix/keypad** for START input
- **L298N stepper driver** for drum rotation
- **2 relays** for inlet & drain pumps
- **LED indicators** for system status

> ✅ Portfolio value: demonstrates system integration (RFID + control logic + hardware IO + UI) using clean modular Arduino architecture.

---

## Demo Workflow (How it works)
1. **Tap authorized RFID card** → system becomes **ENABLED** (`SIGNAL_PIN` goes HIGH)
2. LCD shows **READY** → press **START** on keypad
3. System runs the wash cycle automatically (FSM)
4. Tap RFID again anytime → system **DISABLED** and all outputs stop safely

---


Arduino IDE will compile all `.ino` files in the same folder as one program.

---

## Hardware Required
- Arduino **Mega 2560**
- **MFRC522 RFID reader** + RFID card/tag
- **20x4 I2C LCD** (address usually `0x27`)
- **4×4 keypad** (or button matrix)
- **L298N driver** + stepper motor (drum simulation)
- **2-channel relay module** (Active-LOW recommended)
- LEDs + resistors (Green/Yellow/Red)
- Power supply suitable for motor/relay loads

---

## Pin Mapping (Default)
### RFID (MFRC522)
- `SS`  → **D53**
- `RST` → **D22**
- SPI pins (Mega fixed): `MOSI=51`, `MISO=50`, `SCK=52`

### LCD (I2C)
- `SDA` → **20**
- `SCL` → **21**
- LCD address: **0x27** (change in `pins.h` if needed)

### Master Enable
- `SIGNAL_PIN` → **D6** (HIGH when enabled by RFID)

### Relays (Active-LOW)
- Inlet relay → **D5**
- Drain relay → **D4**

### Stepper (L298N)
- IN1 → **D9**
- IN2 → **D10**
- IN3 → **D11**
- IN4 → **D12**

### Keypad (4×4 matrix)
- Columns (OUTPUT): **30, 31, 32, 33**
- Rows (INPUT_PULLUP): **34, 35, 36, 37**
- Default START key: `row 0, col 0` (configurable in `pins.h`)

### LEDs
- Green → **D38** (RFID enabled indicator)
- Yellow → **D39** (cycle running indicator)
- Red → **D40** (finish / error indicator)

---

## How to Run (Arduino IDE)
1. Install required libraries:
   - `MFRC522` (Miguel Balboa RFID)
   - `LiquidCrystal_I2C`
2. Open **Arduino IDE**
3. Open the project folder **SmartWasher_RFID_FSM/**
4. Select:
   - Board: **Arduino Mega 2560**
   - Port: your COM port
5. Edit authorized UID in `rfid.ino`:
   ```cpp
   const String VALID_UID = "12307989";

6. Upload to Arduino Mega

7. Use:
  - Tap RFID to enable
  - Press START on keypad
  - Observe LCD + LEDs + actuator outputs

## Customization
**Change cycle timing**
Edit duration constants in washer_fsm.ino:
- FILL_MS, WASH_MS, RINSE_MS, SPIN_MS, DRAIN_MS

**Change stepper speed**
Edit:
- stepIntervalUs (smaller = faster)

**Change START key mapping**
- Edit in pins.h:
#define START_ROW 0
#define START_COL 0

## Safety Notes

- This code is for educational/demo use (prototype).
- If controlling real pumps/motors, ensure:
  - correct motor driver rating
  - electrical isolation for relay loads
  - fuse + proper grounding
  - emergency stop logic
