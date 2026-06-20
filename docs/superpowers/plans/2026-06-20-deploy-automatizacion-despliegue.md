# Automatización de despliegue del Rover — Plan de Implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Crear un script interactivo que despliegue el sistema completo del Rover (broker MQTT, web, proxy de vídeo, acceso remoto opcional) en una máquina Ubuntu desde cero, y dejar el repo listo para publicarse sin credenciales reales.

**Architecture:** Orquestador bash modular (`deploy/deploy.sh`) que carga librerías pequeñas (`deploy/lib/*.sh`), cada una responsable de un componente. Toda mutación del sistema pasa por un wrapper `run()` que respeta un modo `--dry-run`. La configuración (web `mqtt.js`, endpoint de vídeo, credenciales de los `.ino`) se genera desde plantillas (`deploy/templates/*.tmpl`) con los datos que introduce el usuario. El repo versiona placeholders; el script rellena los valores reales en local.

**Tech Stack:** Bash (POSIX-ish, `bash` 4+), `envsubst` (gettext-base), systemd, nginx, mosquitto, certbot, cloudflared. Pruebas: harness bash propio (sin dependencias) + `shellcheck`.

**Spec:** [docs/superpowers/specs/2026-06-20-deploy-automatizacion-despliegue-design.md](../specs/2026-06-20-deploy-automatizacion-despliegue-design.md)

---

## Convenciones del plan

- **Tokens de placeholder** (los mismos en repo y en `firmware.sh`/`webgen.sh`):
  `CAMBIA_WIFI_SSID`, `CAMBIA_WIFI_PASS`, `CAMBIA_WIFI_SSID2`, `CAMBIA_WIFI_PASS2`,
  `CAMBIA_WIFI_SSID3`, `CAMBIA_WIFI_PASS3`, `CAMBIA_MQTT_HOST`, `CAMBIA_MQTT_USER`,
  `CAMBIA_MQTT_PASS`, `CAMBIA_VIDEO_HOST`.
- **Ruta de despliegue web por defecto:** `/opt/rover`.
- **Repo por defecto:** `https://github.com/j4py/rover-perseverance`.
- **Las pruebas corren en git-bash (Windows) y en Ubuntu.** Los módulos de sistema se prueban en
  `--dry-run` (no requieren root ni Ubuntu). `shellcheck` se ejecuta donde esté disponible.
- Cada tarea termina con un commit en la rama `feat/deploy-automation`.

## Mapa de archivos

| Archivo                                    | Responsabilidad                                                                                    |
| ------------------------------------------ | -------------------------------------------------------------------------------------------------- |
| `deploy/lib/core.sh`                       | Logging, colores, `run()`/`run_root()`, flag `DRY_RUN`, `die`.                                     |
| `deploy/lib/ui.sh`                         | Prompts interactivos + validadores (`ask`, `ask_secret`, `ask_yesno`, `ask_choice`, `validate_*`). |
| `deploy/lib/template.sh`                   | `render_template` (sustitución segura con `envsubst`).                                             |
| `deploy/lib/checks.sh`                     | Preflight: `is_ubuntu`, `need_cmd`, `ensure_sudo`, `port_in_use`, `check_internet`.                |
| `deploy/lib/webgen.sh`                     | Genera `mqtt.js` y `deploy/generated/rover-web-config.js` según modo.                              |
| `deploy/lib/firmware.sh`                   | Rellena tokens en los `.ino`.                                                                      |
| `deploy/lib/packages.sh`                   | `apt_install` (a través de `run_root`).                                                            |
| `deploy/lib/mosquitto.sh`                  | Instala/configura broker (listeners 1883 + 9001, auth, passwd).                                    |
| `deploy/lib/nginx.sh`                      | Instala/configura site (web + proxy `/mqtt`, `/video`, `/publish`).                                |
| `deploy/lib/video_proxy.sh`                | Copia `server/video_proxy.py` + servicio systemd.                                                  |
| `deploy/lib/fail2ban.sh`                   | Jail sshd (opcional).                                                                              |
| `deploy/lib/ddns.sh`                       | Updater DuckDNS/FreeDNS/Dynu/YDNS + service/timer.                                                 |
| `deploy/lib/cloudflare.sh`                 | `cloudflared service install <token>`.                                                             |
| `deploy/lib/tls.sh`                        | `certbot --nginx` para hostname DDNS.                                                              |
| `deploy/deploy.sh`                         | Orquestador: args, preflight, preguntas, clone, dispatch, generación, resumen.                     |
| `deploy/bootstrap.sh`                      | One-liner: instala git, clona, ejecuta `deploy.sh`.                                                |
| `deploy/scripts/reset-firmware-secrets.sh` | Restaura placeholders en los `.ino`.                                                               |
| `deploy/templates/*.tmpl`                  | Plantillas de configuración.                                                                       |
| `deploy/server/video_proxy.py`             | Relay de vídeo (canónico, traído del servidor).                                                    |
| `deploy/server/cloudflare_ddns.py.example` | DDNS Cloudflare plantillizado.                                                                     |
| `deploy/tests/lib.sh`                      | Mini-framework de aserciones.                                                                      |
| `deploy/tests/test_*.sh`                   | Pruebas por módulo.                                                                                |
| `deploy/tests/run_all.sh`                  | Runner de todas las pruebas.                                                                       |

---

## Task 1: Scaffolding + harness de pruebas + `core.sh`

**Files:**

- Create: `deploy/lib/core.sh`
- Create: `deploy/tests/lib.sh`
- Create: `deploy/tests/run_all.sh`
- Create: `deploy/tests/test_core.sh`

- [ ] **Step 1: Crear el mini-framework de aserciones**

`deploy/tests/lib.sh`:

```bash
#!/usr/bin/env bash
# Mini-framework de pruebas (sin dependencias).
set -u
_TESTS_RUN=0
_TESTS_FAIL=0

assert_eq() {  # assert_eq EXPECTED ACTUAL [msg]
  _TESTS_RUN=$((_TESTS_RUN+1))
  if [ "$1" != "$2" ]; then
    _TESTS_FAIL=$((_TESTS_FAIL+1))
    printf 'FAIL: %s\n  esperado: [%s]\n  obtenido: [%s]\n' "${3:-assert_eq}" "$1" "$2" >&2
  fi
}

assert_contains() {  # assert_contains HAYSTACK NEEDLE [msg]
  _TESTS_RUN=$((_TESTS_RUN+1))
  case "$1" in
    *"$2"*) : ;;
    *)
      _TESTS_FAIL=$((_TESTS_FAIL+1))
      printf 'FAIL: %s\n  no contiene: [%s]\n  en: [%s]\n' "${3:-assert_contains}" "$2" "$1" >&2
      ;;
  esac
}

assert_not_contains() {  # assert_not_contains HAYSTACK NEEDLE [msg]
  _TESTS_RUN=$((_TESTS_RUN+1))
  case "$1" in
    *"$2"*)
      _TESTS_FAIL=$((_TESTS_FAIL+1))
      printf 'FAIL: %s\n  contiene (no debería): [%s]\n' "${3:-assert_not_contains}" "$2" >&2
      ;;
  esac
}

finish() {
  printf '%s: %d aserciones, %d fallos\n' "${0##*/}" "$_TESTS_RUN" "$_TESTS_FAIL"
  [ "$_TESTS_FAIL" -eq 0 ]
}
```

- [ ] **Step 2: Crear el runner**

`deploy/tests/run_all.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
rc=0
for t in "$here"/test_*.sh; do
  [ -f "$t" ] || continue
  bash "$t" || rc=1
done
if [ "$rc" -eq 0 ]; then
  echo "TODAS LAS PRUEBAS OK"
else
  echo "HAY PRUEBAS FALLIDAS" >&2
fi
exit "$rc"
```

- [ ] **Step 3: Escribir el test de `core.sh` (falla primero)**

`deploy/tests/test_core.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"

# run() en dry-run imprime el comando con prefijo y NO lo ejecuta
DRY_RUN=1
out="$(run touch "$here/__should_not_exist__")"
assert_contains "$out" "+ touch" "dry-run imprime comando"
[ -e "$here/__should_not_exist__" ] && { echo "FAIL: dry-run ejecutó el comando" >&2; rm -f "$here/__should_not_exist__"; }

# run() en modo real SÍ ejecuta
DRY_RUN=0
tmp="$(mktemp)"; rm -f "$tmp"
run touch "$tmp"
[ -f "$tmp" ] && assert_eq "ok" "ok" "modo real ejecuta" || assert_eq "ok" "no" "modo real ejecuta"
rm -f "$tmp"

finish
```

- [ ] **Step 4: Ejecutar el test y verificar que falla**

Run: `bash deploy/tests/test_core.sh`
Expected: error porque `deploy/lib/core.sh` no existe todavía.

- [ ] **Step 5: Implementar `core.sh`**

`deploy/lib/core.sh`:

```bash
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
```

- [ ] **Step 6: Ejecutar el test y verificar que pasa**

Run: `bash deploy/tests/test_core.sh`
Expected: `test_core.sh: N aserciones, 0 fallos`

- [ ] **Step 7: Commit**

```bash
git add deploy/lib/core.sh deploy/tests/
git commit -m "feat(deploy): scaffolding, harness de pruebas y core.sh con run()/dry-run"
```

---

## Task 2: `ui.sh` — prompts y validadores

**Files:**

- Create: `deploy/lib/ui.sh`
- Create: `deploy/tests/test_ui.sh`

- [ ] **Step 1: Escribir el test (falla primero)**

`deploy/tests/test_ui.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/ui.sh"

# Validadores
validate_ip "192.168.1.10"   && assert_eq ok ok "ip válida" || assert_eq ok no "ip válida"
validate_ip "999.1.1.1"      && assert_eq ok no "ip inválida rechazada" || assert_eq ok ok "ip inválida rechazada"
validate_hostname "rover.example.com" && assert_eq ok ok "host válido" || assert_eq ok no "host válido"
validate_hostname "no validez!" && assert_eq ok no "host inválido rechazado" || assert_eq ok ok "host inválido rechazado"
validate_nonempty "x" && assert_eq ok ok "nonempty ok" || assert_eq ok no "nonempty ok"
validate_nonempty ""  && assert_eq ok no "nonempty vacío rechazado" || assert_eq ok ok "nonempty vacío rechazado"

# ask usa el default cuando la entrada está vacía
out="$(printf '\n' | ask "Nombre" "valor_por_defecto")"
assert_eq "valor_por_defecto" "$out" "ask devuelve default con entrada vacía"

# ask devuelve lo introducido
out="$(printf 'introducido\n' | ask "Nombre" "def")"
assert_eq "introducido" "$out" "ask devuelve lo introducido"

# ask_yesno: 's' => 0 (sí), 'n' => 1 (no)
printf 's\n' | ask_yesno "¿Sí?" && assert_eq ok ok "yesno s" || assert_eq ok no "yesno s"
printf 'n\n' | ask_yesno "¿No?" && assert_eq ok no "yesno n" || assert_eq ok ok "yesno n"

# ask_choice devuelve el valor de la opción elegida (por número)
out="$(printf '2\n' | ask_choice "Modo" "lan:Local" "ddns:DDNS" "cf:Cloudflare")"
assert_eq "ddns" "$out" "ask_choice elige por índice"

finish
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_ui.sh`
Expected: error (`ui.sh` no existe).

- [ ] **Step 3: Implementar `ui.sh`**

`deploy/lib/ui.sh`:

```bash
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
```

- [ ] **Step 4: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_ui.sh`
Expected: `0 fallos`.

- [ ] **Step 5: Commit**

```bash
git add deploy/lib/ui.sh deploy/tests/test_ui.sh
git commit -m "feat(deploy): ui.sh con prompts y validadores (ip/host/yesno/choice)"
```

---

## Task 3: `template.sh` — render de plantillas

**Files:**

- Create: `deploy/lib/template.sh`
- Create: `deploy/tests/test_template.sh`
- Create: `deploy/tests/fixtures/sample.tmpl`

- [ ] **Step 1: Crear la fixture de plantilla**

`deploy/tests/fixtures/sample.tmpl`:

```
host=${HOST}
port=${PORT}
literal=$NO_TOCAR
```

- [ ] **Step 2: Escribir el test (falla primero)**

`deploy/tests/test_template.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"

export HOST="rover.example.com" PORT="443"
out="$(render_template "$here/fixtures/sample.tmpl" '$HOST $PORT')"
assert_contains "$out" "host=rover.example.com" "sustituye HOST"
assert_contains "$out" "port=443" "sustituye PORT"
# Variables no listadas se dejan literales (no se expande $NO_TOCAR)
assert_contains "$out" 'literal=$NO_TOCAR' "no expande variables fuera de la lista"

finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_template.sh`
Expected: error (`template.sh` no existe).

- [ ] **Step 4: Implementar `template.sh`**

`deploy/lib/template.sh`:

```bash
#!/usr/bin/env bash
# Render seguro de plantillas con envsubst, limitando a una lista de variables.

# render_template TEMPLATE VARLIST  ('$A $B')  -> escribe a stdout
render_template() {
  local tmpl="$1" vars="${2:-}"
  [ -f "$tmpl" ] || die "Plantilla no encontrada: $tmpl"
  command -v envsubst >/dev/null 2>&1 || die "Falta 'envsubst' (paquete gettext-base)"
  if [ -n "$vars" ]; then
    envsubst "$vars" < "$tmpl"
  else
    envsubst < "$tmpl"
  fi
}

# render_to TEMPLATE VARLIST OUTFILE  -> render a un archivo (respeta dry-run vía run_root al instalar)
render_to() {
  local tmpl="$1" vars="$2" out="$3"
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ render %s -> %s\n' "$tmpl" "$out"
    return 0
  fi
  render_template "$tmpl" "$vars" > "$out"
}
```

- [ ] **Step 5: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_template.sh`
Expected: `0 fallos`.

> Nota: `envsubst` viene con Git para Windows y con `gettext-base` en Ubuntu. La dependencia se instala en Task 7.

- [ ] **Step 6: Commit**

```bash
git add deploy/lib/template.sh deploy/tests/test_template.sh deploy/tests/fixtures/sample.tmpl
git commit -m "feat(deploy): template.sh (render seguro con envsubst)"
```

---

## Task 4: `checks.sh` — preflight

**Files:**

- Create: `deploy/lib/checks.sh`
- Create: `deploy/tests/test_checks.sh`
- Create: `deploy/tests/fixtures/os-release-ubuntu`
- Create: `deploy/tests/fixtures/os-release-debian`

- [ ] **Step 1: Crear fixtures de os-release**

`deploy/tests/fixtures/os-release-ubuntu`:

```
NAME="Ubuntu"
ID=ubuntu
VERSION_ID="24.04"
```

`deploy/tests/fixtures/os-release-debian`:

```
NAME="Debian GNU/Linux"
ID=debian
VERSION_ID="12"
```

- [ ] **Step 2: Escribir el test (falla primero)**

`deploy/tests/test_checks.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/checks.sh"

is_ubuntu "$here/fixtures/os-release-ubuntu" && assert_eq ok ok "detecta ubuntu" || assert_eq ok no "detecta ubuntu"
is_ubuntu "$here/fixtures/os-release-debian" && assert_eq ok no "rechaza debian" || assert_eq ok ok "rechaza debian"

need_cmd bash && assert_eq ok ok "need_cmd encuentra bash" || assert_eq ok no "need_cmd encuentra bash"
need_cmd __comando_inexistente__ && assert_eq ok no "need_cmd falla con inexistente" || assert_eq ok ok "need_cmd falla con inexistente"

finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_checks.sh`
Expected: error (`checks.sh` no existe).

- [ ] **Step 4: Implementar `checks.sh`**

`deploy/lib/checks.sh`:

```bash
#!/usr/bin/env bash
# Comprobaciones de preflight.

# is_ubuntu [ruta-os-release]
is_ubuntu() {
  local f="${1:-/etc/os-release}"
  [ -f "$f" ] || return 1
  grep -q '^ID=ubuntu' "$f"
}

# need_cmd está definido en core.sh (helper compartido).

# Asegura que tenemos sudo (o somos root). Cachea el timestamp.
ensure_sudo() {
  [ "$(id -u)" -eq 0 ] && return 0
  need_cmd sudo || die "Se necesita 'sudo' o ejecutar como root."
  info "Algunas acciones requieren privilegios; se pedirá tu contraseña de sudo."
  if [ "$DRY_RUN" != "1" ]; then
    sudo -v || die "No se pudo obtener sudo."
  fi
}

# port_in_use PUERTO -> 0 si está ocupado
port_in_use() {
  local p="$1"
  if need_cmd ss; then
    ss -ltn 2>/dev/null | grep -Eq "[:.]$p[[:space:]]"
  else
    return 1
  fi
}

check_internet() {
  if need_cmd curl; then
    curl -fsS --max-time 8 -o /dev/null https://github.com 2>/dev/null
  else
    return 0
  fi
}
```

- [ ] **Step 5: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_checks.sh`
Expected: `0 fallos`.

- [ ] **Step 6: Commit**

```bash
git add deploy/lib/checks.sh deploy/tests/test_checks.sh deploy/tests/fixtures/os-release-*
git commit -m "feat(deploy): checks.sh (preflight ubuntu/sudo/puertos/internet)"
```

---

## Task 5: `webgen.sh` + plantilla `mqtt.js`

**Files:**

- Create: `deploy/templates/mqtt.js.tmpl`
- Create: `deploy/lib/webgen.sh`
- Create: `deploy/tests/test_webgen.sh`

- [ ] **Step 1: Crear la plantilla `mqtt.js.tmpl`**

`deploy/templates/mqtt.js.tmpl`:

```
window.MQTT_CONFIG = {
  HOST: "${MQTT_WS_HOST}",
  PORT: ${MQTT_WS_PORT},
  PATH: "/mqtt",
  USERNAME: "${MQTT_USER}",
  PASSWORD: "${MQTT_PASS}"
};
window.VIDEO_WS_URL = "${VIDEO_WS_URL}";
```

- [ ] **Step 2: Escribir el test (falla primero)**

`deploy/tests/test_webgen.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/webgen.sh"

TPL_DIR="$here/../templates"
out="$(mktemp)"

# Modo LAN
generate_mqtt_js "$TPL_DIR/mqtt.js.tmpl" "$out" lan "192.168.1.50" "rover" "secreto"
body="$(cat "$out")"
assert_contains "$body" 'HOST: "192.168.1.50"' "LAN host = IP"
assert_contains "$body" 'PORT: 80' "LAN port 80"
assert_contains "$body" 'USERNAME: "rover"' "LAN user"
assert_contains "$body" 'VIDEO_WS_URL = "ws://192.168.1.50:9002"' "LAN video ws"

# Modo DDNS
generate_mqtt_js "$TPL_DIR/mqtt.js.tmpl" "$out" ddns "rover.duckdns.org" "rover" "secreto"
body="$(cat "$out")"
assert_contains "$body" 'HOST: "rover.duckdns.org"' "DDNS host"
assert_contains "$body" 'PORT: 443' "DDNS port 443"
assert_contains "$body" 'VIDEO_WS_URL = "wss://rover.duckdns.org/video"' "DDNS video wss"

# Modo Cloudflare
generate_mqtt_js "$TPL_DIR/mqtt.js.tmpl" "$out" cloudflare "example.com" "rover" "secreto"
body="$(cat "$out")"
assert_contains "$body" 'HOST: "mqtt.example.com"' "CF host = mqtt.<dominio>"
assert_contains "$body" 'PORT: 443' "CF port 443"
assert_contains "$body" 'VIDEO_WS_URL = "wss://video.example.com"' "CF video wss"

rm -f "$out"
finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_webgen.sh`
Expected: error (`webgen.sh` no existe).

- [ ] **Step 4: Implementar `webgen.sh`**

`deploy/lib/webgen.sh`:

```bash
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
      export MQTT_WS_HOST="$host" MQTT_WS_PORT="443"
      export VIDEO_WS_URL="wss://$host/video"
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
```

- [ ] **Step 5: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_webgen.sh`
Expected: `0 fallos`.

- [ ] **Step 6: Commit**

```bash
git add deploy/templates/mqtt.js.tmpl deploy/lib/webgen.sh deploy/tests/test_webgen.sh
git commit -m "feat(deploy): webgen.sh genera mqtt.js + endpoint de vídeo por modo"
```

---

## Task 6: `firmware.sh` + reset de secretos

**Files:**

- Create: `deploy/lib/firmware.sh`
- Create: `deploy/scripts/reset-firmware-secrets.sh`
- Create: `deploy/tests/test_firmware.sh`
- Create: `deploy/tests/fixtures/sample.ino`

- [ ] **Step 1: Crear una fixture `.ino` con placeholders**

`deploy/tests/fixtures/sample.ino`:

```
const char *ssid_primary = "CAMBIA_WIFI_SSID";
const char *password_primary = "CAMBIA_WIFI_PASS";
const char *ssid_secondary = "CAMBIA_WIFI_SSID2";
const char *password_secondary = "CAMBIA_WIFI_PASS2";
const char *mqtt_server = "CAMBIA_MQTT_HOST";
const char *mqtt_user = "CAMBIA_MQTT_USER";
const char *mqtt_password = "CAMBIA_MQTT_PASS";
const char *ws_host = "CAMBIA_VIDEO_HOST";
```

- [ ] **Step 2: Escribir el test (falla primero)**

`deploy/tests/test_firmware.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/firmware.sh"

work="$(mktemp -d)"
cp "$here/fixtures/sample.ino" "$work/sample.ino"

# Rellena: 1 red wifi, mqtt host = IP, video host = IP
fill_firmware_file "$work/sample.ino" \
  "MiWifi" "clave123" "" "" "" "" \
  "192.168.1.50" "rover" "secreto" "192.168.1.50"

body="$(cat "$work/sample.ino")"
assert_contains "$body" 'ssid_primary = "MiWifi"' "wifi primary"
assert_contains "$body" 'password_primary = "clave123"' "pass primary"
# Slots no usados copian el primario (WiFiMulti lo deduplica, inofensivo)
assert_contains "$body" 'ssid_secondary = "MiWifi"' "slot2 = primario"
assert_contains "$body" 'mqtt_server = "192.168.1.50"' "mqtt host"
assert_contains "$body" 'mqtt_password = "secreto"' "mqtt pass"
assert_contains "$body" 'ws_host = "192.168.1.50"' "video host"
assert_not_contains "$body" "CAMBIA_" "no quedan placeholders"

# reset restaura placeholders
bash "$here/../scripts/reset-firmware-secrets.sh" "$work/sample.ino"
body="$(cat "$work/sample.ino")"
assert_contains "$body" 'ssid_primary = "CAMBIA_WIFI_SSID"' "reset restaura placeholder"
assert_not_contains "$body" "MiWifi" "reset elimina secretos"

rm -rf "$work"
finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_firmware.sh`
Expected: error (`firmware.sh` no existe).

- [ ] **Step 4: Implementar `firmware.sh`**

`deploy/lib/firmware.sh`:

```bash
#!/usr/bin/env bash
# Rellena los tokens CAMBIA_* en los .ino con las credenciales del usuario.

# _sed_inplace EXPR FILE  (sed -i portable)
_sed_inplace() {
  local expr="$1" file="$2"
  sed -i.bak "$expr" "$file" && rm -f "$file.bak"
}

# _replace_token TOKEN VALUE FILE  (sustituye el token literal por VALUE, escapando / y &)
_replace_token() {
  local token="$1" value="$2" file="$3" esc
  esc=$(printf '%s' "$value" | sed -e 's/[\/&]/\\&/g')
  _sed_inplace "s/$token/$esc/g" "$file"
}

# fill_firmware_file FILE  S1 P1 S2 P2 S3 P3  MQTT_HOST MQTT_USER MQTT_PASS VIDEO_HOST
fill_firmware_file() {
  local file="$1"; shift
  local s1="$1" p1="$2" s2="$3" p2="$4" s3="$5" p3="$6"
  local mhost="$7" muser="$8" mpass="$9" vhost="${10}"
  [ -f "$file" ] || die "Firmware no encontrado: $file"
  # Slots vacíos copian el primario (inofensivo con WiFiMulti).
  [ -z "$s2" ] && s2="$s1"; [ -z "$p2" ] && p2="$p1"
  [ -z "$s3" ] && s3="$s1"; [ -z "$p3" ] && p3="$p1"
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ rellenar credenciales en %s\n' "$file"
    return 0
  fi
  _replace_token CAMBIA_WIFI_SSID2 "$s2" "$file"
  _replace_token CAMBIA_WIFI_PASS2 "$p2" "$file"
  _replace_token CAMBIA_WIFI_SSID3 "$s3" "$file"
  _replace_token CAMBIA_WIFI_PASS3 "$p3" "$file"
  _replace_token CAMBIA_WIFI_SSID  "$s1" "$file"
  _replace_token CAMBIA_WIFI_PASS  "$p1" "$file"
  _replace_token CAMBIA_MQTT_HOST  "$mhost" "$file"
  _replace_token CAMBIA_MQTT_USER  "$muser" "$file"
  _replace_token CAMBIA_MQTT_PASS  "$mpass" "$file"
  _replace_token CAMBIA_VIDEO_HOST "$vhost" "$file"
}
```

> Orden importante: los tokens con sufijo (`..._SSID2`) se sustituyen **antes** que el base
> (`..._SSID`) para que `s/CAMBIA_WIFI_SSID/.../` no rompa `CAMBIA_WIFI_SSID2`.

- [ ] **Step 5: Implementar `reset-firmware-secrets.sh`**

`deploy/scripts/reset-firmware-secrets.sh`:

```bash
#!/usr/bin/env bash
# Restaura los .ino indicados (o todos los del repo) a placeholders, usando git.
# Uso: reset-firmware-secrets.sh [archivo.ino ...]
set -eu
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if [ "$#" -gt 0 ]; then
  for f in "$@"; do
    git checkout -- "$f" 2>/dev/null || { echo "No se pudo restaurar $f (¿no versionado?)" >&2; exit 1; }
    echo "Restaurado: $f"
  done
else
  git checkout -- 'ESP32/*.ino' 2>/dev/null || true
  echo "Restaurados todos los ESP32/*.ino a su versión versionada (placeholders)."
fi
```

> En las pruebas la fixture no está en git; por eso el test llama a `reset-firmware-secrets.sh`
> con la ruta de la fixture, pero el script real usa `git checkout`. Ajuste: el test usa una
> copia versionada simulada — ver Step 6.

- [ ] **Step 6: Ajustar el reset para ser testeable sin git (modo plantilla)**

Sustituir el cuerpo de `reset-firmware-secrets.sh` por una versión que regenere placeholders por
patrón, válida tanto en repo como en pruebas:

```bash
#!/usr/bin/env bash
# Restaura los .ino a placeholders CAMBIA_*. Funciona con o sin git.
# Uso: reset-firmware-secrets.sh [archivo.ino ...]
set -eu
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"

restore_one() {
  local f="$1"
  # Preferir git si el archivo está versionado y limpio el índice lo permite.
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
```

- [ ] **Step 7: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_firmware.sh`
Expected: `0 fallos`.

- [ ] **Step 8: Commit**

```bash
git add deploy/lib/firmware.sh deploy/scripts/reset-firmware-secrets.sh deploy/tests/test_firmware.sh deploy/tests/fixtures/sample.ino
git commit -m "feat(deploy): firmware.sh rellena tokens CAMBIA_* + reset-firmware-secrets.sh"
```

---

## Task 7: `packages.sh` — instalación apt

**Files:**

- Create: `deploy/lib/packages.sh`
- Create: `deploy/tests/test_packages.sh`

- [ ] **Step 1: Escribir el test (dry-run, falla primero)**

`deploy/tests/test_packages.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/packages.sh"

DRY_RUN=1
out="$(apt_install git curl 2>&1)"
assert_contains "$out" "apt-get install" "apt_install planea instalar"
assert_contains "$out" "git" "incluye git"
assert_contains "$out" "curl" "incluye curl"
finish
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_packages.sh`
Expected: error (`packages.sh` no existe).

- [ ] **Step 3: Implementar `packages.sh`**

`deploy/lib/packages.sh`:

```bash
#!/usr/bin/env bash
# Instalación de paquetes vía apt. Requiere core.sh.

APT_UPDATED=0

apt_update_once() {
  [ "$APT_UPDATED" = "1" ] && return 0
  run_root apt-get update -y
  APT_UPDATED=1
}

# apt_install PKG...
apt_install() {
  [ "$#" -gt 0 ] || return 0
  apt_update_once
  run_root env DEBIAN_FRONTEND=noninteractive apt-get install -y "$@"
}

# install_base: dependencias mínimas siempre necesarias
install_base() {
  info "Se va a instalar (núcleo): git, curl, ca-certificates, gettext-base"
  apt_install git curl ca-certificates gettext-base
}
```

- [ ] **Step 4: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_packages.sh`
Expected: `0 fallos`.

- [ ] **Step 5: Commit**

```bash
git add deploy/lib/packages.sh deploy/tests/test_packages.sh
git commit -m "feat(deploy): packages.sh (apt_install + install_base)"
```

---

## Task 8: `mosquitto.sh` + plantilla de config

**Files:**

- Create: `deploy/templates/mosquitto-rover.conf.tmpl`
- Create: `deploy/lib/mosquitto.sh`
- Create: `deploy/tests/test_mosquitto.sh`

- [ ] **Step 1: Crear la plantilla de mosquitto**

`deploy/templates/mosquitto-rover.conf.tmpl`:

```
# Config del Rover (generado por deploy.sh)
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log

# MQTT nativo (rover/ESP32)
listener 1883 0.0.0.0
protocol mqtt

# WebSockets (web)
listener 9001 0.0.0.0
protocol websockets

allow_anonymous false
password_file /etc/mosquitto/passwd
```

- [ ] **Step 2: Escribir el test (dry-run, falla primero)**

`deploy/tests/test_mosquitto.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/mosquitto.sh"

# La plantilla rinde con los 2 listeners y auth
out="$(render_template "$here/../templates/mosquitto-rover.conf.tmpl")"
assert_contains "$out" "listener 1883" "listener nativo"
assert_contains "$out" "listener 9001" "listener websockets"
assert_contains "$out" "allow_anonymous false" "auth activada"

DRY_RUN=1
out="$(install_mosquitto "rover" "secreto" 2>&1)"
assert_contains "$out" "apt-get install" "instala mosquitto"
assert_contains "$out" "mosquitto_passwd" "crea usuario"
assert_contains "$out" "systemctl" "habilita servicio"
finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_mosquitto.sh`
Expected: error (`mosquitto.sh` no existe).

- [ ] **Step 4: Implementar `mosquitto.sh`**

`deploy/lib/mosquitto.sh`:

```bash
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

  run_root systemctl enable --now mosquitto
  run_root systemctl restart mosquitto
  ok "mosquitto configurado (usuario: $user)"
}
```

- [ ] **Step 5: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_mosquitto.sh`
Expected: `0 fallos`.

- [ ] **Step 6: Commit**

```bash
git add deploy/templates/mosquitto-rover.conf.tmpl deploy/lib/mosquitto.sh deploy/tests/test_mosquitto.sh
git commit -m "feat(deploy): mosquitto.sh (broker 1883+9001 con auth)"
```

---

## Task 9: `nginx.sh` + plantilla de site

**Files:**

- Create: `deploy/templates/nginx-rover.conf.tmpl`
- Create: `deploy/lib/nginx.sh`
- Create: `deploy/tests/test_nginx.sh`

- [ ] **Step 1: Crear la plantilla de nginx**

`deploy/templates/nginx-rover.conf.tmpl`:

```
server {
    listen 80;
    listen [::]:80;
    server_name ${SERVER_NAME};

    root ${WEBROOT};
    index index.html index.htm;

    location / {
        try_files $uri $uri/ =404;
    }

    # MQTT sobre WebSockets
    location /mqtt {
        proxy_pass http://127.0.0.1:9001;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 86400;
    }

    # Vídeo: viewers
    location /video {
        proxy_pass http://127.0.0.1:9002/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_read_timeout 86400;
    }

    # Vídeo: publisher (ESP32-CAM)
    location /publish {
        proxy_pass http://127.0.0.1:9002/publish;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_read_timeout 86400;
    }
}
```

- [ ] **Step 2: Escribir el test (falla primero)**

`deploy/tests/test_nginx.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/nginx.sh"

export SERVER_NAME="_" WEBROOT="/opt/rover"
out="$(render_template "$here/../templates/nginx-rover.conf.tmpl" '$SERVER_NAME $WEBROOT')"
assert_contains "$out" "root /opt/rover;" "root parametrizado"
assert_contains "$out" "location /mqtt" "proxy mqtt"
assert_contains "$out" "location /video" "proxy video"
assert_contains "$out" "location /publish" "proxy publish"
assert_contains "$out" 'try_files $uri' "literal nginx preservado"

DRY_RUN=1
out="$(install_nginx "_" "/opt/rover" 2>&1)"
assert_contains "$out" "apt-get install" "instala nginx"
assert_contains "$out" "sites-available/rover" "escribe site"
assert_contains "$out" "systemctl" "recarga servicio"
finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_nginx.sh`
Expected: error (`nginx.sh` no existe).

- [ ] **Step 4: Implementar `nginx.sh`**

`deploy/lib/nginx.sh`:

```bash
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
```

- [ ] **Step 5: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_nginx.sh`
Expected: `0 fallos`.

- [ ] **Step 6: Commit**

```bash
git add deploy/templates/nginx-rover.conf.tmpl deploy/lib/nginx.sh deploy/tests/test_nginx.sh
git commit -m "feat(deploy): nginx.sh (web + proxy /mqtt /video /publish)"
```

---

## Task 10: `video_proxy.sh` + `video_proxy.py` + servicio

**Files:**

- Create: `deploy/server/video_proxy.py` (copiado del servidor)
- Create: `deploy/templates/video_proxy.service.tmpl`
- Create: `deploy/lib/video_proxy.sh`
- Create: `deploy/tests/test_video_proxy.sh`

- [ ] **Step 1: Copiar `video_proxy.py` canónico**

Crear `deploy/server/video_proxy.py` con exactamente este contenido (traído de `tabserver`):

```python
#!/usr/bin/env python3
import asyncio
import websockets
import sys

PORT = 9002
subscribers = set()
publisher = None

async def handler(websocket, path):
    global publisher
    if path == "/publish":
        if publisher is not None:
            try:
                await publisher.close()
            except Exception:
                pass
        publisher = websocket
        print(f"[PROXY] ESP32-CAM conectado desde {websocket.remote_address}")
        frames_received = 0

        async def safe_send(sub, data):
            try:
                await sub.send(data)
            except Exception:
                subscribers.discard(sub)

        try:
            async for message in websocket:
                frames_received += 1
                if frames_received % 50 == 0:
                    print(f"[PROXY] Frames: {frames_received} | Viewers: {len(subscribers)}")
                if subscribers:
                    for sub in list(subscribers):
                        asyncio.create_task(safe_send(sub, message))
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            if publisher == websocket:
                publisher = None
            print(f"[PROXY] ESP32-CAM desconectado. Frames: {frames_received}")
    else:
        print(f"[PROXY] Viewer conectado desde {websocket.remote_address}")
        subscribers.add(websocket)
        try:
            async for _ in websocket:
                pass
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            subscribers.discard(websocket)
            print(f"[PROXY] Viewer desconectado ({websocket.remote_address})")

async def main():
    print(f"[PROXY] Iniciando en ws://0.0.0.0:{PORT} ...")
    async with websockets.serve(
        handler, "0.0.0.0", PORT,
        max_size=10 * 1024 * 1024,
        ping_interval=None, ping_timeout=None,
    ):
        print(f"[PROXY] Escuchando en el puerto {PORT}.")
        await asyncio.Future()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[PROXY] Detenido.")
```

- [ ] **Step 2: Crear la plantilla de servicio**

`deploy/templates/video_proxy.service.tmpl`:

```
[Unit]
Description=Mars Rover Video Relay Proxy
After=network.target
StartLimitIntervalSec=0

[Service]
ExecStart=/usr/bin/python3 ${PROXY_PATH}
WorkingDirectory=${PROXY_DIR}
StandardOutput=journal
StandardError=journal
Restart=always
RestartSec=5
User=${PROXY_USER}

[Install]
WantedBy=multi-user.target
```

- [ ] **Step 3: Escribir el test (falla primero)**

`deploy/tests/test_video_proxy.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/video_proxy.sh"

export PROXY_PATH="/opt/rover-proxy/video_proxy.py" PROXY_DIR="/opt/rover-proxy" PROXY_USER="rover"
out="$(render_template "$here/../templates/video_proxy.service.tmpl" '$PROXY_PATH $PROXY_DIR $PROXY_USER')"
assert_contains "$out" "ExecStart=/usr/bin/python3 /opt/rover-proxy/video_proxy.py" "execstart"
assert_contains "$out" "User=rover" "user"

DRY_RUN=1
out="$(install_video_proxy "rover" 2>&1)"
assert_contains "$out" "python3" "instala python"
assert_contains "$out" "video_proxy.service" "instala servicio"
assert_contains "$out" "systemctl" "habilita servicio"
finish
```

- [ ] **Step 4: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_video_proxy.sh`
Expected: error (`video_proxy.sh` no existe).

- [ ] **Step 5: Implementar `video_proxy.sh`**

`deploy/lib/video_proxy.sh`:

```bash
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
```

- [ ] **Step 6: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_video_proxy.sh`
Expected: `0 fallos`.

- [ ] **Step 7: Commit**

```bash
git add deploy/server/video_proxy.py deploy/templates/video_proxy.service.tmpl deploy/lib/video_proxy.sh deploy/tests/test_video_proxy.sh
git commit -m "feat(deploy): video_proxy.sh + relay canónico + servicio systemd"
```

---

## Task 11: `fail2ban.sh` (opcional)

**Files:**

- Create: `deploy/templates/fail2ban-sshd.local.tmpl`
- Create: `deploy/lib/fail2ban.sh`
- Create: `deploy/tests/test_fail2ban.sh`

- [ ] **Step 1: Crear la plantilla del jail**

`deploy/templates/fail2ban-sshd.local.tmpl`:

```
[sshd]
enabled = true
port = ssh
maxretry = 5
bantime = 1h
findtime = 10m
```

- [ ] **Step 2: Escribir el test (falla primero)**

`deploy/tests/test_fail2ban.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/fail2ban.sh"

DRY_RUN=1
out="$(install_fail2ban 2>&1)"
assert_contains "$out" "apt-get install" "instala fail2ban"
assert_contains "$out" "jail.d/sshd.local" "escribe jail"
assert_contains "$out" "systemctl" "habilita servicio"
finish
```

- [ ] **Step 3: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_fail2ban.sh`
Expected: error (`fail2ban.sh` no existe).

- [ ] **Step 4: Implementar `fail2ban.sh`**

`deploy/lib/fail2ban.sh`:

```bash
#!/usr/bin/env bash
# Endurecimiento SSH con fail2ban (componente OPCIONAL).
DEPLOY_DIR="${DEPLOY_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

install_fail2ban() {
  info "Instalando fail2ban (jail sshd)"
  apt_install fail2ban
  local tmp; tmp="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/fail2ban-sshd.local.tmpl" "" "$tmp"
  run_root install -m 0644 "$tmp" /etc/fail2ban/jail.d/sshd.local
  rm -f "$tmp"
  run_root systemctl enable --now fail2ban
  run_root systemctl restart fail2ban
  ok "fail2ban configurado"
}
```

- [ ] **Step 5: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_fail2ban.sh`
Expected: `0 fallos`.

- [ ] **Step 6: Commit**

```bash
git add deploy/templates/fail2ban-sshd.local.tmpl deploy/lib/fail2ban.sh deploy/tests/test_fail2ban.sh
git commit -m "feat(deploy): fail2ban.sh (jail sshd opcional)"
```

---

## Task 12: `ddns.sh` + plantillas de los 4 proveedores

**Files:**

- Create: `deploy/templates/ddns-duckdns.sh.tmpl`
- Create: `deploy/templates/ddns-freedns.sh.tmpl`
- Create: `deploy/templates/ddns-dynu.sh.tmpl`
- Create: `deploy/templates/ddns-ydns.sh.tmpl`
- Create: `deploy/templates/ddns-updater.service.tmpl`
- Create: `deploy/templates/ddns-updater.timer.tmpl`
- Create: `deploy/lib/ddns.sh`
- Create: `deploy/tests/test_ddns.sh`

- [ ] **Step 1: Crear las plantillas de updater (una por proveedor)**

`deploy/templates/ddns-duckdns.sh.tmpl`:

```
#!/usr/bin/env bash
# DuckDNS updater (generado)
curl -fsS "https://www.duckdns.org/update?domains=${DDNS_DOMAIN}&token=${DDNS_TOKEN}&ip=" -o /var/log/ddns-rover.log
```

`deploy/templates/ddns-freedns.sh.tmpl`:

```
#!/usr/bin/env bash
# FreeDNS (afraid.org) updater (generado): DDNS_TOKEN es la clave de la URL de actualización
curl -fsS "https://freedns.afraid.org/dynamic/update.php?${DDNS_TOKEN}" -o /var/log/ddns-rover.log
```

`deploy/templates/ddns-dynu.sh.tmpl`:

```
#!/usr/bin/env bash
# Dynu updater (generado)
curl -fsS "https://api.dynu.com/nic/update?hostname=${DDNS_DOMAIN}&password=${DDNS_TOKEN}" -o /var/log/ddns-rover.log
```

`deploy/templates/ddns-ydns.sh.tmpl`:

```
#!/usr/bin/env bash
# YDNS updater (generado): DDNS_TOKEN con formato usuario:contraseña
curl -fsS -u "${DDNS_TOKEN}" "https://ydns.io/api/v1/update/?host=${DDNS_DOMAIN}" -o /var/log/ddns-rover.log
```

- [ ] **Step 2: Crear las plantillas de service y timer**

`deploy/templates/ddns-updater.service.tmpl`:

```
[Unit]
Description=Rover DDNS updater
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/rover-ddns-update.sh
```

`deploy/templates/ddns-updater.timer.tmpl`:

```
[Unit]
Description=Ejecuta el updater DDNS del Rover cada 5 min

[Timer]
OnBootSec=1min
OnUnitActiveSec=5min
Persistent=true

[Install]
WantedBy=timers.target
```

- [ ] **Step 3: Escribir el test (falla primero)**

`deploy/tests/test_ddns.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/ddns.sh"

# Render por proveedor
export DDNS_DOMAIN="rover" DDNS_TOKEN="tok123"
out="$(render_template "$here/../templates/ddns-duckdns.sh.tmpl" '$DDNS_DOMAIN $DDNS_TOKEN')"
assert_contains "$out" "duckdns.org/update?domains=rover&token=tok123" "duckdns url"
out="$(render_template "$here/../templates/ddns-dynu.sh.tmpl" '$DDNS_DOMAIN $DDNS_TOKEN')"
assert_contains "$out" "api.dynu.com/nic/update?hostname=rover&password=tok123" "dynu url"

# La función selecciona la plantilla correcta del proveedor
assert_eq "ddns-duckdns.sh.tmpl" "$(ddns_template_for duckdns)" "tmpl duckdns"
assert_eq "ddns-ydns.sh.tmpl" "$(ddns_template_for ydns)" "tmpl ydns"

DRY_RUN=1
out="$(install_ddns duckdns "rover" "tok123" 2>&1)"
assert_contains "$out" "rover-ddns-update.sh" "instala updater"
assert_contains "$out" "ddns-updater.timer" "instala timer"
assert_contains "$out" "systemctl" "habilita timer"
finish
```

- [ ] **Step 4: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_ddns.sh`
Expected: error (`ddns.sh` no existe).

- [ ] **Step 5: Implementar `ddns.sh`**

`deploy/lib/ddns.sh`:

```bash
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
```

- [ ] **Step 6: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_ddns.sh`
Expected: `0 fallos`.

- [ ] **Step 7: Commit**

```bash
git add deploy/templates/ddns-*.tmpl deploy/lib/ddns.sh deploy/tests/test_ddns.sh
git commit -m "feat(deploy): ddns.sh (DuckDNS/FreeDNS/Dynu/YDNS + timer systemd)"
```

---

## Task 13: `cloudflare.sh` y `tls.sh`

**Files:**

- Create: `deploy/lib/cloudflare.sh`
- Create: `deploy/lib/tls.sh`
- Create: `deploy/tests/test_remote.sh`

- [ ] **Step 1: Escribir el test (falla primero)**

`deploy/tests/test_remote.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/cloudflare.sh"
. "$here/../lib/tls.sh"

DRY_RUN=1
out="$(install_cloudflared "TOKEN_DEMO" 2>&1)"
assert_contains "$out" "cloudflared" "instala cloudflared"
assert_contains "$out" "service install" "instala servicio con token"

out="$(install_tls "rover.duckdns.org" "yo@example.com" 2>&1)"
assert_contains "$out" "certbot" "usa certbot"
assert_contains "$out" "rover.duckdns.org" "incluye dominio"
finish
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_remote.sh`
Expected: error (`cloudflare.sh`/`tls.sh` no existen).

- [ ] **Step 3: Implementar `cloudflare.sh`**

`deploy/lib/cloudflare.sh`:

```bash
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
```

> Requiere un helper `run_sh` para snippets con tuberías. Añadirlo a `core.sh` en el Step 4.

- [ ] **Step 4: Añadir `run_sh` a `core.sh`**

Añadir al final de `deploy/lib/core.sh`:

```bash
# Ejecuta un snippet de shell (permite tuberías/redirecciones). Respeta dry-run.
run_sh() {
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ %s\n' "$*"
    return 0
  fi
  bash -c "$*"
}
```

- [ ] **Step 5: Implementar `tls.sh`**

`deploy/lib/tls.sh`:

```bash
#!/usr/bin/env bash
# TLS con certbot para el hostname DDNS (modo DDNS).

# install_tls HOSTNAME EMAIL
install_tls() {
  local host="$1" email="$2"
  info "Solicitando certificado TLS para $host (certbot)"
  apt_install certbot python3-certbot-nginx
  run_root certbot --nginx -d "$host" --non-interactive --agree-tos -m "$email" --redirect
  ok "TLS configurado para $host"
}
```

- [ ] **Step 6: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_remote.sh`
Expected: `0 fallos`.

- [ ] **Step 7: Commit**

```bash
git add deploy/lib/cloudflare.sh deploy/lib/tls.sh deploy/lib/core.sh deploy/tests/test_remote.sh
git commit -m "feat(deploy): cloudflare.sh (túnel por token) + tls.sh (certbot) + run_sh"
```

---

## Task 14: `deploy.sh` — orquestador

**Files:**

- Create: `deploy/deploy.sh`
- Create: `deploy/templates/rover-deploy.env.tmpl`
- Create: `deploy/tests/test_deploy.sh`
- Create: `deploy/tests/fixtures/answers-lan.env`

- [ ] **Step 1: Crear la plantilla del archivo de respuestas**

`deploy/templates/rover-deploy.env.tmpl`:

```
# Respuestas guardadas por deploy.sh (re-ejecución no interactiva). NO COMMITEAR.
MODE="${MODE}"
WEBROOT="${WEBROOT}"
REPO_URL="${REPO_URL}"
HOST_OR_IP="${HOST_OR_IP}"
MQTT_USER="${MQTT_USER}"
MQTT_PASS="${MQTT_PASS}"
WIFI_SSID1="${WIFI_SSID1}"
WIFI_PASS1="${WIFI_PASS1}"
WIFI_SSID2="${WIFI_SSID2}"
WIFI_PASS2="${WIFI_PASS2}"
WIFI_SSID3="${WIFI_SSID3}"
WIFI_PASS3="${WIFI_PASS3}"
DDNS_PROVIDER="${DDNS_PROVIDER}"
DDNS_TOKEN="${DDNS_TOKEN}"
CF_TOKEN="${CF_TOKEN}"
TLS_EMAIL="${TLS_EMAIL}"
RUN_USER="${RUN_USER}"
WITH_FAIL2BAN="${WITH_FAIL2BAN}"
```

- [ ] **Step 2: Crear una fixture de respuestas (modo LAN)**

`deploy/tests/fixtures/answers-lan.env`:

```
MODE="lan"
WEBROOT="/opt/rover"
REPO_URL="https://github.com/j4py/rover-perseverance"
HOST_OR_IP="192.168.1.50"
MQTT_USER="rover"
MQTT_PASS="secreto"
WIFI_SSID1="MiWifi"
WIFI_PASS1="clave123"
WIFI_SSID2=""
WIFI_PASS2=""
WIFI_SSID3=""
WIFI_PASS3=""
DDNS_PROVIDER=""
DDNS_TOKEN=""
CF_TOKEN=""
TLS_EMAIL=""
RUN_USER="rover"
WITH_FAIL2BAN="0"
```

- [ ] **Step 3: Escribir el test de orquestación (dry-run, falla primero)**

`deploy/tests/test_deploy.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"

# Ejecuta el orquestador en dry-run + no interactivo con la fixture LAN.
out="$(bash "$here/../deploy.sh" --dry-run --env "$here/fixtures/answers-lan.env" --skip-clone 2>&1)"

assert_contains "$out" "núcleo" "informa del núcleo obligatorio"
assert_contains "$out" "mosquitto" "planifica mosquitto"
assert_contains "$out" "nginx" "planifica nginx"
assert_contains "$out" "video_proxy" "planifica video_proxy"
assert_not_contains "$out" "cloudflared service install" "LAN no instala cloudflared"
assert_not_contains "$out" "certbot" "LAN no usa certbot"
assert_contains "$out" "RESUMEN" "muestra resumen final"
finish
```

- [ ] **Step 4: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_deploy.sh`
Expected: error (`deploy.sh` no existe).

- [ ] **Step 5: Implementar `deploy.sh`**

`deploy/deploy.sh`:

```bash
#!/usr/bin/env bash
# Orquestador interactivo de despliegue del Rover.
set -u
DEPLOY_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$DEPLOY_DIR/.." && pwd)"
export DEPLOY_DIR REPO_ROOT

# shellcheck source=/dev/null
for m in core ui template checks packages webgen firmware mosquitto nginx video_proxy fail2ban ddns cloudflare tls; do
  . "$DEPLOY_DIR/lib/$m.sh"
done

DEFAULT_REPO="https://github.com/j4py/rover-perseverance"
DEFAULT_WEBROOT="/opt/rover"
ENV_FILE=""
SKIP_CLONE=0

# --- args ---
while [ "$#" -gt 0 ]; do
  case "$1" in
    --dry-run)   DRY_RUN=1 ;;
    --env)       ENV_FILE="$2"; shift ;;
    --skip-clone) SKIP_CLONE=1 ;;
    -h|--help)
      cat <<'H'
Uso: deploy.sh [--dry-run] [--env ARCHIVO] [--skip-clone]
  --dry-run     No ejecuta nada, solo imprime las acciones.
  --env FILE    Usa respuestas guardadas (no interactivo).
  --skip-clone  No clona/actualiza el repo (para pruebas).
H
      exit 0 ;;
    *) die "Argumento desconocido: $1" ;;
  esac
  shift
done

# --- recoger configuración ---
if [ -n "$ENV_FILE" ]; then
  [ -f "$ENV_FILE" ] || die "No existe el archivo de respuestas: $ENV_FILE"
  # shellcheck source=/dev/null
  . "$ENV_FILE"
else
  info "== Configuración del despliegue del Rover =="
  MODE="$(ask_choice "Modo de despliegue" \
            "lan:Local / LAN (solo red local)" \
            "ddns:DDNS (DuckDNS/FreeDNS/Dynu/YDNS)" \
            "cloudflare:Cloudflare Tunnel")"
  REPO_URL="$(ask "URL del repositorio git" "$DEFAULT_REPO")"
  WEBROOT="$(ask "Ruta de despliegue web" "$DEFAULT_WEBROOT")"
  RUN_USER="$(ask "Usuario del sistema para los servicios" "${SUDO_USER:-$USER}")"

  MQTT_USER="$(ask "Usuario MQTT" "rover" validate_nonempty)"
  MQTT_PASS="$(ask_secret "Contraseña MQTT")"

  case "$MODE" in
    lan)
      HOST_OR_IP="$(ask "IP local del servidor" "$(hostname -I 2>/dev/null | awk '{print $1}')" validate_ip)"
      ;;
    ddns)
      DDNS_PROVIDER="$(ask_choice "Proveedor DDNS" "duckdns:DuckDNS" "freedns:FreeDNS" "dynu:Dynu" "ydns:YDNS")"
      HOST_OR_IP="$(ask "Hostname DDNS (p.ej. rover.duckdns.org)" "" validate_hostname)"
      DDNS_TOKEN="$(ask_secret "Token/clave del proveedor DDNS")"
      TLS_EMAIL="$(ask "Email para el certificado TLS (certbot)" "" validate_nonempty)"
      ;;
    cloudflare)
      HOST_OR_IP="$(ask "Dominio base (p.ej. midominio.com)" "" validate_hostname)"
      CF_TOKEN="$(ask_secret "Token del túnel Cloudflare")"
      ;;
  esac

  info "Credenciales WiFi del ESP32 (puedes dejar vacías las redes extra):"
  WIFI_SSID1="$(ask "WiFi SSID 1" "" validate_nonempty)"
  WIFI_PASS1="$(ask_secret "WiFi contraseña 1")"
  WIFI_SSID2="$(ask "WiFi SSID 2 (opcional)" "")"
  WIFI_PASS2=""; [ -n "$WIFI_SSID2" ] && WIFI_PASS2="$(ask_secret "WiFi contraseña 2")"
  WIFI_SSID3="$(ask "WiFi SSID 3 (opcional)" "")"
  WIFI_PASS3=""; [ -n "$WIFI_SSID3" ] && WIFI_PASS3="$(ask_secret "WiFi contraseña 3")"

  info "El NÚCLEO se instalará siempre: mosquitto, nginx, video_proxy."
  if ask_yesno "¿Instalar también fail2ban (endurecimiento SSH, opcional)?" n; then
    WITH_FAIL2BAN=1
  else
    WITH_FAIL2BAN=0
  fi
fi

# defaults para variables que puedan no venir del env
: "${DDNS_PROVIDER:=}"; : "${DDNS_TOKEN:=}"; : "${CF_TOKEN:=}"; : "${TLS_EMAIL:=}"
: "${WIFI_SSID2:=}"; : "${WIFI_PASS2:=}"; : "${WIFI_SSID3:=}"; : "${WIFI_PASS3:=}"
: "${WITH_FAIL2BAN:=0}"; : "${RUN_USER:=$USER}"

# --- preflight ---
is_ubuntu || warn "Esto está pensado para Ubuntu; continúo bajo tu responsabilidad."
ensure_sudo
check_internet || warn "Sin conexión detectada; la instalación de paquetes podría fallar."

# --- dependencias base ---
install_base

# --- ubicar el proyecto en WEBROOT (sin clonar dos veces) ---
if [ "$SKIP_CLONE" = "1" ]; then
  info "[--skip-clone] usando árbol actual: $REPO_ROOT"
  WEBROOT="$REPO_ROOT"
elif [ -d "$REPO_ROOT/.git" ]; then
  # Ya estamos dentro de un clon (p. ej. lanzado por bootstrap.sh): no re-clonar.
  if [ "$REPO_ROOT" = "$WEBROOT" ]; then
    info "El proyecto ya está en $WEBROOT"
  else
    info "Copiando el proyecto de $REPO_ROOT a $WEBROOT"
    run_root mkdir -p "$WEBROOT"
    run_root cp -a "$REPO_ROOT/." "$WEBROOT/"
  fi
else
  # Ejecución sin clon previo: descargar de git.
  if [ -d "$WEBROOT/.git" ]; then
    info "Actualizando repo en $WEBROOT"
    run_root git -C "$WEBROOT" pull --ff-only
  else
    info "Clonando $REPO_URL en $WEBROOT"
    run_root mkdir -p "$WEBROOT"
    run_root git clone "$REPO_URL" "$WEBROOT"
  fi
fi

# --- núcleo obligatorio ---
install_mosquitto "$MQTT_USER" "$MQTT_PASS"
# server_name: '_' en LAN, hostname/dominio en remoto
case "$MODE" in
  lan)        SRV_NAME="_" ;;
  ddns)       SRV_NAME="$HOST_OR_IP" ;;
  cloudflare) SRV_NAME="rover.$HOST_OR_IP" ;;
esac
install_nginx "$SRV_NAME" "$WEBROOT"
install_video_proxy "$RUN_USER"

# --- ligado al modo ---
case "$MODE" in
  ddns)
    install_ddns "$DDNS_PROVIDER" "$HOST_OR_IP" "$DDNS_TOKEN"
    install_tls "$HOST_OR_IP" "$TLS_EMAIL"
    ;;
  cloudflare)
    install_cloudflared "$CF_TOKEN"
    ;;
esac

# --- opcionales ---
[ "$WITH_FAIL2BAN" = "1" ] && install_fail2ban

# --- generación de configuración web + firmware ---
generate_mqtt_js "$DEPLOY_DIR/templates/mqtt.js.tmpl" "$WEBROOT/mqtt.js" "$MODE" "$HOST_OR_IP" "$MQTT_USER" "$MQTT_PASS"

# host MQTT para el firmware (nativo 1883): IP en LAN; hostname en ddns; direct.<dominio> en cloudflare
case "$MODE" in
  lan)        FW_MQTT_HOST="$HOST_OR_IP"; FW_VIDEO_HOST="$HOST_OR_IP" ;;
  ddns)       FW_MQTT_HOST="$HOST_OR_IP"; FW_VIDEO_HOST="$HOST_OR_IP" ;;
  cloudflare) FW_MQTT_HOST="direct.$HOST_OR_IP"; FW_VIDEO_HOST="video.$HOST_OR_IP" ;;
esac
for ino in "$REPO_ROOT/ESP32/Mars_Rover.ino" "$REPO_ROOT/ESP32/gps.ino" "$REPO_ROOT/ESP32/Camera_Marcelo_20fps_480x320.ino"; do
  [ -f "$ino" ] && fill_firmware_file "$ino" \
    "$WIFI_SSID1" "$WIFI_PASS1" "$WIFI_SSID2" "$WIFI_PASS2" "$WIFI_SSID3" "$WIFI_PASS3" \
    "$FW_MQTT_HOST" "$MQTT_USER" "$MQTT_PASS" "$FW_VIDEO_HOST"
done

# --- guardar respuestas ---
if [ -z "$ENV_FILE" ]; then
  export MODE WEBROOT REPO_URL HOST_OR_IP MQTT_USER MQTT_PASS WIFI_SSID1 WIFI_PASS1 WIFI_SSID2 WIFI_PASS2 WIFI_SSID3 WIFI_PASS3 DDNS_PROVIDER DDNS_TOKEN CF_TOKEN TLS_EMAIL RUN_USER WITH_FAIL2BAN
  render_to "$DEPLOY_DIR/templates/rover-deploy.env.tmpl" \
    '$MODE $WEBROOT $REPO_URL $HOST_OR_IP $MQTT_USER $MQTT_PASS $WIFI_SSID1 $WIFI_PASS1 $WIFI_SSID2 $WIFI_PASS2 $WIFI_SSID3 $WIFI_PASS3 $DDNS_PROVIDER $DDNS_TOKEN $CF_TOKEN $TLS_EMAIL $RUN_USER $WITH_FAIL2BAN' \
    "$DEPLOY_DIR/rover-deploy.env"
fi

# --- resumen ---
echo
ok "===== RESUMEN ====="
case "$MODE" in
  lan)        echo "  Web:   http://$HOST_OR_IP" ;;
  ddns)       echo "  Web:   https://$HOST_OR_IP" ;;
  cloudflare) echo "  Web:   https://rover.$HOST_OR_IP (según ingress de Cloudflare)" ;;
esac
echo "  Componentes núcleo: mosquitto + nginx + video_proxy"
[ "$MODE" = "ddns" ] && echo "  + DDNS ($DDNS_PROVIDER) + TLS"
[ "$MODE" = "cloudflare" ] && echo "  + cloudflared (configura el ingress en el dashboard)"
[ "$WITH_FAIL2BAN" = "1" ] && echo "  + fail2ban"
echo "  Firmware: credenciales escritas en ESP32/*.ino — compílalos y flashéalos con Arduino IDE."
echo "  IMPORTANTE: no commitees los .ino con credenciales (usa deploy/scripts/reset-firmware-secrets.sh)."
```

- [ ] **Step 6: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_deploy.sh`
Expected: `0 fallos`.

- [ ] **Step 7: Commit**

```bash
git add deploy/deploy.sh deploy/templates/rover-deploy.env.tmpl deploy/tests/test_deploy.sh deploy/tests/fixtures/answers-lan.env
git commit -m "feat(deploy): orquestador deploy.sh (modos, núcleo, generación, resumen)"
```

---

## Task 15: `bootstrap.sh` — instalador de una línea

**Files:**

- Create: `deploy/bootstrap.sh`
- Create: `deploy/tests/test_bootstrap.sh`

- [ ] **Step 1: Escribir el test (dry-run, falla primero)**

`deploy/tests/test_bootstrap.sh`:

```bash
#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
DRY_RUN=1 out="$(bash "$here/../bootstrap.sh" --dry-run 2>&1)"
assert_contains "$out" "git clone" "clona el repo"
assert_contains "$out" "deploy.sh" "lanza el orquestador"
finish
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `bash deploy/tests/test_bootstrap.sh`
Expected: error (`bootstrap.sh` no existe).

- [ ] **Step 3: Implementar `bootstrap.sh`**

`deploy/bootstrap.sh`:

```bash
#!/usr/bin/env bash
# Bootstrap de una línea: instala git, clona el repo y ejecuta el despliegue.
#   curl -fsSL https://raw.githubusercontent.com/j4py/rover-perseverance/main/deploy/bootstrap.sh | bash
set -eu

REPO_URL="${ROVER_REPO:-https://github.com/j4py/rover-perseverance}"
CLONE_DIR="${ROVER_DIR:-$HOME/rover-perseverance}"
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
```

- [ ] **Step 4: Ejecutar y verificar que pasa**

Run: `bash deploy/tests/test_bootstrap.sh`
Expected: `0 fallos`.

- [ ] **Step 5: Hacer ejecutables todos los scripts**

Run:

```bash
chmod +x deploy/bootstrap.sh deploy/deploy.sh deploy/scripts/*.sh deploy/tests/*.sh
git update-index --chmod=+x deploy/bootstrap.sh deploy/deploy.sh deploy/scripts/reset-firmware-secrets.sh
```

- [ ] **Step 6: Commit**

```bash
git add deploy/bootstrap.sh deploy/tests/test_bootstrap.sh
git commit -m "feat(deploy): bootstrap.sh (instalación de una línea)"
```

---

## Task 16: Plantillización de secretos en el repo

**Files:**

- Modify: `ESP32/Mars_Rover.ino`, `ESP32/gps.ino`, `ESP32/Camera_Marcelo_20fps_480x320.ino`,
  `ESP32/Camera_Marcelo_5fps_640x480.ino`, `ESP32/Camera_Marcelo/Camera_Marcelo.ino` (si existe),
  `ESP32/Camera_Marcelo_20fps_480x320 backup.ino`
- Modify: `mqtt.js` → contenido placeholder; Create: `mqtt.js.example`
- Modify: `.gitignore`
- Create: `deploy/server/cloudflare_ddns.py.example`
- Modify: `components/Dashboard/VideoHud.js` (leer `window.VIDEO_WS_URL`)
- Modify: HTML con credenciales (`docs/guia.html`, `sensores/gps.html`, `sensores/camara.html`,
  `software/infraestructura.html`, `hardware/electronica.html`)

- [ ] **Step 1: Sustituir credenciales reales por tokens en los `.ino`**

En cada `.ino` listado, reemplazar los valores reales por los tokens `CAMBIA_*`. Ejemplo para
`ESP32/Mars_Rover.ino` (líneas 28-39):

```c
const char *ssid_primary = "CAMBIA_WIFI_SSID";
const char *password_primary = "CAMBIA_WIFI_PASS";
const char *ssid_secondary = "CAMBIA_WIFI_SSID2";
const char *password_secondary = "CAMBIA_WIFI_PASS2";
const char *ssid_tertiary = "CAMBIA_WIFI_SSID3";
const char *password_tertiary = "CAMBIA_WIFI_PASS3";

const char *mqtt_server = "CAMBIA_MQTT_HOST";
const int mqtt_port = 1883;
const char *mqtt_user = "CAMBIA_MQTT_USER";
const char *mqtt_password = "CAMBIA_MQTT_PASS";
```

Para `gps.ino` igual (mismos nombres de variable). Para las cámaras, además del WiFi y MQTT,
`ws_host`:

```c
const char *ws_host = "CAMBIA_VIDEO_HOST";
```

> Atajo verificable: tras editar, `deploy/scripts/reset-firmware-secrets.sh` deja exactamente este
> estado. Puede usarse para normalizar todos los `.ino` de una vez:
> `bash deploy/scripts/reset-firmware-secrets.sh`

- [ ] **Step 2: Convertir `mqtt.js` en placeholder y crear `mqtt.js.example`**

`mqtt.js` (committeado, placeholders; será regenerado por el script):

```javascript
// Generado por deploy/deploy.sh. Este archivo versionado solo tiene placeholders.
window.MQTT_CONFIG = {
  HOST: "CAMBIA_MQTT_HOST",
  PORT: 443,
  PATH: "/mqtt",
  USERNAME: "CAMBIA_MQTT_USER",
  PASSWORD: "CAMBIA_MQTT_PASS",
};
window.VIDEO_WS_URL = "CAMBIA_VIDEO_WS_URL";
```

Copiar el mismo contenido a `mqtt.js.example` como referencia documentada.

- [ ] **Step 3: Actualizar `.gitignore`**

Añadir a `.gitignore`:

```
# ── Despliegue (config generada con secretos reales) ──
mqtt.js
deploy/rover-deploy.env
deploy/generated/
arduino_secrets.h
```

> `mqtt.js` pasa a ignorarse; como ya está trackeado, ejecutar:
> `git rm --cached mqtt.js` y luego commitear el `mqtt.js.example`. El `mqtt.js` con placeholders
> del Step 2 queda en disco pero fuera de git (el script lo sobrescribe en despliegue).

- [ ] **Step 4: Crear `cloudflare_ddns.py.example` plantillizado**

`deploy/server/cloudflare_ddns.py.example` (igual que el del servidor pero con placeholders):

```python
#!/usr/bin/env python3
# Actualiza un registro A en Cloudflare (nube gris). Rellena las constantes.
import urllib.request, json, sys

API_TOKEN = "CAMBIA_CLOUDFLARE_API_TOKEN"
ZONE_ID   = "CAMBIA_CLOUDFLARE_ZONE_ID"
RECORD_NAME = "direct.tu-dominio.example"

def get_public_ip():
    with urllib.request.urlopen("https://api.ipify.org?format=json", timeout=10) as r:
        return json.loads(r.read().decode())["ip"]

def cf_api(url, method="GET", data=None):
    req = urllib.request.Request(url, method=method)
    req.add_header("Authorization", f"Bearer {API_TOKEN}")
    req.add_header("Content-Type", "application/json")
    body = json.dumps(data).encode() if data else None
    with urllib.request.urlopen(req, data=body, timeout=10) as r:
        return json.loads(r.read().decode())

def main():
    ip = get_public_ip()
    res = cf_api(f"https://api.cloudflare.com/client/v4/zones/{ZONE_ID}/dns_records?name={RECORD_NAME}")
    if not res.get("success") or not res.get("result"):
        print("No se encontró el registro DNS; créalo primero."); sys.exit(1)
    rec = res["result"][0]
    if ip == rec["content"]:
        print(f"IP sin cambios ({ip})."); return
    upd = cf_api(f"https://api.cloudflare.com/client/v4/zones/{ZONE_ID}/dns_records/{rec['id']}",
                 method="PUT",
                 data={"type":"A","name":RECORD_NAME,"content":ip,"ttl":120,"proxied":False})
    print("IP actualizada." if upd.get("success") else f"Error: {upd}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Limpiar credenciales/dominios en los HTML de documentación**

Reemplazar en los HTML las apariciones de `••••••••` por `••••••••`, el usuario `oliver` por
`tu_usuario`, y los dominios `*.example.com` / `rover.example.com` por `rover.example.com`,
`mqtt.example.com`, `video.example.com`, `direct.example.com` según el contexto. Archivos:
`docs/guia.html`, `sensores/gps.html`, `sensores/camara.html`, `software/infraestructura.html`,
`hardware/electronica.html`.

> Estos HTML son documentación estática; el objetivo es que no muestren credenciales reales.

- [ ] **Step 5b: Hacer que `VideoHud.js` lea el endpoint generado**

En `components/Dashboard/VideoHud.js` (líneas 172-173), sustituir la decisión hardcodeada por
hostname (que además contiene el dominio real `video.example.com`) por la lectura de la config
generada con fallback a LAN:

```javascript
const WS_URL = window.VIDEO_WS_URL || `ws://${window.location.hostname}:9002`;
```

> Así el endpoint de vídeo lo fija `mqtt.js` (generado por `webgen.sh`) y desaparece el dominio real.

- [ ] **Step 6: Verificar que no quedan secretos reales**

Run:

```bash
grep -rIn -e "••••••••" -e "tu_password_wifi" -e "tu_password_wifi" -e "cfut_" -e "ghp_" \
  -e "example.com" -e "rover.example.com" \
  --exclude-dir=.git --exclude-dir=_notas --exclude="*.md" . || echo "LIMPIO: sin secretos/dominios reales"
```

Expected: `LIMPIO: sin secretos/dominios reales` (o solo coincidencias en `_notas/`, que son notas privadas no publicables — ver Step 7).

- [ ] **Step 7: Confirmar exclusión de `_notas/` del paquete público**

Verificar que `_notas/` (notas privadas con datos reales) esté en `.gitignore` o documentado como
no incluido en la publicación. Si no lo está y contiene secretos, añadirlo a `.gitignore` y
`git rm -r --cached _notas/`.

> DECISIÓN PARA EL EJECUTOR: confirmar con el usuario si `_notas/` se publica. Por defecto, NO se
> publica (contiene material de defensa del proyecto y datos reales).

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "chore(repo): plantillizar secretos (.ino, mqtt.js, docs, cloudflare_ddns) para publicación"
```

---

## Task 17: Reescritura del `README.md`

**Files:**

- Modify: `README.md`

- [ ] **Step 1: Reescribir la sección "Cómo poner en marcha el sistema"**

Sustituir la sección 4 actual por una nueva con: despliegue de una línea, tabla de modos,
paso a paso por modo y nota de firmware/seguridad. Contenido a insertar:

````markdown
## 4. Despliegue automatizado

Todo el lado servidor se instala con un script interactivo. En una máquina **Ubuntu** recién
instalada:

```bash
curl -fsSL https://raw.githubusercontent.com/j4py/rover-perseverance/main/deploy/bootstrap.sh | bash
```

O, de forma manual:

```bash
git clone https://github.com/j4py/rover-perseverance
cd rover-perseverance
./deploy/deploy.sh
```

El script pide los datos necesarios (modo, credenciales MQTT, redes WiFi del ESP32, dominios/tokens)
e instala **siempre** el núcleo (solo lo informa) y pregunta por los opcionales.

### Componentes

| Categoría            | Componentes                                          | Comportamiento                       |
| -------------------- | ---------------------------------------------------- | ------------------------------------ |
| Núcleo (obligatorio) | mosquitto, nginx, video_proxy                        | Se instala siempre; solo se informa. |
| Ligado al modo       | DDNS+TLS (modo DDNS) · cloudflared (modo Cloudflare) | Según el modo elegido.               |
| Opcional             | fail2ban                                             | Se pregunta sí/no.                   |

### Modos de despliegue

| Modo                                 | Para qué                                | Requisitos                                                                  |
| ------------------------------------ | --------------------------------------- | --------------------------------------------------------------------------- |
| **Local / LAN**                      | Usar el rover en la red local           | Solo la red local.                                                          |
| **DDNS** (DuckDNS/FreeDNS/Dynu/YDNS) | Acceso desde Internet con tu IP pública | Cuenta + token del proveedor, port-forward (80/443 y 1883), email para TLS. |
| **Cloudflare Tunnel**                | Acceso desde Internet sin abrir puertos | Cuenta Cloudflare + dominio + token de túnel.                               |

#### Paso a paso — Local / LAN

1. Ejecuta el script y elige **Local / LAN**.
2. Introduce la IP local del servidor, usuario/contraseña MQTT y las redes WiFi del ESP32.
3. Al terminar, abre `http://<IP>` en el navegador.

#### Paso a paso — DDNS

1. Crea una cuenta en DuckDNS/FreeDNS/Dynu/YDNS y un hostname; copia el token.
2. En el router, reenvía los puertos **80**, **443** y **1883** a la IP del servidor.
3. Ejecuta el script, elige **DDNS**, el proveedor, el hostname, el token y un email (TLS).
4. El script obtiene el certificado con certbot. Abre `https://<hostname>`.

#### Paso a paso — Cloudflare Tunnel

1. En el dashboard de Cloudflare crea un túnel y copia su **token**.
2. Configura el _ingress_ del túnel:
   `rover.<dominio>`→`http://localhost:80`, `mqtt.<dominio>`→`http://localhost:9001`,
   `direct.<dominio>`→`tcp://localhost:1883`, `video.<dominio>`→`http://localhost:9002`.
3. Ejecuta el script, elige **Cloudflare Tunnel** e introduce el dominio y el token.
4. Abre `https://rover.<dominio>`.

### Firmware del ESP32

El script **rellena** las credenciales (WiFi y MQTT) en `ESP32/*.ino` con los datos introducidos.
Tú solo tienes que **compilar y flashear** cada sketch con Arduino IDE (ver §3 para placas/librerías).

> ⚠️ Los `.ino` rellenos contienen tus credenciales: **no los commitees**. Para volver a
> placeholders usa: `bash deploy/scripts/reset-firmware-secrets.sh`.

### Seguridad

- El repositorio público no contiene credenciales: todo son placeholders `CAMBIA_*`.
- Si reutilizas este proyecto, **rota** cualquier credencial que hayas usado antes
  (contraseña MQTT, tokens de DDNS/Cloudflare, tokens de GitHub).
````

- [ ] **Step 2: Actualizar la estructura del proyecto (sección 2) y requisitos (sección 3)**

En la sección 2 (estructura), añadir la carpeta `deploy/`:

```
├── deploy/                     → Script de despliegue automatizado (bootstrap, deploy.sh, lib, templates)
```

En la sección 3 (requisitos del servidor), sustituir la lista manual por: _"El despliegue es
automático (ver §4). Requiere Ubuntu y acceso a Internet."_

- [ ] **Step 3: Verificar que el README no contiene dominios/credenciales reales**

Run:

```bash
grep -nE "sk1ll5|••••••••|proyectorover" README.md || echo "README LIMPIO"
```

Expected: `README LIMPIO`.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs(readme): despliegue automatizado (3 modos, paso a paso, firmware, seguridad)"
```

---

## Task 18: Verificación final

**Files:**

- (sin cambios de código; solo verificación y posible fix)

- [ ] **Step 1: Ejecutar toda la batería de pruebas**

Run: `bash deploy/tests/run_all.sh`
Expected: `TODAS LAS PRUEBAS OK`.

- [ ] **Step 2: Lint con shellcheck (si está disponible)**

Run:

```bash
command -v shellcheck >/dev/null 2>&1 && shellcheck deploy/*.sh deploy/lib/*.sh deploy/scripts/*.sh || echo "shellcheck no disponible; omitido"
```

Expected: sin warnings (o "omitido" en Windows). Corregir cualquier warning real.

- [ ] **Step 3: Dry-run completo de los 3 modos**

Crear fixtures `answers-ddns.env` y `answers-cloudflare.env` análogas a la LAN y ejecutar:

```bash
bash deploy/deploy.sh --dry-run --env deploy/tests/fixtures/answers-lan.env --skip-clone
bash deploy/deploy.sh --dry-run --env deploy/tests/fixtures/answers-ddns.env --skip-clone
bash deploy/deploy.sh --dry-run --env deploy/tests/fixtures/answers-cloudflare.env --skip-clone
```

Expected: cada uno imprime el plan correcto (DDNS incluye certbot+timer; Cloudflare incluye
`cloudflared service install`; LAN ninguno de los dos).

- [ ] **Step 4: Verificación final de ausencia de secretos**

Run (mismo grep del Task 16 Step 6):

```bash
grep -rIn -e "••••••••" -e "cfut_" -e "ghp_" -e "example.com" \
  --exclude-dir=.git --exclude-dir=_notas --exclude="*.md" . || echo "LIMPIO"
```

Expected: `LIMPIO`.

- [ ] **Step 5: Commit de fixtures añadidas y cierre**

```bash
git add deploy/tests/fixtures/answers-ddns.env deploy/tests/fixtures/answers-cloudflare.env
git commit -m "test(deploy): fixtures de dry-run para DDNS y Cloudflare + verificación final"
```

---

## Notas para el ejecutor

- **Verificación funcional real:** los tests cubren lógica pura + dry-run. La prueba de fuego
  (instalación real) debe hacerse en una **VM Ubuntu limpia**, no en el servidor `tabserver` de
  producción.
- **`_notas/`:** contiene material privado con datos reales; confirmar con el usuario antes de
  publicarlo (por defecto excluido de la publicación, ver Task 16 Step 7).
- **Rotación de credenciales:** recordar al usuario revocar el token GitHub `ghp_` del `.git/config`
  local y el token Cloudflare `cfut_` que aparecieron durante el diseño.

```

```
