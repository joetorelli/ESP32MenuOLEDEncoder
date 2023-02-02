
#ifndef SENSOR_READINGS_H

#define SENSOR_READINGS_H
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SEALEVELPRESSURE_HPA (1013.25)


struct OLED_SW
{
    int Switch_A = 0;
    int Switch_B = 0;
    int Switch_C = 0;
};

struct LevelSensor
{
    int SensorLevel = 0;
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;
    float power_mW = 0;
    float depth = 0;
    int depthmap = 0;
};

// extern struct OLED_SW Switch_State;
// extern struct BME_Sensor Sensor_Values;


void ReadSwitches(OLED_SW *SwState);
// read analog sensor value
// void ReadLevelSensor(Adafruit_INA219 *LevSensor, LevelSensor *SensorLevelVal);
#endif