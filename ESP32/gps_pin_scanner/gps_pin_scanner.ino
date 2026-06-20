// GPS PIN SCANNER - Diagnostico inalambrico
// USO:
//  1. Compilar: Sketch > Export Compiled Binary
//  2. Subir por OTA: http://<IP_GPS>/update -> cargar el .bin
//  3. Abrir http://<IP_GPS> en el browser
//  4. Cuando termines, subir gps.ino de vuelta por OTA

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <Update.h>

const char* SSID_1 = "CAMBIA_WIFI_SSID";
const char* PASS_1 = "CAMBIA_WIFI_PASS";
const char* SSID_2 = "CAMBIA_WIFI_SSID2";
const char* PASS_2 = "CAMBIA_WIFI_PASS2";

WiFiMulti  wifiMulti;
WebServer  server(80);

const int PINES[] = { 4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33,34,35,36,39 };
const int N_PINES = sizeof(PINES) / sizeof(PINES[0]);

HardwareSerial gpsTest(2);

struct ResultadoPin { int pin; int bytes; bool nmea; uint8_t primerByte; };
ResultadoPin resultados[22];

void escanear() {
  Serial.println("\n[SCAN] Iniciando escaneo de pines...");
  for (int i = 0; i < N_PINES; i++) {
    int pin = PINES[i];
    resultados[i] = { pin, 0, false, 0 };
    Serial.printf("  GPIO %2d ... ", pin);
    gpsTest.begin(9600, SERIAL_8N1, pin, -1);
    delay(15);
    unsigned long t0 = millis();
    while (millis() - t0 < 900) {
      if (gpsTest.available()) {
        uint8_t c = gpsTest.read();
        if (resultados[i].bytes == 0) resultados[i].primerByte = c;
        resultados[i].bytes++;
        if (c == '$') resultados[i].nmea = true;
      }
    }
    gpsTest.end();
    delay(25);
    if (resultados[i].bytes == 0) {
      Serial.println("sin datos");
    } else if (resultados[i].nmea) {
      Serial.printf("*** GPS ENCONTRADO *** (%d bytes)\n", resultados[i].bytes);
    } else {
      Serial.printf("datos extranos (%d bytes, 0x%02X)\n", resultados[i].bytes, resultados[i].primerByte);
    }
  }
  Serial.println("[SCAN] Completo.");
}

void handleOtaGet() {
  server.sendHeader("Connection", "close");
  String html =
    "<!DOCTYPE html><html lang=\"es\"><head>"
    "<meta charset=\"UTF-8\">"
    "<title>OTA - GPS Scanner</title>"
    "<style>"
    "body{font-family:monospace;background:#0a0a0a;color:#e0e0e0;"
    "display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}"
    ".box{background:#111;border:1px solid #222;padding:2rem;max-width:400px;width:100%;text-align:center}"
    "h1{color:#00ff88;font-size:1.4rem;margin-bottom:.5rem}"
    "p{color:#666;font-size:.85rem;margin-bottom:1.5rem}"
    "input[type=file]{display:none}"
    ".drop{border:1px dashed #333;padding:2rem;cursor:pointer;margin-bottom:1rem}"
    ".drop:hover{border-color:#00ff88}"
    ".btn{display:block;width:100%;padding:.7rem;background:transparent;border:1px solid #444;"
    "color:#e0e0e0;font-family:monospace;cursor:pointer;text-transform:uppercase}"
    ".btn:hover{border-color:#fff}.btn:disabled{opacity:.3;cursor:not-allowed}"
    "</style></head><body><div class=\"box\">"
    "<h1>Actualizar Firmware</h1>"
    "<p>Sube el .bin para volver a gps.ino</p>"
    "<form id=\"f\" method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">"
    "<div class=\"drop\" onclick=\"document.getElementById('fi').click()\">"
    "Clic o arrastra el .bin aqui<br>"
    "<small id=\"fn\" style=\"color:#00ff88\"></small></div>"
    "<input id=\"fi\" type=\"file\" name=\"update\" accept=\".bin\""
    " onchange=\"document.getElementById('fn').textContent=this.files[0].name;"
    "document.getElementById('sb').disabled=false\">"
    "<input id=\"sb\" type=\"submit\" class=\"btn\" value=\"Subir y Actualizar\" disabled>"
    "</form></div></body></html>";
  server.send(200, "text/html", html);
}

void handleOtaUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      Update.printError(Serial);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.println("OTA OK. Reiniciando...");
    else Update.printError(Serial);
  }
}

void handleOtaPost() {
  server.sendHeader("Connection", "close");
  bool ok = !Update.hasError();
  server.send(200, "text/html",
    ok ? "<html><body style=\"font-family:monospace;background:#0a0a0a;color:#00ff88;"
         "display:flex;justify-content:center;align-items:center;min-height:100vh\">"
         "<div style=\"text-align:center\"><h1>Actualizado</h1>"
         "<p style=\"color:#666\">Reiniciando... espera 10s</p></div></body></html>"
       : "<html><body style=\"font-family:monospace;background:#0a0a0a;color:#ff4444;"
         "display:flex;justify-content:center;align-items:center;min-height:100vh\">"
         "<div style=\"text-align:center\"><h1>Error al actualizar</h1></div></body></html>");
  delay(1000);
  if (ok) ESP.restart();
}
void handleRoot() {
  String css =
    "body{font-family:monospace;background:#0a0a0a;color:#e0e0e0;padding:1.5rem;max-width:680px;margin:auto}"
    "h1{color:#00ff88}"
    "p{color:#7e7e7e;font-size:.85rem;margin-bottom:1.5rem}"
    "table{width:100%;border-collapse:collapse}"
    "th,td{padding:.55rem .9rem;border:1px solid #1e1e1e;text-align:left}"
    "th{background:#111;color:#555}"
    "tr.found td{background:rgba(0,200,100,.08);color:#00ff88;font-weight:700}"
    "tr.noise td{color:#fcc800}"
    "td.none{color:#333}"
    ".btn{display:inline-block;margin-top:1.2rem;padding:.5rem 1.2rem;border:1px solid #333;color:#aaa;text-decoration:none}"
    ".btn:hover{border-color:#00ff88;color:#00ff88}";

  String html =
    "<!DOCTYPE html><html lang='es'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>GPS Pin Scanner</title>"
    "<style>" + css + "</style></head><body>"
    "<h1>GPS Pin Scanner</h1>"
    "<p>Busca en que GPIO esta el TX del modulo GPS (NMEA a 9600 baud).</p>"
    "<table><tr><th>GPIO</th><th>Estado</th><th>Detalle</th></tr>";

  for (int i = 0; i < N_PINES; i++) {
    ResultadoPin& r = resultados[i];
    if (r.bytes == 0) {
      html += "<tr><td>" + String(r.pin) + "</td><td class='none'>sin datos</td><td class='none'>-</td></tr>";
    } else if (r.nmea) {
      html += "<tr class='found'><td>" + String(r.pin) + "</td><td>GPS ENCONTRADO</td><td>"
              + String(r.bytes) + " bytes - NMEA ($)</td></tr>";
    } else {
      char buf[32]; snprintf(buf, sizeof(buf), "0x%02X", r.primerByte);
      html += "<tr class='noise'><td>" + String(r.pin) + "</td><td>datos extranos</td><td>"
              + String(r.bytes) + " bytes, primer byte: " + String(buf) + "</td></tr>";
    }
  }
  html += "</table><a class='btn' href='/rescan'>Repetir escaneo</a></body></html>";
  server.send(200, "text/html", html);
}

void handleRescan() {
  escanear();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== GPS PIN SCANNER ===");

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(SSID_1, PASS_1);
  wifiMulti.addAP(SSID_2, PASS_2);

  Serial.print("Conectando WiFi");
  while (wifiMulti.run() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[OK] IP: " + WiFi.localIP().toString());
  Serial.println("     Abre: http://" + WiFi.localIP().toString());

  escanear();

  server.on("/",        handleRoot);
  server.on("/rescan", handleRescan);
  server.on("/update", HTTP_GET,  handleOtaGet);
  server.on("/update", HTTP_POST, handleOtaPost, handleOtaUpload);
  server.begin();
  Serial.println("[OK] Servidor web listo.");
}

void loop() {
  server.handleClient();
}
