#pragma once

void weatherUI();
void getWeatherInfo();
bool fetchWeatherWithRetry();
String httpGETRequest(const char* serverName);