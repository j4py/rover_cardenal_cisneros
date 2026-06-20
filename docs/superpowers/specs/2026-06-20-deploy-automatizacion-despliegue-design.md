# Automatización de despliegue del Rover Perseverance — Diseño

**Fecha:** 2026-06-20
**Autor:** Oliver Mignemi (con asistencia de Claude Code)
**Estado:** Aprobado para planificación

---

## 1. Objetivo

Permitir que **cualquier persona**, en una máquina Ubuntu recién instalada, despliegue el
sistema completo del Rover Perseverance ejecutando **un único script interactivo**. El script
pide todos los datos necesarios (modo de despliegue, IPs/dominios, credenciales MQTT, redes
WiFi del ESP32, permiso para instalar dependencias), clona el proyecto desde git, instala y
configura los servicios del servidor, genera la configuración de la web y rellena las
credenciales en los firmware del ESP32.

Como parte del trabajo, el repositorio se **prepara para publicarse al mundo**: se eliminan
todas las credenciales reales del código versionado y se sustituyen por plantillas/placeholders.

### Criterios de éxito

1. En una VM Ubuntu limpia, `curl … | bash` (o clonar + `deploy/deploy.sh`) deja el sistema
   funcional en modo LAN sin pasos manuales adicionales del lado servidor.
2. El script soporta 3 modos de despliegue: **Local/LAN**, **DDNS** (DuckDNS/FreeDNS/Dynu/YDNS)
   y **Cloudflare Tunnel**.
3. El script instala **sin preguntar** el núcleo obligatorio (solo lo informa) y pregunta sí/no
   únicamente por los componentes opcionales.
4. La web (`mqtt.js`) y los firmware (`.ino`) quedan configurados con los datos introducidos.
5. El repositorio publicado **no contiene** ninguna credencial real.
6. El `README.md` documenta requisitos y paso a paso de los 3 modos.

---

## 2. Estado actual (entorno de referencia `tabserver`, Ubuntu 24.04)

Todo el sistema corre **en una sola máquina**:

| Componente             | Detalle                                                                                                                                                         |
| ---------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **mosquitto**          | Listener `1883` MQTT nativo (rover/ESP32) + listener `9001` WebSockets (web). `allow_anonymous false`, `password_file /etc/mosquitto/passwd`, usuario `oliver`. |
| **nginx**              | Sirve la web desde `/var/www/html`; `location /mqtt` → `http://localhost:9001` (proxy WebSocket). Sitio certbot para `rover.example.com:8443`.             |
| **video_proxy.py**     | Relay WebSocket `:9002` (systemd `video_proxy.service`, usuario `oliver`). `/publish` = ESP32-CAM; cualquier otra ruta = viewer.                                |
| **cloudflared**        | Túnel por **token** (ingress en el dashboard de Cloudflare, no en disco). Dominios `rover/mqtt/direct/video.example.com`.                                        |
| **fail2ban**           | Jail SSH.                                                                                                                                                       |
| **cloudflare_ddns.py** | Actualiza el registro A `direct.example.com` vía API de Cloudflare (nube gris).                                                                                  |

### Mapa de credenciales/endpoints repartidos por el repo (a plantillizar)

- `mqtt.js`: `HOST: mqtt.example.com`, `PORT: 443`, `PATH: /mqtt`, `USERNAME: oliver`, `PASSWORD: ••••••••`.
- `components/Dashboard/VideoHud.js`: ya tiene fallback LAN (`ws://<host>:9002`) vs `wss://video.example.com`.
- Firmware (`Mars_Rover.ino`, `gps.ino`, cámaras): SSID/passwords WiFi reales, `mqtt_server=direct.example.com`,
  `mqtt_user=oliver`, `mqtt_password=••••••••`, cámaras con `ws_host=video.example.com`, `ws_path=/publish`.
- HTML de documentación (`docs/guia.html`, `sensores/*.html`, `software/*.html`, `hardware/electronica.html`):
  dominios reales y `oliver / ••••••••` a la vista.
- `cloudflare_ddns.py` (servidor): `API_TOKEN=cfut_…`, `ZONE_ID=…`.

### Secretos a rotar (fuera del repo, responsabilidad del usuario)

- Token GitHub `ghp_…` incrustado en `.git/config` local.
- Token API Cloudflare `cfut_…`.
- Contraseña MQTT `••••••••` y contraseñas WiFi reales.

---

## 3. Arquitectura del script

Orquestador **modular**: un script principal interactivo que invoca módulos pequeños, cada uno
responsable de un componente. Mantenible, testeable con `shellcheck`, extensible.

### Estructura de archivos (nueva carpeta `deploy/`)

```
deploy/
├── bootstrap.sh           # one-liner: instala git, clona el repo y lanza deploy.sh
├── deploy.sh              # orquestador: preflight → preguntas → instala → genera config → resumen
├── lib/
│   ├── ui.sh             # ask, ask_secret, ask_yesno, ask_choice, info/warn/ok (con validación + colores)
│   ├── checks.sh         # preflight: Ubuntu, sudo, conexión a internet, puertos en uso
│   ├── packages.sh       # apt update/install (pide permiso antes de instalar)
│   ├── mosquitto.sh      # broker: passwd, listeners 1883 + 9001 ws, allow_anonymous false
│   ├── nginx.sh          # site del rover, proxy /mqtt→9001 y /video→9002, TLS opcional
│   ├── video_proxy.sh    # copia server/video_proxy.py + instala servicio systemd
│   ├── ddns.sh           # DuckDNS / FreeDNS / Dynu / YDNS: updater + timer systemd
│   ├── cloudflare.sh     # cloudflared service install <token>
│   ├── tls.sh            # certbot --nginx para hostname DDNS
│   ├── fail2ban.sh       # jail sshd
│   ├── webgen.sh         # genera mqtt.js desde plantilla según el modo
│   └── firmware.sh       # rellena credenciales WiFi/MQTT en los .ino (in situ)
├── templates/
│   ├── mqtt.js.tmpl
│   ├── nginx-rover.conf.tmpl
│   ├── video_proxy.service.tmpl
│   ├── ddns-duckdns.sh.tmpl
│   ├── ddns-freedns.sh.tmpl
│   ├── ddns-dynu.sh.tmpl
│   ├── ddns-ydns.sh.tmpl
│   ├── ddns-updater.service.tmpl
│   ├── ddns-updater.timer.tmpl
│   └── rover-deploy.env.tmpl   # registro de respuestas (re-ejecución no interactiva)
├── scripts/
│   └── reset-firmware-secrets.sh   # restaura placeholders en los .ino (evita commitear claves)
└── server/                # artefactos canónicos (hoy solo viven en tabserver)
    ├── video_proxy.py
    └── cloudflare_ddns.py.example   # plantillizado (sin token real)
```

### 3.1. Núcleo obligatorio vs. componentes opcionales

El script distingue dos categorías. El **núcleo** es lo mínimo imprescindible para que el rover
sea usable; **siempre se instala** y solo se **informa** ("Se va a instalar: …"), sin preguntar.
Los **opcionales** sí se preguntan (sí/no). Los componentes **ligados al modo** no son una pregunta
aparte: se instalan porque el modo elegido los exige, y también se informa.

| Categoría                  | Componentes                                                                                                                                                           | Comportamiento                                          |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| **Núcleo (obligatorio)**   | dependencias base apt, clonado del proyecto, **mosquitto** (1883 + 9001 ws + auth), **nginx** (sirve web + proxy `/mqtt`), **video_proxy** (relay de cámara, systemd) | Siempre se instala. Solo se informa.                    |
| **Ligado al modo**         | Modo DDNS → updater DDNS + certbot/TLS · Modo Cloudflare → cloudflared                                                                                                | Se instala porque el modo lo requiere. Solo se informa. |
| **Opcional (se pregunta)** | **fail2ban** (endurecimiento SSH)                                                                                                                                     | Pregunta sí/no.                                         |

Justificación del núcleo: sin mosquitto no hay mensajería (control ni telemetría); sin nginx no se
sirve el Centro de Mando ni el proxy WebSocket de MQTT; sin video_proxy no hay cámara. Los tres
juntos dan un rover plenamente funcional en LAN. fail2ban es endurecimiento, no funcionalidad, por
lo que es lo único realmente opcional. En modo LAN solo se instala el núcleo (+ fail2ban si se acepta).

### Flujo de `deploy.sh`

1. **Preflight** (`checks.sh`): confirma Ubuntu (`/etc/os-release`), disponibilidad de `sudo`
   (lo pide una vez y mantiene vivo el timestamp), conexión a internet. Avisa de puertos ocupados.
2. **Recoger configuración** (interactivo, con defaults y validación):
   - Permiso para `apt install`.
   - Modo de despliegue: `1) Local/LAN  2) DDNS  3) Cloudflare Tunnel`.
   - Ruta de despliegue web (def. `/opt/rover`) y URL del repo git
     (def. `https://github.com/j4py/rover-perseverance`).
   - Credenciales MQTT (usuario + contraseña).
   - Datos según modo (ver §4).
   - Redes WiFi del ESP32 (1–3 SSID/contraseña).
   - **Componentes** (ver §3.1): el **núcleo obligatorio** no se pregunta (solo se informa
     "se va a instalar: …"); los **opcionales** se preguntan sí/no.
3. **Clonar/ubicar** el proyecto en la ruta de despliegue web (idempotente: `git clone` o `git pull`).
4. **Instalar y configurar** cada componente elegido (módulos `lib/*.sh`), idempotente.
5. **Generar configuración**: `webgen.sh` (mqtt.js) y `firmware.sh` (.ino).
6. **Guardar** las respuestas en `deploy/rover-deploy.env` (gitignored) para re-ejecución no interactiva.
7. **Resumen final**: URLs de acceso, recordatorio de flashear el firmware, y qué quedó sin instalar.

### Modos de invocación

- **One-liner (recomendado):** `curl -fsSL <raw>/deploy/bootstrap.sh | bash` → instala git, clona, ejecuta `deploy.sh`.
- **Manual:** `git clone <repo> && cd rover-perseverance && ./deploy/deploy.sh`.
- **No interactivo:** `./deploy/deploy.sh --env deploy/rover-deploy.env` (reusa respuestas guardadas).
- **Dry-run:** `./deploy/deploy.sh --dry-run` (imprime acciones sin ejecutarlas).

---

## 4. Los 3 modos de despliegue

| Modo                     | Configura                                                                      | Endpoints resultantes                                                                                  | Requisitos del usuario                                                                                |
| ------------------------ | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------- |
| **1. Local / LAN**       | mosquitto (1883+9001), nginx http por IP, video_proxy                          | Web `http://<IP>`, MQTT `ws://<IP>/mqtt`, vídeo `ws://<IP>:9002`                                       | Solo red local.                                                                                       |
| **2. DDNS**              | LAN + updater DDNS (timer systemd) + nginx con TLS (certbot) sobre el hostname | Web `https://<host>`, MQTT `wss://<host>/mqtt`, vídeo `wss://<host>/video`, ESP32 nativo `<host>:1883` | Cuenta en DuckDNS/FreeDNS/Dynu/YDNS + token, **port-forward del router** (80/443 y 1883), IP pública. |
| **3. Cloudflare Tunnel** | LAN + `cloudflared` por token (sin abrir puertos)                              | Web/MQTT/vídeo vía dominios del túnel                                                                  | Cuenta Cloudflare + dominio + token de túnel; ingress configurado en el dashboard.                    |

**DDNS — proveedores soportados** (cada uno con su plantilla de updater):

- **DuckDNS:** `https://www.duckdns.org/update?domains=<dominio>&token=<token>&ip=`
- **FreeDNS (afraid.org):** URL de actualización con hash/token.
- **Dynu:** `https://api.dynu.com/nic/update?hostname=<host>&password=<token>`
- **YDNS:** `https://ydns.io/api/v1/update/?host=<host>` con auth básica.

El updater se instala como `ddns-updater.service` + `ddns-updater.timer` (cada 5 min).

**Notas de red por modo:**

- En **DDNS**, nginx termina TLS (certbot) y hace proxy de WebSocket. Como hay un solo host, se usa
  enrutado por ruta hacia `video_proxy` (que distingue publisher de viewer por path):
  `location /publish`→`http://localhost:9002/publish` (ESP32-CAM) y `location /video`→`http://localhost:9002/`
  (viewers web). MQTT: `location /mqtt`→`9001`. La web servida por HTTPS exige `wss://` (por eso TLS
  es necesario en este modo). El rover usa MQTT nativo `<host>:1883` (puerto reenviado en el router).
- En **Cloudflare**, el túnel mapea (dashboard) `rover→:80`, `mqtt→:9001`, `direct→:1883`, `video→:9002`.
  El script solo instala `cloudflared` con el token; el ingress lo configura el usuario en Cloudflare
  (documentado en el README).

---

## 5. Generación de configuración (sin secretos en el repo)

### Web (`mqtt.js`)

- Se commitea `mqtt.js.example` con placeholders. `mqtt.js` real → **gitignored**, generado por `webgen.sh`
  desde `templates/mqtt.js.tmpl` según el modo:
  - LAN: `HOST=<IP>`, `PORT=80`, `PATH=/mqtt`.
  - DDNS/Cloudflare: `HOST=<host>`, `PORT=443`, `PATH=/mqtt`.
  - `USERNAME`/`PASSWORD` = credenciales MQTT introducidas.
- **Endpoint de vídeo:** `VideoHud.js` hoy decide la URL por hostname (`wss://video.example.com` vs
  `ws://<host>:9002`). Se generaliza para leer el endpoint desde la config generada (junto a `mqtt.js`):
  LAN→`ws://<IP>:9002`, DDNS→`wss://<host>/video`, Cloudflare→`wss://video.<dominio>`. La cámara
  (`ws_host`/`ws_path`) se rellena en consecuencia (`/publish` en DDNS, host del proxy en LAN/Cloudflare).

### Firmware (`.ino`) — rellenado in situ

- **Decisión:** el script sustituye **in situ** los valores en los `.ino` (lo que pidió el usuario:
  "los .ino se completan con las credenciales"). Se descarta `secrets.h` por la fragilidad de la
  estructura de carpetas de Arduino (sketch = carpeta con su `.ino`; los `.ino` sueltos en `ESP32/`
  complican un header compartido).
- En el repo, los `.ino` quedan con **placeholders**: `"TU_WIFI_SSID"`, `"TU_WIFI_PASSWORD"`,
  `"TU_MQTT_USER"`, `"TU_MQTT_PASSWORD"`, `"tu.servidor.example"`, `"tu.video.example"`.
- `firmware.sh` reemplaza los placeholders por los valores introducidos en:
  `Mars_Rover.ino`, `gps.ino`, y la cámara canónica `Camera_Marcelo_20fps_480x320.ino`
  (las variantes/backup se alinean o se documentan como secundarias).
- `deploy/scripts/reset-firmware-secrets.sh` restaura los placeholders, para no commitear claves por error.
- Riesgo aceptado: tras rellenar, los `.ino` modificados contienen secretos en el árbol de trabajo;
  se documenta en el README **no commitearlos** (y se ofrece el script de reset).

### Servidor

- **mosquitto:** `conf.d/rover.conf` (listeners 1883 + 9001 ws, `allow_anonymous false`) + `mosquitto_passwd`
  con el usuario/clave introducidos.
- **nginx:** site desde `templates/nginx-rover.conf.tmpl` (root = ruta de despliegue, `server_name` por
  IP o hostname, `location /mqtt`→9001, `/video`→9002, TLS opcional vía certbot).
- **video_proxy:** copia `server/video_proxy.py` + `templates/video_proxy.service.tmpl` a systemd, habilita.
- **DDNS:** updater del proveedor elegido + `.service`/`.timer`.
- **cloudflared:** `cloudflared service install <token>`.

---

## 6. Limpieza de secretos del repo (para publicar)

Sustituir por placeholders/plantillas en archivos versionados:

1. `mqtt.js` → `mqtt.js.example` (placeholders) + gitignore de `mqtt.js`.
2. `.ino` (WiFi + MQTT + dominios) → placeholders.
3. HTML de docs (`docs/guia.html`, `sensores/gps.html`, `sensores/camara.html`,
   `software/infraestructura.html`, `hardware/electronica.html`): reemplazar `••••••••`,
   usuario `oliver` y dominios reales por ejemplos genéricos (`rover.example.com`, `usuario`, `••••`).
4. `cloudflare_ddns.py` → `server/cloudflare_ddns.py.example` con `API_TOKEN`/`ZONE_ID` como placeholders.
5. `.gitignore`: añadir `mqtt.js`, `deploy/rover-deploy.env`, `arduino_secrets.h` (por si acaso).

El README incluirá una nota de **rotación obligatoria** de las credenciales reales históricas
(`ghp_…` de GitHub, `cfut_…` de Cloudflare, `••••••••`, WiFi).

---

## 7. Cambios en `README.md`

Reescribir la sección de puesta en marcha:

- **Despliegue rápido (1 línea)** con `curl … | bash`, y alternativa clonar + `deploy/deploy.sh`.
- **Tabla de los 3 modos** con requisitos.
- **Paso a paso por modo** (LAN, DDNS con los 4 proveedores, Cloudflare Tunnel), incluyendo
  port-forward e ingress donde aplique.
- **Firmware:** el script rellena las credenciales; el usuario compila y flashea con Arduino IDE.
- **Nota de seguridad:** rotación de credenciales, no commitear `.ino` rellenos.

Se mantiene el resto del README (descripción, estructura, manual de uso).

---

## 8. Pruebas

- `shellcheck` sobre todos los `deploy/**/*.sh` (sin warnings).
- `deploy.sh --dry-run`: imprime el plan de acciones sin ejecutar.
- **Idempotencia:** re-ejecutar el script no rompe una instalación existente (clone→pull, configs
  sobrescritas de forma segura, servicios `enable --now` repetible).
- Validación funcional en **VM/máquina limpia** (no se toca el `tabserver` de producción).
- Verificación de plantillización: `grep` del repo no debe encontrar `••••••••`, `cfut_`,
  `ghp_`, ni los SSID/dominios reales tras la limpieza.

---

## 9. Fuera de alcance (YAGNI)

- Auto-compilación/flasheo del ESP32 (el usuario compila y flashea).
- Configuración automática del ingress de Cloudflare (se documenta, no se automatiza vía API).
- Configuración del port-forward del router (se documenta).
- Soporte de distros distintas a Ubuntu.
- Despliegue multi-máquina (todo en una sola máquina, como el entorno actual).
