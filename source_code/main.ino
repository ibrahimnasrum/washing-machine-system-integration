
  main.ino — Modular Washer System
  --------------------------------
  Files in same folder
  - main.ino          (this file)
  - rfid.ino          (RFID access control)
  - washer_fsm.ino    (washing cycle state machine)
  - pins.h            (all pins + constants)

  Flow
  1) rfidLoop() updates washerEnabled (toggle via valid RFID)
  2) if washerEnabled == true
        washerLoop() runs the washing state machine
     else
        washerSafeStop() ensures all actuators OFF + idle screen


#include pins.h

 Shared global (defined here, used by other .ino files)
bool washerEnabled = false;

void setup() {
   Serial optional
  Serial.begin(9600);

   Setup each module
  setupRFID();
  setupWasher();

   Start in safe state
  washerEnabled = false;
  setEnableOutputs();      applies Green LED + SIGNAL_PIN based on washerEnabled
  washerSafeStop();        ensures pumpsmotor OFF + show idle
}

void loop() {
   1) Always listen RFID (master enable)
  rfidLoop();

   2) Run washer only if enabled
  if (washerEnabled) {
    washerLoop();          washing cycle FSM
  } else {
    washerSafeStop();      keep everything off (safety)
  }
}
