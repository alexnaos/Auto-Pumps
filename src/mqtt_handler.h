#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#include "pumps_logic.h"

// Только объявляем (extern), что эти объекты есть в .cpp файле
extern WiFiClient espClientObj;
extern PubSubClient clientObj;
extern boolean now_pumping;

// Только прототипы функций
void mqttInit();
void mqttTick();
void mqttPublishStatus(int id, bool state);
void mqttCallback(char *topic, byte *payload, unsigned int length);
void reconnect();

#endif
