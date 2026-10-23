#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <U8g2lib.h>

namespace DrawingCanvas {
    enum class DisplayMode { BRAND, CANVAS, TEXT, DATE_COUNTER };

    bool pixelAt(int x, int y);
    void registerRoutes(WebServer& server);
    bool authorizeRequest(WebServer& server);
    void clear();
    void setBrandMode();
    void setCanvasMode();
    void setLiveText(const String& text);
    void setDateCounter(const String& type, const String& date);
    DisplayMode getDisplayMode();
    String getLiveText();
    bool needsRender();
    void markRendered();
    void servicePendingTasks();

    void renderToOLED(U8G2& display);

}
