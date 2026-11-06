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

// Home Assistant Discovery constants
#define HA_DEVICE_MANUFACTURER "ESP32"
#define HA_DEVICE_SW_VERSION "2.0"

// Only prototypes of functions
void mqttInit();
void mqttTick();
void mqttPublishStatus(int id, bool state);

// NEW: Helper function for generating HA Discovery payloads
static String haDiscoveryPayload(const String& extraFields) {
    String payload = "{";
    payload += extraFields;
    payload += "}";
    return payload;
}

// NEW: Expanded discovery function - adds 6 entity types per pump
void sendMQTTDiscovery();

#endif
