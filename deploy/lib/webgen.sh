#!/usr/bin/env bash
# Genera mqtt.js (config de la web) según el modo de despliegue.
# Requiere haber cargado core.sh y template.sh.

# generate_mqtt_js TEMPLATE OUTFILE MODE HOST_OR_IP MQTT_USER MQTT_PASS
#   MODE: lan | ddns | cloudflare
#   HOST_OR_IP: IP local (lan), hostname DDNS (ddns) o dominio base (cloudflare)
generate_mqtt_js() {
  local tmpl="$1" out="$2" mode="$3" host="$4" user="$5" pass="$6"
  case "$mode" in
    lan)
      export MQTT_WS_HOST="$host" MQTT_WS_PORT="80"
      export VIDEO_WS_URL="ws://$host:9002"
      ;;
    ddns)
      export MQTT_WS_HOST="$host" MQTT_WS_PORT="8443"
      export VIDEO_WS_URL="wss://$host:8443/video"
      ;;
    cloudflare)
      export MQTT_WS_HOST="mqtt.$host" MQTT_WS_PORT="443"
      export VIDEO_WS_URL="wss://video.$host"
      ;;
    *) die "Modo desconocido en generate_mqtt_js: $mode" ;;
  esac
  export MQTT_USER="$user" MQTT_PASS="$pass"
  local vars='$MQTT_WS_HOST $MQTT_WS_PORT $MQTT_USER $MQTT_PASS $VIDEO_WS_URL'
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ generar mqtt.js (%s) -> %s\n' "$mode" "$out"
  else
    render_template "$tmpl" "$vars" > "$out"
  fi
}
