#!/usr/bin/env python3
"""Host-side checks, no ESP-IDF required.

1. Compiles the platform-independent game core with g++ (C++20, -Werror)
   and runs the unit tests.
2. Smoke-compiles the host (#else) branches of the chat/UI layers.
3. Compiles the ESP (#ifdef ESP_PLATFORM) branches of every NEW source file
   against faithful IDF API stubs (tools/host_stubs). If the environment
   variable VAIWU_LVGL_DIR points at a real lvgl component checkout (e.g.
   <some idf project>/managed_components/lvgl__lvgl), the UI files are
   additionally checked against the REAL LVGL 9.x headers; set
   VAIWU_LVGL_PORT_DIR to the esp_lvgl_port include dir for full fidelity.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "host_build")

GAME_SRC = [
    "components/game/src/Characters.cpp",
    "components/game/src/VaiwuPet.cpp",
    "components/game/src/OfflinePhrases.cpp",
    "components/game/src/SoundSynth.cpp",
    "components/game/src/I18n.cpp",
    "components/game/src/Assistant.cpp",
    "components/game/src/Shop.cpp",
    "components/game/src/Rpg.cpp",
]
HOST_BRANCH_SRC = [
    "components/core/src/ConsoleLogger.cpp",
    "components/chat/src/NeuralChat.cpp",
    "components/chat/src/SpeechToText.cpp",
    "components/chat/src/TextToSpeech.cpp",
    "components/ui/src/VaiwuUi.cpp",
    "components/ui/src/DisplayDriver.cpp",
]
INCLUDES = [
    "components/game/include",
    "components/core/include",
    "components/chat/include",
    "components/ui/include",
    "components/device_hal/include",
    "main",
]
FLAGS = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-O1"]
LV_DEFS = [
    "-DLV_CONF_SKIP=1",
    "-DLV_FONT_MONTSERRAT_12=1", "-DLV_FONT_MONTSERRAT_14=1",
    "-DLV_FONT_MONTSERRAT_16=1", "-DLV_FONT_MONTSERRAT_20=1",
    "-DLV_FONT_MONTSERRAT_24=1",
]


def run(cmd):
    print("+", " ".join(cmd))
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        sys.exit(result.returncode)


def main():
    os.makedirs(OUT, exist_ok=True)
    inc = [f"-I{path}" for path in INCLUDES]

    # 1. Unit tests for the game core.
    test_bin = os.path.join(OUT, "test_pet")
    run(["g++", *FLAGS, *inc, *GAME_SRC, "tests/test_pet.cpp", "-o", test_bin])
    run([test_bin])

    rpg_bin = os.path.join(OUT, "test_rpg")
    run(["g++", *FLAGS, *inc, *GAME_SRC, "tests/test_rpg.cpp", "-o", rpg_bin])
    run([rpg_bin])

    alarm_bin = os.path.join(OUT, "test_alarm")
    run(["g++", *FLAGS, *inc, "-Imain", "tests/test_alarm.cpp", "-o", alarm_bin])
    run([alarm_bin])

    wav_bin = os.path.join(OUT, "test_wav")
    run(["g++", *FLAGS, *inc, "components/chat/src/SpeechToText.cpp",
         "tests/test_wav.cpp", "-o", wav_bin])
    run([wav_bin])

    # 2. Host branches of the platform-dependent layers.
    for src in HOST_BRANCH_SRC:
        obj = os.path.join(OUT, os.path.basename(src) + ".host.o")
        run(["g++", *FLAGS, *inc, "-c", src, "-o", obj])

    # 3. ESP branches against IDF stubs (syntax + API-shape check).
    stub_inc = ["-Itools/host_stubs", *inc]
    esp_flags = [*FLAGS, "-DESP_PLATFORM"]
    run(["g++", *esp_flags, *stub_inc, "-fsyntax-only", "components/chat/src/NeuralChat.cpp"])
    run(["g++", *esp_flags, *stub_inc, "-fsyntax-only", "components/chat/src/SpeechToText.cpp"])
    run(["g++", *esp_flags, *stub_inc, "-fsyntax-only", "components/chat/src/TextToSpeech.cpp"])
    run(["g++", *esp_flags, *stub_inc, "-fsyntax-only", "components/device_hal/esp/OpenMeteoWeatherProvider.cpp"])
    run(["g++", *esp_flags, *stub_inc, "-fsyntax-only", "main/main.cpp"])
    # Wake word must build as a no-op when the opt-in flag is off (default).
    run(["g++", *esp_flags, "-Itools/host_stubs", "-Icomponents/device_hal/include",
         "-Icomponents/core/include", "-fsyntax-only", "components/device_hal/esp/WakeWord.cpp"])

    lvgl_dir = os.environ.get("VAIWU_LVGL_DIR", "")
    if lvgl_dir:
        lv_inc = [f"-I{lvgl_dir}"]
        port_dir = os.environ.get("VAIWU_LVGL_PORT_DIR", "")
        if port_dir:
            lv_inc.append(f"-I{port_dir}")
        full = [*esp_flags, *LV_DEFS, *stub_inc, *lv_inc]
        run(["g++", *full, "-fsyntax-only", "components/ui/src/VaiwuUi.cpp"])
        run(["g++", *full, "-fsyntax-only", "components/ui/src/DisplayDriver.cpp"])
        run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-DESP_PLATFORM", *LV_DEFS,
             *stub_inc, *lv_inc, "-fsyntax-only", "components/ui/src/sprites.c"])
    else:
        # No real LVGL checkout available: fall back to the minimal local
        # stub so the UI's ESP branch still gets a full syntax pass. The
        # stub mirrors LVGL 9 signatures but is NOT the real thing - run
        # with VAIWU_LVGL_DIR when real LVGL headers are available.
        stub = [*esp_flags, *stub_inc, "-Itools/host_stubs/lvgl_stub"]
        run(["g++", *stub, "-fsyntax-only", "components/ui/src/VaiwuUi.cpp"])
        run(["g++", *stub, "-fsyntax-only", "components/ui/src/DisplayDriver.cpp"])
        print("note: VAIWU_LVGL_DIR not set - checked UI against the local LVGL stub")

    print("HOST CHECKS PASSED")


if __name__ == "__main__":
    main()
