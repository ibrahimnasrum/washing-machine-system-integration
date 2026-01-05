#ifndef PINS_H
#define PINS_H

// ===================== Libraries =====================
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

// ===================== Board Note =====================
// This pin map assumes Arduino MEGA (SDA=20, SCL=21, SPI=50/51/52).
// If you use UNO, you must change button matrix pins & SPI pins.


// ===================== LCD (I2C) =====================
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

// Declare LCD object (DEFINED in washer_fsm.ino OR rfid.ino, not here)
extern LiquidCrystal_I2C lcd;


// ===================== RFID (MFRC522) =====================
// IMPORTANT: Avoid conflicts with stepper pins (9–12).
// Mega SPI pins are fixed: MISO=50, MOSI=51, SCK=52
// SS can be any digital pin, but 53 is commonly used for Mega SS.
#define RFID_SS_PIN   53
#define RFID_RST_PIN  22

extern MFRC522 mfrc522;

// Put your authorized card UID here (uppercase HEX, no spaces, 2 digits/byte)
extern const String VALID_UID;


// ===================== Master Enable (Safety Switch) =====================
#define SIGNAL_PIN  6   // 5V enable output (to relay/control input)
extern bool washerEnabled;   // defined in main.ino


// ===================== Button Matrix (4x4) =====================
// Columns OUTPUT, Rows INPUT_PULLUP
static const uint8_t colPins[4] = {30, 31, 32, 33};
static const uint8_t rowPins[4] = {34, 35, 36, 37};

// Define which key is START (row, col)
#define START_ROW 0
#define START_COL 0


// ===================== Status LEDs =====================
// Use consistent LEDs for whole system (change if your hardware differs)
#define LED_GREEN  38
#define LED_YELLOW 39
#define LED_RED    40


// ===================== Relays (Active-LOW) =====================
#define RELAY_INLET  5   // fill pump
#define RELAY_DRAIN  4   // drain pump (moved away from pin 6 to avoid conflict)

inline void relayOn(uint8_t pin)  { digitalWrite(pin, LOW);  }
inline void relayOff(uint8_t pin) { digitalWrite(pin, HIGH); }


// ===================== Stepper Motor via L298N =====================
// Keep original IN pins (no conflict now because RFID moved off 9–12)
#define IN1_PIN  9
#define IN2_PIN  10
#define IN3_PIN  11
#define IN4_PIN  12


// ===================== Shared Function Prototypes =====================
// ---- RFID module (rfid.ino)
void setupRFID();
void rfidLoop();
void setEnableOutputs();   // applies LED_GREEN + SIGNAL_PIN based on washerEnabled

// ---- Washer module (washer_fsm.ino)
void setupWasher();
void washerLoop();
void washerSafeStop();     // stops pumps/motor + shows safe/idle display

#endif
