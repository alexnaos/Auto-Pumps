#pragma once
#include <GyverDBFile.h>
#include <SettingsGyver.h>

// Экспортируем объекты для связи с интерфейсом
extern GyverDBFile db;
extern SettingsGyver sett;

void initWebServer();
void handleWebRequests();