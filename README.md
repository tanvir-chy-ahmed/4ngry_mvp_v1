# 4ngry — v1 Feature Roadmap

**Project:** 4ngry Desk Robot  
**Developer/brand:** MTR  
**Repository:** https://github.com/tanvir-chy-ahmed/4ngry_mvp_v1  
**Version:** v1

## 1. Hardware
- ESP32-based controller; supported configurations include ESP32-S3 SuperMini and ESP32 DevKit/WROOM.
- 128×64 I²C OLED display (configured for SSD1309, address `0x3C`).
- TTP223 capacitive touch input.
- Optional buzzer and battery-powered operation, depending on the assembled hardware revision.
- PlatformIO build and USB firmware upload.

## 2. Startup and Connectivity
- Boot screen while the device initializes.
- Wi-Fi station (STA) mode for connecting to a saved network.
- Setup access-point (AP) mode when credentials are missing or setup is requested.
- Local web configuration from a phone or computer connected to the device AP.
- Show connection/setup status on the OLED and handle connection failures without leaving the UI unresponsive.

## 3. OLED Interface
- Compact 128×64 monochrome interface.
- Touch-driven navigation and interaction.
- Main display modes:
  - **Companion:** expressive/cute face animations.
  - **Clock & weather:** time, date/day, location, and weather information when configured and online.
  - **Timer:** timer setup and countdown.
- Clear boot, Wi-Fi, loading, and error states.

## 4. Local Web Dashboard
The web interface is intended to provide device configuration and companion features, including:
- Wi-Fi setup and device settings.
- OLED brightness and sound/beep controls.
- Alarm selection and reminders.
- Relationship counter and birthday reminders.
- Quote/text display and simple pixel drawing.
- Image-to-monochrome/dither canvas for the 128×64 display.
- Pixel-pet and GIF-style display content.
- Hardware testing and device restart controls.

> Dashboard items should be marked complete only when confirmed in the current firmware; some may be optional or still in development.

## 5. Firmware Maintenance
- Firmware version identification.
- OTA update page/workflow where enabled.
- Check for published firmware updates when network access is available.
- Restart after update or configuration changes.
- PlatformIO build configuration and serial logging for debugging.

## 6. v1 Acceptance Checklist
- [ ] Device boots reliably and initializes the OLED.
- [ ] Touch input is detected consistently.
- [ ] Saved Wi-Fi credentials connect successfully.
- [ ] Missing/invalid credentials allow recovery through AP setup.
- [ ] Web interface is reachable from a connected client.
- [ ] Main OLED screens work without blocking the device loop.
- [ ] Network/API failures produce a recoverable status instead of a frozen display.
- [ ] OTA/update flow is tested before release.
- [ ] Hardware pin assignments and supported board configurations are documented.

## 7. Scope
This document summarizes the intended v1 feature set. It is not a guarantee that every dashboard item is implemented in the current source. Verify each item against the firmware before treating it as complete.

**Current priority:** stabilize core hardware, OLED rendering, touch interaction, Wi-Fi recovery, and the local web interface before expanding optional companion features.
