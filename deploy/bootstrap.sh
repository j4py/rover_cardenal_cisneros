#!/usr/bin/env bash
# Bootstrap de una línea: instala git, clona el repo y ejecuta el despliegue.
#   curl -fsSL https://raw.githubusercontent.com/j4py/rover_cardenal_cisneros/main/deploy/bootstrap.sh | bash
set -eu

REPO_URL="${ROVER_REPO:-https://github.com/j4py/rover_cardenal_cisneros}"
CLONE_DIR="${ROVER_DIR:-$HOME/rover_cardenal_cisneros}"
DRY_RUN=0
[ "${1:-}" = "--dry-run" ] && DRY_RUN=1

_run() { if [ "$DRY_RUN" = "1" ]; then printf '+ %s\n' "$*"; else "$@"; fi; }

echo "== Rover · bootstrap de despliegue =="

# git
if ! command -v git >/dev/null 2>&1; then
  echo "Instalando git..."
  _run sudo apt-get update -y
  _run sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y git
fi

# clonar o actualizar
if [ -d "$CLONE_DIR/.git" ]; then
  echo "Actualizando repo en $CLONE_DIR"
  _run git -C "$CLONE_DIR" pull --ff-only
else
  echo "Clonando $REPO_URL en $CLONE_DIR"
  _run git clone "$REPO_URL" "$CLONE_DIR"
fi

# lanzar el orquestador
echo "Lanzando deploy.sh..."
if [ "$DRY_RUN" = "1" ]; then
  printf '+ bash %s/deploy/deploy.sh\n' "$CLONE_DIR"
else
  exec bash "$CLONE_DIR/deploy/deploy.sh"
fi
