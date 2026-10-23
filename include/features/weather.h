#pragma once
#include <U8g2lib.h>

void weatherUI();
void getWeatherInfo();
bool fetchWeatherWithRetry();
String httpGETRequest(const char* serverName);
void fetchThreeDayForecast();
void drawThreeDayForecast(U8G2 &display);
