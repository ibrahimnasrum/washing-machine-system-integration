// washer_fsm.ino
// ------------------------------------------------------------
// Washing Cycle Finite State Machine (FSM) for Option B modular
// Depends on:
//   - pins.h (pin mapping + prototypes)
//   - rfid.ino (defines lcd, VALID_UID, RFID, and washerEnabled toggling)
// Used by:
//   - main.ino calls washerLoop() when washerEnabled == true
//   - main.ino calls washerSafeStop() when washerEnabled == false
//
// Features:
// - Non-blocking FSM using millis()
// - Non-blocking stepper using micros()
// - 4x4 button matrix: START key triggers cycle
// - States: IDLE -> FILLING -> WASHING -> RINSING -> SPINNING -> DRAINING -> FINISHED
// - Safe stop if washerEnabled becomes false (handled by main.ino calling washerSafeStop)
// ------------------------------------------------------------

#include "pins.h"

// ---------------- Stepper Sequencing (4-step full-step) ----------------
static const uint8_t stepSeq[4][4] = {
  {HIGH, LOW,  HIGH, LOW },
  {LOW,  HIGH, HIGH, LOW },
  {LOW,  HIGH, LOW,  HIGH},
  {HIGH, LOW,  LOW,  HIGH}
};

enum StepperMode { STEPPER_STOP, STEPPER_CW, STEPPER_CCW };
static StepperMode stepperMode = STEPPER_STOP;
static uint8_t stepIndex = 0;
static unsigned long stepIntervalUs = 1200;  // normal speed
static unsigned long lastStepUs = 0;

// ---------------- FSM ----------------
enum WashingState { IDLE, FILLING, WASHING, RINSING, SPINNING, DRAINING, FINISHED };
static WashingState currentState = IDLE;
static unsigned long stateStartMs = 0;

// Durations (ms) — tune these to match your demo
static const unsigned long FILL_MS   = 5000;
static const unsigned long WASH_MS   = 12000;
static const unsigned long RINSE_MS  = 7000;
static const unsigned long SPIN_MS   = 5000;
static const unsigned long DRAIN_MS  = 5000;
static const unsigned long FINISH_MS = 4000;

// Wash direction alternation
static const unsigned long WASH_FORWARD_MS = 3000;
static const unsigned long WASH_REVERSE_MS = 3000;
static unsigned long washPhaseStartMs = 0;
static bool washForward = true;

// LCD refresh throttle (avoid flicker)
static unsigned long lastLcdMs = 0;
static const unsigned long LCD_REFRESH_MS = 200;

// START button debounce/event
static bool prevStartRaw = false;
static unsigned long lastStartChangeMs = 0;
static const unsigned long DEBOUNCE_MS = 150;
static bool startPressedEvent = false;

// To avoid repeatedly clearing LCD in washerSafeStop()
static bool safeScreenShown = false;

// ---------------- Internal Helpers ----------------
static void applyStepperStep(uint8_t idx) {
  digitalWrite(IN1_PIN, stepSeq[idx][0]);
  digitalWrite(IN2_PIN, stepSeq[idx][1]);
  digitalWrite(IN3_PIN, stepSeq[idx][2]);
  digitalWrite(IN4_PIN, stepSeq[idx][3]);
}

static void stepperStop() {
  stepperMode = STEPPER_STOP;
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

static void stepperRun(StepperMode mode) {
  stepperMode = mode;
  lastStepUs = micros();
}

static void updateStepper() {
  if (stepperMode == STEPPER_STOP) return;

  unsigned long nowUs = micros();
  if (nowUs - lastStepUs < stepIntervalUs) return;
  lastStepUs = nowUs;

  if (stepperMode == STEPPER_CW) {
    stepIndex = (stepIndex + 1) & 0x03;
  } else if (stepperMode == STEPPER_CCW) {
    stepIndex = (stepIndex + 3) & 0x03; // -1 mod 4
  }
  applyStepperStep(stepIndex);
}

static void lcdWrite4(const char* l0, const char* l1, const char* l2, const char* l3) {
  if (millis() - lastLcdMs < LCD_REFRESH_MS) return;
  lastLcdMs = millis();

  lcd.clear();
  lcd.setCursor(0,0); lcd.print(l0);
  lcd.setCursor(0,1); lcd.print(l1);
  lcd.setCursor(0,2); lcd.print(l2);
  lcd.setCursor(0,3); lcd.print(l3);
}

static void setRunLeds(bool running, bool finished=false) {
  // LED_GREEN is controlled by RFID module as master enable indicator.
  // Here we use LED_YELLOW for "cycle running" and LED_RED for "finished".
  digitalWrite(LED_YELLOW, running ? HIGH : LOW);
  digitalWrite(LED_RED, finished ? HIGH : LOW);
}

static void stopAllActuators() {
  relayOff(RELAY_INLET);
  relayOff(RELAY_DRAIN);
  stepperStop();
  setRunLeds(false, false);
}

// 4x4 matrix scan: START key = (START_ROW, START_COL)
static void scanStartButton() {
  bool startRaw = false;

  // scan columns
  for (uint8_t c = 0; c < 4; c++) {
    digitalWrite(colPins[c], LOW);
    for (uint8_t r = 0; r < 4; r++) {
      bool pressed = (digitalRead(rowPins[r]) == LOW); // pullup
      if (r == START_ROW && c == START_COL && pressed) startRaw = true;
    }
    digitalWrite(colPins[c], HIGH);
  }

  unsigned long now = millis();
  if (startRaw != prevStartRaw) {
    prevStartRaw = startRaw;
    lastStartChangeMs = now;
  }

  if ((now - lastStartChangeMs) > DEBOUNCE_MS) {
    static bool stablePressed = false;
    bool isPressedStable = startRaw;

    // rising edge event
    if (isPressedStable && !stablePressed) {
      startPressedEvent = true;
    }
    stablePressed = isPressedStable;
  }
}

static void enterState(WashingState s) {
  currentState = s;
  stateStartMs = millis();

  // default safe outputs
  relayOff(RELAY_INLET);
  relayOff(RELAY_DRAIN);

  switch (s) {
    case IDLE:
      stopAllActuators();
      lcdWrite4("WASHER: READY",
                "Press START button",
                "RFID: ENABLED",
                "State: IDLE");
      break;

    case FILLING:
      stepperStop();
      setRunLeds(true, false);
      relayOn(RELAY_INLET);
      lcdWrite4("Cycle Running...",
                "FILLING water",
                "Inlet: ON",
                "State: FILLING");
      break;

    case WASHING:
      setRunLeds(true, false);
      relayOff(RELAY_INLET);

      // normal wash speed
      stepIntervalUs = 1200;
      washForward = true;
      washPhaseStartMs = millis();
      stepperRun(STEPPER_CW);

      lcdWrite4("Cycle Running...",
                "WASHING (drum)",
                "Direction: FWD",
                "State: WASHING");
      break;

    case RINSING:
      setRunLeds(true, false);
      // gentle motion
      stepIntervalUs = 1400;
      stepperRun(STEPPER_CW);
      lcdWrite4("Cycle Running...",
                "RINSING",
                "Drum: slow",
                "State: RINSING");
      break;

    case SPINNING:
      setRunLeds(true, false);
      // faster motion
      stepIntervalUs = 800;
      stepperRun(STEPPER_CW);
      lcdWrite4("Cycle Running...",
                "SPINNING",
                "Drum: fast",
                "State: SPINNING");
      break;

    case DRAINING:
      stepperStop();
      setRunLeds(true, false);
      relayOn(RELAY_DRAIN);
      lcdWrite4("Cycle Running...",
                "DRAINING water",
                "Drain: ON",
                "State: DRAINING");
      break;

    case FINISHED:
      stopAllActuators();
      setRunLeds(false, true);
      lcdWrite4("CYCLE COMPLETE",
                "Remove laundry",
                "Press START to",
                "State: FINISHED");
      break;
  }
}

// ---------------- Module API ----------------
void setupWasher() {
  // Button matrix
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], HIGH);
  }
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(rowPins[i], INPUT_PULLUP);
  }

  // LEDs (RFID module also sets these pins, but safe to set again)
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  // Relays
  pinMode(RELAY_INLET, OUTPUT);
  pinMode(RELAY_DRAIN, OUTPUT);
  relayOff(RELAY_INLET);
  relayOff(RELAY_DRAIN);

  // Stepper pins
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  stepperStop();

  safeScreenShown = false;
  enterState(IDLE);
}

void washerSafeStop() {
  // Called when washerEnabled == false (locked)
  // Avoid spamming LCD; only show once until enabled again.
  stopAllActuators();
  currentState = IDLE;

  if (!safeScreenShown) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("WASHER LOCKED");
    lcd.setCursor(0,1); lcd.print("Tap RFID to enable");
    lcd.setCursor(0,2); lcd.print("Status: DISABLED");
    lcd.setCursor(0,3); lcd.print("All outputs OFF");
    safeScreenShown = true;
  }
}

void washerLoop() {
  // When enabled, allow normal operation and show READY screen if coming from locked
  if (safeScreenShown) {
    safeScreenShown = false;
    enterState(IDLE);
  }

  // Scan user input
  scanStartButton();

  // Keep stepper running smoothly (non-blocking)
  updateStepper();

  unsigned long now = millis();

  switch (currentState) {

    case IDLE:
      // Start cycle
      if (startPressedEvent) {
        startPressedEvent = false;
        enterState(FILLING);
      }
      break;

    case FILLING:
      // Continue showing status (throttled)
      lcdWrite4("Cycle Running...",
                "FILLING water",
                "Inlet: ON",
                "State: FILLING");

      if (now - stateStartMs >= FILL_MS) {
        relayOff(RELAY_INLET);
        enterState(WASHING);
      }

      // Optional cancel: press START to cancel
      if (startPressedEvent) {
        startPressedEvent = false;
        enterState(IDLE);
      }
      break;

    case WASHING: {
      // Alternate direction for drum agitation
      unsigned long phaseElapsed = now - washPhaseStartMs;

      if (washForward && phaseElapsed >= WASH_FORWARD_MS) {
        washForward = false;
        washPhaseStartMs = now;
        stepperRun(STEPPER_CCW);
      } else if (!washForward && phaseElapsed >= WASH_REVERSE_MS) {
        washForward = true;
        washPhaseStartMs = now;
        stepperRun(STEPPER_CW);
      }

      lcdWrite4("Cycle Running...",
                "WASHING (drum)",
                washForward ? "Direction: FWD" : "Direction: REV",
                "State: WASHING");

      if (now - stateStartMs >= WASH_MS) {
        stepperStop();
        enterState(RINSING);
      }

      if (startPressedEvent) { // optional cancel
        startPressedEvent = false;
        enterState(IDLE);
      }
      break;
    }

    case RINSING:
      lcdWrite4("Cycle Running...",
                "RINSING",
                "Drum: slow",
                "State: RINSING");

      if (now - stateStartMs >= RINSE_MS) {
        enterState(SPINNING);
      }

      if (startPressedEvent) { // optional cancel
        startPressedEvent = false;
        enterState(IDLE);
      }
      break;

    case SPINNING:
      lcdWrite4("Cycle Running...",
                "SPINNING",
                "Drum: fast",
                "State: SPINNING");

      if (now - stateStartMs >= SPIN_MS) {
        // restore normal speed for next time
        stepIntervalUs = 1200;
        stepperStop();
        enterState(DRAINING);
      }

      if (startPressedEvent) { // optional cancel
        startPressedEvent = false;
        enterState(IDLE);
      }
      break;

    case DRAINING:
      lcdWrite4("Cycle Running...",
                "DRAINING water",
                "Drain: ON",
                "State: DRAINING");

      if (now - stateStartMs >= DRAIN_MS) {
        relayOff(RELAY_DRAIN);
        enterState(FINISHED);
      }

      if (startPressedEvent) { // optional cancel
        startPressedEvent = false;
        enterState(IDLE);
      }
      break;

    case FINISHED:
      lcdWrite4("CYCLE COMPLETE",
                "Remove laundry",
                "Press START to",
                "restart / idle");

      // Press START to go back to ready
      if (startPressedEvent) {
        startPressedEvent = false;
        enterState(IDLE);
      }

      // Auto return to IDLE after some time
      if (now - stateStartMs >= FINISH_MS) {
        enterState(IDLE);
      }
      break;
  }
}
