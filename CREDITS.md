# Credits / Благодарности

## Вдохновение

- [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
  © 2025 Shenzhen Xinzhi Future Technology Co., Ltd. и контрибьюторы (MIT) —
  идея голосового AI-ассистента на ESP32 и профиль платы
  `freenove-esp32s3-display-2.8-lcd` (распиновка). Код прошивки написан
  независимо, заимствований из xiaozhi-esp32 нет.

## Зависимости (подтягиваются idf component manager)

- ESP-IDF, esp_lcd_ili9341, esp_lcd_touch_ft5x06, esp_lvgl_port, esp_codec_dev,
  esp-sr — © Espressif Systems (Apache-2.0)
- LVGL — MIT

## Инструменты и данные

- Шрифты сгенерированы из DejaVu Sans (DejaVu Fonts License, permissive)
- Сервер распознавания речи: [whisper.cpp](https://github.com/ggml-org/whisper.cpp)
  (MIT), [Caddy](https://caddyserver.com) (Apache-2.0)
- Погода: [Open-Meteo](https://open-meteo.com) (CC BY 4.0)
- LLM (опционально): [OpenRouter](https://openrouter.ai)

## Корпус

- `hardware/stl/XiaoZhi_ESP32S3_28_front_frame_REV_D7-1.stl` — лицевая рамка
  для платы из сообщества xiaozhi-esp32; авторство и лицензия уточняются
  по исходному источнику модели.
