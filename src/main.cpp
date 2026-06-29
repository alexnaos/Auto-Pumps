#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "pumps_logic.h"
#include "mqtt_handler.h"
#include "web_server.h"
#include <time.h>

void setup()
{
  // 1. Запуск консоли
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== AUTO-PUMPS STARTING ===");

  // 2. Инициализация железа (дисплей, пины, EEPROM)
  pumpsInit();
  Serial.println("Hardware initialized.");

  // 3. Подключение к WiFi
  Serial.printf("Connecting to: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n[WiFi] Connection FAILED. Check config.h");
  }
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("NTP Sync started");
  
  // 4. Запуск сетевых сервисов
  mqttInit();
  initWebServer();

  Serial.println("=== Initialization Complete ===\n");
}

void loop()
{
  // Основная логика полива и энкодера (работает всегда)
  pumpsTick();

  // Сетевые задачи
  mqttTick();
  
  // Обработка веб-запросов SettingsGyver
  handleWebRequests();
}
