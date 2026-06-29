#include <Arduino.h>
#include <Wire.h>           // Обязательно для I2C на ESP32
#include <EEPROM.h>
#include "GyverEncoder.h"
#include "LCD_1602_RUS.h"
#include "pumps_logic.h"    // для pumpNames[]

// --- НАСТРОЙКИ ---
#define LCD_BACKL 1         
#define BACKL_TOUT 60       
#define ENCODER_TYPE 1      
#define ENC_REVERSE 1       
#define DRIVER_VERSION 0    
#define PUPM_AMOUNT 8       
#define PUMP_PIN 4          
#define SWITCH_LEVEL 0      
#define PARALLEL 0          
#define TIMER_START 1       

// --- ПИНЫ ЭНКОДЕРА (Перенесли SW с 21 на 5!) ---
#define CLK 18
#define DT 19
#define SW 5                

// --- ПИНЫ ПОМП (Безопасные для ESP32) ---
const byte safe_pins[] = {13, 14, 25, 26, 27, 32, 33, 2};

// Прототипы функций (чтобы компилятор их видел)
void drawlabels();
void changeSet();
void encoderTick();
void periodTick();
void flowTick();
void backlTick();
void changeSettings(int val);
void s_to_hms(uint32_t seconds);
uint32_t hms_to_s();
void drawArrow(byte col, byte row);
void update_EEPROM(int pump_id = -1);

Encoder enc1(CLK, DT, SW);

// Переменные
uint32_t pump_timers[PUPM_AMOUNT];
uint32_t pumping_time[PUPM_AMOUNT];
uint32_t period_time[PUPM_AMOUNT];
boolean pump_state[PUPM_AMOUNT];
byte pump_pins[PUPM_AMOUNT];
int8_t current_set = 0;
int8_t current_pump = 0;
boolean now_pumping = false;
int8_t thisH, thisM, thisS;
long thisPeriod;
boolean startFlag = true;
boolean backlState = true;
uint32_t backlTimer;
LCD_1602_RUS lcd(0x27, 16, 2); // Попробуй 0x27 вместо 0x3f
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Wemos D1 R32 Start");

  // Инициализация I2C для ESP32 (SDA=21, SCL=22)
  Wire.begin(21, 22); 
delay(200); // Даем время дисплею проснуться

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Тестовый вывод - если это появится, дисплей ожил!
  lcd.setCursor(0, 0);
  lcd.print("ESP32 Ready!"); 
  Serial.println("Sent to LCD: ESP32 Ready!");


  if (!EEPROM.begin(512)) Serial.println("EEPROM fail");

  pinMode(SW, INPUT_PULLUP);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, !SWITCH_LEVEL);

  for (byte i = 0; i < PUPM_AMOUNT; i++) {
    pump_pins[i] = safe_pins[i];
    pinMode(pump_pins[i], OUTPUT);
    digitalWrite(pump_pins[i], !SWITCH_LEVEL);
  }
  enc1.setType(ENCODER_TYPE);
  if (ENC_REVERSE) enc1.setDirection(REVERSE);

  // Сброс настроек
  if (digitalRead(SW) == LOW) {
    lcd.setCursor(0, 0);
    lcd.print("Reset settings");
    for (int i = 0; i < 512; i++) {
      EEPROM.write(i, 0);
      if (i % 20 == 0) yield();
    }
    EEPROM.commit();
    while (digitalRead(SW) == LOW) delay(10);
    lcd.clear();
  }

  // Загрузка
  for (byte i = 0; i < PUPM_AMOUNT; i++) {
    EEPROM.get(i * 8, period_time[i]);
    EEPROM.get(i * 8 + 4, pumping_time[i]);
    if (period_time[i] == 0xFFFFFFFF) period_time[i] = 0;
    if (pumping_time[i] == 0xFFFFFFFF) pumping_time[i] = 0;
    pump_state[i] = false;
    pump_timers[i] = millis(); // инициализация таймеров
  }

  drawlabels();
  changeSet();
  backlTimer = millis();
}

void loop() {
  encoderTick();
  periodTick();
  flowTick();
  backlTick();
}

void backlTick() {
  if (LCD_BACKL && backlState && millis() - backlTimer >= BACKL_TOUT * 1000) {
    backlState = false;
    lcd.noBacklight();
  }
}
void backlOn() {
  backlState = true;
  backlTimer = millis();
  lcd.backlight();
}

void periodTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {            // пробегаем по всем помпам
    if (startFlag ||
        (period_time[i] > 0
         && millis() - pump_timers[i] >= period_time[i] * 1000
         && (pump_state[i] != SWITCH_LEVEL)
         && !(now_pumping * !PARALLEL))) {
      pump_state[i] = SWITCH_LEVEL;
      digitalWrite(pump_pins[i], SWITCH_LEVEL);   // открыть КЛАПАН
      pump_timers[i] = millis();
      now_pumping = true;
      digitalWrite(PUMP_PIN, SWITCH_LEVEL);       // включить общую ПОМПУ
      //Serial.println("Pump #" + String(i) + " ON");
    }
  }
  startFlag = false;
}

void flowTick() {
  for (byte i = 0; i < PUPM_AMOUNT; i++) {            // пробегаем по всем помпам
    if (pumping_time[i] > 0
        && millis() - pump_timers[i] >= pumping_time[i] * 1000
        && (pump_state[i] == SWITCH_LEVEL) ) {
      pump_state[i] = !SWITCH_LEVEL;
      digitalWrite(pump_pins[i], !SWITCH_LEVEL);   // закрыть КЛАПАН
      if (TIMER_START) pump_timers[i] = millis();
      now_pumping = false;
      digitalWrite(PUMP_PIN, !SWITCH_LEVEL);       // выключить общую ПОМПУ
      //Serial.println("Pump #" + String(i) + " OFF");
    }
  }
}

/*
  void encISR() {
  enc1.tick();  // отработка энкодера
  }
*/

void encoderTick() {
  enc1.tick();    // отработка энкодера

  if (enc1.isTurn()) {                               // если был совершён поворот
    if (backlState) {
      backlTimer = millis();      // сбросить таймаут дисплея
      if (enc1.isRight()) {
        if (++current_set >= 7) current_set = 6;
      } else if (enc1.isLeft()) {
        if (--current_set < 0) current_set = 0;
      }

      if (enc1.isRightH())
        changeSettings(1);
      else if (enc1.isLeftH())
        changeSettings(-1);

      changeSet();
    } else {
      backlOn();      // включить дисплей
    }
  }
}

// тут меняем номер помпы и настройки
void changeSettings(int increment) {
  if (current_set == 0) {
    current_pump += increment;
    if (current_pump > PUPM_AMOUNT - 1) current_pump = PUPM_AMOUNT - 1;
    if (current_pump < 0) current_pump = 0;
    s_to_hms(period_time[current_pump]);
    drawlabels();
  } else {
    if (current_set == 1 || current_set == 4) {
      thisH += increment;
    } else if (current_set == 2 || current_set == 5) {
      thisM += increment;
    } else if (current_set == 3 || current_set == 6) {
      thisS += increment;
    }
    if (thisS > 59) {
      thisS = 0;
      thisM++;
      if (thisM > 59) {
        thisM = 0;
        thisH++;
      }
    }
    if (thisS < 0) {
      thisS = 59;
      thisM--;
      if (thisM < 0) {
        thisM = 59;
        thisH--;
        if (thisH < 0) thisH = 0;
      }
    }
    if (current_set < 4) period_time[current_pump] = hms_to_s();
    else pumping_time[current_pump] = hms_to_s();
  }
}

// вывести название реле
void drawlabels() {
  lcd.setCursor(1, 0);
  lcd.print("                ");
  lcd.setCursor(1, 0);
  lcd.print(pumpNames[current_pump]);
}

// изменение позиции стрелки и вывод данных
void changeSet() {
  switch (current_set) {
    case 0: drawArrow(0, 0); update_EEPROM(current_pump);
      break;
    case 1: drawArrow(7, 1);
      break;
    case 2: drawArrow(10, 1);
      break;
    case 3: drawArrow(13, 1);
      break;
    case 4: drawArrow(7, 1);
      break;
    case 5: drawArrow(10, 1);
      break;
    case 6: drawArrow(13, 1);
      break;
  }
  lcd.setCursor(0, 1);
  if (current_set < 4) {
    lcd.print(L"ПАУЗА ");
    s_to_hms(period_time[current_pump]);
  }
  else {
    lcd.print(L"РАБОТА");
    s_to_hms(pumping_time[current_pump]);
  }
  lcd.setCursor(8, 1);
  if (thisH < 10) lcd.print(0);
  lcd.print(thisH);
  lcd.setCursor(11, 1);
  if (thisM < 10) lcd.print(0);
  lcd.print(thisM);
  lcd.setCursor(14, 1);
  if (thisS < 10) lcd.print(0);
  lcd.print(thisS);
}

// перевод секунд в ЧЧ:ММ:СС
void s_to_hms(uint32_t period) {
  thisH = floor((long)period / 3600);    // секунды в часы
  thisM = floor((period - (long)thisH * 3600) / 60);
  thisS = period - (long)thisH * 3600 - thisM * 60;
}

// перевод ЧЧ:ММ:СС в секунды
uint32_t hms_to_s() {
  return ((long)thisH * 3600 + thisM * 60 + thisS);
}

// отрисовка стрелки и двоеточий
void drawArrow(byte col, byte row) {
  lcd.setCursor(0, 0); lcd.print(" ");
  lcd.setCursor(7, 1); lcd.print(" ");
  lcd.setCursor(10, 1); lcd.print(":");
  lcd.setCursor(13, 1); lcd.print(":");
  lcd.setCursor(col, row); lcd.write(126);
}

