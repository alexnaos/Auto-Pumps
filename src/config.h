#ifndef CONFIG_H
#define CONFIG_H

// WiFi & MQTT
#define WIFI_SSID "Sloboda100"
#define WIFI_PASS "2716192023"
#define MQTT_SERVER "192.168.1.23"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASS ""
// Настройки времени (NTP)
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600 * 3   // Смещение в секундах (3 часа для МСК)
#define DAYLIGHT_OFFSET_SEC 0     // Смещение для летнего времени (в РФ 0)


// Настройки дисплея и энкодера
#define LCD_BACKL 1         // автоотключение подсветки (1 - вкл)
#define BACKL_TOUT 60       // таймаут подсветки, сек
#define ENCODER_TYPE 0
#define ENC_REVERSE 1
#define MENU_TIMEOUT_MS 15000 // таймаут возврата на главный экран

// Пины управления
#define CLK 18
#define DT 19
#define SW 5

#define SDA_PIN 21
#define SCL_PIN 22

// Настройки логики полива
#define PUPM_AMOUNT 8
#define PUMP_PIN 4          // Общая помпа
#define SWITCH_LEVEL 0      // 0 - низкий уровень, 1 - высокий
#define PARALLEL 0          // 1 - параллельный полив, 0 - очередь
#define TIMER_START 1       // 1 - отсчет от выключения, 0 - от включения

// Home Assistant Discovery
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_DEVICE_NAME "AutoPumps"
#define HA_DEVICE_ID "autopumps_esp32"
#define MQTT_TOPIC_PREFIX "garden"

#endif