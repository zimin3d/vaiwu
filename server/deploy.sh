#!/usr/bin/env bash
# One-command deploy/start for the self-hosted STT server on a Linux VM.
#
# Safe by default: generates a strong token on first run, binds the gateway to
# your LAN IP only (never 0.0.0.0), keeps whisper.cpp unpublished, and can add
# a firewall rule limited to your subnet. Idempotent - re-run to rebuild or
# restart.
#
# Usage:
#   ./deploy.sh                 # interactive first run, then up -d --build
#   ./deploy.sh --model base    # override model
#   ./deploy.sh --status        # show status + health check
#   ./deploy.sh --logs          # follow logs
#   ./deploy.sh --down          # stop
set -euo pipefail
cd "$(dirname "$0")"

MODEL_OVERRIDE=""
ACTION="up"
while [ $# -gt 0 ]; do
  case "$1" in
    --model) MODEL_OVERRIDE="$2"; shift 2;;
    --status) ACTION="status"; shift;;
    --logs) ACTION="logs"; shift;;
    --down) ACTION="down"; shift;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "unknown arg: $1"; exit 1;;
  esac
done

# --- pick docker compose command --------------------------------------------
if docker compose version >/dev/null 2>&1; then
  DC="docker compose"
elif command -v docker-compose >/dev/null 2>&1; then
  DC="docker-compose"
else
  echo "ERROR: Docker Compose not found. Install Docker + the compose plugin first:"
  echo "  https://docs.docker.com/engine/install/"
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "ERROR: cannot talk to the Docker daemon. Is it running, and are you in the 'docker' group?"
  echo "  sudo systemctl start docker ; sudo usermod -aG docker \$USER  (re-login after)"
  exit 1
fi

detect_lan_ip() {
  ip -4 -o addr show scope global 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | head -1
}

# --- first-run .env ----------------------------------------------------------
if [ ! -f .env ]; then
  echo "First run: creating .env"
  TOKEN="$(openssl rand -hex 24 2>/dev/null || head -c 24 /dev/urandom | od -An -tx1 | tr -d ' \n')"
  LAN_IP="$(detect_lan_ip || true)"
  if [ -z "${LAN_IP:-}" ]; then LAN_IP="127.0.0.1"; fi
  read -r -p "Bind gateway to LAN IP [${LAN_IP}]: " ans; BIND_IP="${ans:-$LAN_IP}"
  read -r -p "Whisper model (base/small/medium) [small]: " ans; MODEL="${ans:-small}"
  cat > .env <<EOT
STT_TOKEN=${TOKEN}
WHISPER_MODEL=${MODEL}
BIND_IP=${BIND_IP}
EOT
  chmod 600 .env
  echo "Wrote .env (token generated, kept private with chmod 600)."
fi

# apply model override if given
if [ -n "$MODEL_OVERRIDE" ]; then
  sed -i "s/^WHISPER_MODEL=.*/WHISPER_MODEL=${MODEL_OVERRIDE}/" .env
fi

# shellcheck disable=SC1091
set -a; . ./.env; set +a

case "$ACTION" in
  down)
    $DC down
    exit 0;;
  logs)
    $DC logs -f
    exit 0;;
  status)
    $DC ps
    echo "--- health check ---"
    code=$(curl -s -o /dev/null -w '%{http_code}' "http://${BIND_IP}:8080/inference" || echo "000")
    echo "no-token request -> HTTP ${code} (401 expected = auth is working; 000 = not reachable)"
    exit 0;;
esac

echo "Building and starting (first build downloads the '${WHISPER_MODEL}' model, may take a few minutes)..."
$DC up -d --build

echo
echo "Waiting for the server to answer..."
for i in $(seq 1 60); do
  code=$(curl -s -o /dev/null -w '%{http_code}' "http://${BIND_IP}:8080/inference" || echo "000")
  if [ "$code" = "401" ] || [ "$code" = "400" ] || [ "$code" = "200" ]; then
    echo "Gateway is up (HTTP ${code} without token)."
    break
  fi
  sleep 3
  if [ "$i" = "60" ]; then echo "WARNING: still not answering; check: $DC logs -f"; fi
done

# --- optional firewall hardening --------------------------------------------
if command -v ufw >/dev/null 2>&1; then
  SUBNET="$(echo "$BIND_IP" | awk -F. '{print $1"."$2"."$3".0/24"}')"
  echo
  read -r -p "Restrict port 8080 to your subnet ${SUBNET} with ufw? [y/N]: " ans
  if [ "${ans:-N}" = "y" ] || [ "${ans:-N}" = "Y" ]; then
    sudo ufw allow from "${SUBNET}" to any port 8080 proto tcp
    sudo ufw deny 8080/tcp || true
    echo "Firewall rule added."
  fi
fi

echo
echo "=================================================================="
echo " STT server is running."
echo "   URL   : http://${BIND_IP}:8080/inference"
echo "   token : (STT_TOKEN in ./.env)"
echo
echo " Put these into 'idf.py menuconfig' -> Tamagotchi Vaiwu Configuration:"
echo "   STT endpoint URL = http://${BIND_IP}:8080/inference"
echo "   STT API key      = $(grep '^STT_TOKEN=' .env | cut -d= -f2)"
echo "   STT model        = whisper-1   (any value; ignored by whisper.cpp)"
echo
echo " Test transcription (needs a short test.wav in this folder):"
echo "   curl http://${BIND_IP}:8080/inference \\"
echo "     -H \"Authorization: Bearer \$(grep ^STT_TOKEN= .env | cut -d= -f2)\" \\"
echo "     -F file=@test.wav -F response_format=json -F language=ru"
echo "=================================================================="
