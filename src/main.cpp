//Testing simple menu
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


Adafruit_SSD1306 OLED_Display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void testFunct();
void PumpHiAdjust();
void PumpLowAdjust();
void AlarmHiAdjust();
void AlarmLowAdjust();
void CLTimeAdjust();
void VolumeAdjust();
void MenuBack();

int SSWMode = 1;

menuFrame mainMenu;

void setup()
{

  Serial.begin(115200);

  oledSystemInit(&OLED_Display);

  // Main menu
  mainMenu.addMenu("SetUp", 0);
  mainMenu.addNode("Pump Levels", SUB_NODE, NULL);
  mainMenu.linkNode(1);

  mainMenu.addNode("Alarm Levels", SUB_NODE, NULL);
  mainMenu.linkNode(2);

  mainMenu.addNode("CL Timer", SUB_NODE, NULL);
  mainMenu.linkNode(3);

  mainMenu.addNode("Volume", SUB_NODE, NULL);
  mainMenu.linkNode(4);

  // Submenu 1
  mainMenu.addMenu("Pump mm", 1);
  mainMenu.addNode("High mm", ACT_NODE, &PumpHiAdjust);
  mainMenu.addNode("Low", ACT_NODE, &PumpLowAdjust);
  mainMenu.addNode("Back", ACT_NODE, &MenuBack);
  // mainMenu.addNode("SubM1 Node 4", ACT_NODE, &testFunct);

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
  // mainMenu.addNode("SubM3 Node 6", ACT_NODE, &testFunct);
}

void loop()
{
  mainMenu.build(&OLED_Display);

  if (Serial.available())
  {
    switch (Serial.read())
    {
    case 'u':
      mainMenu.up();
      break;

    case 'd':
      mainMenu.down();
      break;

    case 'c':
      mainMenu.choose();
      break;

    case 'b':
      // Need to backlink nodes to menus with another variable "linkedNode"
      mainMenu.back();
      break;

    default:

      break;
    }
  }

  delay(100);
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
 OLED_Display.print("Inside PumpHi Adj");
 OLED_Display.display();
 delay(1000);
}
void PumpLowAdjust()
{

  // same as pump hi
}
void AlarmHiAdjust()
{

  // same as pump hi
}
void AlarmLowAdjust()
{

  // same as pump hi
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
}
void VolumeAdjust()
{

  // same as cl
}
void MenuBack()
{

  // go back a level
   mainMenu.back();
}

void DisplayUpdate()
{
  switch (SSWMode)
  {
  case 0: // encoder sw

    /*           OLED_Display.clearDisplay();
              OLED_Display.setCursor(0, 0);
              //void DisplayLevelSensor(Adafruit_SSD1306 *Disp, LevelSensor *SenLevVal);
              //DisplayLevelSensor(&OLED_Display, &Sensor_Level_Values);
              OLED_Display.display(); */

    break;

  case 1: // auto

    break;

  case 2: // mode sw alarm

    /*     OLED_Display.clearDisplay();
        OLED_Display.setCursor(0, 0);

        OLED_Display.print("Alarm ON");
        OLED_Display.display(); */

    break;

  case 3: // off /setup

    break;

  case 4: // pump

    break;

  default:

    OLED_Display.setCursor(0, 0);

    OLED_Display.print("Default = Bad");
    OLED_Display.display();

    break;
  }
}

void testFunct()
{
  Serial.println("Function successfully called");
}