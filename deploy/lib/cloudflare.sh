#!/usr/bin/env bash
# Instala cloudflared y registra el túnel por token.

install_cloudflared() {
  local token="$1"
  info "Instalando cloudflared (túnel Cloudflare)"
  if ! need_cmd cloudflared; then
    # Repo oficial de Cloudflare
    run_sh 'curl -fsSL https://pkg.cloudflare.com/cloudflare-main.gpg | sudo tee /usr/share/keyrings/cloudflare-main.gpg >/dev/null'
    run_sh 'echo "deb [signed-by=/usr/share/keyrings/cloudflare-main.gpg] https://pkg.cloudflare.com/cloudflared any main" | sudo tee /etc/apt/sources.list.d/cloudflared.list >/dev/null'
    apt_update_once
    apt_install cloudflared
  fi
  run_root cloudflared service install "$token"
  ok "cloudflared instalado. Configura el ingress (rover/mqtt/direct/video) en el dashboard de Cloudflare."
}
