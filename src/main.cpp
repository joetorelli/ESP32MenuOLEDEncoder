
/* current_menu.ino - Example code using the menu system library.
 *
 * This example shows all items in the current menu.
 *
 * Copyright (c) 2015 arduino-menusystem
 * Licensed under the MIT license (see LICENSE)
 */
#include <Arduino.h>
#include "Wire.h"
#include "OLED.h"
#include "Sensor.h"
// #include <MenuSystem.h>
#include "AiEsp32RotaryEncoder.h"
#include "AiEsp32RotaryEncoderNumberSelector.h"
#include "Button2.h"

/**********************************************
  Pin Definitions
**********************************************/

#define AlarmPin 12  ///////////////// Alarm
#define SensorPin 34 ////////////////// Senso
#define PumpPin 13   /////////////////  Pump
#define CLPumpPin 27 /////////////////  Pump

// i2c
#define I2c_SDA 23
#define I2c_SCL 22

// bluetooth
#define BTStatusPin 36

// encoder
#define ROTARY_ENCODER_A_PIN 26
#define ROTARY_ENCODER_B_PIN 25
#define ROTARY_ENCODER_BUTTON_PIN -1 // button handled by Button2
#define ROTARY_ENCODER_VCC_PIN -1    /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */

// NVM Mode
#define RW_MODE false
#define RO_MODE true

/*********************** button2  **********************/
// SSW SelectSWitch
#define SWEncoderPin 4 // pushbutton
#define SSWAutoPin 15  // select switch Auto Position
#define SSWAlarmPin 32 // select switch Alarm Position
#define SSWOffPin 14   // select switch Off Position
#define SSWPumpPin 21  // select switch PumpPosition

/**********************************************
  Global Vars
**********************************************/
#define ON 1
#define OFF 0
byte SWEncoderFlag = OFF;
byte SSWMode = 0; // position of select switch
boolean DisplayState = OFF;
unsigned long DisplayTimer = 360;
int ENCValue = 0;
int OldENCValue = 0;
boolean ENCCountUp = OFF;
boolean ENCCountDwn = OFF;

int SensorLevelAnalog = 0;
int SensorFailCount = 0;
int AlarmOnLevel = 0;  // 40;  // value for AlarmLevel to ON
int AlarmOffLevel = 0; // 45; // value for AlarmLevel to OFF
int PumpOnLevel = 0;   // 400;      // value for PumpOnLevel
int PumpOffLevel = 0;  // 600;     // value for PumpOffLevel
int CLTimer = 0;       // 10;     // chlorine timer
int AlarmVol = 0;
// DateTime OLEDClock;
int Count = 0;

byte PumpFlag = OFF;     // Pump On/Off
byte PumpManFlag = OFF;  // pump sw state
byte AlarmFlag = OFF;    // Alarm On/Off
byte AlarmManFlag = OFF; // Alarm sw state

byte CLPumpFlag = OFF;
byte CLPumpManFlag = OFF;
byte CLPumpRunOnce = OFF;

// FLAGS
int Page1Once = 1;
int Page2Once = 0;
boolean BTStatusFlag = OFF;
boolean SetUpFlag = 0;
boolean AMSwitchFlag = OFF;

/**********************************************
  Sub/Function Declarations
**********************************************/
void Alarm(void);
void Pump(void);
void CLPump(void);
void Update_Display(void);
void SystemSetUp(void);
void sensor_update();

/*******************   oled display   ******************/
// Declaration for an SSD1306 OLED_Display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 OLED_Display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// struct OLED_SW Switch_State;

// depending on your encoder - try 1,2 or 4 to get expected behaviour
// #define ROTARY_ENCODER_STEPS 1
// #define ROTARY_ENCODER_STEPS 2
#define ROTARY_ENCODER_STEPS 8

/**************************  rotary encoder  **********************/
AiEsp32RotaryEncoder *rotaryEncoder = new AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoderNumberSelector numberSelector = AiEsp32RotaryEncoderNumberSelector();

void rotary_onButtonClick();
void rotary_loop();
void NumberSelectorLoop();

/*********************  ISR  *********************/
void IRAM_ATTR readEncoderISR()
{
  // rotaryEncoder.readEncoder_ISR();    //old
  rotaryEncoder->readEncoder_ISR();
}

/*********************  bounce2  ************************/
// Instantiate a Bounce object
Button2 SWEncoder;
Button2 SSWAuto;
Button2 SSWAlarm;
Button2 SSWOff;
Button2 SSWPump;

void handler(Button2 &btn);
// void longpress(Button2 &btn);
void pressed(Button2 &btn);
// void released(Button2 &btn);
// void changed(Button2 &btn);
// void tap(Button2 &btn);
// byte myButtonStateHandler();
// void myTapHandler(Button2 &btn);

/***********************************  menus  **************************/

void serial_print_help();
void serial_handler();

/**********************************************
  SetUp
**********************************************/
void setup()
{
  Serial.begin(57600);

  /***************************** pin properties  ******************/

  // outputs
  pinMode(AlarmPin, OUTPUT);
  pinMode(PumpPin, OUTPUT);
  pinMode(CLPumpPin, OUTPUT);

  pinMode(SensorPin, INPUT);

  // buttons on OLED board used for select sw
  pinMode(SSWAutoPin, INPUT_PULLUP);
  pinMode(SSWAlarmPin, INPUT_PULLUP);
  pinMode(SSWOffPin, INPUT_PULLUP);
  pinMode(SSWPumpPin, INPUT_PULLUP);

  pinMode(BTStatusPin, INPUT_PULLUP);

  ////////////////////////////////////////////////// turn off outputs
  digitalWrite(AlarmPin, HIGH);
  digitalWrite(PumpPin, HIGH);
  digitalWrite(CLPumpPin, HIGH);

  /*********   init i2c  *********/
  Wire.begin(I2c_SDA, I2c_SCL);
  bool status; // connect status
  // DEBUGPRINTLN("I2C INIT OK");

  /********************* oled  ********************/
  // SSD1306_SWITCHCAPVCC = generate OLED_Display voltage from 3.3V internally
  if (!OLED_Display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) // Address 0x3C for 128x32
  {
    // DEBUGPRINTLN(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  else
  {
    // DEBUG_PRINTLN("SSD1306 Init");
  }

  // Clear the oled buffer.
  OLED_Display.clearDisplay();
  OLED_Display.display();
  OLED_Display.setRotation(ROTATION);
  OLED_Display.setFont();
  OLED_Display.setTextSize(1);
  OLED_Display.setTextColor(SSD1306_WHITE);

  /******************************  encoder  *****************/
  rotaryEncoder->begin();
  rotaryEncoder->setup(readEncoderISR);
  rotaryEncoder->setAcceleration(1);
  numberSelector.attachEncoder(rotaryEncoder);
  // example 1
  numberSelector.setRange(0, 100, 1, false, 1);
  numberSelector.setValue(50);

  /**************************** button2 ******************/
  // encoder push button
  SWEncoder.begin(SWEncoderPin, INPUT_PULLUP);
  SWEncoder.setDebounceTime(5);
  SWEncoder.setPressedHandler(pressed); // returns if still pressed

  // rotary select sw
  // position auto
  SSWAuto.begin(SSWAutoPin, INPUT_PULLUP);
  SSWAuto.setDebounceTime(5);
  SSWAuto.setPressedHandler(pressed); // works good with rotary

  // position alarm
  SSWAlarm.begin(SSWAlarmPin, INPUT_PULLUP);
  SSWAlarm.setDebounceTime(5);
  SSWAlarm.setPressedHandler(pressed);

  // position setup/off
  SSWOff.begin(SSWOffPin, INPUT_PULLUP);
  SSWOff.setDebounceTime(5);
  SSWOff.setPressedHandler(pressed);

  // position pump
  SSWPump.begin(SSWPumpPin, INPUT_PULLUP);
  SSWPump.setDebounceTime(5);
  SSWPump.setPressedHandler(pressed);

  /**********************************  menu  ****************************/
  serial_print_help();
}

void loop()
{
  // OLED_Display.clearDisplay();
  // OLED_Display.setCursor(0, 0);
  //  OLED_Display.println("Start");
  // OLED_Display.display();
  //  numberselector style
  NumberSelectorLoop();
  // rotary_loop();
  /*********************** Button2 **********************/
  SWEncoder.loop(); // Update the Bounce instance
  SSWAuto.loop();   // Update the Bounce instance
  SSWAlarm.loop();  // Update the Bounce instance
  SSWOff.loop();    // Update the Bounce instance
  SSWPump.loop();   // Update the Bounce instance

  Update_Display();
  // Alarm();
  // Pump();
  // CLPump();
  //  system mode
  switch (SSWMode)
  {
  case 0: // none
    Serial.println("SSWMode = 0");
    break;

  case 1: // Auto

    break;

  case 2: // alarm
    /// Serial.println("SSWMode = 2");

    // set alarm flag
    // clear pump flag
    // displayon flag
    // fill displaytimer
    // alarm test
    // alarm level
    // alarm symbol
    break;

  case 3: // off
          // Serial.println("SSWMode = 3");

    // clear alarm flag
    // clear pump flag
    // displayon flag
    // fill displaytimer
    // level
    // pump /alarm

    break;

  case 4: // pump
          // Serial.println("SSWMode = 4");

    // clear alarm flag
    // set pump flag
    // displayon flag
    // fill displaytimer
    break;

  default:
    Serial.println("SSWMode = default");
    break;
  }

  serial_handler();
}
/*************************************************************
 * subs
 * ***********************************************************/
/* void displayMenu()
{

  OLED_Display.clearDisplay();
  OLED_Display.setCursor(0, 0);
  OLED_Display.setTextSize(1);

  // Display the menu
}
 */
void serial_print_help()
{
  Serial.println("***************");
  Serial.println("w: go to previus item (up)");
  Serial.println("s: go to next item (down)");
  Serial.println("a: go back (right)");
  Serial.println("d: select \"selected\" item");
  Serial.println("?: print this help");
  Serial.println("h: print this help");
  Serial.println("***************");
}
void serial_handler()
{

  /* this goes inside loop
  void serialEvent() {
    int incomingByte = Serial.read();
                        Serial.readBytes();
                        Serial.readBytesUntil();
                        Serial.readString();
                        Serial.readStringUntil();

    // prints the received data
    Serial.print("I received: ");
    Serial.println((char)incomingByte);
  } */

  char inChar;
  if ((inChar = Serial.read()) > 0)
  {
    switch (inChar)
    {
    case 'w': // Previus item
      // ms.prev();
      // ms.display();
      break;

    case 's': // Next item
      // ms.next();
      // ms.display();
      break;

    case 'a': // Back pressed
      // ms.back();
      // ms.display();
      break;

    case 'd': // Select pressed
      // ms.select();
      // ms.display();
      break;

    case '?':
    case 'h': // Display help
      serial_print_help();
      break;

    default:
      break;
    }
  }
}

void Update_Display(void)
{
  if (DisplayState == OFF) // display off
  {

    if (SWEncoderFlag == ON) // was encbutton pressed
    {
      DisplayState = ON;
      DisplayTimer = 3600;
      SWEncoderFlag = OFF;
      // Serial.print(" Set Timer ");
    }
  }
  else // display on
  {
    if (DisplayTimer <= 0) // did we time out
    {
      DisplayState = OFF;
      DisplayTimer = 0;
    }
    else // count down display timer
    {
      DisplayTimer--;
      // Serial.print(" DisTimr=");
      // Serial.print(DisplayTimer);
    }
  }
  if (DisplayState == ON)
  {
    if (BTStatusFlag == OFF) //  BT OFF
    {
      switch (SSWMode)
      {
      case 0:

        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.print("Mode 0 Bad");
        OLED_Display.display();

        break;

      case 1: // auto

        /*         OLED_Display.clearDisplay();
                OLED_Display.setCursor(0, 0);
                // DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values);
                // DisplayEnvSensor(&OLED_Display, &Sensor_Env_Values);
                // OLED_Light(&OLED_Display, Count, &Sensor_Level_Values);
                OLED_Display.print("Mode 1 Auto");
                OLED_Display.display(); */
        /*
                if (ENCCountDwn)
                {
                  OLED_Display.clearDisplay();
                  OLED_Display.setCursor(0, 0);
                  OLED_Display.print("ENCCountDwn");
                  OLED_Display.display();
                  ENCCountDwn = 0;
                }
                if (ENCCountUp)
                {
                  OLED_Display.clearDisplay();
                  OLED_Display.setCursor(0, 0);
                  OLED_Display.print("ENCCountUp");
                  OLED_Display.display();
                  ENCCountUp = 0;
                }

                if ((ENCCountDwn == 0) && (ENCCountUp == 0))
                {
                  OLED_Display.clearDisplay();
                  OLED_Display.setCursor(0, 0);
                  OLED_Display.print("ENCCount None");
                  OLED_Display.display();
                }*/
        break;
      case 2: // mode sw alarm

        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.print("Mode 2 Alarm");
        OLED_Display.display();

        break;

      case 3: // off /setup

        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.print("Mode 2 off/setup");
        OLED_Display.display();

        break;

        break;

      case 4: // pump
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.print("Pump ON");
        OLED_Display.display();

        break;

      default:
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.print("Default = Bad");
        OLED_Display.display();

        break;
      }
    }
    else if (BTStatusFlag == ON) // mode sw to auto BT ON
    {

      OLED_Display.clearDisplay();
      OLED_Display.setCursor(0, 0);
      OLED_Display.print("BT Connected!");
      OLED_Display.display();
    }
  }
  else // blank display
  {
    OLED_Display.clearDisplay();
    OLED_Display.setCursor(0, 0);
    OLED_Display.display();
  }

  // Pump();
  // Alarm();
}
// Alaram Control
void Alarm(void)
{
  switch (SSWMode)
  {
  case 1:

    if (BTStatusFlag == OFF)
    {
      if (AMSwitchFlag == ON) // app in auto, mode auto
      {
        if (SensorLevelAnalog >= AlarmOnLevel)
        {
          AlarmFlag = ON;
        }

        if (SensorLevelAnalog <= AlarmOffLevel)
        {
          AlarmFlag = OFF;
        }
      }
      else // app in man
      {
        if (AlarmManFlag == ON)
        {
          AlarmFlag = ON;
        }
        else
        {
          AlarmFlag = OFF;
        }
      }
    }
    else // bt off, mode in auto
    {
      if (SensorLevelAnalog >= AlarmOnLevel)
      {
        AlarmFlag = ON;
      }

      if (SensorLevelAnalog <= AlarmOffLevel)
      {
        AlarmFlag = OFF;
      }

      /*       if (AlarmManFlag == ON)
            {
              AlarmFlag = ON;
            }
            else
            {
              AlarmFlag = OFF;
            } */
    }

    /* code */
    break;
  case 2: // man on
    AlarmFlag = ON;
    break;
  case 3: // man off
    AlarmFlag = OFF;
    break;
  default:

    break;
  }
  digitalWrite(AlarmPin, !AlarmFlag);
}

// Pump Control
void Pump(void)
{
  switch (SSWMode)
  {
  case 1:

    if (BTStatusFlag == ON)
    {
      if (AMSwitchFlag == ON) // app in auto
      {
        if (SensorLevelAnalog >= PumpOnLevel)
        {
          PumpFlag = ON;
          CLPumpRunOnce = ON;
        }

        if (SensorLevelAnalog <= PumpOffLevel)
        {
          PumpFlag = OFF;
        }
      }
      else // app in manual control
      {
        if (PumpManFlag == ON)
        {
          PumpFlag = ON;

          // limit pump level in man mode
          if (SensorLevelAnalog <= 10) ////------------ change this level after testing with pump
          {
            //   digitalWrite(PumpPin, OFF);
            //   PumpFlag = OFF;
          }
        }
        else
        {

          PumpFlag = OFF;
        }
      }
    }
    else // bt off, mode in auto
    {
      if (SensorLevelAnalog >= PumpOnLevel)
      {
        PumpFlag = ON;
        CLPumpRunOnce = ON;
      }

      if (SensorLevelAnalog <= PumpOffLevel)
      {
        PumpFlag = OFF;
      }
      /*
            if (PumpManFlag == ON)
            {
              PumpFlag = ON;
            }
            else
            {
              PumpFlag = OFF;
            } */
    }

    /* code */
    break;

  case 3: // man off
    PumpFlag = OFF;
    break;
  case 4: // man on
    PumpFlag = ON;
    break;

  default:

    break;
  }

  digitalWrite(PumpPin, !PumpFlag);
}
// CLPump Control
void CLPump(void)
{
  if (CLPumpRunOnce == ON && PumpFlag == OFF)
  {

    CLPumpFlag = ON;

    CLPumpRunOnce = OFF; // set in pump()
  }
  if (CLPumpManFlag == ON)
  {

    CLPumpFlag = ON;
  }
  else
  {

    CLPumpFlag = OFF;
  }
  digitalWrite(CLPumpPin, !CLPumpFlag);
}

////////////////////////////////// button  ///////////////////////////

// action for button pressed
void pressed(Button2 &btn)
{

  Serial.print("pressed ");

  if (btn == SWEncoder)
  {
    Serial.println("SWEncoder ");
    SWEncoderFlag = ON;
  }
  else if (btn == SSWAuto)
  {
    Serial.println("SSWAuto");
    SSWMode = 1;
    DisplayState = ON;

    numberSelector.setRange(0, 100, 1, false, 1);
    numberSelector.setValue(50);
  }
  else if (btn == SSWAlarm)
  {
    Serial.println("SSWAlarm");
    SSWMode = 2;
    DisplayState = ON;
  }
  else if (btn == SSWOff)
  {
    Serial.println("SSWOff");
    SSWMode = 3;
    DisplayState = ON;
  }
  else if (btn == SSWPump)
  {
    Serial.println("SSWPump");
    SSWMode = 4;
    DisplayState = ON;
  }
  else
  {
    Serial.println("no button");
    SSWMode = 0;
  }
}

/////////////////////////////////// encoder  /////////////////////////////
void NumberSelectorLoop()
{
  if (rotaryEncoder->encoderChanged())
  {
    ENCValue = numberSelector.getValue();

    if (OldENCValue > ENCValue)
    {
      ENCCountUp = OFF;
      ENCCountDwn = ON;
    }
    else if (OldENCValue < ENCValue)
    {
      ENCCountUp = ON;
      ENCCountDwn = OFF;
    }
    else if (OldENCValue == ENCValue)
    {
      ENCCountUp = OFF;
      ENCCountDwn = OFF;
    }

    Serial.print("ENCValue ");
    Serial.print(ENCValue);
    Serial.print(ENCCountUp);
    Serial.print(ENCCountDwn);

    // Serial.println(" ");

    OldENCValue = ENCValue;
  }

  /*   if (rotaryEncoder->isEncoderButtonClicked())
    {
      Serial.print("Selected value is ");
      Serial.print(numberSelector.getValue(), 1);
      Serial.println(" ***********************");
    } */
}

void rotary_loop()
{
  // dont print anything unless value changed
  if (rotaryEncoder->encoderChanged())
  {
    Serial.print("Value: ");
    Serial.println(rotaryEncoder->readEncoder());
  }
  if (rotaryEncoder->isEncoderButtonClicked())
  {
    rotary_onButtonClick();
  }
}