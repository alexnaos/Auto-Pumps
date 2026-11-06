#include "web_interface.h"
#include "config.h"
#include "pumps_logic.h"
#include "web_server.h"

// Регистрация ключей БД для всех помп + статусные ключи
DB_KEYS(kk,
    p0_period, p0_work,
    p1_period, p1_work,
    p2_period, p2_work,
    p3_period, p3_work,
    p4_period, p4_work,
    p5_period, p5_work,
    p6_period, p6_work,
    p7_period, p7_work,
    // Ключи для имён помп (в БД на LittleFS)
    p0_name, p1_name, p2_name, p3_name,
    p4_name, p5_name, p6_name, p7_name,
    // Выбранная помпа в интерфейсе
    selected_pump,
    // Статусные ключи
    lbl_sys_state,
    lbl_uptime,
    lbl_wifi,
    lbl_fw_version,
    lbl_pump_name,
    lbl_pump_state,
    lbl_pump_period,
    lbl_pump_work
);

// Переменная аптайма - общая для билдера и апдейтера
char uptimeStr[32];

// Вспомогательная функция: получить ключ периода по индексу помпы
static uint32_t getPeriodKey(int idx) {
    switch (idx) {
        case 0: return kk::p0_period;
        case 1: return kk::p1_period;
        case 2: return kk::p2_period;
        case 3: return kk::p3_period;
        case 4: return kk::p4_period;
        case 5: return kk::p5_period;
        case 6: return kk::p6_period;
        case 7: return kk::p7_period;
        default: return kk::p0_period;
    }
}

// Вспомогательная функция: получить ключ работы по индексу помпы
static uint32_t getWorkKey(int idx) {
    switch (idx) {
        case 0: return kk::p0_work;
        case 1: return kk::p1_work;
        case 2: return kk::p2_work;
        case 3: return kk::p3_work;
        case 4: return kk::p4_work;
        case 5: return kk::p5_work;
        case 6: return kk::p6_work;
        case 7: return kk::p7_work;
        default: return kk::p0_work;
    }
}

// Вспомогательная функция: получить ключ имени по индексу помпы
static uint32_t getNameKey(int idx) {
    switch (idx) {
        case 0: return kk::p0_name;
        case 1: return kk::p1_name;
        case 2: return kk::p2_name;
        case 3: return kk::p3_name;
        case 4: return kk::p4_name;
        case 5: return kk::p5_name;
        case 6: return kk::p6_name;
        case 7: return kk::p7_name;
        default: return kk::p0_name;
    }
}

// Формируем строку списка помп для Select (имя из pumpNames)
String getPumpOptions() {
    String list = "";
    for (int i = 0; i < PUPM_AMOUNT; i++) {
        list += pumpNames[i];
        if (i < PUPM_AMOUNT - 1) list += ";";
    }
    return list;
}

void buildInterface(sets::Builder &b) {
    extern GyverDBFile db;
    
    // ---- ГРУППА: СТАТУС СИСТЕМЫ ----
    {
        sets::Group g(b, "Статус системы");
        b.Label(kk::lbl_sys_state, "Состояние", now_pumping ? "💧 Полив активен" : "✅ Ожидание");
        b.Label(kk::lbl_uptime, "Аптайм", uptimeStr);
        b.Label(kk::lbl_wifi, "WiFi", WiFi.status() == WL_CONNECTED ? "✅ Подключен" : "❌ Нет связи");
        // Вывод версии прошивки и даты компиляции (по документации SettingsGyver)
        b.Label(kk::lbl_fw_version, "FW", String(FIRMWARE_VERSION) + " / " + __DATE__ + " " + __TIME__);
    }

    // ---- ВЫБОР ПОМПЫ ----
    {
        sets::Group g(b, "Выбор помпы");
        if (b.Select(kk::selected_pump, "Помпа", getPumpOptions())) {
            int idx = db[kk::selected_pump].toInt();
            if (idx >= 0 && idx < PUPM_AMOUNT) {
                // Загружаем данные выбранной помпы из БД
                period_time[idx] = db[getPeriodKey(idx)].toInt();
                pumping_time[idx] = db[getWorkKey(idx)].toInt();
                // Загружаем имя из БД
                String savedName = db[getNameKey(idx)].toString();
                if (savedName.length() > 0) {
                    pumpNames[idx] = savedName;
                    savePumpName(idx, savedName);
                }
                // Запрос синхронизации LCD в loop()
                web_sync_pump = idx;
                web_sync_requested = true;
            }
            b.reload();
        }
    }

    // ---- УПРАВЛЕНИЕ ВЫБРАННОЙ ПОМПОЙ ----
    {
        int idx = db[kk::selected_pump].toInt();
        if (idx < 0 || idx >= PUPM_AMOUNT) idx = 0;
        
        sets::Group g(b, pumpNames[idx]);
        
        // Редактирование имени помпы
        if (b.Input(getNameKey(idx), "Название помпы")) {
            String newName = db[getNameKey(idx)].toString();
            if (newName.length() > 0) {
                pumpNames[idx] = newName;
                savePumpName(idx, newName);
            }
            b.reload();
        }
        
        // Текущий статус
        b.Label(kk::lbl_pump_state, "Статус", pump_state[idx] == SWITCH_LEVEL ? "🟢 Включена" : "⚪ Выключена");
        b.Label(kk::lbl_pump_name, "Имя", pumpNames[idx]);
        
        // Кнопки управления
        {
            sets::Buttons btns(b);
            if (b.Button("▶ Включить")) {
                digitalWrite(pump_pins[idx], SWITCH_LEVEL);
                pump_state[idx] = SWITCH_LEVEL;
                now_pumping = true;
                digitalWrite(PUMP_PIN, SWITCH_LEVEL);
                mqttPublishStatus(idx, true);
                changeSet();
                b.reload();
            }
            if (b.Button("⏹ Выключить")) {
                digitalWrite(pump_pins[idx], !SWITCH_LEVEL);
                pump_state[idx] = !SWITCH_LEVEL;
                
                bool anyOn = false;
                for (int j = 0; j < PUPM_AMOUNT; j++) {
                    if (pump_state[j] == SWITCH_LEVEL) { anyOn = true; break; }
                }
                if (!anyOn) {
                    now_pumping = false;
                    digitalWrite(PUMP_PIN, !SWITCH_LEVEL);
                }
                mqttPublishStatus(idx, false);
                changeSet();
                b.reload();
            }
        }
        
        // Настройки паузы и полива (в минутах, шаг 1 мин)
        if (b.Slider(getPeriodKey(idx), "Пауза (мин)", 0, 1440, 1)) {
            period_time[idx] = db[getPeriodKey(idx)].toInt() * 60; // конвертируем в секунды
            update_EEPROM(idx);
            // Синхронизация с LCD
            current_pump = idx;
            changeSet();
            drawlabels();
        }
        if (b.Slider(getWorkKey(idx), "Полив (мин)", 0, 60, 1)) {
            pumping_time[idx] = db[getWorkKey(idx)].toInt() * 60; // конвертируем в секунды
            update_EEPROM(idx);
            // Синхронизация с LCD
            current_pump = idx;
            changeSet();
            drawlabels();
        }
        
        // Текущие значения (в минутах)
        char periodStr[16], workStr[16];
        snprintf(periodStr, sizeof(periodStr), "%lu мин", period_time[idx] / 60);
        snprintf(workStr, sizeof(workStr), "%lu мин", pumping_time[idx] / 60);
        b.Label(kk::lbl_pump_period, "Текущая пауза", periodStr);
        b.Label(kk::lbl_pump_work, "Текущий полив", workStr);
    }
    
    // ---- ГРУППА: УПРАВЛЕНИЕ ПОЛИВОМ ----
    {
        sets::Group g(b, "Управление поливом");
        sets::Buttons btns(b);
        if (b.Button("⏹ Остановить всё")) {
            for (int i = 0; i < PUPM_AMOUNT; i++) {
                digitalWrite(pump_pins[i], !SWITCH_LEVEL);
                pump_state[i] = !SWITCH_LEVEL;
                mqttPublishStatus(i, false);
            }
            now_pumping = false;
            digitalWrite(PUMP_PIN, !SWITCH_LEVEL);
            changeSet();
            b.reload();
        }
    }
}

void updateInterface(sets::Updater &upd) {
    extern GyverDBFile db;
    int idx = db[kk::selected_pump].toInt();
    if (idx < 0 || idx >= PUPM_AMOUNT) idx = 0;
    
    uint32_t sec = millis() / 1000;
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    snprintf(uptimeStr, sizeof(uptimeStr), "%02d:%02d:%02d", h, m, s);
    
    char fwVer[40];
    snprintf(fwVer, sizeof(fwVer), "%s / %s %s", FIRMWARE_VERSION, __DATE__, __TIME__);
    
    upd.update(kk::lbl_sys_state, now_pumping ? "💧 Полив активен" : "✅ Ожидание");
    upd.update(kk::lbl_uptime, uptimeStr);
    upd.update(kk::lbl_wifi, WiFi.status() == WL_CONNECTED ? "✅ Подключен" : "❌ Нет связи");
    upd.update(kk::lbl_fw_version, fwVer);
    upd.update(kk::lbl_pump_state, pump_state[idx] == SWITCH_LEVEL ? "🟢 Включена" : "⚪ Выключена");
    upd.update(kk::lbl_pump_name, pumpNames[idx]);
    
    char periodStr[16], workStr[16];
    snprintf(periodStr, sizeof(periodStr), "%lu сек", period_time[idx]);
    snprintf(workStr, sizeof(workStr), "%lu сек", pumping_time[idx]);
    upd.update(kk::lbl_pump_period, periodStr);
    upd.update(kk::lbl_pump_work, workStr);
}