#!/usr/bin/env bash
# Instala y configura el broker MQTT mosquitto. Requiere core/template/packages.
DEPLOY_DIR="${DEPLOY_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

# install_mosquitto MQTT_USER MQTT_PASS
install_mosquitto() {
  local user="$1" pass="$2"
  info "Se va a instalar (núcleo): mosquitto (MQTT 1883 + WebSockets 9001)"
  apt_install mosquitto mosquitto-clients

  local tmp; tmp="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/mosquitto-rover.conf.tmpl" "" "$tmp"
  run_root install -m 0644 "$tmp" /etc/mosquitto/conf.d/rover.conf
  rm -f "$tmp"

  # Crea/actualiza el usuario MQTT (-b: batch, -c crea el archivo si no existe)
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ mosquitto_passwd -b /etc/mosquitto/passwd %s ****\n' "$user"
  else
    if [ -f /etc/mosquitto/passwd ]; then
      run_root mosquitto_passwd -b /etc/mosquitto/passwd "$user" "$pass"
    else
      run_root mosquitto_passwd -b -c /etc/mosquitto/passwd "$user" "$pass"
    fi
  fi

  run_root chown root:mosquitto /etc/mosquitto/passwd
  run_root chmod 0640 /etc/mosquitto/passwd

  run_root systemctl enable --now mosquitto
  run_root systemctl restart mosquitto
  ok "mosquitto configurado (usuario: $user)"
}
