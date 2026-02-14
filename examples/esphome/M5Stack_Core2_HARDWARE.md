# M5Stack Core2 Hardware Reference (for ESPHome example)

Этот документ фиксирует аппаратную базу **M5Stack Core2 (SKU: K010)** для дальнейшего развития `examples/esphome`.

## 1) Целевая ревизия

- Целевая плата: **Core2 (K010)**, классическая ревизия с **AXP192**.
- Важно: **Core2 v1.1 (K010-V11)** отличается по power subsystem (AXP2101 + INA3221) и ряду деталей. При переносе конфига между ревизиями нужна отдельная проверка.

## 2) Основные микросхемы и узлы

| Узел | Компонент |
|---|---|
| MCU | ESP32-D0WDQ6-V3 (2x Xtensa LX6, до 240 МГц) |
| Flash | 16 MB |
| PSRAM | 8 MB |
| LCD | ILI9342C, 2.0", 320x240 |
| Touch | FT6336U (I2C, addr `0x38`) |
| PMU | AXP192 (I2C, addr `0x34`) |
| RTC | BM8563 (I2C, addr `0x51`) |
| IMU | MPU6886 (I2C, addr `0x68`) |
| Audio Amp | NS4168 (I2S) |
| Mic | SPM1423 (PDM) |
| USB-UART | CP2104 или CH9102F |
| DC-DC | SY7088 |
| microSD | до 16 GB |
| Battery | LiPo 500 mAh @ 3.7 V |

## 3) Внутренние шины и адреса

### 3.1 I2C (внутренняя)

- `SDA = GPIO21`
- `SCL = GPIO22`

Устройства на шине:
- AXP192 (`0x34`)
- FT6336U (`0x38`)
- BM8563 (`0x51`)
- MPU6886 (`0x68`)

### 3.2 SPI (LCD + microSD)

- `SCK = GPIO18`
- `MOSI = GPIO23`
- `MISO = GPIO38`

Разделение по CS:
- LCD CS: `GPIO5`
- LCD DC: `GPIO15`
- microSD CS: `GPIO4`

### 3.3 I2S / аудио

По pinmap Core2:
- `BCLK = GPIO12`
- `LRCK = GPIO0`
- `DATA (to NS4168) = GPIO2`
- `Mic DATA = GPIO34`

Примечание: линии аудио и питание/enable узлов завязаны на PMU (AXP192).

## 4) Связи PMU (AXP192)

По таблицам pinmap:
- LCD:
  - `AXP_IO4 -> LCD RST`
  - `AXP_DC3 -> LCD BL` (подсветка)
  - `AXP_LDO2 -> LCD PWR`
- Audio amp:
  - `AXP_IO2 -> NS4168 SPK_EN`
- Индикатор и вибромотор:
  - `AXP_IO1 -> Green LED`
  - `AXP_LDO3 -> Vibration Motor`
- RTC:
  - `AXP_PWR -> BM8563 INT`

## 5) Разъемы и внешние пины

### 5.1 HY2.0-4P порты (на Core2)

| Port | Black | Red | Yellow | White |
|---|---|---|---|---|
| PORT.A | GND | 5V | GPIO32 | GPIO33 |
| PORT.B | GND | 5V | GPIO26 | GPIO36 |
| PORT.C | GND | 5V | GPIO14 | GPIO13 |

### 5.2 M5-Bus (основные полезные линии)

| Функция | GPIO |
|---|---|
| SPI MOSI | GPIO23 |
| SPI MISO | GPIO38 |
| SPI SCK | GPIO18 |
| UART0 TX/RX | GPIO1 / GPIO3 |
| UART2 TX/RX | GPIO14 / GPIO13 |
| I2C SDA/SCL | GPIO21 / GPIO22 |
| PORT.A SDA/SCL | GPIO32 / GPIO33 |
| I2S DOUT/LRCK/DATA | GPIO2 / GPIO0 / GPIO34 |
| Доп. GPIO | GPIO27, GPIO19 |
| ADC | GPIO35, GPIO36 |
| DAC | GPIO25, GPIO26 |
| Питание | 5V, 3V3, BAT, GND |

## 6) Что уже важно для текущего ESPHome примера

Текущий `core2-openrouter.yaml` опирается на:
- I2C: `GPIO21/22` (AXP192, BM8563, FT6336U, MPU6886)
- SPI display: `GPIO18/23`, `CS=GPIO5`, `DC=GPIO15`
- Touch IRQ: `GPIO39`
- (опционально) audio bus: `GPIO12/0/2`

## 7) Ограничения и практические заметки

- `GPIO5` и `GPIO15` являются strap pins (ESP32): использовать аккуратно при обвязке.
- Для stacked-конфигураций есть механическая несовместимость с некоторыми Base-модулями из-за вибромотора (см. официальные notes).
- Для Core2 v1.1 требуется отдельная проверка power-control логики (AXP2101 вместо AXP192).

## 8) Ссылки (первоисточники)

- Core2 (K010): https://docs.m5stack.com/en/core/core2
- Core2 v1.1 (для отличий): https://docs.m5stack.switch-science.com/en/core/Core2%20v1.1
- Core2 Core Section Schematics PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/products/Core/Core2/Sch/M5-Core2%20Schematic.pdf
- Core2 Expansion Board Schematics PDF: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/products/Core/Core2/Sch/M5-Core2%20Bottom%20Schematic.pdf
