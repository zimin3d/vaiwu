# Hardware

## Плата

**Freenove ESP32-S3 2.8" Capacitive Touch Display**
(fnk0104, профиль `freenove-esp32s3-display-2.8-lcd` в xiaozhi-esp32):

- ESP32-S3 (Xtensa LX7, 240 МГц, 2 ядра), 16 МБ flash, 8 МБ octal PSRAM
- Дисплей ILI9341, 320x240 ландшафт (SPI, 20 МГц)
- Ёмкостный тач FT6336 (I2C)
- Аудиокодек ES8311 (I2S): динамик + микрофон
- Wi-Fi

## Распиновка (см. `firmware/main/main.cpp`)

| Функция | GPIO |
|---|---|
| Дисплей SPI MOSI | 11 |
| Дисплей SPI MISO | 13 |
| Дисплей SPI SCLK | 12 |
| Дисплей CS | 10 |
| Дисплей DC | 46 |
| Подсветка (LEDC PWM) | 45 |
| I2C SDA (тач + управление кодеком) | 16 |
| I2C SCL | 15 |
| I2S MCLK | 4 |
| I2S BCLK | 5 |
| I2S WS | 7 |
| I2S DOUT (динамик) | 8 |
| I2S DIN (микрофон) | 6 |
| Усилитель PA (active-low) | 1 |

## Корпус

`stl/XiaoZhi_ESP32S3_28_front_frame_REV_D7-1.stl` — лицевая рамка для этой
платы из сообщества xiaozhi-esp32. Файл взят как есть; авторство и лицензию
уточните по исходному источнику модели (см. ../CREDITS.md), параметры печати —
по рекомендациям автора модели.
