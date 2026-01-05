#include "pins.h"

// ===================== Define extern objects/constants =====================
// These are declared as extern in pins.h, so we DEFINE them here.
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);

// Set your authorized UID here (uppercase HEX, no spaces, 2 digits/byte)
const String VALID_UID = "12307989";

// ===================== RFID behavior settings =====================
static const unsigned long RFID_COOLDOWN_MS = 1200;  // prevents repeat toggles if card stays
static unsigned long lastScanMs = 0;

static const unsigned long INVALID_MSG_MS = 900;
static unsigned long invalidShownMs = 0;
static bool showingInvalid = false;

// ===================== Helpers =====================
static String uidToString() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void setEnableOutputs() {
  // Master enable outputs
  digitalWrite(LED_GREEN, washerEnabled ? HIGH : LOW);
  digitalWrite(SIGNAL_PIN, washerEnabled ? HIGH : LOW);
}

static void lcdHeader() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GOOD DAY FOR A WASH");

  lcd.setCursor(0, 1);
  lcd.print(washerEnabled ? "RFID: ENABLED     " : "RFID: DISABLED    ");

  lcd.setCursor(0, 2);
  lcd.print(washerEnabled ? "Tap RFID to STOP  " : "Tap RFID to START ");

  lcd.setCursor(0, 3);
  lcd.print("Status: ");
  lcd.print(washerEnabled ? "READY          " : "LOCKED         ");
}

static void showInvalidCard() {
  showingInvalid = true;
  invalidShownMs = millis();

  digitalWrite(LED_RED, HIGH);
  lcd.setCursor(0, 3);
  lcd.print("INVALID RFID FOUND ");
}

// ===================== Module API =====================
void setupRFID() {
  // LEDs + enable pin
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(SIGNAL_PIN, OUTPUT);

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(SIGNAL_PIN, LOW);

  // LCD
  lcd.init();
  lcd.backlight();

  // RFID
  SPI.begin();
  mfrc522.PCD_Init();

  lcdHeader();
}

void rfidLoop() {
  // Turn off invalid message after a short time (non-blocking)
  if (showingInvalid && (millis() - invalidShownMs >= INVALID_MSG_MS)) {
    showingInvalid = false;
    digitalWrite(LED_RED, LOW);
    lcdHeader();
  }

  // Cooldown: prevents multiple toggles if card stays near reader
  if (millis() - lastScanMs < RFID_COOLDOWN_MS) return;

  // Check for card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  lastScanMs = millis();

  String uid = uidToString();
  Serial.print("RFID UID: ");
  Serial.println(uid);

  if (uid == VALID_UID) {
    // Toggle master enable
    washerEnabled = !washerEnabled;
    setEnableOutputs();

    // LCD feedback
    lcd.setCursor(0, 3);
    if (washerEnabled) {
      lcd.print("ACCESS GRANTED: ON ");
      digitalWrite(LED_YELLOW, HIGH);   // brief feedback
      delay(120);
      digitalWrite(LED_YELLOW, LOW);
    } else {
      lcd.print("ACCESS GRANTED:OFF ");
      digitalWrite(LED_YELLOW, HIGH);
      delay(120);
      digitalWrite(LED_YELLOW, LOW);
    }

    delay(200);  // tiny feedback pause only (safe)
    lcdHeader();
  } else {
    showInvalidCard();
  }

  // Stop reading same card continuously
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
