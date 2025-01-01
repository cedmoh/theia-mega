#pragma region definitions

#define OSX 0
#define WINDOWS 1
#define UBUNTU 2

int platform = UBUNTU;

#include "Keyboard.h"
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 4);

#include "state_button.h"
#include "queue.h"

#define MOTOR_SPEED 120

// to prevent damage to the engine
#define MOTOR_SAFETY_DELAY 200

// order
#define IN_APP 0
#define RTL 2
#define LTR 4
#define R 6
#define L 8

// led side
#define SIDE 0
#define BACK 2
#define BOTH 4

// mode
#define AUTO 0
#define PANEL 2
#define PEDAL 4
#define CALIB 6

// shortcut keys
#define SHORTCUT_KEY_CALIBRATION_START_LIVE_VIEW KEY_F5 // shortcut key to start the live view when the glass is lowered
#define SHORTCUT_KEY_CALIBRATION_STOP_LIVE_VIEW KEY_F4  // shortcut key to stop the live view when the glass is lifted
#define SHORTCUT_KEY_SWITCH_CAMERA_PREVIOUS KEY_F7      // shortcut key to swap to the previous camera
#define SHORTCUT_KEY_SWITCH_CAMERA_NEXT KEY_F8          // shortcut key to swap to the next camera

// state button names
#define STATE_BUTTON_BUZZER stateButtons[0]                    // 0  Buzzer
#define STATE_BUTTON_LED_MODE stateButtons[1]                  // 1  LED Mode
#define STATE_BUTTON_UNUSED_2 stateButtons[2]                  // 2  [UNUSED]
#define STATE_BUTTON_UNUSED_3 stateButtons[3]                  // 3  [UNUSED]
#define STATE_BUTTON_PEDAL stateButtons[4]                     // 4  Pedal
#define STATE_BUTTON_SCAN_ORDER_SELECT stateButtons[5]         // 5  Scan Order Selector
#define STATE_BUTTON_SCAN_MODE_AND_CALIBRATION stateButtons[6] // 6  Scan Mode & Calibration
#define STATE_BUTTON_SCAN stateButtons[7]                      // 7  Scan
#define STATE_BUTTON_GLASS_DOWN_SENSOR stateButtons[8]         // 8  Glass Down Sensor
#define STATE_BUTTON_GLASS_UP_SENSOR stateButtons[9]           // 9  Glass Up Sensor
#define STATE_BUTTON_LED_PROJECTOR_SENSOR stateButtons[10]     // 10 LED Projector Sensor

const unsigned short LED_PROJECTOR = 9U;
const unsigned short LED_PROJECTOR_HIGH = 255;       // lit
const unsigned short LED_PROJECTOR_LOW = 20;         // dimRL
const unsigned long LED_PROJECTOR_TIMEOUT = 300000U; // 5 minutes

const unsigned short BUZZER = 4U;
const unsigned short SCAN_LED = 13U;

const unsigned short GLASS_UP_SENSOR = 7;
const unsigned short GLASS_DOWN_SENSOR = 8;
const unsigned short GLASS_MOTOR_UP = 6;   // (RED LED)
const unsigned short GLASS_MOTOR_DOWN = 5; // (GREEN LED)

const unsigned long CALIBRATION_DURATION = 240000U;     // 4 minutes
const unsigned long CALIBRATION_ALARM_DURATION = 8000U; // 8 seconds
// const unsigned long CALIBRATION_DURATION = 4000U; // 4 seconds
// const unsigned long CALIBRATION_ALARM_DURATION = 1000U; // 1 second

const unsigned long BUTTON_BEEP_DURATION = 200U; // the duration of every button press beep

bool BottomTouchdownFlag = false; // Used in auto mode, has the pedal touched the bottom sensor.
bool IsCalibrating = false;       // Is the calibration in action
bool IsScanBlinking = false;      // Is the scan button blinking

void nothing() {};

void test(StateButton *self);

void switch_ord(StateButton *self);
void switch_led_side(StateButton *self);
void switch_mode(StateButton *self);

void handle_scan_button(StateButton *self);

void handle_glass_down_sensor(StateButton *self);
void handle_glass_up_sensor(StateButton *self);

void handle_pedal(StateButton *self);

void handle_projector(StateButton *self);

void printSettings();

// attempts to spin the glass up
// does nothing if the up sensor won't allow it
bool spin_glass_up()
{
  if (digitalRead(GLASS_UP_SENSOR) == HIGH)
  {
    digitalWrite(GLASS_MOTOR_UP, LOW);    // start
    digitalWrite(GLASS_MOTOR_DOWN, HIGH); // stop
    return true;
  }

  return false;
}

// attempts to spin the glass down
// does nothing if the down sensor won't allow it
bool spin_glass_down()
{
  if (digitalRead(GLASS_DOWN_SENSOR) == HIGH)
  {
    digitalWrite(GLASS_MOTOR_DOWN, LOW); // start
    digitalWrite(GLASS_MOTOR_UP, HIGH);  // stop
    return true;
  }

  return false;
}

struct StateButton stateButtons[] = {
    {4U, 0U, 0, 0, 0, 0, 0U, nothing},                  // 0  Buzzer
    {13U, 0U, 0, 0, 0, 1, 0U, switch_led_side},         // 1  LED Mode
    {A0, 0U, 0, 0, 0, 1, 0U, nothing},                  // 2  [UNUSED]
    {A1, 0U, 0, 0, 0, 1, 0U, nothing},                  // 3  [UNUSED]
    {A2, 0U, 0, 0, 0, 0, 0U, handle_pedal},             // 4  Pedal
    {A3, 0U, 0, 0, 0, 0, 0U, switch_ord},               // 5  Scan Order Selector
    {A4, 0U, 0, 0, 0, 0, 0U, switch_mode},              // 6  Scan Mode & Calibration
    {A5, 0U, 0, 0, 0, 0, 0U, handle_scan_button},       // 7  Scan
    {8U, 0U, 0, 1, 1, 0, 0U, handle_glass_down_sensor}, // 8  Glass Down Sensor
    {7U, 0U, 0, 1, 1, 0, 0U, handle_glass_up_sensor},   // 9  Glass Up Sensor
    {10U, 0U, 0, 0, 0, 0, 0U, handle_projector},        // 10 LED Projector Sensor
};

#pragma endregion definitions

// utility

void test(StateButton *self)
{
  lcd.clear();
  char str[4];
  sprintf(str, "%d", self->counter);
  lcd.print(str);
}

int count = 0;

void countup()
{
  lcd.clear();

  char str[4];
  sprintf(str, "%d", count++);
  lcd.setCursor(0, 0);
  lcd.print(str);
}

// beeping

void beep_start()
{
  digitalWrite(BUZZER, HIGH);
}

void beep_end()
{
  digitalWrite(BUZZER, LOW);
}

void beep_signal()
{
  beep_start();
  waitcall(beep_end, BUTTON_BEEP_DURATION);
}

// alert

const int MESSAGE_DELAY = 3000;

void alert_lcd(char msg[32])
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg);
  waitcall(printSettings, MESSAGE_DELAY);
}

void scan_blink()
{
  if (IsScanBlinking == false)
    return;

  // blink
  if (digitalRead(SCAN_LED) == HIGH)
    digitalWrite(SCAN_LED, LOW);
  else
    digitalWrite(SCAN_LED, HIGH);

  waitcall(scan_blink, 840);
}

// order

void switch_ord(StateButton *self)
{
  if (STATE_BUTTON_SCAN_ORDER_SELECT.counter % 2 == 1)
  {
    // push
    beep_signal();
  }

  if (self->counter > 9)
    self->counter = IN_APP;

  printSettings();
}

void switch_led_side(StateButton *self)
{
  if (self->counter > 5)
    self->counter = SIDE;

  printSettings();
}

void switch_mode(StateButton *self)
{
  if (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter % 2 == 1)
  {
    // push
    beep_signal();
  }

  // disabled when calibrating

  if (self->counter > 7)
    self->counter = AUTO;

  IsScanBlinking = self->counter == CALIB;

  // enabled only in panel
  if (self->counter == PANEL)
    digitalWrite(SCAN_LED, HIGH);
  // blinking in calib
  else if (IsScanBlinking)
    scan_blink();
  else
    digitalWrite(SCAN_LED, LOW);

  printSettings();
}

// progress

unsigned short progress_state = 16;

char *progress_message;

// calibrating...
// ================ <

// RECURSIVE
//
void progress_rev()
{

  // check for abortion
  if (IsCalibrating == false)
  {
    progress_state = 16;
    return;
  }

  lcd.clear();

  // end recursion when state ends
  if (progress_state-- == 0)
  {
    progress_state = 16;
    printSettings();
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print(progress_message);

  lcd.setCursor(0, 1);
  for (unsigned short i = 0; i <= progress_state; i++)
    lcd.print(">");

  waitcall(progress_rev, floor(CALIBRATION_DURATION / 16));
}

void progress_start(char msg[16], unsigned long duration)
{
  progress_state = 16;
  progress_message = msg;

  lcd.clear();

  progress_rev();
}

void next_camera()
{
  for (size_t i = 0; i < 3; i++)
  {
    Keyboard.press(SHORTCUT_KEY_SWITCH_CAMERA_NEXT);
    Keyboard.releaseAll();
  }
}

void previous_camera()
{
  for (size_t i = 0; i < 3; i++)
  {
    Keyboard.press(SHORTCUT_KEY_SWITCH_CAMERA_PREVIOUS);
    Keyboard.releaseAll();
  }
}

// send scan command according to scan mode
void send_scan_cmd()
{

  switch (STATE_BUTTON_SCAN_ORDER_SELECT.counter)
  {
  case IN_APP:
    Keyboard.press(KEY_F9);
    Keyboard.release(KEY_F9);
    delay(1000);
    break;

  case RTL:
    next_camera();

    delay(100);

    // send space
    Keyboard.press(' ');
    Keyboard.release(' ');

    delay(2100);

    previous_camera();

    delay(100);

    // send space
    Keyboard.press(' ');
    Keyboard.release(' ');
    break;

  case LTR:
    previous_camera();

    delay(100);

    // send space
    Keyboard.press(' ');
    Keyboard.release(' ');

    delay(2100);

    next_camera();

    delay(100);

    // send space
    Keyboard.press(' ');
    Keyboard.release(' ');
    break;

  case R:
    next_camera();

    delay(100);

    // send space
    Keyboard.press(' ');
    Keyboard.release(' ');
    break;

  case L:
    previous_camera();

    delay(100);

    // send space
    Keyboard.press(' ');
    Keyboard.release(' ');
    break;
  }

  Keyboard.releaseAll();
}

void handle_scan_button(StateButton *self)
{
  if (STATE_BUTTON_SCAN.counter % 2 == 1)
    return;

  // push

  switch (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter)
  {
  case PEDAL:
    send_scan_cmd();
    break;

  case PANEL:
    if (digitalRead(GLASS_DOWN_SENSOR) == LOW)
      send_scan_cmd();

    break;

  case AUTO:
    // disabled
    break;

  case CALIB:

    if (!IsCalibrating)
    {

      if (!spin_glass_down())
        break;

      // start calibration
      IsCalibrating = true;

      // disable control panel buttons when calibrating
      STATE_BUTTON_SCAN_ORDER_SELECT.disabled = true;
      STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.disabled = true;

      // progress
      progress_start("Calibration Mode", CALIBRATION_DURATION);

      // stop blinkinkg
      IsScanBlinking = false;
      digitalWrite(SCAN_LED, HIGH);

      // buzzer
      waitcall([]()
               {
                  beep_start();

                  waitcall([]()
                            { 
                              // end calibration
                              beep_end();

                              IsScanBlinking = true; 
                              scan_blink();

                              IsCalibrating = false;

                              // reinable control panel buttons
                              STATE_BUTTON_SCAN_ORDER_SELECT.disabled = false;
                              STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.disabled = false;

                              spin_glass_up();
                              printSettings();
                             },
                            CALIBRATION_ALARM_DURATION + 640U); },
               CALIBRATION_DURATION - CALIBRATION_ALARM_DURATION);
    }
    else
    {
      // abort calibration

      digitalWrite(BUZZER, LOW);
      clear_queue(); // !

      IsScanBlinking = true;
      scan_blink();

      IsCalibrating = false;

      // reinable control panel buttons
      STATE_BUTTON_SCAN_ORDER_SELECT.disabled = false;
      STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.disabled = false;

      spin_glass_up();
      printSettings();
    }
    break;
  }

  // short beep
  beep_signal();
}

// glass

void handle_glass_down_sensor(StateButton *self)
{
  if (STATE_BUTTON_GLASS_DOWN_SENSOR.counter % 2 == 0)
    return;

  // push

  digitalWrite(GLASS_MOTOR_DOWN, HIGH); // stop

  // counter tackle
  digitalWrite(GLASS_MOTOR_UP, LOW); // start
  delay(10);
  digitalWrite(GLASS_MOTOR_UP, HIGH); // stop

  // send shortcut key to start live view when in calibration mode
  if (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter == CALIB)
  {
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(SHORTCUT_KEY_CALIBRATION_START_LIVE_VIEW);
    Keyboard.releaseAll();
  }

  if (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter == AUTO)
  {
    BottomTouchdownFlag = true;
    delay(200);
    send_scan_cmd();
    delay(1500);
    BottomTouchdownFlag = false;

    spin_glass_up();
  }
}

void handle_glass_up_sensor(StateButton *self)
{

  if (STATE_BUTTON_GLASS_UP_SENSOR.counter % 2 == 1)
  {
    // push

    // counter tackle
    digitalWrite(GLASS_MOTOR_DOWN, LOW); // start
    delay(10);
    digitalWrite(GLASS_MOTOR_DOWN, HIGH); // stop

    // send shortcut key to start live view when in calibration mode
    if (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter == CALIB)
    {
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(SHORTCUT_KEY_CALIBRATION_STOP_LIVE_VIEW);
      Keyboard.releaseAll();
    }
  }

  if (STATE_BUTTON_SCAN.counter % 2 == 1)
    return;

  digitalWrite(GLASS_MOTOR_UP, HIGH); // stop
}

void handle_pedal(StateButton *self)
{

  if (STATE_BUTTON_PEDAL.counter % 2 == 0)
  {
    // release

    switch (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter)
    {
    case PANEL:
      digitalWrite(GLASS_MOTOR_DOWN, HIGH); // stop

      // to avoid jitter clicking and getting the pedal stuck
      // only return panel when
      if (digitalRead(GLASS_UP_SENSOR) == HIGH)
      {
        delay(MOTOR_SAFETY_DELAY);
        spin_glass_up();
      }

      break;
    case AUTO:
      // return glass only if it hasn't come all the way down
      if (BottomTouchdownFlag == false)
      {
        digitalWrite(GLASS_MOTOR_DOWN, HIGH); // stop
        delay(MOTOR_SAFETY_DELAY);
        spin_glass_up();
      }
      break;
    }
  }
  else
  {
    // push

    switch (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter)
    {
    case PEDAL:
      send_scan_cmd();
      break;

    case PANEL:
    case AUTO:
      digitalWrite(GLASS_MOTOR_UP, HIGH); // stop
      delay(MOTOR_SAFETY_DELAY);
      spin_glass_down();
      break;

    case CALIB:
      // disabled
      break;
    }
  }
}

unsigned long projector_idle_time = 0U;

// projector
void handle_projector(StateButton *self)
{

  if (STATE_BUTTON_LED_PROJECTOR_SENSOR.counter % 2 == 0)
  {
    // release
    analogWrite(LED_PROJECTOR, LED_PROJECTOR_LOW);
  }
  else
  {
    // push

    analogWrite(LED_PROJECTOR, LED_PROJECTOR_HIGH);
    projector_idle_time = millis();
  }
}

void printSettings()
{

  //| Order:   Mode: |
  //| L to R   PEDAL |
  // ^^^^^^^^^^^^^^^^

  // R to L, L to R, RIGHT, LEFT
  // side, back, both
  // auto, panel, pedal

  lcd.setCursor(0, 0);
  lcd.print(" Order:   ");
  lcd.setCursor(0, 1);

  switch (STATE_BUTTON_SCAN_ORDER_SELECT.counter)
  {
  case 0:
  case 1:
    lcd.print(" In App   ");
    break;
  case 2:
  case 3:
    lcd.print(" R to L   ");
    break;
  case 4:
  case 5:
    lcd.print(" L to R   ");
    break;
  case 6:
  case 7:
    lcd.print(" Right*   ");
    break;
  case 8:
  case 9:
    lcd.print(" Left*    ");
    break;
  }

  lcd.setCursor(10, 0);
  lcd.print("Mode: ");
  lcd.setCursor(10, 1);

  switch (STATE_BUTTON_SCAN_MODE_AND_CALIBRATION.counter)
  {
  case 0:
  case 1:
    lcd.print("Auto ");
    break;
  case 2:
  case 3:
    lcd.print("Panel ");
    break;
  case 4:
  case 5:
    lcd.print("Pedal ");
    break;
  case 6:
  case 7:
    lcd.print("Calib ");
    break;
  }
};

// put this in the loop function
void cycleStateButtons()
{
  for (int i = 0; i < sizeof(stateButtons) / sizeof(struct StateButton); i++)
    state_button_check(&stateButtons[i]);
};

void setup()
{

  // Button PinModes
  for (int i = 0; i < sizeof(stateButtons) / sizeof(struct StateButton); i++)
  {
    pinMode(stateButtons[i].PIN, INPUT);
  }

  pinMode(BUZZER, OUTPUT);
  pinMode(SCAN_LED, OUTPUT);

  pinMode(LED_PROJECTOR, OUTPUT);

  pinMode(13, OUTPUT);

  pinMode(12, INPUT);
  pinMode(11, INPUT);

  pinMode(GLASS_MOTOR_UP, OUTPUT);
  pinMode(GLASS_MOTOR_DOWN, OUTPUT);

  digitalWrite(GLASS_MOTOR_UP, HIGH);   // stop
  digitalWrite(GLASS_MOTOR_DOWN, HIGH); // stop

  analogWrite(LED_PROJECTOR, LED_PROJECTOR_LOW);

  Keyboard.begin();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(1, 0);
  lcd.print("   Theia Pro   ");
  lcd.setCursor(2, 1);
  lcd.print("Book Scanner");

  delay(2000); // bootup delay

  printSettings();

  Serial.begin(9600);
}

void loop()
{

  cycleStateButtons();

  // char str[24];
  // sprintf(str, "%lu", millis());
  // lcd.setCursor(0, 0);
  // lcd.print(str);

  doChores();

  if (millis() > projector_idle_time + LED_PROJECTOR_TIMEOUT)
    analogWrite(LED_PROJECTOR, LED_PROJECTOR_LOW);

  // char str[4];
  // sprintf(str, "%d", digitalRead(A2));
  // lcd.print(str);
}
