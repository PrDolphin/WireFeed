/*
  Contributed by Anastasia
*/

#include <Arduino.h>
#include <GyverTM1637.h>

#define POTENT_PIN A5 // Пин на потенциометре
#define CLK 5
#define DIO 4


uint8_t averageFactor = 10;


GyverTM1637 disp(CLK, DIO);

const uint8_t movingAvgSize = 20;
uint16_t movingAvg[movingAvgSize] = {0};

uint16_t get_speed() {
  uint16_t sum=0;
  for(uint8_t i = 0; i < movingAvgSize - 1; ++i) {
    movingAvg[i] = movingAvg[i + 1];
    sum += movingAvg[i];
  }
  movingAvg[movingAvgSize - 1] = map(analogRead(POTENT_PIN), 0, 1023, 10, 500);
  sum += movingAvg[movingAvgSize - 1];
  return (sum + (movingAvgSize/2)) / movingAvgSize;
}

/*int scolz_srednee() {
  int sum = 0;
  for(int k = 0; k < scolzSize - 1; k++){
    scolzAr[k] = scolzAr[k + 1];
    sum = sum + scolzAr[k];
    int newSensorValue = analogRead(POTENT_PIN);
    newSensorValue = map(newSensorValue, 0, 792, 133, 530); // Для точности выбрал такой диапазон 0, 700, 130, 530
    scolzAr[scolzSize-2] = (scolzAr[scolzSize-2] * (averageFactor - 1) + newSensorValue) / averageFactor;
    scolzAr[scolzSize-1] = oldSensorValue;
  }
  int newSensorValue = analogRead(POTENT_PIN);
  newSensorValue = map(newSensorValue, 0, 792, 133, 530); // Для точности выбрал такой диапазон 0, 700, 130, 530
  scolzAr[scolzSize-2] = (scolzAr[scolzSize-2] * (averageFactor - 1) + newSensorValue) / averageFactor;
  scolzAr[scolzSize-1] = oldSensorValue;
  int res = round((sum + scolzAr[scolzSize-1])/scolzSize);
  //Serial.println(new_s);
  return res;
}*/

void ui_setup() {
  disp.clear();
  disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
  for (uint8_t i = 0; i < movingAvgSize; ++i) {
    movingAvg[i] = map(analogRead(POTENT_PIN), 0, 1023, 10, 500);
  }
}