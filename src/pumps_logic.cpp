#include "pumps_logic.h"
#include "config.h"
#include <time.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <wchar.h>

// ОБЪЕКТЫ (только один раз!)
LCD_1602_RUS lcd(0x27, 16, 2);
Encoder enc1(CLK, DT, SW);

// НАЗВАНИЯ КАНАЛОВ — редактируются из веб-интерфейса (UTF-8)
String pumpNames[PUPM_AMOUNT];

// Загрузить имена помп из EEPROM (или дефолтные)
// Формат хранения: для каждой помпы: 1 байт длина + до 20 байт имени (UTF-8)
static const int EEPROM_NAMES_OFFSET = PUPM_AMOUNT * 8; // после таймеров

void loadPumpNames() {
    // Дефолтные имена (UTF-8)
    const char* defaults[] = {
        "Pump 1", "Pump 2", "Pump 3", "Помпа 4",
        "Огурцы", "Помидоры", "Клубника", "Папин куст"
    };
    
    for (int i = 0; i < PUPM_AMOUNT; i++) {
        int addr = EEPROM_NAMES_OFFSET + i * 22;
        uint8_t len = EEPROM.read(addr);
        if (len > 0 && len <= 20) {
            char buf[24] = {0};
            for (int j = 0; j < len; j++) {
                buf[j] = EEPROM.read(addr + 1 + j);
            }
            pumpNames[i] = String(buf); // уже UTF-8
        } else {
            pumpNames[i] = String(defaults[i]); // UTF-8
        }
    }
}

void savePumpName(int idx, const String& name) {
    if (idx < 0 || idx >= PUPM_AMOUNT) return;
    
    int addr = EEPROM_NAMES_OFFSET + idx * 22;
    String truncated = name.substring(0, 20); // макс 20 символов
    
    // Сохраняем UTF-8 как есть
    EEPROM.write(addr, (uint8_t)truncated.length());
    for (int j = 0; j < truncated.length(); j++) {
        EEPROM.write(addr + 1 + j, (uint8_t)truncated[j]);
    }
    EEPROM.commit();
    
    pumpNames[idx] = truncated;
}

// Получить имя помпы для LCD (библиотека LCD_1602_RUS сама понимает UTF-8)
const wchar_t* getPumpNameW(int idx) {
    if (idx < 0 || idx >= PUPM_AMOUNT) return L"---";
    // Библиотека LCD_1602_RUS::print(const char*) сама конвертирует UTF-8
    // через mbtowc. Поэтому просто возвращаем указатель на внутренний буфер.
    // Но так как функция возвращает wchar_t*, а нам нужно передать UTF-8 строку,
    // используем print(const String&) напрямую в drawlabels/changeSet.
    // Эта функция больше не нужна, оставим для совместимости.
    static wchar_t buf[32];
    String s = pumpNames[idx];
    for (int i = 0; i < 31 && i < s.length(); i++) {
        buf[i] = (wchar_t)(unsigned char)s[i];
        buf[i+1] = 0;
    }
    return buf;
}

// Флаг синхронизации из веб-интерфейса
volatile boolean web_sync_requested = false;
volatile int8_t web_sync_pump = -1;

// ПЕРЕМЕННЫЕ
uint32_t pump_timers[PUPM_AMOUNT];
uint32_t pumping_time[PUPM_AMOUNT];
uint32_t period_time[PUPM_AMOUNT];
boolean pump_state[PUPM_AMOUNT];
byte pump_pins[PUPM_AMOUNT];
int8_t current_set = 0;
int8_t current_pump = 0;
boolean now_pumping = false;
int8_t thisH, thisM, thisS;
boolean startFlag = true;
boolean backlState = true;
uint32_t backlTimer;

// Таймаут меню — возврат на главный экран
static uint32_t menuActivityTimer = 0;
static boolean menuActive = false;

// Список безопасных пинов для Wemos D1 R32
const byte safe_pins[] = {13, 14, 25, 26, 27, 32, 33, 2};

void pumpsInit()
{
    pinMode(CLK, INPUT_PULLUP);
    pinMode(DT, INPUT_PULLUP);
    pinMode(SW, INPUT_PULLUP);

    Wire.begin(21, 22);
    delay(200);
    if (!EEPROM.begin(1024))
        Serial.println("EEPROM fail");
    pinMode(SW, INPUT_PULLUP);
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, !SWITCH_LEVEL);
    for (byte i = 0; i < PUPM_AMOUNT; i++)
    {
        pump_pins[i] = safe_pins[i];
        pinMode(pump_pins[i], OUTPUT);
        digitalWrite(pump_pins[i], !SWITCH_LEVEL);
    }
    lcd.init();
    lcd.backlight();
    lcd.clear();

    enc1.setType(ENCODER_TYPE);
    if (ENC_REVERSE)
        enc1.setDirection(REVERSE);
    
    loadPumpNames();
    
    for (byte i = 0; i < PUPM_AMOUNT; i++)
    {
        EEPROM.get(i * 8, period_time[i]);
        EEPROM.get(i * 8 + 4, pumping_time[i]);

        if (period_time[i] > 360000) period_time[i] = 0;
        if (pumping_time[i] > 360000) pumping_time[i] = 0;

        pump_state[i] = false;
        pump_timers[i] = millis();
    }

    drawlabels();
    changeSet();
    backlTimer = millis();
    menuActive = false;
}

void pumpsTick()
{
    enc1.tick();
    encoderTick();
    periodTick();
    flowTick();
    backlTick();

    // Обработка запроса синхронизации из веб-интерфейса
    if (web_sync_requested) {
        web_sync_requested = false;
        if (web_sync_pump >= 0 && web_sync_pump < PUPM_AMOUNT) {
            current_pump = web_sync_pump;
        }
        drawlabels();
        changeSet();
    }

    if (menuActive && millis() - menuActivityTimer >= MENU_TIMEOUT_MS)
    {
        menuActive = false;
        drawlabels();
        changeSet();
    }

    static uint32_t clockTimer;
    if (millis() - clockTimer >= 10000)
    {
        clockTimer = millis();
        if (!menuActive) { drawlabels(); changeSet(); }
    }

}

void backlTick()
{
    if (LCD_BACKL && backlState && millis() - backlTimer >= BACKL_TOUT * 1000)
    {
        backlState = false;
        lcd.noBacklight();
    }
}

void backlOn()
{
    backlState = true;
    backlTimer = millis();
    lcd.backlight();
}

void periodTick()
{
    for (byte i = 0; i < PUPM_AMOUNT; i++)
    {
        if (startFlag ||
            (period_time[i] > 0 && millis() - pump_timers[i] >= period_time[i] * 1000 && (pump_state[i] != SWITCH_LEVEL) && !(now_pumping * !PARALLEL)))
        {
            pump_state[i] = SWITCH_LEVEL;
            digitalWrite(pump_pins[i], SWITCH_LEVEL);
            pump_timers[i] = millis();
            now_pumping = true;
            digitalWrite(PUMP_PIN, SWITCH_LEVEL);
            mqttPublishStatus(i, true);
        }
    }
    startFlag = false;
}

void flowTick()
{
    for (byte i = 0; i < PUPM_AMOUNT; i++)
    {
        if (pumping_time[i] > 0 && millis() - pump_timers[i] >= pumping_time[i] * 1000 && (pump_state[i] == SWITCH_LEVEL))
        {
            pump_state[i] = !SWITCH_LEVEL;
            digitalWrite(pump_pins[i], !SWITCH_LEVEL);
            if (TIMER_START) pump_timers[i] = millis();
            now_pumping = false;
            digitalWrite(PUMP_PIN, !SWITCH_LEVEL);
            mqttPublishStatus(i, false);
        }
    }
}

void encoderTick()
{
    enc1.tick();

    if (enc1.isTurn())
    {
        backlOn();
        menuActive = true;
        menuActivityTimer = millis();
        if (enc1.isRight()) current_set++;
        else if (enc1.isLeft()) current_set--;
        if (current_set > 6) current_set = 6;
        if (current_set < 0) current_set = 0;
        changeSet();
    }

    if (enc1.isRightH())
    {
        backlOn(); menuActive = true; menuActivityTimer = millis();
        changeSettings(1); changeSet();
    }
    if (enc1.isLeftH())
    {
        backlOn(); menuActive = true; menuActivityTimer = millis();
        changeSettings(-1); changeSet();
    }

    if (enc1.isClick())
    {
        backlOn(); menuActive = true; menuActivityTimer = millis();
        if (current_set < 4) current_set = 4;
        else current_set = 1;
        changeSet();
    }

    if (enc1.isHolded())
    {
        backlOn();
        update_EEPROM();
        menuActive = false;
        drawlabels();
        changeSet();
    }
}

void changeSettings(int increment)
{
    if (current_set == 0)
    {
        current_pump += increment;
        if (current_pump >= PUPM_AMOUNT) current_pump = PUPM_AMOUNT - 1;
        if (current_pump < 0) current_pump = 0;
        drawlabels();
    }
    else
    {
        if (current_set == 1 || current_set == 4) thisH += increment;
        else if (current_set == 2 || current_set == 5) thisM += increment;
        else if (current_set == 3 || current_set == 6) thisS += increment;

        if (thisS > 59) { thisS = 0; thisM++; }
        if (thisS < 0) { thisS = 59; thisM--; }
        if (thisM > 59) { thisM = 0; thisH++; }
        if (thisM < 0) { thisM = 59; thisH--; }
        if (thisH < 0) thisH = 0;

        if (current_set < 4) period_time[current_pump] = hms_to_s();
        else pumping_time[current_pump] = hms_to_s();
    }
}

// Считает количество UTF-8 символов в строке (не байт)
static int utf8CharCount(const String& s) {
    int count = 0;
    const char* p = s.c_str();
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80) p++;          // 1 байт
        else if (c < 0xC0) break;   // невалид
        else if (c < 0xE0) p += 2;  // 2 байта
        else if (c < 0xF0) p += 3;  // 3 байта
        else p += 4;                // 4 байта
        count++;
    }
    return count;
}

void drawlabels() {
    // Используем print(const String&) — библиотека сама конвертирует UTF-8
    lcd.setCursor(1, 0);
    lcd.print(pumpNames[current_pump]);

    // Определяем количество символов на дисплее
    int nameChars = utf8CharCount(pumpNames[current_pump]);
    int nameEnd = 1 + nameChars;
    while (nameEnd < 11) {
        lcd.setCursor(nameEnd++, 0);
        lcd.print(" ");
    }

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[6];
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
        lcd.setCursor(11, 0);
        lcd.print(timeStr);
    }
}

void changeSet()
{
    if (current_set < 4)
        s_to_hms(period_time[current_pump]);
    else
        s_to_hms(pumping_time[current_pump]);

    lcd.setCursor(1, 0);
    lcd.print(pumpNames[current_pump]);
    int nameLen = utf8CharCount(pumpNames[current_pump]);
    // Затираем только до позиции 11 (где начинается время)
    for (int i = 0; i < (11 - 1 - nameLen); i++)
        lcd.print(" ");

    lcd.setCursor(14, 0);
    lcd.print(WiFi.status() == WL_CONNECTED ? "W" : " ");

    // Выводим время NTP (после затирания, чтобы не перезаписать)
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[6];
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
        lcd.setCursor(11, 0);
        lcd.print(timeStr);
    }

    lcd.setCursor(1, 1);
    if (current_set < 4)
        lcd.print(L"ПАУЗА ");
    else
        lcd.print(L"РАБОТА");

    char buf[10];
    sprintf(buf, "%02d:%02d:%02d", (int)thisH, (int)thisM, (int)thisS);
    lcd.setCursor(8, 1);
    lcd.print(buf);

    lcd.setCursor(0, 0); lcd.print(" ");
    lcd.setCursor(7, 1); lcd.print(" ");
    lcd.setCursor(10, 1); lcd.print(" ");
    lcd.setCursor(13, 1); lcd.print(" ");

    if (menuActive) {
        switch (current_set) {
            case 0: lcd.setCursor(0, 0); break;
            case 1: case 4: lcd.setCursor(7, 1); break;
            case 2: case 5: lcd.setCursor(10, 1); break;
            case 3: case 6: lcd.setCursor(13, 1); break;
        }
        lcd.print(">");
    }
}

void s_to_hms(uint32_t period)
{
    thisH = period / 3600;
    thisM = (period % 3600) / 60;
    thisS = period % 60;
}

uint32_t hms_to_s()
{
    return ((long)thisH * 3600 + thisM * 60 + thisS);
}

void update_EEPROM(int pump_id) {
    int id = (pump_id >= 0 && pump_id < PUPM_AMOUNT) ? pump_id : current_pump;
    EEPROM.put(8 * id, period_time[id]);
    EEPROM.put(8 * id + 4, pumping_time[id]);
    EEPROM.commit();
    sendMQTTFullStatus();
}

// === Генерация JSON статуса для веб-интерфейса ===
void webGetStatusJSON(String &json) {
    json = "{";
    json += "\"pumps\":[";
    for (int i = 0; i < PUPM_AMOUNT; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":" + String(i) + ",";
        json += "\"name\":\"" + pumpNames[i] + "\",";
        json += "\"period\":" + String(period_time[i]) + ",";
        json += "\"work\":" + String(pumping_time[i]) + ",";
        json += "\"state\":" + String(pump_state[i] == SWITCH_LEVEL ? "true" : "false");
        json += "}";
    }
    json += "],";
    json += "\"now_pumping\":" + String(now_pumping ? "true" : "false") + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
}