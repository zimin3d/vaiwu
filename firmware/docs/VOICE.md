# Voice input

Two layers, per the hybrid design.

## 1. Push-to-talk (default, works out of the box)

On the chat screen hold the pink microphone button and speak; on release the
recording (ES8311 mic, PCM16 mono) is wrapped in a WAV and sent to a Whisper
transcription endpoint. The recognized text is fed into the same pipeline as
typed input: local intents first (alarm / reminder / joke / time), otherwise
the AI assistant. Recognition language follows the UI language (RU/EN).

Configure in `idf.py menuconfig` -> Tamagotchi Vaiwu Configuration:

- `VAIWU_STT_API_KEY`  - key/token for the transcription endpoint.
- `VAIWU_STT_URL`      - endpoint (default OpenAI Whisper).
- `VAIWU_STT_MODEL`    - model name (default `whisper-1`).

With no key set, the mic button is hidden and typing still works.

### Free option: your own server (no per-minute cost)

Instead of a paid cloud key, point `VAIWU_STT_URL` at a self-hosted
whisper.cpp server on your LAN. A ready, security-hardened Docker setup
(token gateway + private whisper) lives in the repo-root `server/` folder - see its README
and run `server/deploy.sh`. Then:

- `VAIWU_STT_URL = http://<vm-ip>:8080/inference`
- `VAIWU_STT_API_KEY = <your STT_TOKEN>`

This is the same idea xiaozhi uses (heavy lifting on a self-hosted server with
free models), just push-to-talk instead of streaming.

## Spoken replies (TTS) - optional

`VAIWU_TTS_URL` (empty by default = off) points at an OpenAI-compatible
`/v1/audio/speech` endpoint that returns raw PCM16 mono. When set, assistant
answers, weather, jokes, the time and chat replies are also spoken through the
speaker. Configure `VAIWU_TTS_API_KEY` / `VAIWU_TTS_MODEL` / `VAIWU_TTS_VOICE`.
For a free route, self-host Piper behind a tiny PCM adapter next to the STT
server. Requesting `response_format=pcm` at 24 kHz means no on-device audio
decoder is needed.

## 2. Offline wake word (ESP-SR) - advanced, opt-in

`VAIWU_ENABLE_WAKEWORD` (default off) compiles the on-device WakeNet trigger
(`components/device_hal/esp/WakeWord.cpp`). It is a no-op until enabled and
needs the esp-sr component, PSRAM and a model partition. Not compile-verified
in the authoring environment - treat as a starting point.
