# Auto-Pumps Интеграция с Home Assistant: Анализ и План

## Обзор проекта
Auto-Pumps — это система автоматизированного управления поливом на базе ESP32 с 8 каналами помп, веб-интерфейсом и поддержкой MQTT. В проекте уже есть базовая поддержка обнаружения Home Assistant (Home Assistant Discovery) через MQTT.

## Текущее состояние интеграции MQTT
- **Сущности**: Выключатели (Switch) для включения/выключения помп, Number для настройки периода паузы и времени полива (в секундах)
- **MQTT темы**: `garden/pumpX/status`, `garden/pumpX/set`, `garden/pumpX/period`, `garden/pumpX/work`
- **Discovery**: Автоматически отправляется при подключении к MQTT брокеру на префикс `homeassistant/`



### 1. Добавить функцию-помощник
Создать функцию `haDiscoveryPayload()` для генерации стандартизированных payload'ов обнаружения Home Assistant с информацией об устройстве.

### 2. Расширить функцию `sendMQTTDiscovery()`
Добавить 6 новых типов сущностей на каждую помпу:
1. **Switch** — улучшенная реализация
2. **Number** — период паузы (с min/max/step/единица измерения)
3. **Number** — время полива (с min/max/step/единица измерения)
4. **Sensor** — статус помпы (enum: ON/OFF)
5. **Sensor** — оставшееся время паузы (duration, секунды)
6. **Sensor** — оставшееся время полива (duration, секунды)

### 3. Улучшить MQTT callback
Добавить поддержку новых тем:
- `garden/pumpX/set` — общая тема команд
- `garden/pumpX/period` — значение периода
- `garden/pumpX/work` — длительность полива

### 4. Документация конфигурации Home Assistant
Представить примеры YAML для:
- Сущностей выключателей
- Сенсоров
- Бинарных сенсоров
- Автоматизаций и сцен
- Групп и дашбордов

### 5. Файлы для изменения
- `src/mqtt_handler.cpp` — добавить функцию-помощник и расширить обнаружение

## Необходимые изменения в коде

### Новая функция-helper (добавляется рядом с `mqttPublishStatus`):
```cpp
static String haDiscoveryPayload(const String& extraFields) {
    String payload = "{";
    payload += "\"dev\":{\"ids\":\"" + String(HA_DEVICE_ID) + "\",\"name\":\"" + String(HA_DEVICE_NAME) + "\",\"mf\":\"ESP32\",\"sw\":\"2.0\"}";
    payload += extraFields;
    payload += "}";
    return payload;
}
```

### Расширенная функция обнаружения включает:
- Улучшенную реализацию выключателя
- Number сущности с правильными диапазонами (0-360000 секунд = 0-100 часов)
- Сенсоры для статуса и оставшегося времени
- Правильные device class и unit_of_measurement

## Построение и тестирование
1. `pio run --environment wemos_d1_r32` — скомпилировать

Прошивка по воздуху, согласно используемой библиотеки
C:\Projects\PlatformIO\RegPowerMqtt\docsГайвер


## Примеры конфигурации Home Assistant YAML

### Сущности выключателей (Switch)
```yaml
switch:
  - platform: mqtt
    name: "Помпа 1"
    state_topic: "garden/pump1/status"
    command_topic: "garden/pump1/manual"
    payload_on: "ON"
    payload_off: "OFF"
    device_class: switch
    icon: mdi:water-pump

  - platform: mqtt
    name: "Помпа 2"
    state_topic: "garden/pump2/status"
    command_topic: "garden/pump2/manual"
    payload_on: "ON"
    payload_off: "OFF"
    device_class: switch
    icon: mdi:water-pump
```

### Сенсоры (Sensor)

#### Статус помпы
```yaml
sensor:
  - platform: mqtt
    name: "Статус Помпа 1"
    state_topic: "garden/pump1/status"
    device_class: enum
    icon: mdi:water-pump
```

#### Оставшееся время паузы
```yaml
  - platform: mqtt
    name: "Остаток паузы Помпа 1"
    state_topic: "garden/pump1/period_remaining"
    unit_of_measurement: "с"
    device_class: duration
    state_class: measurement
    icon: mdi:timer-outline
```

#### Оставшееся время полива
```yaml
  - platform: mqtt
    name: "Остаток полива Помпа 1"
    state_topic: "garden/pump1/work_remaining"
    unit_of_measurement: "с"
    device_class: duration
    state_class: measurement
    icon: mdi:timer-outline
```

### Бинарные сенсоры (Binary Sensor)
```yaml
binary_sensor:
  - platform: mqtt
    name: "Полив активен Помпа 1"
    state_topic: "garden/pump1/status"
    payload_on: "ON"
    payload_off: "OFF"
    device_class: running
    icon: mdi:water-outline
```

### Числовые сенсоры (Number) — настройка периодов
```yaml
number:
  - platform: mqtt
    name: "Пауза Помпа 1 (сек)"
    state_topic: "garden/pump1/period"
    command_topic: "garden/pump1/set_period"
    min: 0
    max: 360000
    step: 1
    unit_of_measurement: "с"
    mode: box

  - platform: mqtt
    name: "Полив Помпа 1 (сек)"
    state_topic: "garden/pump1/work"
    command_topic: "garden/pump1/set_work"
    min: 0
    max: 360000
    step: 1
    unit_of_measurement: "с"
    mode: box
```

### Автоматизации (Automation)
```yaml
automation:
  - alias: "Утренний полив огурцов"
    trigger:
      - platform: time
        at: "08:00:00"
    condition:
      - condition: state
        entity_id: switch.pompa_1
        state: "off"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.pompa_1

  - alias: "Вечерний полив томатов"
    trigger:
      - platform: time
        at: "19:00:00"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.pompa_2

  - alias: "Остановить всё ночью"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: switch.turn_off
        target:
          entity_id:
            - switch.pompa_1
            - switch.pompa_2
            - switch.pompa_3
            - switch.pompa_4
            - switch.pompa_5
            - switch.pompa_6
            - switch.pompa_7
            - switch.pompa_8
```

### Группы (Group)
```yaml
group:
  pompa_all:
    name: "Все помпы"
    entities:
      - switch.pompa_1
      - switch.pompa_2
      - switch.pompa_3
      - switch.pompa_4
      - switch.pompa_5
      - switch.pompa_6
      - switch.pompa_7
      - switch.pompa_8
    icon: mdi:water-pump
```

### Дашборд (Lovelace)
Пример карточки для управления помпой:
```yaml
type: entities
title: "Авто-помпы"
entities:
  - entity: switch.pompa_1
    name: "Помпа 1"
  - entity: sensor.ostatok_pauzy_pompa_1
    name: "Пауза остаток"
  - entity: sensor.ostatok_poliva_pompa_1
    name: "Полив остаток"
  - entity: number.pauza_pompa_1
    name: "Пауза (сек)"
  - entity: number.poliv_pompa_1
    name: "Полив (сек)"
show_header_toggle: false
```

## Ограничения (из .clinerules)
- C++17/20 для встраиваемых систем
- Структура проекта PlatformIO (src/, include/, platformio.ini)
- Эффективное использование памяти ESP32 (320KB RAM)
- Избегать `delay()` в loop(), использовать таймеры на основе `millis()`
- MQTT темы должны быть структурированы четко
- Русский язык для комментариев/сообщений, английский для кода
- Без "воды" (вербального текста), только essential код/конфигурация
- Формат дифф/патч для изменений кода

