#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "update/ota_ui.h"

#include "features/watch.h"
#include "features/weather.h"
#define FRAME_MS 33
#define TOUCH_DEBOUNCE 250

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

enum Expression
{
    IDLE = 0,
    BLINK,
    HAPPY,
    SAD,
    ANGRY,
    SURPRISED,
    TIRED,
    CURIOUS,
    CONFUSED,
    LAUGHING,
    LOVE,
    WINK,
    SCARED,
    SUSPICIOUS,
    DIZZY,
    SLEEP,
    MUSIC,
    HUNGRY,
    EXPR_COUNT
};
enum Screen
{
    SCREEN_IDLE = 0,
    SCREEN_WEATHER,
    SCREEN_CLOCK,
    SCREEN_UPDATE,
    // SCREEN_MENU,
    SCREEN_COUNT
};

Screen currentScreen = SCREEN_IDLE;
struct AnimState
{
    int phase = 0;
    float t = 0.0f;          // 0..1 within current phase
    float phaseSpeed = 0.0f; // t increment per frame

    // ── eye geometry ──
    float eyeW = 20.0f; // base width
    float eyeH = 20.0f; // base height
    float eyeScaleX = 1.0f;
    float eyeScaleY = 1.0f;
    float eyeOffsetX = 0.0f; // horizontal shift of both eyes
    float eyeOffsetY = 0.0f; // vertical shift of both eyes
    float leftEyeDY = 0.0f;  // independent vertical offset
    float rightEyeDY = 0.0f;
    float leftEyeScale = 1.0f; // independent scale
    float rightEyeScale = 1.0f;
    int eyeRadius = 4; // corner radius

    // ── eyebrows ──
    bool showBrows = false;
    int leftBrowAngle = 0; // negative = angry tilt
    int rightBrowAngle = 0;
    float browOffsetY = -3.0f;

    // ── mouth ──
    bool showMouth = true;
    float mouthOpen = 0.0f;  // 0..1
    float mouthSmile = 0.0f; // -1 frown .. +1 smile
    float mouthOffsetY = 0.0f;

    // ── face motion ──
    float bounce = 0.0f;
    float shake = 0.0f;
    float faceOffsetY = 0.0f;

    // ── decoration ──
    bool showHeart = false;
    bool showTear = false;
    bool showZzz = false;
    bool showDizzy = false;
    bool showSweat = false;
    bool showStars = false;
    float heartScale = 1.0f;
    int zzzPhase = 0; // 0=Z 1=Zz 2=Zzz
    float zzzY = 0.0f;

    // ── wink ──
    bool winkLeft = false;
    float winkClose = 0.0f; // 0=open 1=closed

    // ── sleep ──
    float breathe = 0.0f;

    // ── dizzy rotation index ──
    int dizzyAngle = 0;

    // ── music notes ──
    bool showMusic = false;
    float note1Y = 0.0f; // floating note positions
    float note2Y = 0.0f;
    int noteFrame = 0;

    // ── hungry / drool ──
    bool showDrool = false;
    float droolLen = 0.0f;   // drool drop length (0..8)
    bool showRumble = false; // belly rumble lines
    float rumblePhase = 0.0f;

    // ── cute talk ──
    bool cuteMode = false;  // if true, use kawaii mouth shape
    float talkPhase = 0.0f; // 0..1 oscillates for cute talk
};

// ─────────────────────────────────────────────
//  Global state
// ─────────────────────────────────────────────
Expression currentExpr = IDLE;
Expression nextExpr = IDLE;
bool transitioning = false;
float transitionT = 0.0f;

AnimState anim;

unsigned long lastFrameMs = 0;

// add for menu
unsigned long lastTouchMs = 0;
/*

unsigned long lastTouchMs = 0;
bool lastTouchVal = false;

*/

bool lastTouchVal = false;
bool touchActive = false;
unsigned long touchStartMs = 0;
#define LONG_PRESS_MS 400
#define TOUCH_RELEASE_DEBOUNCE 50

// Idle sub-state
unsigned long idleNextBlink = 0;
unsigned long idleNextLook = 0;
float idleGazeX = 0.0f;
float idleGazeTarget = 0.0f;
float idleBreath = 0.0f;

// ─────────────────────────────────────────────
//  Math helpers
// ─────────────────────────────────────────────
static float lerpF(float a, float b, float t) { return a + (b - a) * t; }
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1
                                                         : v; }
static float easeIn(float t) { return t * t; }
static float easeOut(float t)
{
    float f = 1.0f - t;
    return 1.0f - f * f;
}
static float easeInOut(float t) { return t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t); }
static float bounce(float t)
{
    float x = 1 - t;
    return 1 - (x * x * x * x - 1.5f * x * x + 0.5f); // simple bounce
}

// Sine wave oscillator — returns -1..+1
static float osc(float tMs, float periodMs)
{
    return sinf(tMs * 2 * PI / periodMs);
}

// ─────────────────────────────────────────────
//  Drawing primitives
// ─────────────────────────────────────────────

// Draw a rounded-rect eye at (cx,cy) with given w, h, radius
static void drawEyeShape(int cx, int cy, int w, int h, int r)
{
    if (w < 2)
        w = 2;
    if (h < 2)
        h = 2;
    if (r > w / 2)
        r = w / 2;
    if (r > h / 2)
        r = h / 2;
    if (r < 0)
        r = 0;
    int x = cx - w / 2;
    int y = cy - h / 2;
    u8g2.drawRBox(x, y, w, h, r);
}

// Draw a single pixel-art eyebrow above eye at (cx, eyeTop)
// angle: negative = inner corner down (angry), positive = inner corner up (sad)
static void drawEyebrow(int cx, int eyeTop, int angle, int eyeW)
{
    int bw = eyeW + 4;
    int bx = cx - bw / 2;
    int by = eyeTop - 6;
    // 3-pixel thick brow with angle
    for (int i = 0; i < bw; i++)
    {
        int dy = (angle * i) / bw; // slope across eyebrow
        u8g2.drawPixel(bx + i, by + dy);
        u8g2.drawPixel(bx + i, by + dy + 1);
    }
}

// Draw kawaii cute mouth (UwU style) or classic mouth
// cuteMode=true: small rounded ω-shape (three bumps) with animated talk
// cuteMode=false: classic arc/ellipse mouth
static void drawMouth(int cx, int cy, float smile, float openness,
                      bool cuteMode = false, float talkPhase = 0.0f)
{

    if (cuteMode)
    {
        // ── Kawaii ω-mouth ────────────────────────────
        // Three small bumps: left, center, right — classic robot cute style
        // openness drives how wide each bump is; talkPhase animates the center bump
        int bumpR = 3 + (int)(openness * 3.0f);            // bump radius 3..6
        int spacing = bumpR + 3;                           // gap between bumps
        int centerBumpR = bumpR + (int)(talkPhase * 3.0f); // center bounces when talking

        // Draw filled arcs as quarter-circles (bottom halves)
        // Left bump
        u8g2.drawDisc(cx - spacing, cy, bumpR, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
        // Center bump (bigger, animated)
        u8g2.drawDisc(cx, cy, centerBumpR, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
        // Right bump
        u8g2.drawDisc(cx + spacing, cy, bumpR, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);

        // Thin top connector line to tie the three bumps
        u8g2.drawHLine(cx - spacing - bumpR, cy, spacing * 2 + bumpR * 2 + 1);

        // Cheek blush dots when smiling
        if (smile > 0.4f)
        {
            u8g2.drawPixel(cx - spacing - bumpR - 5, cy + 2);
            u8g2.drawPixel(cx - spacing - bumpR - 4, cy + 2);
            u8g2.drawPixel(cx + spacing + bumpR + 4, cy + 2);
            u8g2.drawPixel(cx + spacing + bumpR + 5, cy + 2);
        }
        return;
    }

    // ── Classic mouth (non-cute mode) ────────────────
    int mw = 16;
    int mh = (int)(4 * fabsf(smile));
    if (mh < 1)
        mh = 1;

    if (openness > 0.2f)
    {
        // Open mouth — small rounded rect (cuter than ellipse)
        int ow = mw;
        int oh = (int)(openness * 10) + 2;
        int rx = 2; // corner radius for cute rounded open mouth
        if (rx > ow / 2)
            rx = ow / 2;
        if (rx > oh / 2)
            rx = oh / 2;
        u8g2.drawRBox(cx - ow / 2, cy - oh / 2, ow, oh, rx);
    }
    else
    {
        // Draw arc-approximated curve with pixels
        for (int i = -mw / 2; i <= mw / 2; i++)
        {
            float norm = (float)i / (mw / 2);
            int dy = (int)(smile * mh * (1 - norm * norm));
            u8g2.drawPixel(cx + i, cy - dy);
            if (fabsf(smile) > 0.5f)
                u8g2.drawPixel(cx + i, cy - dy + 1);
        }
    }
}

// Draw a pixel-art music note at (cx, cy)
// type 0 = eighth note ♪, type 1 = quarter note ♩
static void drawMusicNote(int cx, int cy, int type)
{
    // Note head (2×2 filled oval)
    u8g2.drawPixel(cx, cy);
    u8g2.drawPixel(cx + 1, cy);
    u8g2.drawPixel(cx, cy + 1);
    u8g2.drawPixel(cx + 1, cy + 1);
    // Stem (4px tall going up)
    u8g2.drawVLine(cx + 2, cy - 4, 5);
    if (type == 0)
    {
        // Flag on the stem (eighth note)
        u8g2.drawPixel(cx + 3, cy - 4);
        u8g2.drawPixel(cx + 4, cy - 3);
        u8g2.drawPixel(cx + 3, cy - 2);
    }
}

// Draw drool drop below mouth center at (cx, mouthBottom)
static void drawDrool(int cx, int mouthBottom, float len)
{
    int iLen = (int)len;
    if (iLen < 1)
        return;
    // Thin stem
    for (int i = 0; i < iLen - 1; i++)
    {
        u8g2.drawPixel(cx, mouthBottom + i);
    }
    // Teardrop tip
    u8g2.drawPixel(cx - 1, mouthBottom + iLen - 1);
    u8g2.drawPixel(cx, mouthBottom + iLen - 1);
    u8g2.drawPixel(cx + 1, mouthBottom + iLen - 1);
    u8g2.drawPixel(cx, mouthBottom + iLen);
}

// Draw animated hunger-rumble lines below the face (belly grumble)
static void drawRumbleLines(int cx, int faceBottom, float phase)
{
    int shiftX = (int)(sinf(phase) * 2.0f);
    // Three short wavy lines
    for (int row = 0; row < 3; row++)
    {
        int y = faceBottom + 4 + row * 3;
        for (int i = -6; i <= 6; i++)
        {
            int dy = (int)(sinf((i + shiftX + row * 2) * 0.9f) * 1.5f);
            u8g2.drawPixel(cx + i, y + dy);
        }
    }
}

// Draw a simple pixel-heart centered at (cx, cy) scaled by s
static void drawHeart(int cx, int cy, float s)
{
    int sz = (int)(8 * s);
    if (sz < 3)
        sz = 3;
    // Classic heart pixel art
    for (int y = 0; y < sz; y++)
    {
        for (int x = -sz; x <= sz; x++)
        {
            float nx = (float)x / sz;
            float ny = (float)y / sz;
            // Heart formula: (x²+y²-1)³ - x²y³ < 0
            float v = (nx * nx + ny * ny - 1);
            if (v * v * v - nx * nx * ny * ny * ny < 0.15f)
                u8g2.drawPixel(cx + x, cy - y + sz / 2);
        }
    }
}

// Draw a tear drop at (cx, cy)
static void drawTear(int cx, int cy)
{
    u8g2.drawPixel(cx, cy);
    u8g2.drawPixel(cx, cy + 1);
    u8g2.drawPixel(cx - 1, cy + 1);
    u8g2.drawPixel(cx + 1, cy + 1);
    u8g2.drawPixel(cx, cy + 2);
}

// Draw a single Z of size (for Zzz)
static void drawZ(int x, int y, int sz)
{
    // Top bar
    for (int i = 0; i < sz; i++)
        u8g2.drawPixel(x + i, y);
    // Diagonal
    for (int i = 0; i < sz; i++)
        u8g2.drawPixel(x + sz - 1 - i, y + i * (sz / sz));
    // Bottom bar
    for (int i = 0; i < sz; i++)
        u8g2.drawPixel(x + i, y + sz - 1);
}

// Approximate Zzz with a small pixel Z
static void drawZLetter(int x, int y, int size)
{
    // top
    for (int i = 0; i < size; i++)
    {
        u8g2.drawPixel(x + i, y);
    }
    // diagonal
    for (int i = 0; i < size; i++)
    {
        u8g2.drawPixel(x + size - 1 - i, y + i);
    }
    // bottom
    for (int i = 0; i < size; i++)
    {
        u8g2.drawPixel(x + i, y + size - 1);
    }
}

// Draw small sweat drop
static void drawSweatDrop(int cx, int cy)
{
    u8g2.drawPixel(cx, cy);
    u8g2.drawPixel(cx - 1, cy + 1);
    u8g2.drawPixel(cx + 1, cy + 1);
    u8g2.drawPixel(cx, cy + 2);
    u8g2.drawPixel(cx, cy + 3);
}

// Draw dizzy spiral mark (two arcs approximated)
static void drawDizzyMark(int cx, int cy, int angle)
{
    // 8 pixels in a circle, rotated by angle
    for (int i = 0; i < 5; i++)
    {
        float a = (float)(i + angle) * (PI / 4.0f);
        int px = cx + (int)(5 * cosf(a));
        int py = cy + (int)(3 * sinf(a));
        u8g2.drawPixel(px, py);
    }
}

// Draw a small star at (cx, cy)
static void drawStar(int cx, int cy)
{
    u8g2.drawPixel(cx, cy);
    u8g2.drawPixel(cx - 2, cy);
    u8g2.drawPixel(cx + 2, cy);
    u8g2.drawPixel(cx, cy - 2);
    u8g2.drawPixel(cx, cy + 2);
    u8g2.drawPixel(cx - 1, cy - 1);
    u8g2.drawPixel(cx + 1, cy - 1);
    u8g2.drawPixel(cx - 1, cy + 1);
    u8g2.drawPixel(cx + 1, cy + 1);
}

// Half-closed eye (for tired / wink) — draws only the top half filled
static void drawHalfEye(int cx, int cy, int w, int h, int r, float closeFrac)
{
    // Draw full eye base
    if (w < 2)
        w = 2;
    if (h < 2)
        h = 2;
    int x0 = cx - w / 2, y0 = cy - h / 2;
    u8g2.drawRBox(x0, y0, w, h, r);
    // Erase lower portion with black rect to simulate eyelid
    int lidH = (int)(h * closeFrac);
    if (lidH > 0)
    {
        u8g2.setDrawColor(0);
        u8g2.drawBox(x0, y0 + h - lidH, w, lidH);
        u8g2.setDrawColor(1);
    }
}

// ─────────────────────────────────────────────
//  Composite face renderer
//  Uses AnimState to draw the complete face
// ─────────────────────────────────────────────

// Eye center positions (base)
#define L_EYE_CX 32
#define R_EYE_CX 96
#define EYE_CY 28
#define MOUTH_CX 64
#define MOUTH_CY 50

static void renderFace(const AnimState &a, float globalMs)
{
    // Apply global shake
    int shakeX = (int)(a.shake * sinf(globalMs * 0.08f) * 3.0f);
    int shakeY = 0;

    // Face Y offset (bounce/breathe)
    int faceY = (int)(a.bounce * -4.0f + a.faceOffsetY + a.breathe * -1.5f);

    // ─── Left eye ────────────────────────────
    int lCX = L_EYE_CX + (int)a.eyeOffsetX + shakeX;
    int lCY = EYE_CY + faceY + (int)a.eyeOffsetY + (int)a.leftEyeDY;
    int lW = (int)(a.eyeW * a.eyeScaleX * a.leftEyeScale);
    int lH = (int)(a.eyeH * a.eyeScaleY * a.leftEyeScale);

    // ─── Right eye ───────────────────────────
    int rCX = R_EYE_CX + (int)a.eyeOffsetX + shakeX;
    int rCY = EYE_CY + faceY + (int)a.eyeOffsetY + (int)a.rightEyeDY;
    int rW = (int)(a.eyeW * a.eyeScaleX * a.rightEyeScale);
    int rH = (int)(a.eyeH * a.eyeScaleY * a.rightEyeScale);

    int r = a.eyeRadius;

    // Draw eyes
    if (a.winkLeft)
    {
        // Wink: left eye closes
        drawHalfEye(lCX, lCY, lW, lH, r, a.winkClose);
        drawEyeShape(rCX, rCY, rW, rH, r);
    }
    else
    {
        drawEyeShape(lCX, lCY, lW, lH, r);
        drawEyeShape(rCX, rCY, rW, rH, r);
    }

    // ─── Eyebrows ─────────────────────────────
    if (a.showBrows)
    {
        drawEyebrow(lCX, lCY - lH / 2, a.leftBrowAngle, lW);
        drawEyebrow(rCX, rCY - rH / 2, a.rightBrowAngle, rW);
    }

    // ─── Mouth ────────────────────────────────
    if (a.showMouth)
    {
        int mY = MOUTH_CY + faceY + (int)a.mouthOffsetY;
        drawMouth(MOUTH_CX + shakeX, mY, a.mouthSmile, a.mouthOpen,
                  a.cuteMode, a.talkPhase);
    }

    // ─── Decorations ──────────────────────────
    if (a.showHeart)
    {
        drawHeart(lCX - 2, lCY - lH / 2 - 8, a.heartScale * 0.7f);
        drawHeart(rCX + 2, rCY - rH / 2 - 8, a.heartScale * 0.7f);
    }
    if (a.showTear)
    {
        int tearX = lCX - lW / 2 + 2;
        int tearY = lCY + lH / 2 + 2;
        drawTear(tearX, tearY);
    }
    if (a.showZzz)
    {
        int zx = rCX + rW / 2 + 3;
        int zy = rCY - rH / 2 - (int)(a.zzzY);
        if (a.zzzPhase >= 0)
        {
            drawZLetter(zx, zy + 0, 4);
        }
        if (a.zzzPhase >= 1)
        {
            drawZLetter(zx + 2, zy - 5, 5);
        }
        if (a.zzzPhase >= 2)
        {
            drawZLetter(zx + 4, zy - 11, 6);
        }
    }
    if (a.showDizzy)
    {
        drawDizzyMark(lCX, lCY, a.dizzyAngle);
        drawDizzyMark(rCX, rCY, a.dizzyAngle + 2);
    }
    if (a.showStars)
    {
        drawStar(lCX - 2, lCY - lH / 2 - 9);
        drawStar(rCX + 2, rCY - rH / 2 - 9);
        drawStar(64, EYE_CY - 16 + faceY);
    }
    if (a.showSweat)
    {
        drawSweatDrop(lCX - lW / 2 - 2, lCY - lH / 2);
    }
    // ─── Music notes ──────────────────────────
    if (a.showMusic)
    {
        // Two notes that float upward on opposite sides, staggered
        int n1y = (int)(a.note1Y);
        int n2y = (int)(a.note2Y);
        drawMusicNote(lCX - lW / 2 - 8, lCY - lH / 2 - n1y, 0); // left ♪
        drawMusicNote(rCX + rW / 2 + 3, rCY - rH / 2 - n2y, 1); // right ♩
    }
    // ─── Drool ────────────────────────────────
    if (a.showDrool)
    {
        int mY = MOUTH_CY + faceY + (int)a.mouthOffsetY + 5;
        drawDrool(MOUTH_CX + shakeX, mY, a.droolLen);
    }
    // ─── Hunger rumble ────────────────────────
    if (a.showRumble)
    {
        drawRumbleLines(MOUTH_CX, MOUTH_CY + faceY + 8, a.rumblePhase);
    }
}

// ─────────────────────────────────────────────
//  Expression initializers
//  Each function resets AnimState for the expression
//  and sets phase=0, phaseSpeed for phase 0
// ─────────────────────────────────────────────

static void resetAnim(AnimState &a)
{
    a = AnimState();
    a.eyeW = 20;
    a.eyeH = 20;
    a.eyeRadius = 4;
    a.showMouth = true;
}

static void initIdle(AnimState &a)
{
    resetAnim(a);
    a.mouthSmile = 0.1f;
}
static void initBlink(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.12f;
    a.mouthSmile = 0.15f;
}
static void initHappy(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.05f;
    a.mouthSmile = 0.8f;
}
static void initSad(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.03f;
    a.mouthSmile = -0.7f;
    a.showTear = true;
}
static void initAngry(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.05f;
    a.showBrows = true;
    a.leftBrowAngle = 4; // inner corner tilts down
    a.rightBrowAngle = -4;
    a.mouthSmile = -0.3f;
}
static void initSurprised(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.08f;
    a.mouthOpen = 0.7f;
}
static void initTired(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.025f;
    a.eyeScaleY = 0.5f;
    a.mouthSmile = 0.0f;
    a.mouthOpen = 0.3f;
}
static void initCurious(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.04f;
    a.rightEyeScale = 1.2f;
    a.leftEyeScale = 0.85f;
    a.rightEyeDY = -3.0f;
    a.showBrows = true;
    a.rightBrowAngle = -2;
    a.mouthSmile = 0.2f;
}
static void initConfused(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.035f;
    a.leftEyeDY = 3.0f;
    a.rightEyeDY = -3.0f;
    a.showBrows = true;
    a.leftBrowAngle = -3;
    a.rightBrowAngle = 3;
    a.mouthSmile = 0.0f;
    a.mouthOpen = 0.15f;
}
static void initLaughing(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.07f;
    a.eyeScaleY = 0.45f;
    a.mouthSmile = 1.0f;
    a.mouthOpen = 0.5f;
}
static void initLove(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.04f;
    a.showHeart = true;
    a.heartScale = 1.0f;
    a.mouthSmile = 0.9f;
    a.eyeScaleY = 0.8f;
}
static void initWink(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.09f;
    a.winkLeft = true;
    a.winkClose = 0.0f;
    a.mouthSmile = 0.7f;
}
static void initScared(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.06f;
    a.eyeScaleX = 1.3f;
    a.eyeScaleY = 1.3f;
    a.eyeRadius = 2;
    a.mouthOpen = 0.6f;
    a.mouthSmile = -0.2f;
    a.showSweat = true;
}
static void initSuspicious(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.03f;
    a.eyeScaleY = 0.55f;
    a.eyeOffsetX = -5.0f;
    a.showBrows = true;
    a.leftBrowAngle = 2;
    a.rightBrowAngle = -2;
    a.mouthSmile = -0.15f;
}
static void initDizzy(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.04f;
    a.showDizzy = true;
    a.showStars = true;
    a.mouthOpen = 0.3f;
    a.dizzyAngle = 0;
}
static void initSleep(AnimState &a)
{
    resetAnim(a);
    a.phase = 0;
    a.phaseSpeed = 0.02f;
    a.eyeScaleY = 0.15f;
    a.mouthSmile = 0.0f;
    a.showZzz = true;
    a.zzzY = 0.0f;
    a.zzzPhase = 0;
}

// ─────────────────────────────────────────────
//  Expression animators
//  Called every frame while the expression is active.
//  anim.t advances from 0→1 for the current phase,
//  then phase increments.
// ─────────────────────────────────────────────
static unsigned long exprStartMs = 0;

static void advancePhase(AnimState &a)
{
    a.phase++;
    a.t = 0.0f;
}

// ── IDLE ─────────────────────────────────────
static void updateIdle(AnimState &a, float ms)
{
    // Handled mostly by the idle sub-loop below
    // Small breath oscillation
    a.breathe = osc(ms, 3000) * 1.2f;
    a.eyeScaleX = 1.0f;
    a.eyeScaleY = 1.0f + osc(ms, 3000) * 0.04f;
    a.mouthSmile = 0.15f + osc(ms, 4000) * 0.05f;
    // Gaze drift
    a.eyeOffsetX = idleGazeX * 4.0f;
}

// ── BLINK ────────────────────────────────────
static void updateBlink(AnimState &a, float ms)
{
    // Phases: 0=close 1=hold 2=open
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    float close = 0.0f;
    switch (a.phase)
    {
    case 0:
        close = easeIn(a.t);
        a.phaseSpeed = 0.14f;
        break;
    case 1:
        close = 1.0f;
        a.phaseSpeed = 0.5f;
        break;
    case 2:
        close = easeOut(1.0f - a.t);
        a.phaseSpeed = 0.10f;
        break;
    default:
        a.eyeScaleY = 1.0f;
        return;
    }
    a.eyeScaleY = 1.0f - 0.92f * close;
}

// ── HAPPY ────────────────────────────────────
static void updateHappy(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes appear and grow
        a.eyeScaleX = lerpF(0.3f, 1.0f, easeOut(a.t));
        a.eyeScaleY = lerpF(0.3f, 1.0f, easeOut(a.t));
        a.mouthSmile = lerpF(0.0f, 0.8f, a.t);
        a.phaseSpeed = 0.06f;
        break;
    case 1: // Bounce up
        a.eyeOffsetY = lerpF(0.0f, -5.0f, easeOut(a.t));
        a.bounce = lerpF(0.0f, 1.0f, easeOut(a.t));
        a.phaseSpeed = 0.07f;
        break;
    case 2: // Squash
        a.eyeScaleY = lerpF(1.0f, 0.7f, a.t);
        a.eyeOffsetY = lerpF(-5.0f, 0.0f, a.t);
        a.bounce = lerpF(1.0f, 0.0f, a.t);
        a.phaseSpeed = 0.08f;
        break;
    case 3: // Hold and gentle oscillation
        a.eyeScaleY = 1.0f + osc(ms, 1200) * 0.06f;
        a.mouthSmile = 0.9f + osc(ms, 1500) * 0.07f;
        a.bounce = osc(ms, 800) * 0.15f;
        a.phaseSpeed = 0.0f; // hold
        break;
    }
}

// ── SAD ──────────────────────────────────────
static void updateSad(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes droop downward, shrink
        a.eyeOffsetY = lerpF(0.0f, 4.0f, easeIn(a.t));
        a.eyeScaleX = lerpF(1.0f, 0.85f, a.t);
        a.eyeScaleY = lerpF(1.0f, 0.75f, a.t);
        a.mouthSmile = lerpF(0.0f, -0.7f, a.t);
        a.phaseSpeed = 0.03f;
        break;
    case 1: // Small downward slump
        a.faceOffsetY = lerpF(0.0f, 3.0f, easeIn(a.t));
        a.showBrows = true;
        a.leftBrowAngle = -2;
        a.rightBrowAngle = 2;
        a.phaseSpeed = 0.025f;
        break;
    case 2: // Hold sad, tiny shake of tear
        a.showTear = true;
        a.faceOffsetY = 3.0f + osc(ms, 3000) * 0.3f;
        a.eyeScaleY = 0.75f + osc(ms, 4000) * 0.02f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── ANGRY ────────────────────────────────────
static void updateAngry(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes shrink and brows tilt
        a.eyeScaleX = lerpF(1.0f, 0.8f, a.t);
        a.eyeScaleY = lerpF(1.0f, 0.7f, a.t);
        a.showBrows = true;
        a.leftBrowAngle = (int)lerpF(0, 5, a.t);
        a.rightBrowAngle = (int)lerpF(0, -5, a.t);
        a.browOffsetY = lerpF(-3, -1, a.t);
        a.mouthSmile = lerpF(0, -0.4f, a.t);
        a.phaseSpeed = 0.05f;
        break;
    case 1: // Shake
        a.shake = 1.0f;
        a.phaseSpeed = 0.018f;
        break;
    case 2: // Hold angry
        a.shake = 0.5f + osc(ms, 200) * 0.3f;
        a.eyeScaleX = 0.8f;
        a.eyeScaleY = 0.7f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── SURPRISED ───────────────────────────────
static void updateSurprised(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes burst open
        a.eyeScaleX = lerpF(1.0f, 1.5f, easeOut(a.t));
        a.eyeScaleY = lerpF(1.0f, 1.5f, easeOut(a.t));
        a.mouthOpen = lerpF(0.0f, 0.8f, a.t);
        a.eyeRadius = 2;
        a.phaseSpeed = 0.09f;
        break;
    case 1: // Bounce upward
        a.bounce = lerpF(0, 1, easeOut(a.t));
        a.eyeOffsetY = lerpF(0, -4, a.t);
        a.phaseSpeed = 0.10f;
        break;
    case 2: // Hold
        a.eyeScaleX = 1.45f + osc(ms, 600) * 0.05f;
        a.eyeScaleY = 1.45f + osc(ms, 600) * 0.05f;
        a.bounce = osc(ms, 500) * 0.15f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── TIRED ───────────────────────────────────
static void updateTired(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes slowly droop
        a.eyeScaleY = lerpF(1.0f, 0.45f, easeIn(a.t));
        a.mouthOpen = lerpF(0.0f, 0.35f, a.t);
        a.phaseSpeed = 0.025f;
        break;
    case 1: // Gentle breathing
        a.eyeScaleY = 0.45f + osc(ms, 3500) * 0.05f;
        a.breathe = osc(ms, 3500) * 1.5f;
        a.mouthOpen = 0.3f + osc(ms, 3500) * 0.08f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── CURIOUS ─────────────────────────────────
static void updateCurious(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Tilt — one eye rises
        a.rightEyeScale = lerpF(1.0f, 1.2f, easeOut(a.t));
        a.leftEyeScale = lerpF(1.0f, 0.85f, a.t);
        a.rightEyeDY = lerpF(0.0f, -3.0f, a.t);
        a.eyeOffsetX = lerpF(0.0f, 4.0f, a.t);
        a.showBrows = true;
        a.rightBrowAngle = -2;
        a.mouthSmile = lerpF(0.0f, 0.25f, a.t);
        a.phaseSpeed = 0.04f;
        break;
    case 1: // Gentle curious bob
        a.eyeOffsetY = osc(ms, 1600) * 1.5f;
        a.eyeOffsetX = 4.0f + osc(ms, 2000) * 1.0f;
        a.rightEyeScale = 1.2f + osc(ms, 1600) * 0.04f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── CONFUSED ────────────────────────────────
static void updateConfused(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0:
        a.leftEyeDY = lerpF(0.0f, 4.0f, a.t);
        a.rightEyeDY = lerpF(0.0f, -4.0f, a.t);
        a.showBrows = true;
        a.leftBrowAngle = (int)lerpF(0, -3, a.t);
        a.rightBrowAngle = (int)lerpF(0, 3, a.t);
        a.mouthOpen = lerpF(0.0f, 0.2f, a.t);
        a.phaseSpeed = 0.035f;
        break;
    case 1: // Wobble
        a.leftEyeDY = 4.0f + osc(ms, 900) * 1.5f;
        a.rightEyeDY = -4.0f + osc(ms, 900) * 1.5f;
        a.shake = osc(ms, 700) * 0.4f;
        a.mouthOpen = 0.15f + osc(ms, 900) * 0.07f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── LAUGHING ────────────────────────────────
static void updateLaughing(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Squish eyes, open mouth
        a.eyeScaleY = lerpF(1.0f, 0.35f, easeIn(a.t));
        a.mouthSmile = lerpF(0.0f, 1.0f, a.t);
        a.mouthOpen = lerpF(0.0f, 0.55f, a.t);
        a.phaseSpeed = 0.07f;
        break;
    case 1: // Repeat bounce
        a.bounce = fabsf(osc(ms, 350)) * 0.8f;
        a.eyeScaleY = 0.35f + fabsf(osc(ms, 350)) * 0.08f;
        a.mouthSmile = 1.0f;
        a.mouthOpen = 0.5f + fabsf(osc(ms, 350)) * 0.1f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── LOVE ────────────────────────────────────
static void updateLove(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Hearts appear
        a.heartScale = lerpF(0.0f, 1.0f, easeOut(a.t));
        a.eyeScaleY = lerpF(1.0f, 0.75f, a.t);
        a.mouthSmile = lerpF(0.0f, 0.9f, a.t);
        a.phaseSpeed = 0.04f;
        break;
    case 1: // Gentle pulse
        a.heartScale = 1.0f + osc(ms, 800) * 0.15f;
        a.bounce = osc(ms, 1200) * 0.2f;
        a.eyeScaleY = 0.75f + osc(ms, 900) * 0.04f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── WINK ────────────────────────────────────
static void updateWink(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Close left eye
        a.winkClose = easeIn(a.t);
        a.phaseSpeed = 0.10f;
        break;
    case 1: // Hold wink
        a.winkClose = 1.0f;
        a.phaseSpeed = 0.20f;
        break;
    case 2: // Open left eye + bounce
        a.winkClose = 1.0f - easeOut(a.t);
        a.bounce = osc(ms, 400) * 0.4f;
        a.phaseSpeed = 0.09f;
        break;
    case 3: // Settle
        a.winkClose = 0.0f;
        a.bounce = osc(ms, 800) * 0.1f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── SCARED ──────────────────────────────────
static void updateScared(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes burst wide
        a.eyeScaleX = lerpF(1.0f, 1.35f, easeOut(a.t));
        a.eyeScaleY = lerpF(1.0f, 1.35f, easeOut(a.t));
        a.mouthOpen = lerpF(0.0f, 0.65f, a.t);
        a.eyeRadius = 2;
        a.phaseSpeed = 0.08f;
        break;
    case 1: // Tremble
        a.shake = 0.7f;
        a.eyeOffsetY = osc(ms, 150) * 0.8f;
        a.eyeScaleX = 1.35f + osc(ms, 120) * 0.05f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── SUSPICIOUS ──────────────────────────────
static void updateSuspicious(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    switch (a.phase)
    {
    case 0: // Eyes narrow, look sideways
        a.eyeScaleY = lerpF(1.0f, 0.5f, easeIn(a.t));
        a.eyeOffsetX = lerpF(0.0f, -5.0f, a.t);
        a.showBrows = true;
        a.leftBrowAngle = 2;
        a.rightBrowAngle = -2;
        a.mouthSmile = lerpF(0.0f, -0.2f, a.t);
        a.phaseSpeed = 0.03f;
        break;
    case 1: // Hold with slight gaze shift
        a.eyeOffsetX = -5.0f + osc(ms, 2500) * 1.5f;
        a.eyeScaleY = 0.5f + osc(ms, 3000) * 0.03f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── DIZZY ───────────────────────────────────
static void updateDizzy(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    a.dizzyAngle = (int)(ms / 80.0f) % 8;

    switch (a.phase)
    {
    case 0: // Spin-in
        a.eyeScaleX = lerpF(0.5f, 1.0f, a.t);
        a.eyeScaleY = lerpF(0.5f, 1.0f, a.t);
        a.mouthOpen = lerpF(0.0f, 0.35f, a.t);
        a.phaseSpeed = 0.04f;
        break;
    case 1: // Spin and sway
        a.eyeOffsetX = osc(ms, 700) * 4.0f;
        a.eyeOffsetY = osc(ms, 500) * 2.5f;
        a.shake = 0.5f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ── SLEEP ───────────────────────────────────
static void updateSleep(AnimState &a, float ms)
{
    a.t += a.phaseSpeed;
    if (a.t >= 1.0f)
    {
        a.t = 0.0f;
        advancePhase(a);
    }

    // Z advancement every ~2 seconds
    a.zzzPhase = (int)(ms / 2000.0f) % 3;
    a.zzzY = fmodf(ms / 80.0f, 14.0f); // float upward

    switch (a.phase)
    {
    case 0: // Eyes close
        a.eyeScaleY = lerpF(1.0f, 0.12f, easeIn(a.t));
        a.phaseSpeed = 0.025f;
        break;
    case 1: // Breathing sleep
        a.breathe = osc(ms, 4000) * 2.0f;
        a.eyeScaleY = 0.12f + osc(ms, 4000) * 0.02f;
        a.faceOffsetY = osc(ms, 4000) * 0.8f;
        a.phaseSpeed = 0.0f;
        break;
    }
}

// ─────────────────────────────────────────────
//  Dispatch updater
// ─────────────────────────────────────────────

static void updateExpression(Expression e, AnimState &a, float ms)
{
    switch (e)
    {
    case IDLE:
        updateIdle(a, ms);
        break;
    case BLINK:
        updateBlink(a, ms);
        break;
    case HAPPY:
        updateHappy(a, ms);
        break;
    case SAD:
        updateSad(a, ms);
        break;
    case ANGRY:
        updateAngry(a, ms);
        break;
    case SURPRISED:
        updateSurprised(a, ms);
        break;
    case TIRED:
        updateTired(a, ms);
        break;
    case CURIOUS:
        updateCurious(a, ms);
        break;
    case CONFUSED:
        updateConfused(a, ms);
        break;
    case LAUGHING:
        updateLaughing(a, ms);
        break;
    case LOVE:
        updateLove(a, ms);
        break;
    case WINK:
        updateWink(a, ms);
        break;
    case SCARED:
        updateScared(a, ms);
        break;
    case SUSPICIOUS:
        updateSuspicious(a, ms);
        break;
    case DIZZY:
        updateDizzy(a, ms);
        break;
    case SLEEP:
        updateSleep(a, ms);
        break;
    default:
        break;
    }
}

static void initExpression(Expression e, AnimState &a)
{
    switch (e)
    {
    case IDLE:
        initIdle(a);
        break;
    case BLINK:
        initBlink(a);
        break;
    case HAPPY:
        initHappy(a);
        break;
    case SAD:
        initSad(a);
        break;
    case ANGRY:
        initAngry(a);
        break;
    case SURPRISED:
        initSurprised(a);
        break;
    case TIRED:
        initTired(a);
        break;
    case CURIOUS:
        initCurious(a);
        break;
    case CONFUSED:
        initConfused(a);
        break;
    case LAUGHING:
        initLaughing(a);
        break;
    case LOVE:
        initLove(a);
        break;
    case WINK:
        initWink(a);
        break;
    case SCARED:
        initScared(a);
        break;
    case SUSPICIOUS:
        initSuspicious(a);
        break;
    case DIZZY:
        initDizzy(a);
        break;
    case SLEEP:
        initSleep(a);
        break;
    default:
        break;
    }
}

// ─────────────────────────────────────────────
//  Transition animation state
// ─────────────────────────────────────────────
static AnimState transAnim; // blended state during transition

static void beginTransition(Expression to)
{
    nextExpr = to;
    transitioning = true;
    transitionT = 0.0f;
}

// ─────────────────────────────────────────────
//  Idle sub-state machine
// ─────────────────────────────────────────────
static void scheduleIdleBlink(unsigned long now)
{
    idleNextBlink = now + random(2500, 6000);
}
static void scheduleIdleLook(unsigned long now)
{
    idleNextLook = now + random(3000, 8000);
}

static void nextScreen()
{
    int next = ((int)currentScreen + 1) % SCREEN_COUNT;
    currentScreen = (Screen)next;

    Serial.print("Screen changed to: ");
    Serial.println((int)currentScreen);
}

static void changeExpression()
{
    int next = ((int)currentExpr + 1) % (int)EXPR_COUNT;

    beginTransition((Expression)next);

    Serial.print("Expression changed to: ");
    Serial.println((int)next);
}

static void handleTouch(unsigned long now)
{
    bool raw = digitalRead(TOUCH_PIN);

    // ─────────────────────────────────────────
    // TOUCH START
    // ─────────────────────────────────────────
    if (raw && !lastTouchVal)
    {
        if ((now - lastTouchMs) > TOUCH_DEBOUNCE)
        {
            touchActive = true;
            touchStartMs = now;

            Serial.println("Touch DOWN");
        }
    }

    // ─────────────────────────────────────────
    // TOUCH RELEASE
    // ─────────────────────────────────────────
    if (!raw && lastTouchVal && touchActive)
    {
        unsigned long pressDuration = now - touchStartMs;

        touchActive = false;
        lastTouchMs = now;

        Serial.print("Press duration: ");
        Serial.println(pressDuration);

        // ─────────────────────────────────────
        // LONG PRESS
        // ─────────────────────────────────────
        if (pressDuration >= LONG_PRESS_MS)
        {
            nextScreen();
        }

        // ─────────────────────────────────────
        // SHORT / SINGLE TAP
        // ─────────────────────────────────────
        else if (pressDuration >= TOUCH_RELEASE_DEBOUNCE)
        {
            changeExpression();
        }
    }

    lastTouchVal = raw;
}

static void renderScreen(Screen screen)
{
    switch (screen)
    {
    case SCREEN_IDLE:
        renderFace(anim, millis());
        break;

    case SCREEN_WEATHER:
        weatherUI();
        break;

    case SCREEN_CLOCK:
        watchUI();
        break;
        
    case SCREEN_UPDATE:
            otaUpdateScreen(40);
        break;



    default:
        break;
    }
}
