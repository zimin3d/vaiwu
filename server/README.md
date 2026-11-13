# Self-hosted free speech-to-text server (whisper.cpp)

Runs your own Whisper transcription on a Linux VM so voice commands cost
nothing and never leave your network. The firmware talks to it exactly like
the OpenAI Whisper API, so only three settings change on the device.

Two containers:

- **whisper** - whisper.cpp HTTP server. Built from source (no fragile image
  tag). **Not** published to the host - it can only be reached from the
  gateway on a private Docker network.
- **gateway** - Caddy. The only exposed port. Rejects any request that does
  not carry the exact bearer token, then forwards to whisper.

```
device --(http, Bearer token)--> gateway (Caddy, LAN only) --> whisper (private)
```

## Requirements

Docker + Docker Compose on the VM. A few CPU cores and ~1-2 GB free RAM are
enough for the `small` model on short push-to-talk clips (a few seconds).
This is a light, on-demand load - it is idle between requests.

## Quick start (recommended)

```sh
cd server
./deploy.sh          # generates a token, detects your LAN IP, builds & starts,
                     # runs a health check, offers a firewall rule, and prints
                     # the exact menuconfig values to paste into the firmware
```

`./deploy.sh --status` shows health, `--logs` follows logs, `--down` stops it,
`--model base` switches model.

## Manual setup (if you prefer)

```sh
cd server
cp .env.example .env
# edit .env:
#   STT_TOKEN  -> a long random secret:  openssl rand -hex 24
#   BIND_IP    -> the VM's LAN IP (ip -4 addr), e.g. 192.168.1.50
#   WHISPER_MODEL -> small (default) / base / medium
docker compose up -d --build       # first build downloads the model, be patient
```

Check it is alive from the VM itself:

```sh
TOKEN=$(grep STT_TOKEN .env | cut -d= -f2)
# should return 401 without the token:
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:8080/inference
# real transcription (record or copy any short wav as test.wav):
curl -s http://127.0.0.1:8080/inference \
  -H "Authorization: Bearer $TOKEN" \
  -F file=@test.wav -F response_format=json -F language=ru
```

## Point the device at it

`idf.py menuconfig` -> Tamagotchi Vaiwu Configuration:

- **STT endpoint URL** -> `http://<BIND_IP>:8080/inference`  (http, not https)
- **STT model** -> anything (ignored by whisper.cpp), e.g. `whisper-1`
- **STT API key** -> the same value as `STT_TOKEN`

Rebuild + flash. Hold the mic button on the chat screen and speak.

## Security checklist (important)

- The gateway binds to your **LAN IP**, never `0.0.0.0`. Do **not** port-forward
  it on your router - keep it inside your home network.
- Only the tokened gateway is reachable; whisper itself has no host port.
- Use a long random `STT_TOKEN`. Treat it like a password; it is your only
  auth on the LAN.
- Optional extra: restrict the port to your subnet with the firewall, e.g.
  `sudo ufw allow from 192.168.1.0/24 to any port 8080 proto tcp`.
- Traffic on the LAN is plain http (unencrypted). For a home toy that is fine;
  do not send anything sensitive by voice. Real TLS would need a certificate
  the device trusts and is out of scope here.
- Keep `.env` out of git (already covered by .gitignore).

## Notes / troubleshooting

- First `--build` compiles whisper.cpp and downloads the model - it can take
  several minutes. Later starts are instant.
- If transcription is slow, use `WHISPER_MODEL=base`; if quality is poor on
  Russian, try `medium` (needs more RAM).
- `docker compose logs -f whisper` shows recognition activity;
  `docker compose logs -f gateway` shows auth/proxy activity (401 = wrong or
  missing token).
- The server binary name differs across whisper.cpp versions; the image picks
  `whisper-server` or `server` automatically.
- Alternative engine: if you later want an all-in-one ASR+LLM+TTS box like
  xiaozhi uses, the same URL setting can point at any OpenAI-compatible
  `/audio/transcriptions` endpoint (e.g. a faster-whisper server).
