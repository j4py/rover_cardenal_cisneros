#!/usr/bin/env bash
# DDNS: instala un updater para el proveedor elegido + timer systemd.
DEPLOY_DIR="${DEPLOY_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

# ddns_template_for PROVIDER -> nombre de plantilla
ddns_template_for() {
  case "$1" in
    duckdns) echo "ddns-duckdns.sh.tmpl" ;;
    freedns) echo "ddns-freedns.sh.tmpl" ;;
    dynu)    echo "ddns-dynu.sh.tmpl" ;;
    ydns)    echo "ddns-ydns.sh.tmpl" ;;
    *) die "Proveedor DDNS desconocido: $1" ;;
  esac
}

# install_ddns PROVIDER DOMAIN TOKEN
install_ddns() {
  local provider="$1" domain="$2" token="$3"
  info "Configurando DDNS ($provider) para $domain"
  apt_install curl
  export DDNS_DOMAIN="$domain" DDNS_TOKEN="$token"
  local tmpl; tmpl="$(ddns_template_for "$provider")"

  local tmp; tmp="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/$tmpl" '$DDNS_DOMAIN $DDNS_TOKEN' "$tmp"
  run_root install -m 0755 "$tmp" /usr/local/bin/rover-ddns-update.sh
  rm -f "$tmp"

  local svc; svc="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/ddns-updater.service.tmpl" "" "$svc"
  run_root install -m 0644 "$svc" /etc/systemd/system/ddns-updater.service
  rm -f "$svc"

  local tim; tim="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/ddns-updater.timer.tmpl" "" "$tim"
  run_root install -m 0644 "$tim" /etc/systemd/system/ddns-updater.timer
  rm -f "$tim"

  run_root systemctl daemon-reload
  run_root systemctl enable --now ddns-updater.timer
  # Primera actualización inmediata
  run_root systemctl start ddns-updater.service
  ok "DDNS configurado ($provider, cada 5 min)"
}
