/******************************************************************
 Created - 2/2023
 Description :  ESP32 with a menu display on oled and controller by encoder

I originally had every thing working here first and used the result for
TurdFloaterFeather32_G. But some how I lost the last update. so I am
cutting from there and trimmed it down


              I2C Adr

                OLED - 0X3C  change to 128x64 display************

                ina3221/INA219 0x40
                * //Explanation of I2C address for INA3221:
                *   //INA3221_ADDR40_GND = 0b1000000, // A0 pin -> GND
                *   //INA3221_ADDR41_VCC = 0b1000001, // A0 pin -> VCC
                *   //INA3221_ADDR42_SDA = 0b1000010, // A0 pin -> SDA
                *   //INA3221_ADDR43_SCL = 0b1000011  // A0 pin -> SCL
                * * got a 3chan board and changed the resistors to
                * 8.20 * 20ma = 164mv
                * 7.50 * 20ma = 150mv
                * 6.19 * 20ma = 124mv
                *
              Define all pins
              I2C             23 I2c_SDA
                              22 I2c_SCL

              SD_Card - SPI
                              5 SCK
                              18 MOSI
                              19 MISO
                              33 SD_CS
              Inputs -
                              34 SensorPin analog
                              15,32,14,21 rotory select sw
                              4 encoder sw
                                enca, encb
                              36 blue tooth connect from bt board
              Outputs -
                              12 AlarmPin
                              13 PumpPin
                              27 CL Pump    // currently being used for toggle pin during SD_Update
              Bluetooth -
                              17 tx
                              16 rx
                              Name: TurdFloater
                              PSWD: 1234
                              Baud:19200,8,n,1
              debugger
                              GPIO12 — TDI
                              GPIO15 — TDO
                              GPIO13 — TCK
                              GPIO14 — TMS

              Relays: Pump, CL Pump
              Leds: Alarm, Pump, CL Pump, BTStatus
              switches: BT Connect(reset?), rotate knob auto/man,alarm,pump

******************************************************************/

/**********************************************
  Includes
**********************************************/
#include <Arduino.h>
#include <Preferences.h> //NVM
#include <WiFi.h>

#include "Wire.h"

#include "sensor_readings.h"
#include "OLED.h"

#include "Simple_Menu.h"
#include "Button2.h"
#include "AiEsp32RotaryEncoder.h"

// #include "INA3221.h"   // included in sensor_READINGS.H

/**********************************************
  Pin Definitions
**********************************************/

#define AlarmPin 12  // Alarm
#define PumpPin 13   //  Pump
#define CLPumpPin 27 //  Chlorine Pump

#define SD_CS 33 // SD Card

#define BTStatusPin 36 // bluetooth

// assign i2c pin numbers
#define I2c_SDA 23
#define I2c_SCL 22

// NVM Mode - EEPROM on ESP32
#define RW_MODE false
#define RO_MODE true

/*********************** button2  **********************/
// SSW SelectSWitch
#define SSWAutoPin 21 // auto/man pos
#define SSWAlarmPin 4 // alarm pos
#define SSWOffPin 25  // off pos
#define SSWPumpPin 26 // pump pos

#define SWEncoderPin 14 // enc push button

// Rotary Encoder
#define ROTARY_ENCODER_A_PIN 32
#define ROTARY_ENCODER_B_PIN 15
#define ROTARY_ENCODER_BUTTON_PIN SWEncoderPin // button handled by Button2
#define ROTARY_ENCODER_VCC_PIN -1              /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
#define ROTARY_ENCODER_STEPS 10

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
long CLPump_RunTime = 2000;

/*************************** BT APP Vars ********************/
int BTStatusFlag = OFF;
char data_in; // data received from Serial1 link BT
boolean DisplayState = ON;

// FLAGS
int Page1Once = 1;
int Page2Once = 0;

/**************************** Switches ****************************/
struct Select_SW Switch_State; // switch position
byte SWEncoderFlag = OFF;      // encoder push button
byte SSWMode = 1;              // position of select switch and encoder

/****************** misc vars ********************/
double Count = 0; // used for log save count

/******************* eeprom ******************/
Preferences Settings; // NVM

/**********************************************************************
*******************  Sub/Function Declarations
**********************************************************************/

void ReadLevelSensor(SDL_Arduino_INA3221 *LevSensor, LevelSensor *SensorLevelVal);

void WriteData(void); // save to eprom

void Alarm(void);  // alarm control auto/man & on/off
void Pump(void);   // pump control auto/man & on/off
void CLPump(void); // CLpump control on sets timer for off

/*************************************************************************
 ********************** Init Hardware
 ************************************************************************/

/*******************   oled display   **************/
// Declaration for an SSD1306 OLED_Display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 OLED_Display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/**********************  ina3221  ********************/
// #include "SDL_Arduino_INA3221.h"
static const uint8_t _INA_addr = 64; //  0x40 I2C address of sdl board

// tweeked the resistor value while measuring the current (@11.5ma center of 4-20ma) with a meter. To make the numbers very close.
// with sig gen
// ina3221(address, chan1 shunt miliohm, chan2 shunt miliohm, chan3 shunt miliohm)
SDL_Arduino_INA3221 ina3221(_INA_addr, 6205, 7530, 8220); // 6170, 7590, 8200);
// the 7.5ohm resistor works out he best. Shunt mv=~30-150, max out of register at 163.8mv.
//  this leaves some head room for when sensor fails and goes max
//  need to add test condition for <4ma(open) and >20ma (fault)
struct LevelSensor Sensor_Level_Values;
// values for 3 chan

/***********************  encoder  *********************/
// AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoder *rotaryEncoder = new AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
// AiEsp32RotaryEncoderNumberSelector numberSelector = AiEsp32RotaryEncoderNumberSelector();
//  void rotary_onButtonClick();
//  void rotary_loop();
void rotary_loop();
int ENCValue = 0;

/********************* encoder ISR  *********************/
void IRAM_ATTR readEncoderISR()
{
    // rotaryEncoder.readEncoder_ISR();    //old
    rotaryEncoder->readEncoder_ISR();
}

/*******************  switches **************************/
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

/****************************  menu code  ***************************/
menuFrame AlarmMenu; // runs when SSW in Alarm Position
menuFrame PumpMenu;  // runs when SSW in Pump Position
menuFrame TestMenu;  // runs when Program Starts

// menu call functions
void testFunct();
void PumpOnAdjust();
void PumpOffAdjust();
void AlarmOnAdjust();
void AlarmOffAdjust();
void CLTimeAdjust();
void VolumeAdjust();
void PumpToggle();
void AlarmToggle();
void CLPumpToggle();                //add to_G
void MenuChoose(int Mode);

/********  physical poistion of SSW ***********/
byte AlarmPositionFlag = OFF;
byte OffPositionFlag = OFF;
byte PumpPositionFlag = OFF;
byte AutoPositionFlag = OFF;

/******************************************************************************
 **************************************  SetUp
 *******************************************************************************/
void setup()
{

    // serial ports
    Serial.begin(57600); // debug
    Serial.println("Serial 0 Start");

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

    /********************   init i2c  *****************/
    Wire.begin(I2c_SDA, I2c_SCL);
    // bool status; // connect status
    Serial.println("I2C INIT OK");

    /********************* oled  ********************/
    oledSystemInit(&OLED_Display); // in Simple_Menu.cpp

    // set up parameters
    OLED_Display.setRotation(ROTATION);
    OLED_Display.setTextSize(1);
    OLED_Display.setTextColor(SSD1306_WHITE);
    OLED_Display.clearDisplay();
    OLED_Display.display();

    /*************************  ina3221  ************************/
    // setup ina3221 SDL lib
    ina3221.begin();
    // Serial.println("ina3221.begin");
    //  en/dis channel as needed. effects response time
    //  ina3221.setChannelEnable(INA3221_CH1);
    ina3221.setChannelEnable(INA3221_CH2);
    // ina3221.setChannelEnable(INA3221_CH3);
    ina3221.setChannelDisable(INA3221_CH1);
    // ina3221.setChannelDisable(INA3221_CH2);
    ina3221.setChannelDisable(INA3221_CH3);

    // values for avg, effects response time
    ina3221.setAveragingMode(INA3221_REG_CONF_AVG_64);
    //  Sets bus-voltage conversion time.
    ina3221.setBusConversionTime(INA3221_REG_CONF_CT_1100US);
    //  Sets shunt-voltage conversion time.
    ina3221.setShuntConversionTime(INA3221_REG_CONF_CT_1100US);

    /****************************   NVM   ************************/
    // test for first run time
    Settings.begin("storage", RO_MODE); // nvm storage space, set to read
    bool doesExist = Settings.isKey("NVSInit");
    if (doesExist == false)
    {

        Serial1.println("-----------------NVM first time----------------");
        // first time run code to create keys & assign their values
        Settings.end();                     // close the namespace in RO mode.
        Settings.begin("storage", RW_MODE); //  create and open it in RW mode.

        // load NVM with default values
        Settings.putInt("PumpOnLevel", 2000);
        Settings.putInt("PumpOffLevel", 1500);
        Settings.putInt("AlarmOnLevel", 2200);
        Settings.putInt("AlarmOffLevel", 2000);
        Settings.putInt("CLTimer", 25);
        Settings.putInt("AlarmVol", 50);
        Settings.putInt("NVSInit", true);
        Settings.end();                     // close the namespace
        Settings.begin("storage", RO_MODE); // nvm storage space, set to read
    }

    // load NVM into vars
    PumpOnLevel = Settings.getInt("PumpOnLevel");
    PumpOffLevel = Settings.getInt("PumpOffLevel");
    AlarmOnLevel = Settings.getInt("AlarmOnLevel");
    AlarmOffLevel = Settings.getInt("AlarmOffLevel");
    CLPump_RunTime = Settings.getInt("CLTimer");
    AlarmVol = Settings.getInt("AlarmVol");
    Settings.end(); // close the namespace

    /************************************ encoder  *********************/
    rotaryEncoder->begin();
    rotaryEncoder->setup(readEncoderISR);

    // minValue, maxValue, circleValues true|false (when max go to min and vice versa)
    // rotaryEncoder->setBoundaries(0, 2, false); // dosen't work good in true
    rotaryEncoder->setAcceleration(0); // use this with ROTARY_ENCODER_STEPS, acts like debouce and changes response
    rotaryEncoder->setEncoderValue(0); // enc start value

    /******************************** rotary select sw *************************/
    SSWAuto.begin(SSWAutoPin, INPUT_PULLUP);
    SSWAuto.setDebounceTime(50);
    // SSWAuto.setClickHandler(handler);                // bad with rotary
    //  SSWAuto.setLongClickDetectedHandler(handler);   // works good always thinks long
    // SSWAuto.setChangedHandler(changed);              // give current and next position
    SSWAuto.setPressedHandler(pressed); // works good with rotary

    SSWAlarm.begin(SSWAlarmPin, INPUT_PULLUP);
    SSWAlarm.setDebounceTime(50);
    // SSWAlarm.setClickHandler(handler);
    // SSWAlarm.setLongClickDetectedHandler(handler);
    // SSWAlarm.setChangedHandler(changed);
    SSWAlarm.setPressedHandler(pressed);

    SSWOff.begin(SSWOffPin, INPUT_PULLUP);
    SSWOff.setDebounceTime(50);
    // SSWOff.setClickHandler(handler);
    // SSWOff.setLongClickDetectedHandler(handler);
    // SSWOff.setChangedHandler(changed);
    SSWOff.setPressedHandler(pressed);
    // SSWOff.setReleasedHandler(released);
    // SSWOff.setTapHandler(tap);

    SSWPump.begin(SSWPumpPin, INPUT_PULLUP);
    SSWPump.setDebounceTime(50);
    // SSWPump.setClickHandler(handler);
    // SSWPump.setLongClickDetectedHandler(handler);
    // SSWPump.setChangedHandler(changed);
    SSWPump.setPressedHandler(pressed);

    /************************** Blue Tooth ******************/
    // this will download both screens to a new user
    // connect to bluetooth then press reset on eps32
    BTStatusFlag = digitalRead(BTStatusPin);

    /********************  menu ********************/
    // Pump Menu
    PumpMenu.addMenu("Pump mm", 0);
    // mainMenu.addNode("Pump Levels", SUB_NODE, NULL);
    //  mainMenu.linkNode(1);
    //  // Submenu 1
    //  mainMenu.addMenu("Pump mm", 1);
    PumpMenu.addNode("ON/OFF", ACT_NODE, &PumpToggle);
    PumpMenu.addNode("On Level", ACT_NODE, &PumpOnAdjust);
    PumpMenu.addNode("Off Level", ACT_NODE, &PumpOffAdjust);
    PumpMenu.addNode("CL Time", ACT_NODE, &CLTimeAdjust);

    // Alarm Menu
    AlarmMenu.addMenu("Alarm", 0);
    // mainMenu.addNode("Pump Levels", SUB_NODE, NULL);
    //  mainMenu.linkNode(1);
    //  // Submenu 1
    //  mainMenu.addMenu("Pump mm", 1);
    AlarmMenu.addNode("ON/OFF", ACT_NODE, &AlarmToggle);
    AlarmMenu.addNode("On Level", ACT_NODE, &AlarmOnAdjust);
    AlarmMenu.addNode("Off Level", ACT_NODE, &AlarmOffAdjust);

    // Testing menu
    TestMenu.addMenu("Testing", 0);
    TestMenu.addNode("PowerSupply", ACT_NODE, &testFunct);
    TestMenu.addNode("Sensor", ACT_NODE, &testFunct);
    TestMenu.addNode("ON/OFF", ACT_NODE, &testFunct);
    TestMenu.nodeIndex = 0;

    /****************   test menu run **********/
    TestMenu.build(&OLED_Display);
    // OLED_Display.display();
    delay(500);

    TestMenu.nodeIndex = 1;
    TestMenu.build(&OLED_Display);
    delay(500);

    TestMenu.nodeIndex = 2;
    TestMenu.build(&OLED_Display);
    delay(500);

    OLED_Display.clearDisplay();
    OLED_Display.setCursor(0, 0);
    OLED_Display.display();

    // Force to Auto Position
    PumpPositionFlag = OFF;
    AlarmPositionFlag = OFF;
    OffPositionFlag = OFF;
    AutoPositionFlag = ON;
    SSWMode = 1;
}

/**********************************************
  Run Loop
**********************************************/
void loop()
{

    /******************  encoder & PB ********************/
    rotary_loop();

    /*********************** Read Switches **********************/
    // SWEncoder.loop(); // Update Encoder Switch instance
    SSWAuto.loop();  // Update Select Switch Auto Position instance
    SSWAlarm.loop(); // Update Select Switch Alarm Position instance
    SSWOff.loop();   // Update Select Switch Off Position instance
    SSWPump.loop();  // Update Select Switch Pump Position instance

    /********************  blue tooth ************************/
    BTStatusFlag = digitalRead(BTStatusPin); // BTStatus from hc-05 mod

    SensorRead();
    DisplayUpdate();

    /********* run every loop *********/
    Pump();
    CLPump();
    Alarm();
}

/**************************************************************************************************
*******************************************  Sub/Function Definitions  ****************************
***************************************************************************************************/

void MenuChoose(int Mode)
{
    if (Mode == 2)
    {
        SWEncoderFlag = OFF;
        AlarmMenu.choose();
    }
    if (Mode == 4)
    {
        SWEncoderFlag = OFF;
        PumpMenu.choose();
    }
}

void PumpOnAdjust()
{
    // turn flag off
    SWEncoderFlag = OFF;

    // menu settings
    rotaryEncoder->setBoundaries((PumpOffLevel + 1), 3000, false);
    rotaryEncoder->setAcceleration(4000);
    // put current Pump Hi level here
    rotaryEncoder->setEncoderValue(PumpOnLevel);

    // stay in loop while changing value exit when push button pressed
    while (SWEncoderFlag == OFF)
    {

        // check enc and pushbutton
        rotary_loop();

        // set up display
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.setTextSize(2);

        OLED_Display.println("-Pump  ON-");
        OLED_Display.setCursor(0, 20);
        OLED_Display.printf(" %d", ENCValue);
        OLED_Display.setCursor(80, 20);
        OLED_Display.println("MM");

        OLED_Display.setTextSize(1);
        OLED_Display.println("");
        OLED_Display.println("  Rotate to change");
        OLED_Display.print(("   Press to enter"));
        OLED_Display.display();

        // Serial.printf("PumpHiAdjust while Loop SWEncoderFlag %d ENCVal %d\n\r", SWEncoderFlag, ENCValue);
    }
    // Serial.println("Left loop");
    //  may have to save changes here
    PumpOnLevel = ENCValue;
    WriteData();
    // put encvalue into pump level high

    // go back to Pump menu
    // set up menu settings for pump
    rotaryEncoder->setBoundaries(0, 3, false);
    rotaryEncoder->setAcceleration(0);
    rotaryEncoder->setEncoderValue(0); // stop indicator from jumping on next screen

    SWEncoderFlag = OFF;
    ENCValue = 0;
    PumpMenu.nodeIndex = 0;
    // PumpMenu.choose();
}

void PumpOffAdjust()
{

    // turn flag off
    SWEncoderFlag = OFF;

    // menu settings
    rotaryEncoder->setBoundaries(300, (PumpOnLevel - 1), false);
    rotaryEncoder->setAcceleration(2000);
    // put current Pump Hi level here
    rotaryEncoder->setEncoderValue(PumpOffLevel);

    // stay in loop while changing value exit when push button pressed
    while (SWEncoderFlag == OFF)
    {

        // check enc and pushbutton
        rotary_loop();

        // set up display
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.setTextSize(2);

        OLED_Display.println("-Pump OFF-");
        OLED_Display.setCursor(0, 20);
        OLED_Display.printf(" %d", ENCValue);
        OLED_Display.setCursor(80, 20);
        OLED_Display.println("MM");

        OLED_Display.setTextSize(1);
        OLED_Display.println("");
        OLED_Display.println("  Rotate to change");
        OLED_Display.print(("   Press to enter"));
        OLED_Display.display();

        // Serial.printf("PumpHiAdjust while Loop SWEncoderFlag %d ENCVal %d\n\r", SWEncoderFlag, ENCValue);
    }
    // Serial.println("Left loop");
    //  may have to save changes here
    PumpOffLevel = ENCValue;
    WriteData();
    // put encvalue into pump level high

    // go back to Pump menu
    // set up menu settings for pump
    rotaryEncoder->setBoundaries(0, 3, false);
    rotaryEncoder->setAcceleration(0);
    rotaryEncoder->setEncoderValue(0); // stop indicator from jumping on next screen

    SWEncoderFlag = OFF;
    ENCValue = 0;
    PumpMenu.nodeIndex = 0;
    // PumpMenu.choose();
}

void AlarmOnAdjust()
{
    // turn flag off
    SWEncoderFlag = OFF;

    // menu settings
    rotaryEncoder->setBoundaries((AlarmOffLevel + 1), 3000, false);
    rotaryEncoder->setAcceleration(3000);
    // put current Pump Hi level here
    rotaryEncoder->setEncoderValue(AlarmOnLevel);

    // stay in loop while changing value exit when push button pressed
    while (SWEncoderFlag == OFF)
    {

        // check enc and pushbutton
        rotary_loop();

        // set up display
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.setTextSize(2);

        OLED_Display.println("-Alarm On-");
        OLED_Display.setCursor(0, 20);
        OLED_Display.printf(" %d", ENCValue);
        OLED_Display.setCursor(80, 20);
        OLED_Display.println("MM");

        OLED_Display.setTextSize(1);
        OLED_Display.println("");
        OLED_Display.println("  Rotate to change");
        OLED_Display.print(("   Press to enter"));
        OLED_Display.display();

        // Serial.printf("PumpHiAdjust while Loop SWEncoderFlag %d ENCVal %d\n\r", SWEncoderFlag, ENCValue);
    }
    // Serial.println("Left loop");
    //  may have to save changes here
    AlarmOnLevel = ENCValue;
    WriteData();
    // put encvalue into pump level high

    // go back to Pump menu
    // set up menu settings for pump
    rotaryEncoder->setBoundaries(0, 2, false);
    rotaryEncoder->setAcceleration(0);
    rotaryEncoder->setEncoderValue(0); // stop indicator from jumping on next screen

    SWEncoderFlag = OFF;
    ENCValue = 0;
    AlarmMenu.nodeIndex = 0;
    // PumpMenu.choose();
}

void AlarmOffAdjust()
{
    // turn flag off
    SWEncoderFlag = OFF;

    // menu settings
    rotaryEncoder->setBoundaries(0, (AlarmOnLevel - 1), false);
    rotaryEncoder->setAcceleration(3000);
    // put current Pump Hi level here
    rotaryEncoder->setEncoderValue(AlarmOffLevel);

    // stay in loop while changing value exit when push button pressed
    while (SWEncoderFlag == OFF)
    {
        // check enc and pushbutton
        rotary_loop();

        // set up display
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.setTextSize(2);

        OLED_Display.println("-Alrm OFF-");
        OLED_Display.setCursor(0, 20);
        OLED_Display.printf(" %d", ENCValue);
        OLED_Display.setCursor(80, 20);
        OLED_Display.println("MM");

        OLED_Display.setTextSize(1);
        OLED_Display.println("");
        OLED_Display.println("  Rotate to change");
        OLED_Display.print(("   Press to enter"));
        OLED_Display.display();

        // Serial.printf("PumpHiAdjust while Loop SWEncoderFlag %d ENCVal %d\n\r", SWEncoderFlag, ENCValue);
    }
    // Serial.println("Left loop");
    //  may have to save changes here
    AlarmOffLevel = ENCValue;
    WriteData();
    // put encvalue into pump level high

    // go back to Pump menu
    // set up menu settings for pump
    rotaryEncoder->setBoundaries(0, 2, false);
    rotaryEncoder->setAcceleration(0);
    rotaryEncoder->setEncoderValue(0); // stop indicator from jumping on next screen

    SWEncoderFlag = OFF;
    ENCValue = 0;
    AlarmMenu.nodeIndex = 0;
    // PumpMenu.choose();
}

void CLTimeAdjust()
{

    // turn flag off
    SWEncoderFlag = OFF;

    // menu settings
    rotaryEncoder->setBoundaries(0, 30, false);
    rotaryEncoder->setAcceleration(0);
    // put current Pump Hi level here
    int x = CLPump_RunTime;
    rotaryEncoder->setEncoderValue(x);

    // stay in loop while changing value exit when push button pressed
    while (SWEncoderFlag == OFF)
    {
        // check enc and pushbutton
        rotary_loop();

        // set up display
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.setTextSize(2);

        OLED_Display.println("CL RunTime");
        OLED_Display.setCursor(0, 20);
        OLED_Display.printf(" %d", ENCValue);
        OLED_Display.setCursor(75, 20);
        OLED_Display.println("SEC");

        OLED_Display.setTextSize(1);
        OLED_Display.println("");
        OLED_Display.println("  Rotate to change");
        OLED_Display.print(("   Press to enter"));
        OLED_Display.display();

        // Serial.printf("PumpHiAdjust while Loop SWEncoderFlag %d ENCVal %d\n\r", SWEncoderFlag, ENCValue);
    }
    // Serial.println("Left loop");
    //  may have to save changes here
    CLPump_RunTime = ENCValue;
    WriteData();
    // put encvalue into pump level high

    // go back to Pump menu
    // set up menu settings for pump
    rotaryEncoder->setBoundaries(0, 3, false);
    rotaryEncoder->setAcceleration(0);
    rotaryEncoder->setEncoderValue(0); // stop indicator from jumping on next screen

    SWEncoderFlag = OFF;
    ENCValue = 0;
    PumpMenu.nodeIndex = 0;
    // PumpMenu.choose();
}

void VolumeAdjust()
{

    // turn flag off
    SWEncoderFlag = OFF;

    // menu settings
    rotaryEncoder->setBoundaries(0, 100, false);
    rotaryEncoder->setAcceleration(1);
    // put current Pump Hi level here
    rotaryEncoder->setEncoderValue(AlarmVol);

    // stay in loop while changing value exit when push button pressed
    while (SWEncoderFlag == OFF)
    {

        // check enc and pushbutton
        rotary_loop();

        // set up display
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.setTextSize(2);

        OLED_Display.println("- Volume -");
        OLED_Display.setCursor(0, 20);
        OLED_Display.printf(" %d", ENCValue);
        OLED_Display.setCursor(80, 20);
        OLED_Display.println("MM");

        OLED_Display.setTextSize(1);
        OLED_Display.println("");
        OLED_Display.println("  Rotate to change");
        OLED_Display.print(("   Press to enter"));
        OLED_Display.display();

        // Serial.printf("PumpHiAdjust while Loop SWEncoderFlag %d ENCVal %d\n\r", SWEncoderFlag, ENCValue);
    }
    // Serial.println("Left loop");
    //  may have to save changes here
    AlarmVol = ENCValue;
    WriteData();
    // put encvalue into pump level high

    // go back to Pump menu
    // set up menu settings for pump
    rotaryEncoder->setBoundaries(0, 3, false);
    rotaryEncoder->setAcceleration(0);
    rotaryEncoder->setEncoderValue(0); // stop indicator from jumping on next screen

    SWEncoderFlag = OFF;
    ENCValue = 0;
    PumpMenu.nodeIndex = 0;
    // PumpMenu.choose();
}

// toggle pump on/off
void PumpToggle()
{

    PumpManFlag = !PumpManFlag;
    digitalWrite(PumpPin, PumpManFlag);

    // add to _G toggle pump status
    // PumpStatus = ON;
}

// toggle alram on/off
void AlarmToggle()
{

    AlarmManFlag = !AlarmManFlag;
    digitalWrite(AlarmPin, AlarmManFlag);
    // add to _G toggle pump status
    // PumpStatus = ON;
}

// add to _G
void CLPumpToggle()
{

    CLPumpManFlag = !CLPumpManFlag;
    digitalWrite(AlarmPin, CLPumpManFlag);
    // add to _G toggle pump status
    // PumpStatus = ON;
}
// blank
void testFunct()
{
}

// send data to display
void DisplayUpdate(void)
{
    // Serial.println("DisplayUpdate()");
    if (BTStatusFlag == ON) // mode sw to auto BT ON
    {

        // Serial.println("DisplayUpdate() / DisplayState=ON / BTStatusFlag=ON");
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        OLED_Display.println("*** BT Connected! ***");

        DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values);

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

            int CLPRT = CLPump_RunTime;
            // Serial.println("DisplayUpdate() / DisplayState=ON / BTStatusFlag=OFF");
            switch (SSWMode)
            {

            case 0: // encoder sw

                break;

            case 1: // auto

                OLED_Display.clearDisplay();
                OLED_Display.setCursor(0, 0);

                DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values); // level sensor value
                OLED_Light(&OLED_Display, Count, &Sensor_Level_Values);  // sd write count

                OLED_Display.println("");
                OLED_Display.printf("Alarm On:  %d\r\n", AlarmOnLevel);
                OLED_Display.printf("Alarm Off: %d\r\n", AlarmOffLevel);
                OLED_Display.printf("Pump On:   %d\r\n", PumpOnLevel);
                OLED_Display.printf("PumpOff:   %d\r\n", PumpOffLevel);
                OLED_Display.printf("CL Time:   %d\r\n", CLPRT);

                OLED_Display.display();
                break;

            case 2: // mode sw alarm

                AlarmMenu.build(&OLED_Display);
                if (SWEncoderFlag)
                {
                    MenuChoose(2);
                    Serial.println("                AlarmMenuChoose");
                }
                break;

            case 3: // off

                OLED_Display.clearDisplay();
                OLED_Display.setCursor(0, 0);
                OLED_Display.setTextSize(2);
                OLED_Display.println("System OFF");
                OLED_Display.setTextSize(1);
                OLED_Display.printf("Alarm On:  %d\r\n", AlarmOnLevel);
                OLED_Display.printf("Alarm Off: %d\r\n", AlarmOffLevel);
                OLED_Display.printf("Pump On:   %d\r\n", PumpOnLevel);
                OLED_Display.printf("PumpOff:   %d\r\n", PumpOffLevel);
                OLED_Display.printf("CL Time:   %d\r\n", CLPRT);

                OLED_Display.display();

                break;

            case 4: // pump

                // DisplayState = ON;

                PumpMenu.build(&OLED_Display); // display menu
                if (SWEncoderFlag)
                {

                    MenuChoose(4);
                    Serial.printf("                PumpMenuChoose SWEncoderFlag %d", SWEncoderFlag);
                }

                break;

            default:

                OLED_Display.clearDisplay();
                OLED_Display.setCursor(0, 0);

                OLED_Display.print("Default = Bad");
                OLED_Display.display();

                break;
            }
        }
    }
    else // blank display
    {
        // Serial.println("DisplayUpdate() / DisplayState=OFF");
        // if (BTStatusFlag == ON)
        // {
        OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);
        // OLED_Display.println("BT Connected! DisplayStateOFF");
        OLED_Display.display();
        // }
    }
}

// read rotary encoder and Push Button
void rotary_loop()
{
    ///////////////////////////////////// encoder
    if (rotaryEncoder->encoderChanged())
    {

        ENCValue = rotaryEncoder->readEncoder();
        if (SSWMode == 4) // if in Pump position change pump menu
        {

            PumpMenu.nodeIndex = ENCValue; // move enc value to menu
            Serial.printf("PMPENC: %d \n\r", ENCValue);
        }

        if (SSWMode == 2)
        {

            AlarmMenu.nodeIndex = ENCValue;
            Serial.printf("ALMENC: %d \n\r", ENCValue);
        }
    }

    ///////////////////////////////////////////////// push button
    if (rotaryEncoder->isEncoderButtonClicked())
    {

        if (DisplayState == OFF)
        {

            Serial.println("SWEncoder DispON ");

            DisplayUpdate();
        }
        else
        {

            // only set flag inAlarm or Pump position
            if (PumpPositionFlag || AlarmPositionFlag)
            {

                SWEncoderFlag = ON;
                Serial.println("SWEncoderFlag On Pressed");
            }
            else // if you leave the SSW in Auto or Off, don't set flag for now. causes menu to change after SSW moved to any menu position
            {

                SWEncoderFlag = OFF;
                Serial.println("SWEncoderFlag Off Pressed");
            }
        }
    }
}

// read  select switch  SSW
// only runs when SSW changes position
void pressed(Button2 &btn)
{

    /*****************************************************************************
     * *****************  temp  ***************************************************
     * *************************************************************************/

    Serial.print("pressed ");

    // look at switch when BT NOT connected
    if (BTStatusFlag == OFF)
    {

        if (btn == SSWAuto)
        {

            Serial.println("SSWAuto");

            // keep track of SSW position
            PumpPositionFlag = OFF;
            AlarmPositionFlag = OFF;
            OffPositionFlag = OFF;
            AutoPositionFlag = ON;

            /* theses are set in prgam loop
                   PumpManFlag
                   AlarmManFlag
              */

            AutoManControl = ON;

            if (SSWMode != 1) // comming from another switch position
            {
                digitalWrite(PumpPin, OFF);
                digitalWrite(AlarmPin, OFF);
                digitalWrite(CLPumpPin, OFF);
                SSWMode = 1;

                // DisplayUpdate();
            }
        }

        else if (btn == SSWAlarm)
        {

            Serial.println("SSWAlarm");

            SSWMode = 2;
            // keep track of SSW position
            PumpPositionFlag = OFF;
            AlarmPositionFlag = ON;
            OffPositionFlag = OFF;
            AutoPositionFlag = OFF;

            PumpManFlag = OFF;
            AlarmManFlag = OFF; // set in menu

            AutoManControl = OFF;

            // blank display on SSW change
            // clear display and set up encoder for Alarm menu
            OLED_Display.clearDisplay();
            OLED_Display.setCursor(0, 0);
            OLED_Display.display();
            rotaryEncoder->setBoundaries(0, 2, false); // dosen't work good in true
        }

        else if (btn == SSWOff)
        {
            // Serial.println("SSWOff");
            SSWMode = 3;

            PumpPositionFlag = OFF;
            AlarmPositionFlag = OFF;
            OffPositionFlag = ON;
            AutoPositionFlag = OFF;

            PumpManFlag = OFF;
            AlarmManFlag = OFF;

            AutoManControl = OFF;
            digitalWrite(PumpPin, OFF);
            digitalWrite(AlarmPin, OFF);
            digitalWrite(CLPumpPin, OFF);
        }
        else if (btn == SSWPump)
        {
            Serial.println("SSWPump");
            SSWMode = 4;

            PumpPositionFlag = ON;
            AlarmPositionFlag = OFF;
            OffPositionFlag = OFF;
            AutoPositionFlag = OFF;

            PumpManFlag = OFF; // will be controlled by menu
            AlarmManFlag = OFF;

            AutoManControl = OFF;

            OLED_Display.clearDisplay();
            OLED_Display.setCursor(0, 0);
            OLED_Display.display();
            rotaryEncoder->setBoundaries(0, 3, false); // dosen't work good in true
        }
        else
        {
            // Serial.println("no button");
            // //SSWMode = 0;
        }
    }
    else // BT mode pretend SSW always in Auto Position
    {

        SSWMode = 1;

        PumpPositionFlag = OFF;
        AlarmPositionFlag = OFF;
        OffPositionFlag = OFF;
        AutoPositionFlag = ON;

        AutoManControl = ON;
    }
}

// Alaram Control
void Alarm(void)
{
    if (AutoManControl == ON)
    {
        if (Sensor_Level_Values.DepthMM >= AlarmOnLevel)
        {
            digitalWrite(AlarmPin, ON);
            AlarmStatus = ON;
            // DEBUGPRINT(" AutoAlarmStatusON ");
            // DEBUGPRINTLN(AlarmStatus);
        }

        if (Sensor_Level_Values.DepthMM <= AlarmOffLevel)
        {
            digitalWrite(AlarmPin, OFF);
            AlarmStatus = OFF;
            // DEBUGPRINT(" AutoAlarmStatusOFF ");
            // DEBUGPRINTLN(AlarmStatus);
        }
    }
    else // manual control
    {
        /*     if (AlarmManFlag == ON)
            {
              digitalWrite(AlarmPin, ON);
              AlarmStatus = ON;
              // DEBUGPRINT(" ManAlarmStatusON ");
              //   DEBUGPRINTLN(AlarmStatus);
            }
            else
            {
              digitalWrite(AlarmPin, OFF);
              AlarmStatus = OFF;
              // DEBUGPRINT(" ManAlarmStatusOFF ");
              //   DEBUGPRINTLN(AlarmStatus);
            } */
    }
}

// Pump Control
void Pump(void)
{
    if (AutoManControl == ON) // auto
    {
        if (Sensor_Level_Values.DepthMM >= PumpOnLevel)
        {
            digitalWrite(PumpPin, ON);
            PumpStatus = ON;
            Serial.println(" AutoPumpStatusON ");
            // DEBUGPRINTLN(PumpStatus);
            CLPumpRunOnce = ON;
        }

        if (Sensor_Level_Values.DepthMM <= PumpOffLevel)
        {
            digitalWrite(PumpPin, OFF);
            PumpStatus = OFF;
            // DEBUGPRINT(" AutoPumpStatusOFF ");
            //  DEBUGPRINTLN(PumpStatus);
        }
    }
    else // manual control
    {    ///////////////////////////////////////////////////// maybe changes this to PumpToggle

        /*     if (BTStatusFlag)
            {
              if (PumpManFlag == ON)
              {
                digitalWrite(PumpPin, ON);
                PumpStatus = ON;
                // DEBUGPRINT("ManPumpStatusON ");
                //  DEBUGPRINTLN(PumpStatus);
              }
              else
              {
                digitalWrite(PumpPin, OFF);
                PumpStatus = OFF;
                // DEBUGPRINT("ManPumpStatusOFF ");
                //  DEBUGPRINTLN(PumpStatus);
              }
            } */
    }
}

// CLPump Control
void CLPump(void)
{
    if (AutoManControl == ON) // auto
    {

        if (CLPumpRunOnce == ON && PumpStatus == OFF)
        {

            digitalWrite(CLPumpPin, ON);
            Serial.println("CLPump ON Auto");
            CLPumpStatus = ON;
            CLPumpRunOnce = OFF; // set in pump()
            // CLPumpTimer.once(CLPump_RunTime, CLPumpOFF);
            delay(CLPump_RunTime);
        }
    }
    else // manual control
    {
        if (CLPumpManFlag == ON)
        {
            digitalWrite(CLPumpPin, ON);
            CLPumpStatus = ON;
            // DEBUGPRINT("CLPumpStatusON Man ");
            //  DEBUGPRINTLN(CLPumpStatus);
        }
        else
        {
            digitalWrite(CLPumpPin, OFF);
            CLPumpStatus = OFF;
            // DEBUGPRINT("CLPumpStatusOFF Man ");
            //  DEBUGPRINTLN(CLPumpStatus);
        }
    }
}

// get sensor value
void SensorRead()
{
    // DEBUGPRINTLN("Read sensor***********");
    //  ReadEnvSensor(&bme, &Sensor_Env_Values);

    ReadLevelSensor(&ina3221, &Sensor_Level_Values);
}

void WriteData()
{

    Serial.println("--------- Write to NVM -----------");

    // write to NVM
    Settings.begin("storage", RW_MODE); //  create and open it in RW mode.
    // store NVM values
    Settings.putInt("PumpOnLevel", PumpOnLevel);
    Settings.putInt("PumpOffLevel", PumpOffLevel);
    Settings.putInt("AlarmOnLevel", AlarmOnLevel);
    Settings.putInt("AlarmOffLevel", AlarmOffLevel);
    Settings.putInt("CLTimer", CLPump_RunTime);
    Settings.putInt("AlarmVol", AlarmVol);
    Settings.end(); // close the namespace
}
