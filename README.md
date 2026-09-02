# ESP32 OLED + Touch Project

A simple ESP32-based project using a **128×64 I2C OLED display** and a **TTP223 touch sensor**.

The project supports two ESP32 configurations:

* ESP32-S3 SuperMini
* ESP32 DevKit / ESP32-WROOM

---

## Hardware

### Required

| Component          | Description             |
| ------------------ | ----------------------- |
| ESP32-S3 SuperMini | Main controller         |
| ESP32 DevKit       | Alternative controller  |
| 128×64 OLED        | SSD1309, I2C            |
| TTP223             | Capacitive touch sensor |
| Jumper wires       | Connections             |

OLED I2C address:

```text
0x3C
```

---

# ESP32-S3 SuperMini

## Pin Configuration

The S3 SuperMini uses:

```text
OLED SDA  → GPIO 13
OLED SCL  → GPIO 12
OLED VCC  → 3.3V
OLED GND  → GND

Touch OUT → GPIO 11
Touch VCC → 3.3V
Touch GND → GND
```

## Connection Diagram

```text
        ESP32-S3 SuperMini
       ┌───────────────────┐
       │                   │
3.3V ──┤ 3V3           GND ├──── OLED GND
       │                   │
GPIO13 ┤ SDA               │
       │                   │
GPIO12 ┤ SCL               │
       │                   │
GPIO11 ┤ Touch             │
       │                   │
       └───────────────────┘
          │       │
          │       │
          ▼       ▼

       ┌─────────────┐
       │ OLED 128x64 │
       │   SSD1309   │
       ├─────────────┤
       │ VCC → 3.3V │
       │ GND → GND  │
       │ SDA → GPIO13│
       │ SCL → GPIO12│
       └─────────────┘


       ┌─────────────┐
       │    TTP223   │
       ├─────────────┤
       │ VCC → 3.3V │
       │ GND → GND  │
       │ OUT → GPIO11│
       └─────────────┘
```

### Quick Wiring Table

| OLED / Touch | ESP32-S3 |
| ------------ | -------: |
| OLED VCC     |     3.3V |
| OLED GND     |      GND |
| OLED SDA     |  GPIO 13 |
| OLED SCL     |  GPIO 12 |
| TTP223 VCC   |     3.3V |
| TTP223 GND   |      GND |
| TTP223 OUT   |  GPIO 11 |

---

# ESP32 DevKit

## Pin Configuration

The ESP32 DevKit uses the standard I2C pins:

```text
OLED SDA  → GPIO 21
OLED SCL  → GPIO 22
OLED VCC  → 3.3V
OLED GND  → GND

Touch OUT → GPIO 4
Touch VCC → 3.3V
Touch GND → GND
```

## Connection Diagram

```text
          ESP32 DevKit
       ┌───────────────────┐
       │                   │
3.3V ──┤ 3V3           GND ├──── OLED GND
       │                   │
GPIO21 ┤ SDA               │
       │                   │
GPIO22 ┤ SCL               │
       │                   │
GPIO4  ┤ Touch             │
       │                   │
       └───────────────────┘
          │       │
          │       │
          ▼       ▼

       ┌─────────────┐
       │ OLED 128x64 │
       │   SSD1309   │
       ├─────────────┤
       │ VCC → 3.3V │
       │ GND → GND  │
       │ SDA → GPIO21│
       │ SCL → GPIO22│
       └─────────────┘


       ┌─────────────┐
       │    TTP223   │
       ├─────────────┤
       │ VCC → 3.3V │
       │ GND → GND  │
       │ OUT → GPIO4 │
       └─────────────┘
```

### Quick Wiring Table

| OLED / Touch | ESP32 DevKit |
| ------------ | -----------: |
| OLED VCC     |         3.3V |
| OLED GND     |          GND |
| OLED SDA     |      GPIO 21 |
| OLED SCL     |      GPIO 22 |
| TTP223 VCC   |         3.3V |
| TTP223 GND   |          GND |
| TTP223 OUT   |       GPIO 4 |

---

# Important

### OLED

This project is configured for:

```text
Resolution: 128 × 64
Driver: SSD1309
Interface: I2C
Address: 0x3C
```

If your OLED has a different I2C address, commonly `0x3D`, change:

```text
-D OLED_I2C_ADDR=0x3C
```

to:

```text
-D OLED_I2C_ADDR=0x3D
```

### TTP223

The TTP223 is a digital touch sensor.

When touched, the `OUT` pin normally becomes:

```text
HIGH
```

When not touched:

```text
LOW
```

---

# PlatformIO Configuration

## ESP32-S3 SuperMini

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

build_type = debug

board_build.arduino.memory_type = qio_qspi
board_build.flash_mode = qio
board_build.psram_type = qio

board_upload.flash_size = 4MB
board_upload.maximum_size = 4194304

board_build.partitions = default.csv

build_flags =
    -I include
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
    -D OLED_WIDTH=128
    -D OLED_HEIGHT=64
    -D OLED_SDA=13
    -D OLED_SCL=12
    -D OLED_RST=-1
    -D OLED_DRIVER_SSD1309
    -D CORE_DEBUG_LEVEL=0
    -D TOUCH_PIN=11
    -D OLED_I2C_ADDR=0x3C

board_build.filesystem = littlefs

monitor_speed = 115200
monitor_filters = esp32_exception_decoder

lib_deps =
    moononournation/GFX Library for Arduino@^1.6.7
    olikraus/U8g2
    adafruit/Adafruit SSD1306@^2.5.17
    arduino-libraries/Arduino_JSON@^0.2.2
    bblanchon/ArduinoJson@^7.2.2
```

---

## ESP32 DevKit

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32dev
framework = arduino

monitor_speed = 115200

lib_deps =
    moononournation/GFX Library for Arduino@^1.6.7
    olikraus/U8g2
    adafruit/Adafruit SSD1306@^2.5.17
    arduino-libraries/Arduino_JSON@^0.2.2
    bblanchon/ArduinoJson@^7.2.2

build_flags =
    -D OLED_WIDTH=128
    -D OLED_HEIGHT=64
    -D OLED_SDA=21
    -D OLED_SCL=22
    -D OLED_RST=-1
    -D OLED_DRIVER_SSD1309
    -DCORE_DEBUG_LEVEL=0
    -O2
    -D TOUCH_PIN=4
    -D OLED_I2C_ADDR=0x3C
```

---

# Pin Summary

| Function        | ESP32-S3 SuperMini | ESP32 DevKit |
| --------------- | -----------------: | -----------: |
| OLED SDA        |            GPIO 13 |      GPIO 21 |
| OLED SCL        |            GPIO 12 |      GPIO 22 |
| OLED VCC        |               3.3V |         3.3V |
| OLED GND        |                GND |          GND |
| Touch OUT       |            GPIO 11 |       GPIO 4 |
| Touch VCC       |               3.3V |         3.3V |
| Touch GND       |                GND |          GND |
| OLED Address    |             `0x3C` |       `0x3C` |
| OLED Resolution |             128×64 |       128×64 |
| OLED Driver     |            SSD1309 |      SSD1309 |

---

# Project Structure

Recommended structure:

```text
project/
│
├── platformio.ini
│
├── include/
│   └── ...
│
├── src/
│   └── main.cpp
│
├── lib/
│   └── ...
│
└── data/
    └── ...
```

---

# Upload

Connect the ESP32 to the computer using USB.

Then run:

```bash
pio run
```

Upload:

```bash
pio run -t upload
```

Open Serial Monitor:

```bash
pio device monitor
```

The configured baud rate is:

```text
115200
```

For the ESP32-S3, USB CDC is enabled with:

```text
-DARDUINO_USB_CDC_ON_BOOT=1
```

---

# Troubleshooting

## OLED shows nothing

Check:

1. OLED VCC → 3.3V
2. OLED GND → GND
3. SDA is connected to the correct GPIO
4. SCL is connected to the correct GPIO
5. OLED address is `0x3C`
6. OLED controller is actually SSD1309

For S3:

```text
SDA = GPIO13
SCL = GPIO12
```

For ESP32 DevKit:

```text
SDA = GPIO21
SCL = GPIO22
```

---

## Touch sensor does not work

Check:

```text
TTP223 VCC → 3.3V
TTP223 GND → GND
TTP223 OUT → configured TOUCH_PIN
```

S3:

```text
TOUCH_PIN = 11
```

ESP32:

```text
TOUCH_PIN = 4
```

---

# License

Use, modify, and build upon this project for your own embedded projects.
