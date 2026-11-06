# 📋 Отчёт по проекту Auto-Pumps

**Путь**: `C:\Projects\PlatformIO\Auto-Pumps-master`  
**Версия контроля**: `41d5a89d35dfc9763b2730a4189e72a6ce352410` (main, origin/main)  
**Платформа**: ESP32 (Wemos D1 R32) · PlatformIO · Arduino Framework  
**Степень готовности**: ⚠️ **Сборка ломается** — критическая ошибка в одном файле

---

## 1. Общая структура проекта

```
Auto-Pumps-master/
├── src/                      # Исходники (4 KB)
│   ├── main.cpp              # Точка входа, инициализация
│   ├── config.h              # Конфигурация (WiFi, MQTT, пинны, настройки)
│   ├── pumps_logic.cpp/h     # Логика полива, LCD, энкодер
│   ├── mqtt_handler.cpp/h    # MQTT клиент + Home Assistant Discovery
│   ├── web_server.cpp/h      # SettingsGyver web-сервер (LittleFS)
│   └── web_interface.cpp/h   # Констрёр интерфейса
├── lib/                      # Библиотеки (вложеные в .pio/libdeps/ при сборке)
│   ├── GyverEncoder/         # Энкодер
│   ├── LCD_1602_RUS/         # Дисплей с кириллицей
│   ├── LiquidCrystal/        # I2C дисплей
│   └── LiquidCrystal_I2C-master/  # Библиотека I2C для LCD
├── data/                     # auto-pumps_valve_v2.1.cpp (архив старой версии)
├── .vscode/                  # Настройки VS Code (c_cpp_properties.json, launch.json)
├── .gitignore                # Исключения для git
├── README.md                 # Документация
├── HA_INTEGRATION_PLAN.md    # План интеграции с Home Assistant
├── error.log                 # Лог неудачной сборки (692 KB)
└── platformio.ini            # Конфигурация PlatformIO
```

**Всего файлов**: ~130 (включая бинарные в `.pio/` и `lib/`)

---

## 2. Ключевые файлы: обзор

### `platformio.ini` (435 байт)
- **Целевая плата**: `wemos_d1_r32` (ESP32dev)
- **Framework**: Arduino
- **Флеш**: qio, CPU 240 МГц
- **ФС**: LittleFS
- **Зависимости** (7 шт): GyverEncoder ^4.10, GyverDB 1.4.3, Settings 1.3.16,
  PubSubClient ^2.8, LiquidCrystal_I2C ^1.1.4, Wire, LCD_1602_RUS

### `README.md` (5 675 байт)
Полная документация: возможности, схема подключения, установка, использование веб-интерфейса, настройки проекта.

### `src/config.h` (1 607 байт)
```cpp
#define WIFI_SSID "Sloboda100"
#define WIFI_PASS "2716192023"
#define MQTT_SERVER "192.168.1.23"
#define NTP_SERVER "pool.ntp.org"
#define PUPM_AMOUNT 8
#define PUMP_PIN 4
#define SWITCH_LEVEL 0
#define PARALLEL 0
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_DEVICE_ID "autopumps_esp32"
#define MQTT_TOPIC_PREFIX "garden"
```
> ⚠️ `WIFI_PASS` захардкожен в тексте — для релиза небезопасно.

### `src/main.cpp` (1 531 байт)
Линейная инициализация: `Serial` → `pumpsInit()` → `WiFi.begin()` → `configTime()` → `mqttInit()` → `initWebServer()`. `loop()` вызывает `pumpsTick()`, `mqttTick()`, `handleWebRequests()`.

### `src/pumps_logic.cpp/h` (всего ~14,7 KB)
Логика 8 каналов полива. Содержит:
- `pumpsInit()` — инициализация пинов, LCD, EEPROM
- `pumpsTick()` — основной тик с таймерами, синхронизация LCD
- `periodTick()` / `flowTick()` — запуск и остановку помп
- `encoderTick()` — обработка энкодера (вращение, щелчок, удержание)
- `changeSettings()` — меню настроек (Пауза/Полив в секундах)
- `loadPumpNames()` / `savePumpName()` — имён помп в EEPROM (UTF-8, 20 байт на помпу)
- `webGetStatusJSON()` — JSON-статус для веба

### `src/mqtt_handler.cpp/h` (всего ~1,5 KB) — ⚠️ **ИСКЛЮЧЕНИЕ**
См. раздел 3.1.

### `src/web_server.cpp/h` (всего ~0,5 KB)
- `initWebServer()` — инициализация LittleFS + SettingsGyver, регистрация 16+ ключей БД
- `handleWebRequests()` — `esp_task_wdt_reset()` + `sett.tick()`

### `src/web_interface.cpp/h` (всего ~0,9 KB)
- `buildInterface()` — визуальная конструкция: Статус системы, Выбор помп, Настройки, Управление
- `updateInterface()` — актуализация меток без перезагрузки


## 3. Критические и серьёзные проблемы

### 🚨 3.1. `src/mqtt_handler.cpp` — **КРИТИЧЕСКАЯ БАГА**
- **Проблема**: файл сохранён в кодировке **UTF-16LE с BOM** (`\xff\xfe`), а не в UTF-8.
- **Судя по Git**: `Bin 7323 → 14446 bytes` — размер almost удвоился, внутри 6942 null-байта.
- **Последствие**: компилятор ESP32 вместо C++ кода читает двоичные данные → ошибки вида:
  ```
  error: 'i' does not name a type
  error: stray '\' in program
  warning: null character(s) ignored
  ```
- **Доказательство**: git `HEAD` содержит рабочую UTF-8 версию (7 323 байта). Рабочая копия — бинарная (14 446 байт).
- **Восстановление**: выполнить `git checkout -- src/mqtt_handler.cpp`
- Или перекодировать: `iconv -f UTF-16LE -t UTF-8 src/mqtt_handler.cpp > tmp && mv tmp src/mqtt_handler.cpp`

### 🚨 3.2. `error.log` — заглушка от неудачной сборки (692 654 байт)
Содержит полный лог ошибок компиляции. Показывает именно проблему UTF-16LE в `mqtt_handler.cpp` (строк 312 и т.д.). Это не часть проекта, а артефакт.

### ⚠️ 3.3. `src/mqtt_handler.cpp.bak` — бэкап (7 875 байт)
Содержит версию до битых изменений. Можно использовать как временный оригинал:
```bash
cp src/mqtt_handler.cpp.bak src/mqtt_handler.cpp
```
(но уточнить кодировку — .bak, скорее всего, тоже бинарный)

### ⚠️ 3.4. `src/mqtt_handler.h` — изменённый заголовок (21 строка)
Добавлены константы и функции для Home Assistant Discovery:
- `HA_DEVICE_ID`, `HA_DEVICE_NAME`, `HA_DEVICE_MANUFACTURER`, `HA_DEVICE_SW_VERSION`
- `haDiscoveryPayload()` — вспомогательная функция генерации payload'ов
- `sendMQTTDiscovery()` — 6 сущностей на одну помпу (Switch, Number×2, Sensor×2 — по факту 4)
> Проверьте, что новое API совместимо с существующим кодом.

### ⚠️ 3.5. `src/mqtt_handler.h` — `haDiscoveryPayload` не используется
Функция `haDiscoveryPayload` объявлена, но внутри `sendMQTTDiscovery` всё строится вручную. Лишний код — не отключайте, но и не оставляйте мёртвый.

### ⚠️ 3.6. `src/config.h` — захардкоженные пароли
- `WIFI_SSID`, `WIFI_PASS` — в репозитории. Для продакшена вынесите в `SettingsGyver` (LittleFS) или `.gitignore`.
- `MQTT_USER`/`MQTT_PASS` пустые — нет шифрования по MQTT. Для production требуется TLS/SSL (MQTT over WebSocket + auth).

### ⚠️ 3.7. `data/auto-pumps_valve_v2.1.cpp` (8,4 KB)
Архив старой версии прошивки. В проект не входит логически, но в репо — может сбивать новичков. Рекомендуется переместить в `.gitignore` или папку `archive/`.

### ⚠️ 3.8. `.clinerules` (3,4 KB) — собственное правило для CLI
Инструментальный файл, не для релиза. Исключён из git через `.gitignore`.

- Поддержка UTF-8 (кириллица) на вебе

## 10. Применённые изменения (HA Integration Plan)

### Выполнено (все шаги плана)

| # | Шаг плана | Статус | Описание |
|---|---|---|---|
| 1 | `haDiscoveryPayload()` helper | ✅ Выполнено | Добавлена в `src/mqtt_handler.h` — генерирует стандартизированный JSON payload с информацией об устройстве |
| 2 | Расширить `sendMQTTDiscovery()` | ✅ Выполнено | Добавлены 6 типов сущностей на каждую помпу: Switch, Number×2, Sensor×3 |
| 3 | Улучшить MQTT callback | ✅ Выполнено | Добавлена поддержка тем `garden/pumpX/set`, `garden/pumpX/period`, `garden/pumpX/work` |
| 4 | Документация YAML | ✅ Выполнено | Добавлены примеры для switch, sensor, binary_sensor, number, automation, group, lovelace |
| 5 | Файлы для изменения | ✅ Выполнено | `src/mqtt_handler.cpp`, `src/mqtt_handler.h` обновлены |

### Исправленные критические ошибки (до начала работы с планом)

| # | Проблема | Решение |
|---|---|---|
| 1 | `src/mqtt_handler.cpp` в кодировке UTF-16LE | Конвертировано в UTF-8 |
| 2 | Дубликат `HA_DEVICE_NAME` в `mqtt_handler.h` | Удалён (оригинал остаётся в `config.h`) |
| 3 | `haDiscoveryPayload()` без объявления `payload` | Добавлено `String payload = "{";` |
| 4 | `sendMQTTFullStatus()` без определения | Восстановлено определение функции |

### Новые MQTT темы (добавлены в callback)

| Тема | Действие |
|---|---|
| `garden/pumpX/set` | Общая команда включения/выключения (ON/OFF) |
| `garden/pumpX/period` | Установка периода паузы (секунды) |
| `garden/pumpX/work` | Установка времени полива (секунды) |
| `garden/pumpX/set_period` | Legacy — период паузы |
| `garden/pumpX/set_work` | Legacy — время полива |
| `garden/pumpX/manual` | Legacy — ручное управление |

### Новые Home Assistant сущности (на каждую помпу)

| # | Тип | Тема конфигурации | Описание |
|---|---|---|---|
| 1 | `switch` | `homeassistant/switch/auto-pumps-esp32_pumpX/config` | Включение/выключение помпы |
| 2 | `number` | `homeassistant/number/auto-pumps-esp32_pumpX_period/config` | Период паузы (0–360000 с) |
| 3 | `number` | `homeassistant/number/auto-pumps-esp32_pumpX_work/config` | Время полива (0–360000 с) |
| 4 | `sensor` | `homeassistant/sensor/auto-pumps-esp32_pumpX_status/config` | Статус помпы (ON/OFF) |
| 5 | `sensor` | `homeassistant/sensor/auto-pumps-esp32_pumpX_period_remaining/config` | Остаток паузы (duration) |
| 6 | `sensor` | `homeassistant/sensor/auto-pumps-esp32_pumpX_work_remaining/config` | Остаток полива (duration) |

## 11. Файлы для очистки (рекомендация)

| Файл | Статус | Рекомендация |
|---|---|---|
| `error.log` | ⚠️ | Удалить — артефакт неудачной сборки |
| `src/mqtt_handler.cpp.bak` | ⚠️ | Удалить после проверки что всё работает |
| `data/auto-pumps_valve_v2.1.cpp` | ⚠️ | Переместить в `archive/` или добавить в `.gitignore` |
| `.clinerules` | ⚠️ | Already in `.gitignore` |
