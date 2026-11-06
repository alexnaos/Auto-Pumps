#include "mqtt_handler.h"

// 1. Физическое создание объектов
WiFiClient espClientObj;
PubSubClient clientObj(espClientObj);

// 2. Реализация функций
void mqttCallback(char *topic, byte *payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    String t = String(topic);
    uint32_t val = msg.toInt();

    Serial.printf("[MQTT] Recv: %s -> %s\n", t.c_str(), msg.c_str());

    int pPos = t.indexOf("pump");
    if (pPos != -1) {
        int pIdx = t.substring(pPos + 4, t.indexOf("/", pPos + 4)).toInt() - 1;
        if (pIdx >= 0 && pIdx < PUPM_AMOUNT) {
            if (t.endsWith("set_period")) period_time[pIdx] = val;
            else if (t.endsWith("set_work"))   pumping_time[pIdx] = val;
            else if (t.endsWith("manual")) {
                if (val == 1) {
                    digitalWrite(pump_pins[pIdx], SWITCH_LEVEL);
                    mqttPublishStatus(pIdx, true);
                } else {
                    digitalWrite(pump_pins[pIdx], !SWITCH_LEVEL);
                    mqttPublishStatus(pIdx, false);
                }
            }
            update_EEPROM();
            changeSet();
        }
    }
}

void reconnect() {
    if (!clientObj.connected() && WiFi.status() == WL_CONNECTED) {
        Serial.print("[MQTT] Connecting...");
        String clientId = "ESP32_Water_" + String(WiFi.macAddress());
        if (clientObj.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println("OK");
            // Подписка на команды для каждой помпы
            clientObj.subscribe("garden/+/set_period");
            clientObj.subscribe("garden/+/set_work");
            clientObj.subscribe("garden/+/manual");
            clientObj.subscribe("garden/command"); // глобальная команда

            // Отправляем конфигурацию HA Discovery
            sendMQTTDiscovery();

            // Публикуем текущий статус всех помп
            sendMQTTFullStatus();
        } else {
            Serial.print("Fail, rc=");
            Serial.println(clientObj.state());
        }
    }
}

void mqttInit() {
    clientObj.setServer(MQTT_SERVER, MQTT_PORT);
    clientObj.setCallback(mqttCallback);
}

void mqttTick() {
    static uint32_t lastMqttT;
    if (millis() - lastMqttT > 5000) {
        lastMqttT = millis();
        if (!clientObj.connected()) reconnect();
    }
    clientObj.loop();
}

void mqttPublishStatus(int id, bool state) {
    if (clientObj.connected()) {
        String topic = String(MQTT_TOPIC_PREFIX) + "/pump" + String(id + 1) + "/status";
        const char* topic_c = topic.c_str();
        clientObj.publish(topic_c, state ? "ON" : "OFF", true);
    }
}

// === Home Assistant Discovery ===
void sendMQTTDiscovery() {
    if (!clientObj.connected()) return;

    for (int i = 0; i < PUPM_AMOUNT; i++) {
        String pumpId = String(i + 1);
        String baseTopic = String(MQTT_TOPIC_PREFIX) + "/pump" + pumpId;
        String name = pumpNames[i];

        // 1. Switch (включение/выключение помпы)
        {
            String topic = String(HA_DISCOVERY_PREFIX) + "/switch/"
                + HA_DEVICE_ID + "_pump" + pumpId + "/config";
            String payload = "{";
            payload += "\"name\":\"Помпа " + pumpId + " (" + name + ")\",";
            payload += "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_pump" + pumpId + "\",";
            payload += "\"stat_t\":\"" + baseTopic + "/status\",";
            payload += "\"cmd_t\":\"" + baseTopic + "/manual\",";
            payload += "\"payload_on\":\"1\",";
            payload += "\"payload_off\":\"0\",";
            payload += "\"state_on\":\"ON\",";
            payload += "\"state_off\":\"OFF\",";
            payload += "\"qos\":1,";
            payload += "\"dev\":{\"ids\":\"" + String(HA_DEVICE_ID) + "\",";
            payload += "\"name\":\"" + String(HA_DEVICE_NAME) + "\",";
            payload += "\"mf\":\"ESP32\",";
            payload += "\"sw\":\"2.0\"}";
            payload += "}";
            clientObj.publish(topic.c_str(), payload.c_str(), true);
        }

        // 2. Number: период (пауза) в секундах
        {
            String topic = String(HA_DISCOVERY_PREFIX) + "/number/"
                + HA_DEVICE_ID + "_pump" + pumpId + "_period/config";
            String payload = "{";
            payload += "\"name\":\"Пауза помпа " + pumpId + "\",";
            payload += "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_pump" + pumpId + "_period\",";
            payload += "\"stat_t\":\"" + baseTopic + "/period\",";
            payload += "\"cmd_t\":\"" + baseTopic + "/set_period\",";
            payload += "\"min\":0,";
            payload += "\"max\":360000,";
            payload += "\"step\":1,";
            payload += "\"unit_of_meas\":\"s\",";
            payload += "\"mode\":\"box\",";
            payload += "\"qos\":1,";
            payload += "\"dev\":{\"ids\":\"" + String(HA_DEVICE_ID) + "\"}";
            payload += "}";
            clientObj.publish(topic.c_str(), payload.c_str(), true);
        }

        // 3. Number: время работы (полив) в секундах
        {
            String topic = String(HA_DISCOVERY_PREFIX) + "/number/"
                + HA_DEVICE_ID + "_pump" + pumpId + "_work/config";
            String payload = "{";
            payload += "\"name\":\"Полив помпа " + pumpId + "\",";
            payload += "\"uniq_id\":\"" + String(HA_DEVICE_ID) + "_pump" + pumpId + "_work\",";
            payload += "\"stat_t\":\"" + baseTopic + "/work\",";
            payload += "\"cmd_t\":\"" + baseTopic + "/set_work\",";
            payload += "\"min\":0,";
            payload += "\"max\":360000,";
            payload += "\"step\":1,";
            payload += "\"unit_of_meas\":\"s\",";
            payload += "\"mode\":\"box\",";
            payload += "\"qos\":1,";
            payload += "\"dev\":{\"ids\":\"" + String(HA_DEVICE_ID) + "\"}";
            payload += "}";
            clientObj.publish(topic.c_str(), payload.c_str(), true);
        }
    }

    Serial.println("[MQTT] HA Discovery sent for all pumps");
}

// === Публикация полного статуса всех помп ===
void sendMQTTFullStatus() {
    if (!clientObj.connected()) return;

    for (int i = 0; i < PUPM_AMOUNT; i++) {
        String base = String(MQTT_TOPIC_PREFIX) + "/pump" + String(i + 1);
        String periodTopic = base + "/period";
        String workTopic = base + "/work";
        String statusTopic = base + "/status";
        clientObj.publish(periodTopic.c_str(), String(period_time[i]).c_str(), true);
        clientObj.publish(workTopic.c_str(), String(pumping_time[i]).c_str(), true);
        clientObj.publish(statusTopic.c_str(), pump_state[i] == SWITCH_LEVEL ? "ON" : "OFF", true);
    }

    // Общий статус JSON
    String json = "{\"now_pumping\":";
    json += now_pumping ? "true" : "false";
    json += ",\"uptime\":";
    json += String(millis() / 1000);
    json += ", \"fw_version\": \"";
    json += FIRMWARE_VERSION;
    json += "\"";
    json += "}";
    String statusTop = String(MQTT_TOPIC_PREFIX) + "/status";
    clientObj.publish(statusTop.c_str(), json.c_str(), true);
}