


/**********************************************
  Includes
**********************************************/
#include <Arduino.h>
#include <Preferences.h> //NVM
#include <WiFi.h>
#include <Ticker.h>

#include "Wire.h"
#include "network_config.h"

#include "settings.h"        // The order is important! for nvm
#include "sensor_readings.h" // The order is important!

// #include <ezTime.h>
#include "time.h"
////#include <TaskScheduler.h>
#include "RTClib.h"

#include "OLED.h"
//#include "SD_Card.h"

#include "Button2.h"
#include "AiEsp32RotaryEncoder.h"
#include "AiEsp32RotaryEncoderNumberSelector.h"

// #include <driver/adc.h> //adc
//#include "INA3221.h"   // included in sensor_READINGS.H

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

/************************ timer vars ************************/
Ticker SDTimer;         // how often to write to SD Card
Ticker APPTimer;        // how often to Update BT App
Ticker SensorTimer;     // how often to Read Sensor
Ticker DisPlayTimer;    // how often to update OLED
Ticker DisplayOffTimer; // when to blank display
Ticker CLPumpTimer;     // how long to run CLPump

float SD_interval = 6;              // sec for updating file on sd card
unsigned int APP_interval = 500;    // ms for updating BT panel
unsigned int Sensor_interval = 250; // ms for sensor reading
unsigned int DISP_interval = 250;   // ms for oled disp data update
float DISP_TimeOut = 15;            // sec how long before blank screen
float CLPump_RunTime = 5;           // sec for CL Pump to run

/**************************** Switches ****************************/
struct Select_SW Switch_State; // switch position
byte SWEncoderFlag = OFF;      // encoder push button
byte SSWMode = 0;              // position of select switch and encoder

/****************** misc vars ********************/
double Count = 0;            // used for log save count
boolean SDConnectOK = ON;    // SD card valid
boolean WiFiConnected = OFF; // WIFI Connected
byte SetUpFlag = 0;          ///////////////// oled menu inwork

/******************* eeprom ******************/
Preferences Settings; // NVM

/**********************************************************************
*******************  Sub/Function Declarations
**********************************************************************/

/******/ //void ReadLevelSensor(SDL_Arduino_INA3221 *LevSensor, LevelSensor *SensorLevelVal);

void Alarm(void);     // alarm control auto/man & on/off
void Pump(void);      // pump control auto/man & on/off
void CLPump(void);    // CLpump control on sets timer for off
void CLPumpOFF(void); // CLPump off

void BuildPanel(void); // builds app panels on phone

void DisplayData(void); // send serial data debug
// DateTime OLEDClock = rtc.now();

void SystemSetUp(void); /////////////////// oled menu

/********  ticker timers callback functions  *********/
void DisplayUpdate(void);        // update oled data
void DisplayUpdateSetFlag(void); // set flag to run display update
boolean DisplayUpdateFlag = ON;  // update flag

void DisplayOff(void);        // blanks disp
void DisplayOffSetFlag();     // set flag to run displayoff
boolean DisplayOffFlag = OFF; // update flag

void DisplayOn(void); // update disp on start blank timer

void SD_Update();           // write to sd file
void SD_UpdateSetFlag();    // set flag to run SD update
boolean SDUpdateFlag = OFF; // update flag

void SensorRead();            // read sensor value
void SensorReadSetFlag();     // set flag to run sd update
boolean SensorReadFlag = OFF; // update flag

void SendAppData();            // send data to app
void SendAppDataSetFlag();     // set flag to run app update
boolean SendAppDataFlag = OFF; // update flag

/*************************************************************************
 ********************** Init Hardware
 ************************************************************************/

/*******************   oled display   **************/
// Declaration for an SSD1306 OLED_Display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 OLED_Display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/*******************  rtc  *************************/
RTC_PCF8523 rtc; // on feather logger board

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6 * 60 * 60;
const int daylightOffset_sec = 0; // DISPTimeOut;
struct tm timeinfo;

/**********************  ina3221  ********************/
// #include "SDL_Arduino_INA3221.h"
static const uint8_t _INA_addr = 64; //  0x40 I2C address of sdl board

// tweeked the resistor value while measuring the current (@11.5ma center of 4-20ma) with a meter. To make the numbers very close.
// with sig gen
// ina3221(address, chan1 shunt miliohm, chan2 shunt miliohm, chan3 shunt miliohm)
/******/ //SDL_Arduino_INA3221 ina3221(_INA_addr, 6205, 7530, 8220); // 6170, 7590, 8200);
// the 7.5ohm resistor works out he best. Shunt mv=~30-150, max out of register at 163.8mv.
//  this leaves some head room for when sensor fails and goes max
//  need to add test condition for <4ma(open) and >20ma (fault)
struct LevelSensor Sensor_Level_Values;
// values for 3 chan

/***********************  encoder  *********************/
// AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoder *rotaryEncoder = new AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
AiEsp32RotaryEncoderNumberSelector numberSelector = AiEsp32RotaryEncoderNumberSelector();
// void rotary_onButtonClick();
// void rotary_loop();
void NumberSelectorLoop();
int ENCValue = 0;

void RotaryGroup(void);

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



/******************************************************************************
 **************************************  SetUp
 *******************************************************************************/
void setup()
{
    Serial.begin(57600);


 // serial ports
  Serial1.begin(9600); // bluetooth mod   needs to be 19200
  Serial.begin(57600); // debug
  DEBUGPRINTLN("Serial 0 Start");

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
  DEBUGPRINTLN("I2C INIT OK");

  /********************* oled  ********************/
  // SSD1306_SWITCHCAPVCC = generate OLED_Display voltage from 3.3V internally
  if (!OLED_Display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) // Address 0x3C for 128x32 board
  {
    DEBUGPRINTLN(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
  else
  {
    DEBUGPRINTLN("SSD1306 Init");
  }

  // Clear the oled buffer.
  OLED_Display.clearDisplay();
  OLED_Display.display();

  // set up parameters
  OLED_Display.setRotation(ROTATION);
  OLED_Display.setTextSize(1);
  OLED_Display.setTextColor(SSD1306_WHITE);


  /******************* encoder  *********************/
  rotaryEncoder->begin();
  rotaryEncoder->setup(readEncoderISR);
  rotaryEncoder->setAcceleration(1);
  numberSelector.attachEncoder(rotaryEncoder);
  // example 1
  numberSelector.setRange(0, 2, 1, false, 1);
  numberSelector.setValue(2);

    /*
    numberSelector.setRange parameters:
        float minValue,                set minimum value for example -12.0
        float maxValue,                set maximum value for example 31.5
        float step,                    set step increment, default 1, can be smaller steps like 0.5 or 10
        bool cycleValues,              set true only if you want going to miminum value after maximum 
        unsigned int decimals = 0      precision - how many decimal places you want, default is 0

    numberSelector.setValue - sets initial value    
    */


}

void loop()
{

RotaryGroup();
}

void RotaryGroup()
{

    if (rotaryEncoder->encoderChanged())
    {
        Serial.print(numberSelector.getValue());
        Serial.println(" ");
    }

    if (rotaryEncoder->isEncoderButtonClicked())
    {
        Serial.print("Selected value is ");
        Serial.print(numberSelector.getValue(), 1);
        Serial.println(" ***********************");
    }

}