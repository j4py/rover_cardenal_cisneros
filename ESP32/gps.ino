#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <MechaQMC5883.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <TinyGPS++.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Wire.h>


// ==========================================
// 1. RED Y CREDENCIALES
// ==========================================
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

// ==========================================
// 2. HARDWARE (PINES OFICIALES)
// ==========================================
#define RXD2 17    // GPS TX
#define TXD2 16    // GPS RX
#define I2C_SDA 21 // Brújula SDA
#define I2C_SCL 22 // Brújula SCL
#define HEADING_OFFSET 280.0
#define SPIN_THRESHOLD 12.0 // Grados de error max antes de girar en punto

WiFiClient espClient;
WiFiMulti wifiMulti;
PubSubClient client(espClient);
WebServer httpServer(80);
TinyGPSPlus gps;
HardwareSerial neogps(2);
MechaQMC5883 qmc;

float current_heading = 0.0;
float filt_sin_h = 0.0;
float filt_cos_h = 1.0;

Preferences prefs;
float mag_x_offset = 0.0f;
float mag_y_offset = 0.0f;
bool calibrating = false;
int cal_x_min = 32767, cal_x_max = -32768;
int cal_y_min = 32767, cal_y_max = -32768;

//==========================================
// 3. VARIABLES DE NAVEGACIÓN Y TIMERS
// ==========================================
unsigned long lastTelemetryMsg = 0;
unsigned long lastStatusMsg = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeadingUpdate = 0;

#define TELEMETRY_INTERVAL 1000 // Envío a la web cada 1000 milisegundos
#define STATUS_INTERVAL 5000    // Señal de vida cada 5 segundos

bool modo_autonomo = false;
double target_lat = 0.0;
double target_lon = 0.0;
bool i2c_scan_done = false;
int dbg_x_raw = 0, dbg_y_raw = 0;
float dbg_heading_raw = 0.0f;

// ==========================================
// CALIBRACION DE BRUJULA (INALAMBRICA)
// ==========================================
void loadCalibration() {
  prefs.begin("compass", true);
  mag_x_offset = prefs.getFloat("x_off", 0.0f);
  mag_y_offset = prefs.getFloat("y_off", 0.0f);
  prefs.end();
  Serial.printf("[CAL] Offsets cargados: X=%.1f  Y=%.1f\n", mag_x_offset,
                mag_y_offset);
}

void startCalibration() {
  modo_autonomo = false;
  calibrating = true;
  cal_x_min = 32767;
  cal_x_max = -32768;
  cal_y_min = 32767;
  cal_y_max = -32768;
  Serial.println("[CAL] Iniciada. Gira el rover 360 grados varias veces...");
  client.publish("rover/calibration", "{\"status\":\"INICIADA\"}");
}

void stopCalibration() {
  calibrating = false;
  mag_x_offset = (cal_x_max + cal_x_min) / 2.0f;
  mag_y_offset = (cal_y_max + cal_y_min) / 2.0f;
  prefs.begin("compass", false);
  prefs.putFloat("x_off", mag_x_offset);
  prefs.putFloat("y_off", mag_y_offset);
  prefs.end();
  StaticJsonDocument<192> doc;
  doc["status"] = "COMPLETA";
  doc["x_offset"] = mag_x_offset;
  doc["y_offset"] = mag_y_offset;
  doc["x_min"] = cal_x_min;
  doc["x_max"] = cal_x_max;
  doc["y_min"] = cal_y_min;
  doc["y_max"] = cal_y_max;
  char buf[192];
  serializeJson(doc, buf);
  client.publish("rover/calibration", buf);
  Serial.printf("[CAL] Completa. X_off=%.1f  Y_off=%.1f (guardado en flash)\n",
                mag_x_offset, mag_y_offset);
}

// ==========================================
// CALLBACK MQTT (ESCUCHAR ÓRDENES DE LA WEB)
// ==========================================
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String topicStr = String(topic);
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++)
    mensaje += (char)payload[i];

  // CHIVATO: Mostrar todo lo que entra desde la web
  Serial.print("\n[<<< RECIBIDO WEB] Topic: ");
  Serial.print(topicStr);
  Serial.print(" | Mensaje: ");
  Serial.println(mensaje);

  if (topicStr == "rover/mode") {
    // Normalizamos a mayúsculas para aceptar lo que envía la web (AUTO/MANUAL),
    // la documentación (auto/manual) y variantes heredadas
    // (GPS/Autonomo/Remoto).
    String modo = mensaje;
    modo.toUpperCase();
    if (modo.indexOf("AUTO") >= 0 || modo.indexOf("GPS") >= 0) {
      modo_autonomo = true;
      Serial.println(
          "[MODO] AUTÓNOMO/GPS ACTIVADO - Tomando control de motores.");
    } else if (modo.indexOf("MANUAL") >= 0 || modo.indexOf("REMOTO") >= 0) {
      modo_autonomo = false;
      Serial.println(
          "[MODO] MANUAL/REMOTO ACTIVADO - Cediendo control de motores al "
          "Joystick.");
    }
  } else if (topicStr == "rover/cmd") {
    if (mensaje.indexOf("STOP") >= 0) {
      modo_autonomo = false;
      Serial.println("[E-STOP] ¡PARADA DE EMERGENCIA RECIBIDA DESDE LA WEB!");
    } else if (mensaje.indexOf("CALIBRATE_START") >= 0) {
      startCalibration();
    } else if (mensaje.indexOf("CALIBRATE_END") >= 0) {
      stopCalibration();
    }
  } else if (topicStr == "rover/target") {
    StaticJsonDocument<200> docDestino;
    DeserializationError err = deserializeJson(docDestino, mensaje);

    // LA SOLUCIÓN: Leer como Array en lugar de Objeto
    if (!err && docDestino.is<JsonArray>()) {
      target_lat = docDestino[0]; // Latitud
      target_lon = docDestino[1]; // Longitud
      Serial.print("[TARGET] Nuevo destino fijado -> Lat: ");
      Serial.print(target_lat, 6);
      Serial.print(" | Lon: ");
      Serial.println(target_lon, 6);
    } else {
      Serial.println(
          "[ERROR] No se pudo leer el Array de coordenadas desde la web.");
    }
  }
}

// ==========================================
// FUNCIONES DE RED
// ==========================================
void setup_wifi() {
  delay(10);
  Serial.println("\nInicializando WiFi Multi...");
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP(ssid_primary, password_primary);
  wifiMulti.addAP(ssid_secondary, password_secondary);
  wifiMulti.addAP(ssid_tertiary, password_tertiary);

  Serial.println("Conectando a la mejor red disponible...");

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[OK] WiFi Conectado.");
  Serial.print("SSID actual: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  Serial.print("Intentando conexión MQTT...");
  String clientId = "RoverBrain-" + String(random(0xffff), HEX);
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
    Serial.println(" [OK]");
    client.subscribe("rover/mode");
    client.subscribe("rover/cmd");
    client.subscribe("rover/target");
    if (!i2c_scan_done) {
      scanI2C();
      i2c_scan_done = true;
    }
  } else {
    Serial.println(" [FALLO]");
  }
}

// ==========================================
// FILTRO Y LECTURA DE BRUJULA
// ==========================================
void updateHeading() {
  int x_mag, y_mag, z_mag;
  qmc.read(&x_mag, &y_mag, &z_mag);
  if (calibrating) {
    if (x_mag < cal_x_min)
      cal_x_min = x_mag;
    if (x_mag > cal_x_max)
      cal_x_max = x_mag;
    if (y_mag < cal_y_min)
      cal_y_min = y_mag;
    if (y_mag > cal_y_max)
      cal_y_max = y_mag;
  }
  dbg_x_raw = x_mag;
  dbg_y_raw = y_mag;
  float rad =
      atan2f(-((float)y_mag - mag_y_offset), (float)x_mag - mag_x_offset);
  const float alpha = 0.3f;
  filt_sin_h = alpha * sinf(rad) + (1.0f - alpha) * filt_sin_h;
  filt_cos_h = alpha * cosf(rad) + (1.0f - alpha) * filt_cos_h;
  float h = atan2f(filt_sin_h, filt_cos_h) * 180.0f / PI;
  if (h < 0.0f)
    h += 360.0f;
  dbg_heading_raw = h;
  h += HEADING_OFFSET;
  if (h < 0.0f)
    h += 360.0f;
  if (h >= 360.0f)
    h -= 360.0f;
  current_heading = h;
}

// ==========================================
// CEREBRO DE NAVEGACIÓN AUTÓNOMA
// ==========================================
void navegarAuto() {
  // BLOQUEO DE SEGURIDAD: Si estamos en modo Manual o no hay señal GPS, salimos
  // y NO movemos motores.
  if (!modo_autonomo || !gps.location.isValid() ||
      (target_lat == 0.0 && target_lon == 0.0))
    return;

  double current_lat = gps.location.lat();
  double current_lon = gps.location.lng();

  // Matemáticas de navegación: Se recalculan en cada vuelta (cada segundo)
  double distancia = TinyGPSPlus::distanceBetween(current_lat, current_lon,
                                                  target_lat, target_lon);
  double bearing =
      TinyGPSPlus::courseTo(current_lat, current_lon, target_lat, target_lon);

  float error_grados = bearing - current_heading;
  if (error_grados > 180.0)
    error_grados -= 360.0;
  if (error_grados < -180.0)
    error_grados += 360.0;

  float joy_x = 0.0;
  float joy_y = 0.0;

  if (distancia < 2.0) { // Si llegamos al margen de error de 2 metros
    // IMPORTANTE: NO apagamos el modo_autonomo. Se queda a la espera de un
    // nuevo clic en el mapa.
    Serial.println("[NAVEGACIÓN] ¡Destino alcanzado! Deteniendo motores y a la "
                   "espera de un nuevo destino...");
    joy_x = 0.0;
    joy_y = 0.0;

  } else if (abs(error_grados) > SPIN_THRESHOLD) {
    joy_x = (error_grados > 0) ? 1.0 : -1.0;
    joy_y = 0.0;
  } else {
    // Corrección proporcional al error mientras avanza (sin spin)
    joy_x = (error_grados / SPIN_THRESHOLD) * 0.5f;
    joy_y = 0.6f;
  }

  // Chivato para ver en tiempo real cómo tu ESP32 calcula los ángulos
  const char *fase = (distancia < 2.0)                      ? "LLEGADO"
                     : (abs(error_grados) > SPIN_THRESHOLD) ? "SPIN"
                                                            : "RECTO";
  Serial.print("[AUTO] ");
  Serial.print(fase);
  Serial.print(" | Dist: ");
  Serial.print(distancia);
  Serial.print("m | Error: ");
  Serial.print(error_grados);
  Serial.print("deg | X: ");
  Serial.print(joy_x);
  Serial.print(" Y: ");
  Serial.println(joy_y);

  bool is_spin = (distancia >= 2.0) && (abs(error_grados) > SPIN_THRESHOLD);

  StaticJsonDocument<100> docMove;
  docMove["x"] = joy_x;
  docMove["y"] = joy_y;
  docMove["spin"] = is_spin;
  char bufferMove[100];
  serializeJson(docMove, bufferMove);

  // Publicar la orden de movimiento al ESP32 de Oliver
  client.publish("rover/move", bufferMove);
}

// ==========================================
// ENVÍO DE DATOS A LA WEB Y TERMINAL
// ==========================================
void enviarTelemetria() {
  StaticJsonDocument<256> doc;

  doc["lat"] = gps.location.isValid() ? gps.location.lat() : 0.0;
  doc["lon"] = gps.location.isValid() ? gps.location.lng() : 0.0;
  doc["heading"] = (int)current_heading;
  doc["auto"] =
      modo_autonomo; // Estado REAL del modo autónomo para el dashboard
  doc["ip_gps"] = WiFi.localIP().toString();

  const char *gps_state_val;
  if (gps.charsProcessed() == 0) {
    gps_state_val = "OFF";
  } else if (!gps.location.isValid()) {
    gps_state_val = "SEARCHING";
  } else {
    gps_state_val = "FIX";
  }
  doc["gps_state"] = gps_state_val;
  doc["gps_sats"] = (int)gps.satellites.value();
  doc["x_raw"] = dbg_x_raw;
  doc["y_raw"] = dbg_y_raw;
  doc["h_raw"] = (int)dbg_heading_raw;
  doc["x_off"] = (int)mag_x_offset;
  doc["y_off"] = (int)mag_y_offset;

  char buffer[256];
  serializeJson(doc, buffer);

  // CHIVATO: Mostrar JSON de telemetría enviado
  Serial.print("\n[>>> ENVÍO WEB] Topic: rover/telemetry | Datos: ");
  Serial.println(buffer);

  client.publish("rover/telemetry", buffer);

  // CHIVATO: Resumen local limpio de las mediciones
  Serial.print("[Info Local] Satélites: ");
  Serial.print(gps.satellites.value());
  Serial.print(" | Rumbo: ");
  Serial.print(current_heading, 1);
  Serial.print("°");
  if (gps.location.isValid()) {
    Serial.print(" | Posición: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.print(gps.location.lng(), 6);
  } else {
    Serial.print(" | Buscando señal GPS...");
  }

  if (target_lat != 0.0) {
    Serial.print("  >>>  Destino fijado: ");
    Serial.print(target_lat, 6);
    Serial.print(", ");
    Serial.println(target_lon, 6);
  } else {
    Serial.println("  >>>  Sin destino fijado.");
  }
}

void enviarStatus() {
  bool gps_fix = gps.location.isValid();
  StaticJsonDocument<50> doc;
  doc["gps"] = gps_fix;
  char buffer[50];
  serializeJson(doc, buffer);

  client.publish("rover/status", buffer);
}

// ==========================================
// ESCANER I2C (debug via MQTT)
// ==========================================
void scanI2C() {
  String result = "{\"devices\":[";
  bool first = true;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (!first)
        result += ",";
      result += "{\"addr\":\"0x";
      if (addr < 16)
        result += "0";
      result += String(addr, HEX) + "\"";
      if (addr == 0x0D)
        result += ",\"chip\":\"QMC5883L\"";
      if (addr == 0x1E)
        result += ",\"chip\":\"HMC5883L\"";
      if (addr == 0x42 || addr == 0x43)
        result += ",\"chip\":\"GPS_ublox\"";
      result += "}";
      first = false;
    }
  }
  result += "]}";
  client.publish("rover/debug", result.c_str());
  Serial.println("[I2C] " + result);
}

// ==========================================
// MAIN LOOP
// ==========================================
void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);
  qmc.init();
  loadCalibration();

  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // Configurar e iniciar Arduino OTA
  ArduinoOTA.setHostname("Perseverance-GPS-ESP32");
  ArduinoOTA.onStart([]() {
    String type =
        (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Inicio de actualizacion OTA (" + type + ")...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nActualizacion OTA completada con exito. Reiniciando...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Fallo de autenticacion");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Fallo al iniciar");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Fallo de conexion");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Fallo al recibir");
    else if (error == OTA_END_ERROR)
      Serial.println("Fallo al finalizar");
  });
  ArduinoOTA.begin();

  // Configurar e iniciar Web OTA
  httpServer.on("/update", HTTP_GET, []() {
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>GPS OTA Update</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600&family=Roboto+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-base: #0a0a0a;
      --bg-panel: #111111;
      --bg-panel-hover: #161616;
      --border-color: rgba(255, 255, 255, 0.05);
      --border-highlight: rgba(255, 255, 255, 0.15);
      --border-radius: 3px;
      --text-primary: #e0e0e0;
      --text-secondary: #7e7e7e;
      --text-accent: #fcfcfc;
      --accent-green: #00ff88;
      --accent-blue: #0088ff;
      --shadow-soft: 0 4px 20px rgba(0, 0, 0, 0.5);
      --shadow-inset: inset 0 1px 0 rgba(255, 255, 255, 0.05);
      --ease-anime: cubic-bezier(0.25, 1, 0.5, 1);
    }
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Inter', sans-serif;
      background-color: var(--bg-base);
      color: var(--text-primary);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      overflow-x: hidden;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }
    body::before {
      content: '';
      position: fixed;
      top: 0; left: 0; width: 100vw; height: 100vh;
      background: radial-gradient(circle 800px at 50% 50%, rgba(255, 255, 255, 0.02), transparent 40%);
      pointer-events: none;
      z-index: 0;
    }
    .app-wrapper {
      position: relative;
      z-index: 1;
      width: 100%;
      max-width: 440px;
      padding: 1px;
      background: var(--border-color);
      border: 1px solid var(--border-color);
      border-radius: var(--border-radius);
      box-shadow: var(--shadow-soft);
      margin: 1rem;
    }
    .module {
      background-color: var(--bg-panel);
      padding: 2.5rem 2rem;
      box-shadow: var(--shadow-inset);
      transition: background-color 0.4s var(--ease-anime);
      text-align: center;
    }
    h1 {
      color: var(--text-accent);
      font-size: 1.75rem;
      font-weight: 500;
      letter-spacing: -0.02em;
      margin-bottom: 0.5rem;
    }
    p {
      color: var(--text-secondary);
      font-size: 0.9rem;
      margin-bottom: 2rem;
    }
    .font-mono {
      font-family: 'Roboto Mono', monospace;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .dropzone {
      border: 1px dashed var(--border-color);
      border-radius: var(--border-radius);
      padding: 2.5rem 1.5rem;
      background: rgba(255, 255, 255, 0.01);
      cursor: pointer;
      transition: all 0.4s var(--ease-anime);
      display: flex;
      flex-direction: column;
      align-items: center;
      position: relative;
      margin-bottom: 1.5rem;
    }
    .dropzone:hover, .dropzone.dragover {
      border-color: var(--accent-green);
      background: rgba(0, 255, 136, 0.02);
    }
    .dropzone svg {
      width: 48px;
      height: 48px;
      color: var(--text-secondary);
      margin-bottom: 1rem;
      transition: color 0.4s var(--ease-anime), transform 0.4s var(--ease-anime);
    }
    .dropzone:hover svg, .dropzone.dragover svg {
      color: var(--accent-green);
      transform: scale(1.1);
    }
    .dropzone span {
      font-size: 0.9rem;
      color: var(--text-primary);
      font-weight: 500;
    }
    .dropzone p {
      font-size: 0.8rem;
      color: var(--text-secondary);
      margin: 0.25rem 0 0 0;
    }
    #fileInput {
      display: none;
    }
    .file-info {
      margin-bottom: 1.5rem;
      display: none;
      font-family: 'Roboto Mono', monospace;
      font-size: 0.75rem;
      color: var(--accent-blue);
      background: rgba(0, 136, 255, 0.05);
      padding: 0.75rem;
      border-radius: var(--border-radius);
      border: 1px solid rgba(0, 136, 255, 0.15);
      word-break: break-all;
      text-align: left;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      background: transparent;
      border: 1px solid var(--border-color);
      color: var(--text-primary);
      padding: 0.75rem 1.5rem;
      font-family: 'Roboto Mono', monospace;
      font-size: 0.8rem;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      cursor: pointer;
      transition: all 0.4s var(--ease-anime);
      position: relative;
      overflow: hidden;
      text-decoration: none;
      border-radius: var(--border-radius);
      width: 100%;
    }
    .btn::before {
      content: '';
      position: absolute;
      top: 0; left: 0; width: 100%; height: 100%;
      background: var(--text-accent);
      transform: scaleX(0);
      transform-origin: right;
      transition: transform 0.4s var(--ease-anime);
      z-index: -1;
    }
    .btn:hover {
      color: var(--bg-base);
      border-color: var(--text-accent);
      transform: scale(1.02);
    }
    .btn:hover::before {
      transform: scaleX(1);
      transform-origin: left;
    }
    .btn:disabled {
      opacity: 0.3;
      cursor: not-allowed;
      transform: none;
    }
    .btn:disabled::before {
      display: none;
    }
  </style>
</head>
<body>
  <div class="app-wrapper">
    <div class="module">
      <h1>Actualizar Firmware</h1>
      <p class="font-mono">Módulo GPS Perseverance</p>
      <form id="uploadForm" method="POST" action="/update" enctype="multipart/form-data">
        <div id="dropzone" class="dropzone">
          <svg fill="none" stroke="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12"></path>
          </svg>
          <span id="droptext">Arrastra tu archivo .bin aquí</span>
          <p>o haz clic para explorar en tu PC</p>
          <input id="fileInput" type="file" name="update" accept=".bin">
        </div>
        <div id="fileInfo" class="file-info"></div>
        <input id="submitBtn" type="submit" class="btn" value="Subir y Actualizar" disabled>
      </form>
    </div>
  </div>
  <script>
    const dropzone = document.getElementById('dropzone');
    const fileInput = document.getElementById('fileInput');
    const fileInfo = document.getElementById('fileInfo');
    const submitBtn = document.getElementById('submitBtn');
    const droptext = document.getElementById('droptext');

    dropzone.addEventListener('click', () => fileInput.click());

    dropzone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropzone.classList.add('dragover');
    });

    ['dragleave', 'dragend'].forEach(type => {
      dropzone.addEventListener(type, () => {
        dropzone.classList.remove('dragover');
      });
    });

    dropzone.addEventListener('drop', (e) => {
      e.preventDefault();
      dropzone.classList.remove('dragover');
      if (e.dataTransfer.files.length) {
        fileInput.files = e.dataTransfer.files;
        handleFileChange();
      }
    });

    fileInput.addEventListener('change', handleFileChange);

    function handleFileChange() {
      if (fileInput.files.length) {
        const file = fileInput.files[0];
        if (file.name.endsWith('.bin')) {
          fileInfo.textContent = 'Archivo: ' + file.name + ' (' + (file.size/1024/1024).toFixed(2) + ' MB)';
          fileInfo.style.display = 'block';
          submitBtn.disabled = false;
          droptext.textContent = '¡Archivo seleccionado!';
        } else {
          fileInfo.textContent = 'Error: Solo se admiten archivos .bin';
          fileInfo.style.display = 'block';
          submitBtn.disabled = true;
          droptext.textContent = 'Arrastra tu archivo .bin aquí';
        }
      }
    }
  </script>
</body>
</html>
)rawhtml");
  });
  httpServer.on(
      "/update", HTTP_POST,
      []() {
        httpServer.sendHeader("Connection", "close");
        httpServer.send(200, "text/html", R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Actualización Exitosa</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600&family=Roboto+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-base: #0a0a0a;
      --bg-panel: #111111;
      --bg-panel-hover: #161616;
      --border-color: rgba(255, 255, 255, 0.05);
      --border-radius: 3px;
      --text-primary: #e0e0e0;
      --text-secondary: #7e7e7e;
      --text-accent: #fcfcfc;
      --accent-green: #00ff88;
      --shadow-soft: 0 4px 20px rgba(0, 0, 0, 0.5);
      --shadow-inset: inset 0 1px 0 rgba(255, 255, 255, 0.05);
      --ease-anime: cubic-bezier(0.25, 1, 0.5, 1);
    }
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Inter', sans-serif;
      background-color: var(--bg-base);
      color: var(--text-primary);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      overflow-x: hidden;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }
    body::before {
      content: '';
      position: fixed;
      top: 0; left: 0; width: 100vw; height: 100vh;
      background: radial-gradient(circle 800px at 50% 50%, rgba(255, 255, 255, 0.02), transparent 40%);
      pointer-events: none;
      z-index: 0;
    }
    .app-wrapper {
      position: relative;
      z-index: 1;
      width: 100%;
      max-width: 440px;
      padding: 1px;
      background: var(--border-color);
      border: 1px solid var(--border-color);
      border-radius: var(--border-radius);
      box-shadow: var(--shadow-soft);
      margin: 1rem;
    }
    .module {
      background-color: var(--bg-panel);
      padding: 2.5rem 2rem;
      box-shadow: var(--shadow-inset);
      transition: background-color 0.4s var(--ease-anime);
      text-align: center;
    }
    h1 {
      color: var(--accent-green);
      font-size: 1.75rem;
      font-weight: 500;
      letter-spacing: -0.02em;
      margin-bottom: 1rem;
      text-shadow: 0 0 15px rgba(0, 255, 136, 0.2);
    }
    p {
      color: var(--text-secondary);
      font-size: 0.95rem;
      margin-bottom: 2rem;
      line-height: 1.6;
    }
    .font-mono {
      font-family: 'Roboto Mono', monospace;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      background: transparent;
      border: 1px solid var(--border-color);
      color: var(--text-primary);
      padding: 0.75rem 1.5rem;
      font-family: 'Roboto Mono', monospace;
      font-size: 0.8rem;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      cursor: pointer;
      transition: all 0.4s var(--ease-anime);
      position: relative;
      overflow: hidden;
      text-decoration: none;
      border-radius: var(--border-radius);
      width: 100%;
    }
    .btn::before {
      content: '';
      position: absolute;
      top: 0; left: 0; width: 100%; height: 100%;
      background: var(--text-accent);
      transform: scaleX(0);
      transform-origin: right;
      transition: transform 0.4s var(--ease-anime);
      z-index: -1;
    }
    .btn:hover {
      color: var(--bg-base);
      border-color: var(--text-accent);
      transform: scale(1.02);
    }
    .btn:hover::before {
      transform: scaleX(1);
      transform-origin: left;
    }
  </style>
  <script>
    setTimeout(function() {
      window.location.href = "/update";
    }, 6000);
  </script>
</head>
<body>
  <div class="app-wrapper">
    <div class="module">
      <h1>Actualización Correcta</h1>
      <p>El firmware se ha cargado con éxito.<br>Reiniciando el Módulo GPS, por favor espera unos segundos...</p>
      <a href="/update" class="btn">Volver</a>
    </div>
  </div>
</body>
</html>
)rawhtml");
        delay(1000);
        ESP.restart();
      },
      []() {
        HTTPUpload &upload = httpServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
          Serial.printf("Update: %s\n", upload.filename.c_str());
          if (!Update.begin(
                  UPDATE_SIZE_UNKNOWN)) { // start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(
                  true)) { // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n",
                          upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
      });
  httpServer.begin();
}

void loop() {
  ArduinoOTA.handle();
  httpServer.handleClient();

  // Asegurar que no se satura el serial del GPS
  while (neogps.available() > 0) {
    gps.encode(neogps.read());
  }

  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnect();
    }
  } else {
    client.loop();
  }

  unsigned long now = millis();

  // Heading cada 50ms (independiente de telemetría)
  if (now - lastHeadingUpdate > 50) {
    lastHeadingUpdate = now;
    updateHeading();
  }

  // Cada 1 segundo...
  if (now - lastTelemetryMsg > TELEMETRY_INTERVAL) {
    lastTelemetryMsg = now;
    if (client.connected()) {
      enviarTelemetria();
      if (calibrating) {
        StaticJsonDocument<128> calDoc;
        calDoc["status"] = "CALIBRANDO";
        calDoc["x_min"] = cal_x_min;
        calDoc["x_max"] = cal_x_max;
        calDoc["y_min"] = cal_y_min;
        calDoc["y_max"] = cal_y_max;
        char calBuf[128];
        serializeJson(calDoc, calBuf);
        client.publish("rover/calibration", calBuf);
      } else {         // <-- Esto SIEMPRE se envía para que el mapa web no
                       // se congele
        navegarAuto(); // <-- Esto SOLO envía órdenes a los motores si hay un
                       // target fijado
      }
    }
  }

  // Cada 5 segundos...
  if (now - lastStatusMsg > STATUS_INTERVAL) {
    lastStatusMsg = now;
    if (client.connected()) {
      enviarStatus();
    }
  }
}
