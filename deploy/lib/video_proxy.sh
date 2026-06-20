#!/usr/bin/env bash
# Instala el relay de vídeo (python websockets) como servicio systemd.
DEPLOY_DIR="${DEPLOY_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
PROXY_INSTALL_DIR="/opt/rover-proxy"

# install_video_proxy RUN_USER
install_video_proxy() {
  local run_user="$1"
  info "Se va a instalar (núcleo): video_proxy (relay de cámara, puerto 9002)"
  apt_install python3 python3-websockets

  run_root mkdir -p "$PROXY_INSTALL_DIR"
  run_root install -m 0755 "$DEPLOY_DIR/server/video_proxy.py" "$PROXY_INSTALL_DIR/video_proxy.py"

  export PROXY_PATH="$PROXY_INSTALL_DIR/video_proxy.py" PROXY_DIR="$PROXY_INSTALL_DIR" PROXY_USER="$run_user"
  local tmp; tmp="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/video_proxy.service.tmpl" '$PROXY_PATH $PROXY_DIR $PROXY_USER' "$tmp"
  run_root install -m 0644 "$tmp" /etc/systemd/system/video_proxy.service
  rm -f "$tmp"

  run_root systemctl daemon-reload
  run_root systemctl enable --now video_proxy.service
  run_root systemctl restart video_proxy.service
  ok "video_proxy configurado (usuario: $run_user)"
}
