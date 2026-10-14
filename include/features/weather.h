#pragma once

#include <Arduino.h>

void weatherUI();
void getWeatherInfo();
void updateWeatherPeriodically();
bool fetchWeatherWithRetry();
bool isWeatherAvailable();
String httpGETRequest(const char *serverName);