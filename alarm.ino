#include <EEPROMAnything.h>
#include <AnalogButtons.h>
#include <Button.h>
#include <DS1307.h>
#include <EEPROM.h>
#include <Timer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#define DEFAULT_TIME 1420070400UL // 1/1/2015 00:00:00 GMT
#define BUTTON_PULLUP true
#define BUTTON_INVERT true
#define BUTTON_DEBOUNCE 1
#define BUTTON_MARGIN 10
#define BUTTON_LONG 1000
#define SPEAKER_PIN 9
#define LED_PIN 10
#define JOYSTICK_X_PIN A1
#define JOYSTICK_Y_PIN A0
#define SELECT_PIN A2
#define MENU_MAX 3
#define MENU_NONE 0
#define MENU_TIME 1
#define MENU_ALARM 2
#define MENU_SNOOZE 3
#define EEPROM_ADDRESS 0
#define ALARM_FREQ 2110

// Simple 12-hour time storing struct.
struct Time12H {
  byte hour = 0;
  byte minute = 0;
};

// EEPROM storage struct to load in setup() and save during menu operations.
struct AlarmData {
  char header = 'A';
  byte hour = 0;
  byte minute = 0;
  byte snoozeMinutes = 10;
  byte status = 0;
};

// All of the possible program states are listed here.
enum {
  // Showing the current time.
  TIME,
  // Alarm is on/sounding.
  ALARM_ACTIVE,
  // Alarm is on, but snoozed.
  ALARM_SNOOZE,
  // Menu (config) mode.
  MENU
};
byte state = TIME;

byte tickEvent = 0;
byte displayEvent = 0;
byte menuIndex = 0;
bool context = 0;
bool isColonOn = true;
bool isModified = false;
bool isLongPressed = false;
bool isAlarmSound = false;
bool hasSnoozed = false;
unsigned long snoozeMillis = 0;

Analog::Buttons xButtons = Analog::Buttons(JOYSTICK_X_PIN, BUTTON_DEBOUNCE, BUTTON_MARGIN);
Analog::Buttons yButtons = Analog::Buttons(JOYSTICK_Y_PIN, BUTTON_DEBOUNCE, BUTTON_MARGIN);
Button selectBtn = Button(SELECT_PIN, BUTTON_PULLUP, BUTTON_INVERT, BUTTON_DEBOUNCE);
Adafruit_7segment matrix = Adafruit_7segment();
DS1307 clock;
RTCDateTime dt;
Time12H editTime;
AlarmData alarm;
Timer t;

// Joystick LEFT
void handleButtonLeft() {
  if (state == MENU && menuIndex == MENU_TIME) {
    if (editTime.minute == 0) {
      editTime.minute = 59;
    }
    else {
      editTime.minute--;
    }
  }
  else if (state == MENU && menuIndex == MENU_ALARM) {
    if (alarm.minute == 0) {
      alarm.minute = 59;
    }
    else {
      alarm.minute--;
    }
  }
  else if (state == MENU && menuIndex == MENU_SNOOZE) {
    if (alarm.snoozeMinutes == 1) {
      alarm.snoozeMinutes = 15;
    }
    else {
      alarm.snoozeMinutes--;
    }
  }
  isModified = true;
  tone(SPEAKER_PIN, 1500, 10);
  updateDisplay(&context);
}

// Joystick RIGHT
void handleButtonRight() {
  if (state == MENU && menuIndex == MENU_TIME) {
    if (editTime.minute == 59) {
      editTime.minute = 0;
    }
    else {
      editTime.minute++;
    }
  }
  else if (state == MENU && menuIndex == MENU_ALARM) {
    if (alarm.minute == 59) {
      alarm.minute = 0;
    }
    else {
      alarm.minute++;
    }
  }
  else if (state == MENU && menuIndex == MENU_SNOOZE) {
    if (alarm.snoozeMinutes == 15) {
      alarm.snoozeMinutes = 1;
    }
    else {
      alarm.snoozeMinutes++;
    }
  }
  isModified = true;
  tone(SPEAKER_PIN, 2000, 10);
  updateDisplay(&context);
}

// Joystick UP
void handleButtonUp() {
  if (state == MENU && menuIndex == MENU_TIME) {
    if (editTime.hour == 23) {
      editTime.hour = 0;
    }
    else {
      editTime.hour++;
    }
  }
  else if (state == MENU && menuIndex == MENU_ALARM) {
    if (alarm.hour == 23) {
      alarm.hour = 0;
    }
    else {
      alarm.hour++;
    }
  }
  isModified = true;
  tone(SPEAKER_PIN, 2000, 10);
  updateDisplay(&context);
}

// Joystick DOWN
void handleButtonDown() {
  if (state == MENU && menuIndex == MENU_TIME) {
    if (editTime.hour == 0) {
      editTime.hour = 23;
    }
    else {
      editTime.hour--;
    }
  }
  else if (state == MENU && menuIndex == MENU_ALARM) {
    if (alarm.hour == 0) {
      alarm.hour = 23;
    }
    else {
      alarm.hour--;
    }
  }
  isModified = true;
  tone(SPEAKER_PIN, 1500, 10);
  updateDisplay(&context);
}

void setup() {
  Serial.begin(9600);

  EEPROM_readAnything(EEPROM_ADDRESS, alarm);


  pinMode(LED_PIN, OUTPUT);
  pinMode(SELECT_PIN, INPUT_PULLUP);

  matrix.begin(0x70);
  matrix.setBrightness(0);
  matrix.writeDisplay();

  // Each button is defined in isolation in terms of:
  // - an associated value (an unsigned 10 bits integer in the [0-1023] range)
  // - a click function executed upon button click
  // - an hold function executed once the button is identified as being held (defaults to click function)
  // - an hold duration determining the number of milliseconds the button must remain pressed before being identified as held down (defaults to 1 second)
  // - an hold interval determining the number of milliseconds between each activation of the hold function while the button is kept pressed (defaults to 250 milliseconds)
  Analog::Button leftBtn = Analog::Button(1023 - BUTTON_MARGIN, &handleButtonLeft);
  Analog::Button rightBtn = Analog::Button(0 + BUTTON_MARGIN, &handleButtonRight);
  xButtons.add(leftBtn);
  xButtons.add(rightBtn);
  Analog::Button upBtn = Analog::Button(1023 - BUTTON_MARGIN, &handleButtonUp);
  Analog::Button downBtn = Analog::Button(0 + BUTTON_MARGIN, &handleButtonDown);
  yButtons.add(upBtn);
  yButtons.add(downBtn);

  tickEvent = t.every(20, tick, (void*)0);
  displayEvent = t.every(333, updateDisplay, (void*)0);
}

void loop() {
  // Update timer.
  t.update();
}

void tick(void* context) {
  readSelectButton();
  xButtons.check();
  yButtons.check();
}

void readSelectButton() {
  selectBtn.read();
  // Button released?
  if (selectBtn.wasReleased()) {
    if (isLongPressed) {
      tone(SPEAKER_PIN, 1000, 40);
      if (state == ALARM_ACTIVE || state == ALARM_SNOOZE) {
        // Alarm active or snoozed? Turn off the alarm.
        snoozeStart();
        state = TIME;
      }
      else if (state == TIME) {
        // In normal time mode, a long press toggles the alarm.
        alarm.status = !alarm.status;
        writeAlarmData();
      }
    }
    else if (state == TIME || state == MENU) {
      menuNext();
    }
    else if (state == ALARM_ACTIVE) {
      snoozeStart();
      state = ALARM_SNOOZE;
    }
    isLongPressed = false;
  }
  // Set long pressed flag if held long enough.
  else if (selectBtn.pressedFor(BUTTON_LONG)) {
    tone(SPEAKER_PIN, 500);
    isLongPressed = true;
  }
}

void menuNext() {
  unsigned long newUnixTime;;

  // Save previous values if changed.
  if (isModified) {
    if (menuIndex == MENU_TIME) {
      // Set time.
      newUnixTime = DEFAULT_TIME + (3600 * (long)editTime.hour) + (60 * (long)editTime.minute);
      clock.setDateTime(newUnixTime);
    }
    else if (menuIndex == MENU_ALARM || menuIndex == MENU_SNOOZE) {
      writeAlarmData();
    }
  }

  menuIndex++;
  isModified = false;
  state = MENU;
  if (menuIndex > MENU_MAX) {
    menuIndex = 0;
    state = TIME;
  }

  // Save the current time into the temporary, user edited time.
  if (menuIndex == MENU_TIME) {
    editTime.hour = dt.hour;
    editTime.minute = dt.minute;
  }
  else if (menuIndex == MENU_ALARM) {
    editTime.hour = dt.hour;
    editTime.minute = dt.minute;    
  }

  tone(SPEAKER_PIN, 2000, 20);
  updateDisplay(&context);
}

void updateDisplay(void* context) {
  unsigned long elapsed = snoozeElapsed();
  byte lightBufferIndex;
  byte hour;
  byte minute;
  bool isTime = true;
  bool pm = false;
  bool isSnooze = state == ALARM_SNOOZE;
  bool isAlarmTime = alarm.hour == dt.hour && alarm.minute == dt.minute;
  bool isPastSnooze = elapsed > (60000 * (long)alarm.snoozeMinutes);
  
  digitalWrite(LED_PIN, alarm.status ? HIGH : LOW);

  if (state != MENU) {
    dt = clock.getDateTime();
    hour = dt.hour;
    minute = dt.minute;
    // Check if we should activate the alarm.
    if (alarm.status && (isAlarmTime || isSnooze) && (isPastSnooze || !hasSnoozed)) {
      state = ALARM_ACTIVE;
      snoozeStart();
    }
    if (state == ALARM_ACTIVE) {
      isAlarmSound = !isAlarmSound;
      if (isAlarmSound) {
        tone(SPEAKER_PIN, ALARM_FREQ, 333);
      }
    }
  }
  else if (state == MENU && menuIndex == MENU_TIME) {
    hour = editTime.hour;
    minute = editTime.minute;
  }
  else if (state == MENU && menuIndex == MENU_ALARM) {
    hour = alarm.hour;
    minute = alarm.minute;
  }
  else if (state == MENU && menuIndex == MENU_SNOOZE) {
    isTime = false;
    // Draw snooze number.
    matrix.print((long)alarm.snoozeMinutes);
  }

  if (state != TIME) {
    isColonOn = !isColonOn;
  }
  else {
    isColonOn = true;
  }

  if (isTime) {
    // Draw 12-hour time.
    if (hour >= 12) {
      pm = true;
    }
    if (hour == 0) {
      hour = 12;
    }
    else if (hour > 12) {
      hour = hour - 12;
    }
    matrix.print((long)100 * hour + minute);
    // Set the last "dot" to OFF (AM).
    bitWrite(matrix.displaybuffer[4], 7, pm);
  }

  // Draw menu dot indicator (when colon is flashing on).
  if (menuIndex > 0) {  
    lightBufferIndex = menuIndex - 1;
    if (lightBufferIndex >= 2) {
      lightBufferIndex++;
    }
    for (byte i = 0; i < 4; i++) {
      bitWrite(matrix.displaybuffer[i], 7, i == lightBufferIndex && isColonOn);
    }
  }

  matrix.drawColon(isColonOn);    

  // Actually update the display.
  matrix.writeDisplay(); 
}

void writeAlarmData() {
  EEPROM_writeAnything(EEPROM_ADDRESS, alarm);
}

void snoozeStart() {
  snoozeMillis = millis();
  hasSnoozed = true;
}

unsigned long snoozeElapsed() {
  return millis() - snoozeMillis;
}
