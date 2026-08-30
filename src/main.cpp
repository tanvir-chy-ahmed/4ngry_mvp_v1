#include <Arduino.h>
#include <U8g2lib.h>
#include "all_expressions.h"
#include <WiFi.h>
#include "time.h"
#include "core/Global.h"
#include "features/watch.h"
#include "core/boot.h"
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

void setup()
{
    Serial.begin(115200);
    BootUI::run();
    randomSeed(analogRead(0) ^ (analogRead(1) << 8));

    // Start in IDLE
    initExpression(IDLE, anim);
    exprStartMs = millis();
    scheduleIdleBlink(millis());
    scheduleIdleLook(millis());
    
}

void loop()
{

    unsigned long now = millis();
    handleTouch(now);

    // ─────────────────────────────────────────
    // Frame rate gate
    // ─────────────────────────────────────────
    if (now - lastFrameMs < FRAME_MS)
        return;

    lastFrameMs = now;

    float ms = (float)now;

    if (transitioning)
    {
        transitionT += 0.08f;

        if (transitionT >= 1.0f)
        {
            // Transition finished
            transitioning = false;

            currentExpr = nextExpr;
            exprStartMs = now;

            initExpression(currentExpr, anim);

            // Reset idle-specific scheduling
            if (currentExpr == IDLE)
            {
                scheduleIdleBlink(now);
                scheduleIdleLook(now);
            }
        }
        else
        {
            // ─────────────────────────────────
            // Pinch transition
            // ─────────────────────────────────
            float squeeze;

            if (transitionT < 0.5f)
            {
                squeeze =
                    1.0f -
                    easeIn(transitionT * 2.0f);
            }
            else
            {
                squeeze =
                    easeOut((transitionT - 0.5f) * 2.0f);
            }

            anim.eyeScaleX = squeeze;
            anim.eyeScaleY = squeeze;

            anim.bounce =
                (1.0f - squeeze) * -3.0f;
        }
    }
    else
    {
        // ─────────────────────────────────────
        // IDLE background behavior
        // ─────────────────────────────────────
        if (currentExpr == IDLE)
        {
            // Scheduled blink
            if (now >= idleNextBlink)
            {
                float blinkPhase =
                    fmodf(
                        (now - idleNextBlink) / 200.0f,
                        1.0f);

                float c;

                if (blinkPhase < 0.5f)
                {
                    c = easeIn(blinkPhase * 2.0f);
                }
                else
                {
                    c = easeOut(
                        (1.0f - blinkPhase) * 2.0f);
                }

                anim.eyeScaleY =
                    1.0f - 0.9f * c;

                if (blinkPhase > 0.95f)
                {
                    anim.eyeScaleY = 1.0f;

                    scheduleIdleBlink(now);
                }
            }

            // ─────────────────────────────────
            // Scheduled gaze shift
            // ─────────────────────────────────
            if (now >= idleNextLook)
            {
                idleGazeTarget =
                    (float)random(-3, 4) / 3.0f;

                scheduleIdleLook(now);
            }

            // Smooth gaze movement
            idleGazeX +=
                (idleGazeTarget - idleGazeX) * 0.06f;
        }

        // ─────────────────────────────────────
        // Update expression animation
        // ─────────────────────────────────────
        updateExpression(
            currentExpr,
            anim,
            ms);
    }

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    // Draw currently selected screen
    renderScreen(currentScreen);

    u8g2.sendBuffer();
}