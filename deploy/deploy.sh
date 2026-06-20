#!/usr/bin/env bash
# Orquestador interactivo de despliegue del Rover.
set -u
DEPLOY_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$DEPLOY_DIR/.." && pwd)"
export DEPLOY_DIR REPO_ROOT

# shellcheck source=/dev/null
for m in core ui template checks packages webgen firmware mosquitto nginx video_proxy fail2ban ddns cloudflare tls; do
  . "$DEPLOY_DIR/lib/$m.sh"
done

DEFAULT_REPO="https://github.com/j4py/rover-perseverance"
DEFAULT_WEBROOT="/opt/rover"
ENV_FILE=""
SKIP_CLONE=0

# --- args ---
while [ "$#" -gt 0 ]; do
  case "$1" in
    --dry-run)   DRY_RUN=1 ;;
    --env)       ENV_FILE="$2"; shift ;;
    --skip-clone) SKIP_CLONE=1 ;;
    -h|--help)
      cat <<'H'
Uso: deploy.sh [--dry-run] [--env ARCHIVO] [--skip-clone]
  --dry-run     No ejecuta nada, solo imprime las acciones.
  --env FILE    Usa respuestas guardadas (no interactivo).
  --skip-clone  No clona/actualiza el repo (para pruebas).
H
      exit 0 ;;
    *) die "Argumento desconocido: $1" ;;
  esac
  shift
done

# --- recoger configuración ---
if [ -n "$ENV_FILE" ]; then
  [ -f "$ENV_FILE" ] || die "No existe el archivo de respuestas: $ENV_FILE"
  # shellcheck source=/dev/null
  . "$ENV_FILE"
else
  info "== Configuración del despliegue del Rover =="
  MODE="$(ask_choice "Modo de despliegue" \
            "lan:Local / LAN (solo red local)" \
            "ddns:DDNS (DuckDNS/FreeDNS/Dynu/YDNS)" \
            "cloudflare:Cloudflare Tunnel")"
  REPO_URL="$(ask "URL del repositorio git" "$DEFAULT_REPO")"
  WEBROOT="$(ask "Ruta de despliegue web" "$DEFAULT_WEBROOT")"
  RUN_USER="$(ask "Usuario del sistema para los servicios" "${SUDO_USER:-$USER}")"

  MQTT_USER="$(ask "Usuario MQTT" "rover" validate_nonempty)"
  MQTT_PASS="$(ask_secret "Contraseña MQTT")"

  case "$MODE" in
    lan)
      HOST_OR_IP="$(ask "IP local del servidor" "$(hostname -I 2>/dev/null | awk '{print $1}')" validate_ip)"
      ;;
    ddns)
      DDNS_PROVIDER="$(ask_choice "Proveedor DDNS" "duckdns:DuckDNS" "freedns:FreeDNS" "dynu:Dynu" "ydns:YDNS")"
      HOST_OR_IP="$(ask "Hostname DDNS (p.ej. rover.duckdns.org)" "" validate_hostname)"
      DDNS_TOKEN="$(ask_secret "Token/clave del proveedor DDNS")"
      TLS_EMAIL="$(ask "Email para el certificado TLS (certbot)" "" validate_nonempty)"
      ;;
    cloudflare)
      HOST_OR_IP="$(ask "Dominio base (p.ej. midominio.com)" "" validate_hostname)"
      CF_TOKEN="$(ask_secret "Token del túnel Cloudflare")"
      ;;
  esac

  info "Credenciales WiFi del ESP32 (puedes dejar vacías las redes extra):"
  WIFI_SSID1="$(ask "WiFi SSID 1" "" validate_nonempty)"
  WIFI_PASS1="$(ask_secret "WiFi contraseña 1")"
  WIFI_SSID2="$(ask "WiFi SSID 2 (opcional)" "")"
  WIFI_PASS2=""; [ -n "$WIFI_SSID2" ] && WIFI_PASS2="$(ask_secret "WiFi contraseña 2")"
  WIFI_SSID3="$(ask "WiFi SSID 3 (opcional)" "")"
  WIFI_PASS3=""; [ -n "$WIFI_SSID3" ] && WIFI_PASS3="$(ask_secret "WiFi contraseña 3")"

  info "El NÚCLEO se instalará siempre: mosquitto, nginx, video_proxy."
  if ask_yesno "¿Instalar también fail2ban (endurecimiento SSH, opcional)?" n; then
    WITH_FAIL2BAN=1
  else
    WITH_FAIL2BAN=0
  fi
fi

# defaults para variables que puedan no venir del env
: "${DDNS_PROVIDER:=}"; : "${DDNS_TOKEN:=}"; : "${CF_TOKEN:=}"; : "${TLS_EMAIL:=}"
: "${WIFI_SSID2:=}"; : "${WIFI_PASS2:=}"; : "${WIFI_SSID3:=}"; : "${WIFI_PASS3:=}"
: "${WITH_FAIL2BAN:=0}"; : "${RUN_USER:=$USER}"

# --- preflight ---
is_ubuntu || warn "Esto está pensado para Ubuntu; continúo bajo tu responsabilidad."
ensure_sudo
check_internet || warn "Sin conexión detectada; la instalación de paquetes podría fallar."

# --- dependencias base ---
install_base

# --- ubicar el proyecto en WEBROOT (sin clonar dos veces) ---
if [ "$SKIP_CLONE" = "1" ]; then
  info "[--skip-clone] usando árbol actual: $REPO_ROOT"
  WEBROOT="$REPO_ROOT"
elif [ -d "$REPO_ROOT/.git" ]; then
  # Ya estamos dentro de un clon (p. ej. lanzado por bootstrap.sh): no re-clonar.
  if [ "$REPO_ROOT" = "$WEBROOT" ]; then
    info "El proyecto ya está en $WEBROOT"
  else
    info "Copiando el proyecto de $REPO_ROOT a $WEBROOT"
    run_root mkdir -p "$WEBROOT"
    run_root cp -a "$REPO_ROOT/." "$WEBROOT/"
  fi
else
  # Ejecución sin clon previo: descargar de git.
  if [ -d "$WEBROOT/.git" ]; then
    info "Actualizando repo en $WEBROOT"
    run_root git -C "$WEBROOT" pull --ff-only
  else
    info "Clonando $REPO_URL en $WEBROOT"
    run_root mkdir -p "$WEBROOT"
    run_root git clone "$REPO_URL" "$WEBROOT"
  fi
fi

# --- núcleo obligatorio ---
install_mosquitto "$MQTT_USER" "$MQTT_PASS"
# server_name: '_' en LAN, hostname/dominio en remoto
case "$MODE" in
  lan)        SRV_NAME="_" ;;
  ddns)       SRV_NAME="$HOST_OR_IP" ;;
  cloudflare) SRV_NAME="rover.$HOST_OR_IP" ;;
esac
install_nginx "$SRV_NAME" "$WEBROOT"
install_video_proxy "$RUN_USER"

# --- ligado al modo ---
case "$MODE" in
  ddns)
    install_ddns "$DDNS_PROVIDER" "$HOST_OR_IP" "$DDNS_TOKEN"
    install_tls "$HOST_OR_IP" "$TLS_EMAIL"
    ;;
  cloudflare)
    install_cloudflared "$CF_TOKEN"
    ;;
esac

# --- opcionales ---
[ "$WITH_FAIL2BAN" = "1" ] && install_fail2ban

# --- generación de configuración web + firmware ---
generate_mqtt_js "$DEPLOY_DIR/templates/mqtt.js.tmpl" "$WEBROOT/mqtt.js" "$MODE" "$HOST_OR_IP" "$MQTT_USER" "$MQTT_PASS"

# host MQTT para el firmware (nativo 1883): IP en LAN; hostname en ddns; direct.<dominio> en cloudflare
case "$MODE" in
  lan)        FW_MQTT_HOST="$HOST_OR_IP"; FW_VIDEO_HOST="$HOST_OR_IP" ;;
  ddns)       FW_MQTT_HOST="$HOST_OR_IP"; FW_VIDEO_HOST="$HOST_OR_IP" ;;
  cloudflare) FW_MQTT_HOST="direct.$HOST_OR_IP"; FW_VIDEO_HOST="video.$HOST_OR_IP" ;;
esac
for ino in "$REPO_ROOT/ESP32/Mars_Rover.ino" "$REPO_ROOT/ESP32/gps.ino" "$REPO_ROOT/ESP32/Camera_Marcelo_20fps_480x320.ino"; do
  [ -f "$ino" ] && fill_firmware_file "$ino" \
    "$WIFI_SSID1" "$WIFI_PASS1" "$WIFI_SSID2" "$WIFI_PASS2" "$WIFI_SSID3" "$WIFI_PASS3" \
    "$FW_MQTT_HOST" "$MQTT_USER" "$MQTT_PASS" "$FW_VIDEO_HOST"
done

# --- guardar respuestas ---
if [ -z "$ENV_FILE" ]; then
  export MODE WEBROOT REPO_URL HOST_OR_IP MQTT_USER MQTT_PASS WIFI_SSID1 WIFI_PASS1 WIFI_SSID2 WIFI_PASS2 WIFI_SSID3 WIFI_PASS3 DDNS_PROVIDER DDNS_TOKEN CF_TOKEN TLS_EMAIL RUN_USER WITH_FAIL2BAN
  render_to "$DEPLOY_DIR/templates/rover-deploy.env.tmpl" \
    '$MODE $WEBROOT $REPO_URL $HOST_OR_IP $MQTT_USER $MQTT_PASS $WIFI_SSID1 $WIFI_PASS1 $WIFI_SSID2 $WIFI_PASS2 $WIFI_SSID3 $WIFI_PASS3 $DDNS_PROVIDER $DDNS_TOKEN $CF_TOKEN $TLS_EMAIL $RUN_USER $WITH_FAIL2BAN' \
    "$DEPLOY_DIR/rover-deploy.env"
fi

# --- resumen ---
echo
ok "===== RESUMEN ====="
case "$MODE" in
  lan)        echo "  Web:   http://$HOST_OR_IP" ;;
  ddns)       echo "  Web:   https://$HOST_OR_IP" ;;
  cloudflare) echo "  Web:   https://rover.$HOST_OR_IP (según ingress de Cloudflare)" ;;
esac
echo "  Componentes núcleo: mosquitto + nginx + video_proxy"
[ "$MODE" = "ddns" ] && echo "  + DDNS ($DDNS_PROVIDER) + TLS"
[ "$MODE" = "cloudflare" ] && echo "  + cloudflared (configura el ingress en el dashboard)"
[ "$WITH_FAIL2BAN" = "1" ] && echo "  + fail2ban"
echo "  Firmware: credenciales escritas en ESP32/*.ino — compílalos y flashéalos con Arduino IDE."
echo "  IMPORTANTE: no commitees los .ino con credenciales (usa deploy/scripts/reset-firmware-secrets.sh)."
