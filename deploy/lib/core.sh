#!/usr/bin/env bash
# Núcleo compartido: logging, colores y ejecución con soporte de dry-run.
: "${DRY_RUN:=0}"

if [ -t 1 ]; then
  C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'; C_BLU=$'\033[34m'; C_RST=$'\033[0m'
else
  C_RED=''; C_GRN=''; C_YLW=''; C_BLU=''; C_RST=''
fi

info() { printf '%s[INFO]%s %s\n' "$C_BLU" "$C_RST" "$*"; }
ok()   { printf '%s[ OK ]%s %s\n' "$C_GRN" "$C_RST" "$*"; }
warn() { printf '%s[WARN]%s %s\n' "$C_YLW" "$C_RST" "$*" >&2; }
err()  { printf '%s[FAIL]%s %s\n' "$C_RED" "$C_RST" "$*" >&2; }
die()  { err "$*"; exit 1; }

# Helper genérico (lo usan checks.sh, cloudflare.sh, etc.): ¿existe el comando?
need_cmd() { command -v "$1" >/dev/null 2>&1; }

# Ejecuta un comando (argv). En dry-run solo lo imprime.
run() {
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ %s\n' "$*"
    return 0
  fi
  "$@"
}

# Igual que run() pero con sudo si no somos root.
run_root() {
  if [ "$(id -u)" -eq 0 ]; then
    run "$@"
  else
    run sudo "$@"
  fi
}

# Ejecuta un snippet de shell (permite tuberías/redirecciones). Respeta dry-run.
run_sh() {
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ %s\n' "$*"
    return 0
  fi
  bash -c "$*"
}
