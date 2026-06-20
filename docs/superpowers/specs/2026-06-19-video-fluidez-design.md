# Diseño — Vídeo fluido en tiempo real para el Rover (quitar Cloudflare de la ruta de vídeo)

- **Fecha:** 2026-06-19
- **Estado:** Aprobado (pendiente de plan de implementación)
- **Checkpoint de rollback:** tag git `checkpoint/2026-06-19-pre-video-fix` + `/home/oliver/backups/2026-06-19-pre-video-fix/` (ver `ROLLBACK.md`)

## 1. Problema

La cámara del rover (Freenove ESP32-S3) tarda mucho en conectar y, cuando lo hace,
a menudo transmite a **1–2 fps**, lo que impide pilotar en tiempo real. Además hubo un
incidente de **disco lleno (30 GB de syslog en horas)** por el proxy de vídeo (ya mitigado
el 2026-06-18, pero el diseño de fan-out sigue sin backpressure).

## 2. Causa raíz (con evidencia)

| Síntoma           | Causa                                                                                                                                                                                                                                                | Evidencia                                                                                                                                           |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tarda en conectar | Handshake TLS + apertura de WebSocket contra el **edge de Cloudflare** y bajada por el **túnel cloudflared** hasta el origen                                                                                                                         | `video.example.com` → `104.21.70.138 / 172.67.223.210` (IPs Cloudflare). Nginx **no** tiene server block de vídeo: el stream depende 100% del túnel. |
| 1–2 fps           | Cloudflare bufferea/throttlea WebSocket binario de alta frecuencia (streaming de vídeo continuo va además contra su ToS). A eso se suma cifrar TLS de cada frame JPEG en el propio ESP32 (`beginSSL` + `sendBIN` síncrono), que bloquea el `loop()`. | Firmware `beginSSL(ws_host,443,...)`; `xclk` a 10 MHz                                                                                               |
| Riesgo de disco   | `video_proxy.py` crea una `asyncio.create_task` por viewer y frame, sin backpressure                                                                                                                                                                 | `video_proxy.py:47`                                                                                                                                 |

El control MQTT funciona porque es poco ancho de banda y sobrevive a Cloudflare; el vídeo no.

## 3. Topología (confirmada en vivo)

- **Rover:** roams — a veces WiFi local `sk1ll24` (192.168.1.x), a veces hotspot `M3` / fuera de casa.
- **Operador:** roams — a veces LAN, a veces remoto.
- **Puertos en IP pública `79.145.214.46`:** `9002` (proxy), `8443` (Nginx HTTPS), `1883`/`9001` (MQTT) **ABIERTOS**; `443`/`80` cerrados (Movistar). Hairpin NAT funciona en LAN.
- **Hostnames Cloudflare (túnel):** `rover.example.com`, `video.example.com`, `mqtt.example.com`.
- **Hostnames directos (DNS-only → IP real, port-forward):** `direct.example.com`, `rover.example.com`.
- **Nginx `rover.example.com:8443`** ya tiene `location /mqtt → :9001` (patrón a copiar).

## 4. Arquitectura objetivo

```
            ┌─ EN CASA (sk1ll24) ──►  ws://192.168.1.169:9002/publish?key=TOKEN   (LAN, plano)
 ESP32-CAM ─┤
            └─ FUERA  (M3/datos) ──►  ws://direct.example.com:9002/publish?key=TOKEN (IP pública, plano)
                                          │   sin Cloudflare · sin TLS en el ESP32
                                          ▼
                                   video_proxy.py :9002  (token + backpressure "último frame gana")
                                          │
            ┌─ Operador en LAN ────►  ws://192.168.1.169:9002/?key=TOKEN
 Navegador ─┤
            └─ Operador remoto ───►  wss://rover.example.com:8443/video?key=TOKEN  (Nginx TLS, sin CF)
```

**Principios:**

- Cloudflare desaparece de la ruta de **vídeo** (cámara y socket del navegador).
- El ESP32 nunca cifra; el TLS lo termina Nginx solo en el salto remoto del navegador.
- La **página** sigue sirviéndose por `https://rover.example.com/control.html` (Cloudflare): es carga única y poco ancho de banda. Una vez cargada, el navegador abre el WS de vídeo a otro host (cross-origin `wss` permitido, sin mixed-content, cert válido de `rover.example.com`).
- El control **MQTT se deja como está**.

## 5. Unidades de cambio (4, independientes)

### 5.1 Firmware ESP32 — `ESP32/Camera_Marcelo_20fps_480x320.ino` (OTA `cam-20fps-hvga.local`)

- **Transporte:** `webSocket.begin(host, 9002, "/publish?key=TOKEN")` plano (sustituye `beginSSL`).
- **Destino según SSID:** si `WiFi.SSID() == "tu_ssid_wifi"` → host `192.168.1.169`; si no → `direct.example.com`.
- **Fluidez/latencia:** `xclk_freq_hz` 10→**20 MHz** (probar; fallback 16/10 si hay artefactos); `config.grab_mode = CAMERA_GRAB_LATEST` (enviar siempre el frame más nuevo, descartar viejos).
- **Loop:** no llamar `wifiMulti.run()` cada iteración; comprobar `WiFi.status()` y reconectar solo al caer. Saltar captura si `sendBIN` devuelve `false` (buffer lleno). `setReconnectInterval` 5000→**2500**.

### 5.2 Proxy — `/home/oliver/video_proxy.py`

- **Token:** parsear path+query; extraer `key`; rechazar (cerrar) conexión sin token válido, tanto en `/publish` como en visores.
- **Backpressure:** sustituir `asyncio.create_task(safe_send())` por frame por una **`asyncio.Queue(maxsize=1)` por subscriber** + task emisora dedicada con política _último frame gana_ (si la cola está llena, se descarta el viejo y se mete el nuevo). Elimina la acumulación de tasks/memoria y reduce jitter. Mantener `ping_interval=None` y `StandardOutput=null`.

### 5.3 Nginx — bloque `rover.example.com:8443`

```nginx
location /video {
    proxy_pass http://localhost:9002;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "Upgrade";
    proxy_set_header Host $host;
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;
}
```

Da al navegador remoto una ruta `wss` cifrada sin Cloudflare. La query `?key=TOKEN` se conserva hacia el upstream.

### 5.4 Visor — `components/Dashboard/VideoHud.js`

```js
const onLan = /^192\.168\./.test(window.location.hostname);
const WS_URL = onLan
  ? `ws://${window.location.hostname}:9002/?key=${VIDEO_TOKEN}`
  : `wss://rover.example.com:8443/video?key=${VIDEO_TOKEN}`;
```

Mantener el backoff exponencial actual.

## 6. Seguridad

- **Token en la ruta** (decisión del usuario). Protege bien al _publisher_ (secreto solo en firmware). Para _viewers_ es **best-effort**: el token viaja en el JS público; sirve para frenar escáneres, no es secreto fuerte.
- `9002` plano queda expuesto a internet; lo mitiga el token. Opción futura: restringir por firewall.

## 7. Ejecución por fases

- **Fase 0 (validación):** solo repointar la cámara a `9002` directo (5.1 mínimo) y medir fps por OTA. Confirma la mejora antes de invertir en el resto.
- **Fase 1:** token + backpressure en proxy (5.2), `location /video` en Nginx (5.3), visor (5.4).
- **Fase 2:** tuning de cámara (xclk/grab/loop) y medición final.
- **Fase 3 (opcional):** si el hotspot M3 sigue cortando, evaluar transporte UDP/WebRTC (evita head-of-line blocking de TCP).

## 8. Criterios de éxito

- Tiempo de arranque→primer frame en LAN < ~5 s.
- fps sostenido: LAN ≥ 15 fps; remoto best-effort pero fluido (sin caídas a 1–2 fps).
- Verificable en DevTools: la URL del WS de vídeo **no** es `video.example.com`.
- Sin regresión de disco/logs (syslog estable, backpressure activo).

## 9. Riesgos / decisiones abiertas

- `xclk` 20 MHz puede dar artefactos en algunas placas Freenove → "probar y medir", fallback a 16/10.
- Token de viewer no es secreto fuerte (asumido).
- La página sigue dependiendo de Cloudflare para `https` en 443; si algún día se quita el naranja, habrá que servirla por `:8443`.

## 10. Rollback

Ver `/home/oliver/backups/2026-06-19-pre-video-fix/ROLLBACK.md`. Resumen: `git checkout checkpoint/2026-06-19-pre-video-fix -- <ficheros>`, restaurar `video_proxy.py` y Nginx desde las copias, y re-flashear el `.bin` exportado.
