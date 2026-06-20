#!/usr/bin/env bash
# Instala y configura nginx para servir la web y proxyar MQTT/vídeo.
DEPLOY_DIR="${DEPLOY_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

# install_nginx SERVER_NAME WEBROOT
install_nginx() {
  local server_name="$1" webroot="$2"
  info "Se va a instalar (núcleo): nginx (web + proxy WebSocket /mqtt, /video, /publish)"
  apt_install nginx

  export SERVER_NAME="$server_name" WEBROOT="$webroot"
  local tmp; tmp="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/nginx-rover.conf.tmpl" '$SERVER_NAME $WEBROOT' "$tmp"
  run_root install -m 0644 "$tmp" /etc/nginx/sites-available/rover
  rm -f "$tmp"
  run_root ln -sf /etc/nginx/sites-available/rover /etc/nginx/sites-enabled/rover
  # Evita choque con el default si captura el mismo server_name
  run_root rm -f /etc/nginx/sites-enabled/default

  run_root nginx -t
  run_root systemctl enable --now nginx
  run_root systemctl reload nginx
  ok "nginx configurado (root: $webroot)"
}
