#!/usr/bin/env bash
# Prompts interactivos y validadores. Las funciones ask* leen de stdin
# para poder probarse con tuberías.

validate_nonempty() { [ -n "${1:-}" ]; }

validate_ip() {
  local ip="${1:-}" o IFS=.
  case "$ip" in
    *[!0-9.]*|.*|*.|*..*) return 1 ;;
  esac
  # shellcheck disable=SC2086
  set -- $ip
  [ "$#" -eq 4 ] || return 1
  for o in "$@"; do
    [ "$o" -ge 0 ] 2>/dev/null && [ "$o" -le 255 ] || return 1
  done
  return 0
}

validate_hostname() {
  local h="${1:-}"
  [ -n "$h" ] || return 1
  printf '%s' "$h" | grep -Eq '^([a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?$'
}

# ask PROMPT [DEFAULT] [VALIDATOR]
# Devuelve el valor por stdout. Reintenta si el validador falla.
ask() {
  local prompt="$1" def="${2:-}" validator="${3:-}" reply
  while :; do
    if [ -n "$def" ]; then
      printf '%s [%s]: ' "$prompt" "$def" >&2
    else
      printf '%s: ' "$prompt" >&2
    fi
    IFS= read -r reply || reply=''
    [ -z "$reply" ] && reply="$def"
    if [ -n "$validator" ]; then
      if "$validator" "$reply"; then printf '%s\n' "$reply"; return 0; fi
      printf '  Valor no válido, inténtalo de nuevo.\n' >&2
      continue
    fi
    printf '%s\n' "$reply"; return 0
  done
}

# ask_secret PROMPT  -> lee sin eco (si es terminal), devuelve por stdout
ask_secret() {
  local prompt="$1" reply
  printf '%s: ' "$prompt" >&2
  if [ -t 0 ]; then
    IFS= read -r -s reply || reply=''
    printf '\n' >&2
  else
    IFS= read -r reply || reply=''
  fi
  printf '%s\n' "$reply"
}

# ask_yesno PROMPT [default s|n]  -> return 0 si sí, 1 si no
ask_yesno() {
  local prompt="$1" def="${2:-s}" reply
  while :; do
    printf '%s [%s/%s]: ' "$prompt" \
      "$([ "$def" = s ] && echo S || echo s)" \
      "$([ "$def" = n ] && echo N || echo n)" >&2
    IFS= read -r reply || reply=''
    [ -z "$reply" ] && reply="$def"
    case "$reply" in
      s|S|si|Si|SI|y|Y|yes) return 0 ;;
      n|N|no|No|NO)         return 1 ;;
      *) printf '  Responde s o n.\n' >&2 ;;
    esac
  done
}

# ask_choice PROMPT "val:etiqueta" ...  -> devuelve el "val" elegido
ask_choice() {
  local prompt="$1"; shift
  local opts=("$@") i reply
  while :; do
    printf '%s:\n' "$prompt" >&2
    i=1
    for o in "${opts[@]}"; do
      printf '  %d) %s\n' "$i" "${o#*:}" >&2
      i=$((i+1))
    done
    printf 'Elige [1-%d]: ' "${#opts[@]}" >&2
    IFS= read -r reply || reply=''
    if [ "$reply" -ge 1 ] 2>/dev/null && [ "$reply" -le "${#opts[@]}" ]; then
      printf '%s\n' "${opts[$((reply-1))]%%:*}"
      return 0
    fi
    printf '  Opción no válida.\n' >&2
  done
}
