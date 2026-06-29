#ifndef PUMPS_LOGIC_H
#define PUMPS_LOGIC_H

#include <Arduino.h>
#include "GyverEncoder.h"
#include <LCD_1602_RUS.h>
#include "config.h"

extern LCD_1602_RUS lcd;
extern Encoder enc1;
extern int8_t current_pump;
extern int8_t current_set;

// Названия каналов — редактируемые из веб-интерфейса
extern String pumpNames[PUPM_AMOUNT];

// Массивы таймеров
extern uint32_t pump_timers[PUPM_AMOUNT];
extern uint32_t pumping_time[PUPM_AMOUNT];
extern uint32_t period_time[PUPM_AMOUNT];
extern boolean pump_state[PUPM_AMOUNT];
extern byte pump_pins[PUPM_AMOUNT];
extern boolean now_pumping;

// Флаг для синхронизации LCD из веб-интерфейса
extern volatile boolean web_sync_requested;
extern volatile int8_t web_sync_pump;

// Функции
void pumpsInit();
void pumpsTick();
void encoderTick();
void periodTick();
void flowTick();
void backlTick();
void backlOn();
void changeSettings(int increment);
void changeSet();
void drawlabels();
void s_to_hms(uint32_t period);
uint32_t hms_to_s();
void update_EEPROM(int pump_id = -1);
void mqttPublishStatus(int id, bool state);
void sendMQTTDiscovery();
void sendMQTTFullStatus();
void webGetStatusJSON(String &json);

// Управление именами помп
void loadPumpNames();
void savePumpName(int idx, const String& name);
const wchar_t* getPumpNameW(int idx);

#endif