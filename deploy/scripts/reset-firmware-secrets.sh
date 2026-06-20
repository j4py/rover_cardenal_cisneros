#!/usr/bin/env bash
# Restaura los .ino a placeholders CAMBIA_*. Funciona con o sin git.
# Uso: reset-firmware-secrets.sh [archivo.ino ...]
set -eu
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

restore_one() {
  local f="$1"
  # Preferir git si el archivo está versionado.
  if git -C "$repo_root" ls-files --error-unmatch "$f" >/dev/null 2>&1; then
    git -C "$repo_root" checkout -- "$f"
  else
    sed -i.bak \
      -e 's/ssid_primary = "[^"]*"/ssid_primary = "CAMBIA_WIFI_SSID"/' \
      -e 's/password_primary = "[^"]*"/password_primary = "CAMBIA_WIFI_PASS"/' \
      -e 's/ssid_secondary = "[^"]*"/ssid_secondary = "CAMBIA_WIFI_SSID2"/' \
      -e 's/password_secondary = "[^"]*"/password_secondary = "CAMBIA_WIFI_PASS2"/' \
      -e 's/ssid_tertiary = "[^"]*"/ssid_tertiary = "CAMBIA_WIFI_SSID3"/' \
      -e 's/password_tertiary = "[^"]*"/password_tertiary = "CAMBIA_WIFI_PASS3"/' \
      -e 's/mqtt_server = "[^"]*"/mqtt_server = "CAMBIA_MQTT_HOST"/' \
      -e 's/mqtt_user = "[^"]*"/mqtt_user = "CAMBIA_MQTT_USER"/' \
      -e 's/mqtt_password = "[^"]*"/mqtt_password = "CAMBIA_MQTT_PASS"/' \
      -e 's/ws_host = "[^"]*"/ws_host = "CAMBIA_VIDEO_HOST"/' \
      "$f"
    rm -f "$f.bak"
  fi
  echo "Restaurado: $f"
}

if [ "$#" -gt 0 ]; then
  for f in "$@"; do restore_one "$f"; done
else
  for f in "$repo_root"/ESP32/*.ino "$repo_root"/ESP32/Camera_Marcelo/*.ino; do
    [ -f "$f" ] && restore_one "$f"
  done
fi
