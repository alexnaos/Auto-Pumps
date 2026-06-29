#include "web_server.h"
#include "web_interface.h"
#include "config.h"
#include "pumps_logic.h"
#include "esp_task_wdt.h"
#include <LittleFS.h>

// Физическое выделение памяти под БД и сервер
GyverDBFile db(&LittleFS, "/db.bin");
SettingsGyver sett("Auto-Pumps Control", &db);

void initWebServer() {
    if (LittleFS.begin(true)) {
        db.begin();
        
        // Регистрация и инициализация дефолтных ключей БД
        // Периоды и время работы для 8 помп
        db.init(SH("p0_period"), 0);
        db.init(SH("p0_work"), 0);
        db.init(SH("p1_period"), 0);
        db.init(SH("p1_work"), 0);
        db.init(SH("p2_period"), 0);
        db.init(SH("p2_work"), 0);
        db.init(SH("p3_period"), 0);
        db.init(SH("p3_work"), 0);
        db.init(SH("p4_period"), 0);
        db.init(SH("p4_work"), 0);
        db.init(SH("p5_period"), 0);
        db.init(SH("p5_work"), 0);
        db.init(SH("p6_period"), 0);
        db.init(SH("p6_work"), 0);
        db.init(SH("p7_period"), 0);
        db.init(SH("p7_work"), 0);
        db.init(SH("selected_pump"), 0);
        // Ключи для имен помп
        db.init(SH("p0_name"), pumpNames[0]);
        db.init(SH("p1_name"), pumpNames[1]);
        db.init(SH("p2_name"), pumpNames[2]);
        db.init(SH("p3_name"), pumpNames[3]);
        db.init(SH("p4_name"), pumpNames[4]);
        db.init(SH("p5_name"), pumpNames[5]);
        db.init(SH("p6_name"), pumpNames[6]);
        db.init(SH("p7_name"), pumpNames[7]);
        db.update();
        
        Serial.println("[WEB] База данных инициализирована");
    } else {
        Serial.println("[WEB] ОШИБКА: LittleFS не запущена!");
    }

    // Регистрация кастомного CSS если есть
    if (LittleFS.exists("/custom.js")) {
        sett.setCustomFile("/custom.js");
        Serial.println("[WEB] Кастомный CSS/JS загружен");
    }

    // Настройка темы — тёмная синяя, кнопки будут видны
    sett.config.theme = sets::Colors::Blue;
    
    sett.begin();
    sett.onBuild(buildInterface);
    sett.onUpdate(updateInterface);
    
    Serial.printf("[WEB] Сервер запущен: http://%s\n", WiFi.localIP().toString().c_str());
}

void handleWebRequests() {
    esp_task_wdt_reset();
    sett.tick();
}