// Testing simple menu
/*

Modified Simple_Menu.h/.CPP
so menu can be called from main
don't have to call the display display. anymore can us any name







*/

#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "oled.h"
#include "Simple_Menu.h"
#include "Simple_Menu.h"
#include "Button2.h"
#include "AiEsp32RotaryEncoder.h"
#include "AiEsp32RotaryEncoderNumberSelector.h"
#include "network_config.h"

#include "settings.h"        // The order is important! for nvm
#include "sensor_readings.h" // The order is important!

// #include <ezTime.h>
#include "time.h"
////#include <TaskScheduler.h>
#include "RTClib.h"

/**********************************************
  Pin Definitions
**********************************************/
/********************************* changed pin definition on ver F
 * stopped using analog and allowed for better connections  ****************/
#define AlarmPin 12  // Alarm
#define PumpPin 13   //  Pump
#define CLPumpPin 27 //  Chlorine Pump
// #define SensorPin 34 ////////////////// Sensor

#define SD_CS 33 // SD Card

#define BTStatusPin 36 // bluetooth

// assign i2c pin numbers
#define I2c_SDA 23
#define I2c_SCL 22

// NVM Mode - EEPROM on ESP32
#define RW_MODE false
#define RO_MODE true

// Rotary Encoder
#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 15
#define ROTARY_ENCODER_BUTTON_PIN -1 // button handled by Button2
#define ROTARY_ENCODER_VCC_PIN -1    /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
#define ROTARY_ENCODER_STEPS 4

/*********************** button2  **********************/
// SSW SelectSWitch
#define SSWAutoPin 21 // auto/man pos
#define SSWAlarmPin 4 // alarm pos
#define SSWOffPin 25  // off pos
#define SSWPumpPin 26 // pump pos

#define SWEncoderPin 14 // enc push button

/**********************************************
  Global Vars
**********************************************/

const byte ON = 1;
const byte OFF = 0;

int AlarmOnLevel = 0;  // value for AlarmLevel to ON
int AlarmOffLevel = 0; // value for AlarmLevel to OFF
int PumpOnLevel = 0;   // value for PumpOnLevel
int PumpOffLevel = 0;  // value for PumpOffLevel
int AlarmVol = 0;

// status flags
byte PumpStatus = OFF;    // Pump On/Off
byte PumpManFlag = OFF;   // pump sw state
byte AlarmStatus = OFF;   // Alarm On/Off
byte AlarmManFlag = OFF;  // Alarm sw state
byte AutoManControl = ON; // auto/manual sw state
byte CLPumpStatus = OFF;  // CLPump On/Off
byte CLPumpManFlag = OFF; // CLpump sw state
byte CLPumpRunOnce = OFF; // run CLPump after Pump stops

float SD_interval = 6;              // sec for updating file on sd card
unsigned int APP_interval = 500;    // ms for updating BT panel
unsigned int Sensor_interval = 250; // ms for sensor reading
unsigned int DISP_interval = 250;   // ms for oled disp data update
float DISP_TimeOut = 15;            // sec how long before blank screen
float CLPump_RunTime = 5;           // sec for CL Pump to run

/*************************** BT APP Vars ********************/
int BTStatusFlag = OFF;
char data_in; // data received from Serial1 link BT

// app flags
byte PumpManFl = OFF;
byte AlarmManFl = OFF;
byte AMSwitchFl = OFF;
byte CLPumpManFl = OFF;

// sliders
int PumpOnLevelSliderValue;
int PumpOffLevelSliderValue;
int AlarmOnLevelSliderValue;
int AlarmOffLevelSliderValue;
int CLTimerSliderValue;
int AlarmVolSliderValue;

// digits
int PumpOnLevelDisplayValue;
int PumpOffLevelDisplayValue;
int AlarmOnLevelDisplayValue;
int AlarmOffLevelDisplayValue;
int CLTimerDisplayValue;
int AlarmVolDisplayValue;
boolean DisplayState = ON;

// misc
String text;          // String for text elements
int red, green, blue; // RGB color
int bubbles;          // Bubble Gauge Value

int PumpPlotVal = 0;
int AlarmPlotVal = 0;
int CLPumpPlotVal = 0;

// FLAGS
int Page1Once = 1;
int Page2Once = 0;

Adafruit_SSD1306 OLED_Display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
void DisplayData(void); // send serial data debug
// DateTime OLEDClock = rtc.now();
// void DisplayOn(void); // update disp on start blank timer
void DisplayUpdate(void);

/**************************** Switches ****************************/
struct Select_SW Switch_State; // switch position
byte SWEncoderFlag = OFF;      // encoder push button
// byte SSWMode = 0;              // position of select switch and encoder
int SSWMode = 1;
/***********************  encoder  *********************/
// AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoder *rotaryEncoder = new AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoderNumberSelector numberSelector = AiEsp32RotaryEncoderNumberSelector();
// void rotary_onButtonClick();
// void rotary_loop();
void NumberSelectorLoop();
int ENCValue = 0;
/********************* encoder ISR  *********************/
void IRAM_ATTR readEncoderISR()
{
  // rotaryEncoder.readEncoder_ISR();    //old
  rotaryEncoder->readEncoder_ISR();
}

/*******************  switches **************************/
// Instantiate switch
Button2 SWEncoder;
Button2 SSWAuto;
Button2 SSWAlarm;
Button2 SSWOff;
Button2 SSWPump;

// types of switch handlers
// void handler(Button2 &btn);
// void longpress(Button2 &btn);
void pressed(Button2 &btn); // when button/sw pressed
// void released(Button2 &btn);
// void changed(Button2 &btn);
// void tap(Button2 &btn);
// byte myButtonStateHandler();
// void myTapHandler(Button2 &btn);

// void DisplayData(void); // send serial data debug
//  DateTime OLEDClock = rtc.now();
void testFunct();
void PumpHiAdjust();
void PumpLowAdjust();
void AlarmHiAdjust();
void AlarmLowAdjust();
void CLTimeAdjust();
void VolumeAdjust();
void MenuBack();
void PumpChoose();

menuFrame AlarmMenu;
menuFrame PumpMenu;
byte AlarmPositionFlag = OFF;
byte OffPositionFlag = OFF;
byte PumpPositionFlag = OFF;

boolean MenuUPFlag = OFF;
boolean MenuDWNFlag = OFF;

void setup()
{

  Serial.begin(57600);
  /***************************** pin properties  ******************/
  // relay
  pinMode(AlarmPin, OUTPUT);
  pinMode(PumpPin, OUTPUT);
  pinMode(CLPumpPin, OUTPUT);

  // select sw
  pinMode(SSWAutoPin, INPUT_PULLUP);
  pinMode(SSWAlarmPin, INPUT_PULLUP);
  pinMode(SSWOffPin, INPUT_PULLUP);
  pinMode(SSWPumpPin, INPUT_PULLUP);

  // BT mod
  pinMode(BTStatusPin, INPUT_PULLUP);

  //// turn off outputs
  digitalWrite(AlarmPin, OFF);
  digitalWrite(PumpPin, OFF);
  digitalWrite(CLPumpPin, OFF);

  /********************* oled  ********************/
  // SSD1306_SWITCHCAPVCC = generate OLED_Display voltage from 3.3V internally
  /*   if (!OLED_Display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) // Address 0x3C for 128x32 board
    {
      Serial.println(F("SSD1306 allocation failed"));
      for (;;)
        ; // Don't proceed, loop forever
    }
    else
    {
      Serial.println("SSD1306 Init");
      OLED_Display.clearDisplay();
      OLED_Display.setCursor(0,0);
      OLED_Display.println("SSD1306 Init");
      OLED_Display.display();
      delay(1000);

    } */
  oledSystemInit(&OLED_Display);
  /******************* encoder  *********************/
  rotaryEncoder->begin();
  rotaryEncoder->setup(readEncoderISR);
  rotaryEncoder->setAcceleration(1);
  numberSelector.attachEncoder(rotaryEncoder);
  // example 1
  numberSelector.setRange(0, 100, 1, false, 1);
  numberSelector.setValue(50);

  // encoder switch
  SWEncoder.begin(SWEncoderPin, INPUT_PULLUP);
  // SWEncoder.setLongClickTime(1000);
  SWEncoder.setDebounceTime(25);

  // SWEncoder.setChangedHandler(changed);             // trigger on press and release
  SWEncoder.setPressedHandler(pressed); // returns if still pressed

  /******************************** rotary select sw *************************/
  SSWAuto.begin(SSWAutoPin, INPUT_PULLUP);
  SSWAuto.setDebounceTime(25);
  // SSWAuto.setClickHandler(handler);                // bad with rotary
  //  SSWAuto.setLongClickDetectedHandler(handler);   // works good always thinks long
  // SSWAuto.setChangedHandler(changed);              // give current and next position
  SSWAuto.setPressedHandler(pressed); // works good with rotary

  SSWAlarm.begin(SSWAlarmPin, INPUT_PULLUP);
  SSWAlarm.setDebounceTime(25);
  // SSWAlarm.setClickHandler(handler);
  // SSWAlarm.setLongClickDetectedHandler(handler);
  // SSWAlarm.setChangedHandler(changed);
  SSWAlarm.setPressedHandler(pressed);

  SSWOff.begin(SSWOffPin, INPUT_PULLUP);
  SSWOff.setDebounceTime(25);
  // SSWOff.setClickHandler(handler);
  // SSWOff.setLongClickDetectedHandler(handler);
  // SSWOff.setChangedHandler(changed);
  SSWOff.setPressedHandler(pressed);
  // SSWOff.setReleasedHandler(released);
  // SSWOff.setTapHandler(tap);

  SSWPump.begin(SSWPumpPin, INPUT_PULLUP);
  SSWPump.setDebounceTime(25);
  // SSWPump.setClickHandler(handler);
  // SSWPump.setLongClickDetectedHandler(handler);
  // SSWPump.setChangedHandler(changed);
  SSWPump.setPressedHandler(pressed);

  /********************  menu ********************/
  // oledSystemInit(&OLED_Display);
  /*
    // Main menu
    //mainMenu.addMenu("SetUp", 0);
    //mainMenu.addNode("Pump Levels", SUB_NODE, NULL);
    //mainMenu.linkNode(1);

    mainMenu.addNode("Alarm Levels", SUB_NODE, NULL);
    mainMenu.linkNode(2);

    mainMenu.addNode("CL Timer", SUB_NODE, NULL);
    mainMenu.linkNode(3);

    mainMenu.addNode("Volume", SUB_NODE, NULL);
    mainMenu.linkNode(4);

    // // Submenu 1
    // mainMenu.addMenu("Pump mm", 1);
    // mainMenu.addNode("High mm", ACT_NODE, &PumpHiAdjust);
    // mainMenu.addNode("Low", ACT_NODE, &PumpLowAdjust);
    // mainMenu.addNode("Back", ACT_NODE, &MenuBack);
    // // mainMenu.addNode("SubM1 Node 4", ACT_NODE, &testFunct);

    // Submenu 2
    mainMenu.addMenu("Alarm mm", 2);
    mainMenu.addNode("High mm", ACT_NODE, &AlarmHiAdjust);
    mainMenu.addNode("Low", ACT_NODE, &AlarmLowAdjust);
    mainMenu.addNode("Back", ACT_NODE, &MenuBack);
    // mainMenu.addNode("SubM2 Node 4", ACT_NODE, &testFunct);
    // mainMenu.addNode("SubM2 Node 5", ACT_NODE, &testFunct);

    // Submenu 3
    mainMenu.addMenu("CL Timer", 3);
    mainMenu.addNode("Time Sec", ACT_NODE, &CLTimeAdjust);
    mainMenu.addNode("Back", ACT_NODE, &MenuBack);
    // mainMenu.addNode("SubM3 Node 4", ACT_NODE, &testFunct);
    // mainMenu.addNode("SubM3 Node 5", ACT_NODE, &testFunct);
    // mainMenu.addNode("SubM3 Node 6", ACT_NODE, &testFunct);

    // Submenu 4
    mainMenu.addMenu("Volume", 4);
    mainMenu.addNode("Vol %", ACT_NODE, &VolumeAdjust);
    mainMenu.addNode("Back", ACT_NODE, &testFunct);
    // mainMenu.addNode("SubM3 Node 4", ACT_NODE, &testFunct);
    // mainMenu.addNode("SubM3 Node 5", ACT_NODE, &testFunct);
    // mainMenu.addNode("SubM3 Node 6", ACT_NODE, &testFunct); */
  PumpMenu.addMenu("Pump", 0);
  // mainMenu.addNode("Pump Levels", SUB_NODE, NULL);
  //  mainMenu.linkNode(1);
  //  // Submenu 1
  //  mainMenu.addMenu("Pump mm", 1);
  PumpMenu.addNode("High mm", ACT_NODE, &PumpHiAdjust);
  PumpMenu.addNode("Low", ACT_NODE, &PumpLowAdjust);
  PumpMenu.addNode("Back", ACT_NODE, &MenuBack);
  AlarmMenu.addMenu("Pump", 0);
  // mainMenu.addNode("Pump Levels", SUB_NODE, NULL);
  //  mainMenu.linkNode(1);
  //  // Submenu 1
  //  mainMenu.addMenu("Pump mm", 1);
  AlarmMenu.addNode("High mm", ACT_NODE, &PumpHiAdjust);
  AlarmMenu.addNode("Low", ACT_NODE, &PumpLowAdjust);
  AlarmMenu.addNode("Back", ACT_NODE, &MenuBack);
}

/**************************************************************************/
/*****************************  loop  **********************************/
void loop()
{
  // mainMenu.build(&OLED_Display);
  /******************  encoder  ********************/
  // numberselector style
  NumberSelectorLoop();

  /*********************** Read Switches **********************/
  SWEncoder.loop(); // Update Encoder Switch instance
  SSWAuto.loop();   // Update Select Switch Auto Position instance
  SSWAlarm.loop();  // Update Select Switch Alarm Position instance
  SSWOff.loop();    // Update Select Switch Off Position instance
  SSWPump.loop();   // Update Select Switch Pump Position instance

  if (Serial.available())
  {
    switch (Serial.read())
    {
    case 'u': // up

      switch (SSWMode)
      {
      case 3: // alarm position
        // AlarmMenu.up();
        break;
      case 4: // pump position
        PumpMenu.up();
        break;
      }
      break;

    case 'd':

      switch (SSWMode)
      {
      case 3:
        // AlarmMenu.down();
        break;
      case 4:
        PumpMenu.down();
        break;
      }
      break;

    case 'c':

      switch (SSWMode)
      {
      case 3:
        // AlarmMenu.choose();
        break;
      case 4:
        PumpMenu.choose();
        break;
      }
      break;

    case 'b':
      // Need to backlink nodes to menus with another variable "linkedNode"

      switch (SSWMode)
      {
      case 3:
        // AlarmMenu.back();
        break;
      case 4:
        PumpMenu.back();
        break;
      }
      break;

    default:

      break;
    }
  }

  DisplayUpdate();
  delay(100);
}

/*************************************************************/
/*********************  subs  ********************************/
void PumpChoose(void)
{

  PumpMenu.choose();
}
void PumpHiAdjust()
{

  /*
  Cls
  Title Pump large font
  Next line High in Large font
  Next Line Value in large
  rotate value change
  press to accept and menu back
  */

  OLED_Display.clearDisplay();
  OLED_Display.print("Inside Pump HI Adj");
  OLED_Display.display();
  ;

  delay(1000);
}
void PumpLowAdjust()
{

  OLED_Display.clearDisplay();
  OLED_Display.print("Inside Pump LOW Adj");
  OLED_Display.display();
  delay(1000);
}
void AlarmHiAdjust()
{
  OLED_Display.clearDisplay();
  OLED_Display.print("Inside AlarmHI Adj");
  OLED_Display.display();
  delay(1000);
}
void AlarmLowAdjust()
{
  OLED_Display.clearDisplay();
  OLED_Display.print("Inside AlarmLOW Adj");
  OLED_Display.display();
  delay(1000);
}
void CLTimeAdjust()
{
  /*
    Cls
  Title large font
  Next line Time in Large font
  Next Line Value in large
  rotate value change
  press to accept and menu back


  */
  OLED_Display.clearDisplay();
  OLED_Display.print("Inside CLTime Adj");
  OLED_Display.display();
  delay(1000);
}
void VolumeAdjust()
{

  // same as cl
  OLED_Display.clearDisplay();
  OLED_Display.print("Inside Volume Adj");
  OLED_Display.display();
  delay(1000);
}
void MenuBack()
{

  // go back a level
  PumpMenu.back();
}

void testFunct()
{
  Serial.println("Function successfully called");
}

// send data to oled
void DisplayUpdate(void)
{

  /////////////////////////////////////////////debug
  BTStatusFlag = OFF;
  DisplayState = ON;
  AutoManControl = OFF;
  // SSWMode = 1;

  // Serial.println("DisplayUpdate()");
  if (BTStatusFlag == ON) // mode sw to auto BT ON
  {
    // Serial.println("DisplayUpdate() / DisplayState=ON / BTStatusFlag=ON");
    OLED_Display.clearDisplay();
    OLED_Display.setCursor(0, 0);
    OLED_Display.println("*** BT Connected! ***");

    ////////////////////////////////////DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values);
    // DisplayEnvSensor(&OLED_Display, &Sensor_Env_Values);
    // OLED_Light(&OLED_Display, Count, &Sensor_Level_Values);
    OLED_Display.printf("Pump: %d\n", PumpStatus);
    OLED_Display.printf(" On: %d Off: %d\n", PumpOnLevel, PumpOffLevel);
    OLED_Display.printf("Alarm: %d\n", AlarmStatus);
    OLED_Display.printf(" On: %d Off: %d\n", AlarmOnLevel, AlarmOffLevel);
    OLED_Display.printf("CLPmp: %d\n", CLPumpStatus);
    int x = CLPump_RunTime;
    OLED_Display.printf(" RunTime: %d \n", x);

    OLED_Display.display();
    return;
  }

  if (DisplayState == ON)
  {
    // Serial.println("DisplayUpdate() / DisplayState=ON");
    if (BTStatusFlag == OFF) //  BT OFF
    {
      // Serial.println("DisplayUpdate() / DisplayState=ON / BTStatusFlag=OFF");
      switch (SSWMode)
      {
      case 0: // encoder sw
        if (PumpPositionFlag && SWEncoderFlag)
        {
          PumpMenu.choose();
          SWEncoderFlag = OFF;
          SSWMode = 4;
        }
        /*         OLED_Display.clearDisplay();
                OLED_Display.setCursor(0, 0);
                OLED_Display.print("Mode 0 = Encoder");
                OLED_Display.display();
                Serial.println("Mode 0 = Encoder");
                // if Blanked and in auto Mode, when Encoder switch pressed it will turn on and update display
                if (AutoManControl)
                {

                  OLED_Display.clearDisplay();
                  OLED_Display.setCursor(0, 0);

                  // ///////////////////// cut DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values);
                  // DisplayEnvSensor(&OLED_Display, &Sensor_Env_Values);

                  // ///////////////////// cut OLED_Light(&OLED_Display, Count, &Sensor_Level_Values);

                  OLED_Display.display();
                } */

        break;

      case 1: // auto

        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);

        // //////////////////////////////////// cut DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values);
        // DisplayEnvSensor(&OLED_Display, &Sensor_Env_Values);

        //////////////////////////////////////// cut OLED_Light(&OLED_Display, Count, &Sensor_Level_Values);
        OLED_Display.print("Mode 1 = Auto");
        OLED_Display.display();
        Serial.println("Mode 1 = Auto");
        break;

      case 2: // mode sw alarm

        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);

        OLED_Display.print("Mode 2 = Alarm");
        OLED_Display.display();
        Serial.println("Mode 2 = Alarm");
        break;

      case 3: // off /setup
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);

        OLED_Display.println("Mode 3 = SetUp");

        OLED_Display.display();
        Serial.println("Mode 3 = Setup");
        break;
      case 4: // pump
        Serial.println("Mode 4 = Pump");
        PumpMenu.build(&OLED_Display);
        // if (SWEncoderFlag)
        // {
        //   Serial.println("                PumpMenuChoose");
        //   PumpChoose();
        //   SWEncoderFlag = OFF;
        // }

        break;
      default:
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);

        OLED_Display.print("Default = Bad");
        OLED_Display.display();
        Serial.println("Default ModeSW DisplayUpdate");

        break;
      }
    }
  }
  else // blank display
  {
    // Serial.println("DisplayUpdate() / DisplayState=OFF");
    // if (BTStatusFlag == ON)
    // {
    // OLED_Display.clearDisplay();
    // OLED_Display.setCursor(0, 0);
    // // OLED_Display.println("BT Connected! DisplayStateOFF");
    // OLED_Display.display();
    // }
  }
}

// read rotary encoder
void NumberSelectorLoop()
{
    if (rotaryEncoder->encoderChanged())
    {
      ENCValue = numberSelector.getValue();
    }
  /*   static int OldENCValue;
    if (rotaryEncoder->encoderChanged())
    {
      ENCValue = numberSelector.getValue();
      if (PumpPositionFlag)
      {
        PumpMenu.nodeIndex = ENCValue;
        PumpMenu.
      }
    }
    if (ENCValue < OldENCValue)
    {
      MenuDWNFlag = ON;
    }
    // Serial.print(ENCValue);
    // Serial.println(" ");
    OldENCValue = ENCValue; */
}

// read switches
void pressed(Button2 &btn)
{
  ////////////////////////////////////// debug
  DisplayState = ON;
  /*****************************************************************************
   * *****************  temp  ***************************************************
   * *************************************************************************
  // btn = SSWAuto;

          PumpStatus = OFF;
        PumpManFlag = OFF;
        AlarmStatus = OFF;
        AlarmManFlag = OFF;
        CLPumpStatus = OFF;
      CLPumpManFlag = OFF;
  ******************************/

  // Serial.print("pressed ");
  //  if (DisplayState == OFF)
  //  {
  //    Serial.println("SWEncoder DispON ");
  //    // DisplayOn();
  //    DisplayState = ON;
  //    DisplayUpdate();
  //  }
  if (btn == SWEncoder)
  {

    SSWMode = 0;

    Serial.printf("SWEncoder Mode %d", SSWMode);

    if (DisplayState == OFF)
    {
      // Serial.println("SWEncoder DispON ");
      ////////////////////////////////////// cut DisplayOn();
      // DisplayState = ON;
      // DisplayUpdate();
    }
    else
    {
      Serial.println("SWEncoderFlag ");
      SWEncoderFlag = ON;
    }
  }

  // look at switch when BT NOT connected
  if (BTStatusFlag == OFF)
  {
    if (btn == SSWAuto)
    {
      // Serial.println("SSWAuto");

      if (SSWMode != 1)
      {

        ///////////////////////// cut DisplayOn();
        // DisplayState = ON;
        // DisplayUpdate();
      }
      SSWMode = 1;

      PumpPositionFlag = OFF;
      AlarmPositionFlag = OFF;
      OffPositionFlag = OFF;

      AutoManControl = ON;
      // DisplayState = ON;
    }
    else if (btn == SSWAlarm)
    {
      // Serial.println("SSWAlarm");
      SSWMode = 2;

      PumpPositionFlag = OFF;
      AlarmPositionFlag = ON;
      OffPositionFlag = OFF;

      PumpManFlag = OFF;
      AlarmManFlag = ON;

      AutoManControl = OFF;

      if (DisplayState == OFF)
      {
        // DisplayOn();
        DisplayState = ON;
        DisplayUpdate();
      }
    }

    else if (btn == SSWOff)
    {
      // Serial.println("SSWOff");
      SSWMode = 3;

      PumpPositionFlag = OFF;
      AlarmPositionFlag = OFF;
      OffPositionFlag = ON;

      PumpManFlag = OFF;
      AlarmManFlag = OFF;

      AutoManControl = OFF;

      if (DisplayState == OFF)
      {
        // DisplayOn();
        DisplayState = ON;
        DisplayUpdate();
      }
      // SystemSetUp();
      /*     if (SWEncoderFlag == ON)
                {
                  SetUpFlag = 1;
                  SWEncoderFlag = OFF;
                  SystemSetUp();
                } */
    }
    else if (btn == SSWPump)
    {
      // Serial.println("SSWPump");
      SSWMode = 4;

      PumpPositionFlag = ON;
      AlarmPositionFlag = OFF;
      OffPositionFlag = OFF;

      PumpManFlag = ON;
      AlarmManFlag = OFF;

      AutoManControl = OFF;

      // if (DisplayState == OFF)
      // {
      //   // DisplayOn();
      //   DisplayState = ON;
      //   DisplayUpdate();
      // }
      OLED_Display.clearDisplay();
      OLED_Display.setCursor(0, 0);
      OLED_Display.display(); // main menu
      numberSelector.setRange(0, 100, 1, false, 1);
      numberSelector.setValue(50);
      // mainMenu.addNode("SubM1 Node 4", ACT_NODE, &testFunct);
    }
    else
    {
      // Serial.println("no button");
      // //SSWMode = 0;
    }
  }
}
