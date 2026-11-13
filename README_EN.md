# 🐣 Tamagotchi Vaiwu (EN)

**V.A.I.W.U. — Virtual AI Interactive Waifu Unit**

A tamagotchi waifu for the ESP32-S3: a virtual pet, a mini-RPG and an AI
companion in one pocket device with a 2.8" touch display.
[Русская версия](README.md).

## Features

- 7 characters with distinct personalities, voices and battle skills
- Live needs: hunger, happiness, energy, discipline, weight; sickness, sleep, death
- Mini-RPG: turn-based battles, 5 stats, levels, equipment, dungeons with bosses
- AI assistant: LLM chat, voice input, weather, timers, alarms, jokes —
  plus an offline keyword fallback that works with no keys and no network
- She talks: an offline chiptune "babble" voice with a per-character timbre,
  or real speech via TTS
- Russian and English UI
- OTA updates over Wi-Fi from GitHub Releases (Settings → Update)

## Repository layout

| Folder | Contents |
|---|---|
| [`firmware/`](firmware/) | Firmware (ESP-IDF 5.5.x, C++20, LVGL 9.5) |
| [`server/`](server/) | Free self-hosted speech-to-text server (whisper.cpp + Caddy) |
| [`hardware/`](hardware/) | Board info, pinout, 3D-print case (STL) |

## Quick start

Prebuilt firmware lives in [Releases](https://github.com/zimin3d/vaiwu/releases)
(flash with esptool per the release notes). To build from source:

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig   # API keys are optional — the device works fully offline
idf.py build flash monitor
```

## Status

Runs on real hardware (Freenove ESP32-S3 2.8" Capacitive Touch Display);
latest verified version: **v4.9**.

## License & credits

[MIT](LICENSE). Inspired by xiaozhi-esp32 — see [CREDITS.md](CREDITS.md).
