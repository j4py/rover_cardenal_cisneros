# Rover Perseverance — Proyecto Intermodular 2025/26

**IES Cardenal Cisneros · 2º SMR**  
Oliver Mignemi Martos · Marcelo A. Huayhua Moreno · Juan José Naranjo Lobato · Marc Oliva Marqués

---

## Índice

1. [Descripción General](#1-descripción-general)
2. [Estructura del Proyecto](#2-estructura-del-proyecto)
3. [Requisitos Previos](#3-requisitos-previos)
4. [Cómo poner en marcha el sistema](#4-cómo-poner-en-marcha-el-sistema)
5. [Manual de Uso Rápido](#5-manual-de-uso-rápido)

---

## 1. Descripción General

El **Rover Perseverance** es un vehículo todo-terreno de 6 ruedas motrices basado en el proyecto de [HowToMechatronics](https://howtomechatronics.com/projects/diy-mars-perseverance-rover-replica-with-arduino/), construido y programado íntegramente por alumnos de 2º SMR como proyecto intermodular del curso 2025/2026.

El sistema se divide en **tres ESP32 independientes**, cada uno con responsabilidad propia:

- **Mars_Rover.ino** — Firmware principal. Controla los 6 motores DC (tracción 6WD), los 4 servos de dirección (geometría Ackermann), el stepper NEMA 17 con driver TMC2209 (pan de cámara) y el servo de tilt. Se conecta al broker MQTT y publica telemetría de batería cada segundo. Soporta actualización OTA.
- **Camera_Marcelo.ino** — Firmware de cámara. Captura vídeo JPEG a ~20 FPS con la cámara OV2640 del módulo Freenove ESP32-S3 y lo transmite en binario vía WebSocket al proxy de vídeo del servidor.
- **gps.ino** — Firmware de navegación. Lee el módulo GPS NEO-6M (NMEA, UART2) y la brújula magnetométrica QMC5883 (I2C). Publica posición y heading en tiempo real y ejecuta un algoritmo de navegación autónoma hacia coordenadas objetivo enviadas por MQTT.

Toda la comunicación entre los ESP32 y la interfaz web pasa por un **broker MQTT Mosquitto** alojado en servidor propio, accesible globalmente a través de un **túnel Cloudflare** sin abrir puertos en el router.

La interfaz web (**Centro de Mando**) está construida con React 18 (UMD, sin bundler) y TailwindCSS vía CDN, sirviendo como panel de control desde cualquier navegador y dispositivo.

---

## 2. Estructura del Proyecto

```
Rover/
├── index.html                  → Portada: hero, specs, equipo
├── control.html                → Centro de Mando (joystick, cámara, GPS, telemetría)
├── test.html                   → Test Deck (prueba individual de motores y servos)
├── style.css                   → Estilos globales compartidos
├── script.js                   → Funciones JS compartidas (usado por conexiones.html)
│
├── components/
│   └── NavBar.js               → Mega-menú React unificado (v2.0)
│
├── hardware/
│   ├── traccion.html           → Tracción 6WD: motores DC, drivers DRV8871H
│   ├── electronica.html        → ESP32 y electrónica de control
│   ├── energia.html            → Sistema de alimentación LiPo 3S
│   └── conexiones.html         → Diagrama interactivo de pines ESP32
│
├── software/
│   ├── firmware.html           → Visor de los 3 .ino con syntax highlighting
│   ├── mqtt.html               → Lógica de mensajería MQTT y tópicos
│   ├── infraestructura.html    → Red, Nginx, Cloudflare, Fail2Ban, SSHFS
│   └── tecnologias.html        → Stack tecnológico usado
│
├── sensores/
│   ├── gps.html                → GPS NEO-6M, brújula QMC5883, navegación autónoma
│   ├── camara.html             → Cámara OV2640, gimbal, streaming WebSocket
│   └── lidar.html              → Sensor LiDAR (integración pendiente)
│
├── docs/
│   ├── guia.html               → Manual de uso (exportable a PDF)
│   ├── bom.html                → Bill of Materials por subsistema
│   ├── bitacora.html           → Registro cronológico del desarrollo
│   └── pruebas.html            → Casos de prueba con estado de superación
│
├── ESP32/
│   ├── Mars_Rover.ino          → Firmware principal (tracción, gimbal, MQTT, OTA)
│   ├── gps.ino                 → Firmware GPS y navegación autónoma
│   └── Camera_Marcelo/
│       └── Camera_Marcelo.ino  → Firmware cámara y streaming
│
├── imagenes/                   → Recursos gráficos del sitio
├── Diagrama/                   → Diagramas de conexiones
└── deploy/                     → Despliegue automatizado
    ├── bootstrap.sh            → Instalador de una línea (clona y lanza deploy.sh)
    ├── deploy.sh               → Orquestador interactivo (3 modos)
    ├── lib/                    → Módulos por componente (mosquitto, nginx, ddns, …)
    ├── templates/              → Plantillas de configuración
    ├── server/                 → video_proxy.py (relay de cámara) y ejemplos
    └── scripts/                → Utilidades (reset de credenciales del firmware)
```

---

## 3. Requisitos Previos

### Para el servidor

- **Ubuntu** (probado en 24.04) y conexión a Internet. **Nada más**: el script de
  despliegue (ver [§4](#4-despliegue-automatizado)) instala y configura automáticamente
  `mosquitto`, `nginx` y `video_proxy`, y —según el modo elegido— DDNS+TLS o `cloudflared`.

### Para los ESP32

- **Arduino IDE** 2.x con soporte para ESP32 (Espressif ESP32 Board Package)
- Librerías instaladas (Arduino Library Manager):
  - `AccelStepper`, `ServoEasing`, `PubSubClient`, `ArduinoJson` (Mars_Rover.ino)
  - `WebSocketsClient`, `esp_camera.h` (Camera_Marcelo.ino)
  - `TinyGPS++`, `MechaQMC5883` (gps.ino)

### Para el operador

- Navegador web moderno (Chrome 90+, Firefox 88+, Edge 90+, Safari 15+)
- Conexión a Internet

---

## 4. Despliegue automatizado

Todo el lado servidor se instala con un **script interactivo**. En una máquina **Ubuntu** recién instalada, en una sola línea:

```bash
curl -fsSL https://raw.githubusercontent.com/j4py/rover_cardenal_cisneros/main/deploy/bootstrap.sh | bash
```

O de forma manual:

```bash
git clone https://github.com/j4py/rover_cardenal_cisneros
cd rover_cardenal_cisneros
./deploy/deploy.sh
```

> **Si al lanzarlo te sale `Permission denied`**, es que los scripts no tienen permiso de ejecución. Dáselo con:
> ```bash
> chmod +x deploy/deploy.sh deploy/bootstrap.sh deploy/scripts/*.sh
> ```
> O bien ejecútalo directamente con su intérprete (no necesita el bit de ejecución):
> ```bash
> sudo bash deploy/deploy.sh
> ```

El script pregunta todos los datos necesarios (modo, credenciales MQTT, redes WiFi del ESP32, dominios/tokens), **instala siempre el núcleo** (solo lo informa) y **pregunta sí/no** por los opcionales. Genera `mqtt.js` y **rellena las credenciales en los `ESP32/*.ino`** con lo que introduzcas.

### Componentes

| Categoría                | Componentes                                            | Comportamiento                       |
| ------------------------ | ------------------------------------------------------ | ------------------------------------ |
| **Núcleo (obligatorio)** | mosquitto, nginx, video_proxy                          | Se instala siempre; solo se informa. |
| **Ligado al modo**       | DDNS + TLS (modo DDNS) · cloudflared (modo Cloudflare) | Según el modo elegido.               |
| **Opcional**             | fail2ban (endurecimiento SSH)                          | Se pregunta sí/no.                   |

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

### Flashear el firmware del ESP32

El script **rellena** las credenciales (WiFi y MQTT) en `ESP32/*.ino`. Tú solo tienes que **compilar y flashear** cada sketch con Arduino IDE:

1. Abre cada `.ino` desde `ESP32/` en Arduino IDE.
2. Selecciona la placa: `Mars_Rover.ino` y `gps.ino` → **ESP32 Dev Module**; cámara → **Freenove ESP32-S3 WROOM**.
3. Compila y sube (primera vez por USB; luego OTA).

> ⚠️ Los `.ino` rellenos contienen tus credenciales: **no los commitees**. Para volver a placeholders ejecuta: `bash deploy/scripts/reset-firmware-secrets.sh`.

### Seguridad

- El repositorio público **no contiene credenciales**: todo son placeholders `CAMBIA_*` / `example.com`.
- Si reutilizas este proyecto, **rota** cualquier credencial que hayas usado antes (contraseña MQTT, tokens de DDNS/Cloudflare, tokens de GitHub).

---

## 5. Manual de Uso Rápido

1. **Movimiento básico**: Usar el joystick táctil o los sliders del panel para mover el rover.

2. **Control del gimbal**: Los controles de gimbal en el panel mueven la cámara en pan (stepper NEMA 17, izquierda/derecha) y tilt (servo, arriba/abajo).

3. **Vídeo en tiempo real**: El stream de la cámara aparece en el panel central. Si la imagen no carga, verificar que el ESP32-CAM está encendido y con señal WiFi (ver sección de diagnóstico en `sensores/camara.html`).

4. **GPS y mapa**: La posición del rover se actualiza en el mapa Leaflet.js cada segundo mientras el módulo GPS tenga fix. En exteriores el fix tarda entre 2 y 60 segundos.

5. **Modo autónomo**: Hacer clic en el mapa sobre el punto de destino y pulsar **"Modo Auto"**. El rover navega hacia las coordenadas usando la brújula para corregir el heading. Se detiene automáticamente al llegar a menos de 2 m del objetivo. Pulsar **"Modo Manual"** o `Espacio` para cancelar en cualquier momento.

6. **Test Deck**: Acceder a `test.html` para probar cada motor y servo de forma individual sin mover el rover completo. Útil para diagnóstico y calibración.

7. **Actualización OTA**: Los firmware de `Mars_Rover.ino` y `gps.ino` soportan actualización inalámbrica vía `http://<IP_ESP32>/update` (formulario web).
