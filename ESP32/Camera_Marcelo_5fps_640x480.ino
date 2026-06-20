/*
 *  Camera Marcelo - Modo calidad: 640x480 VGA / ~5 FPS
 *  Freenove ESP32-S3 CAM
 *
 *  Librerias necesarias (Arduino IDE Library Manager):
 *    - "WebSockets" by Markus Sattler (links2004/WebSockets)
 *    - "PubSubClient" by Nick O'Leary
 *    - "esp32-camera" incluida en el core ESP32
 *
 *  OTA: hostname "cam-5fps-vga"
 *  Para flashear OTA: Tools > Port > cam-5fps-vga.local
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

// ==================== RED ====================
WiFiMulti wifiMulti;
const char* ssid_primary       = "CAMBIA_WIFI_SSID";
const char* password_primary   = "CAMBIA_WIFI_PASS";
const char* ssid_secondary     = "CAMBIA_WIFI_SSID2";
const char* password_secondary = "CAMBIA_WIFI_PASS2";

// ==================== MQTT ====================
const char* mqtt_server   = "CAMBIA_MQTT_HOST";
const int   mqtt_port     = 1883;
const char* mqtt_user     = "CAMBIA_MQTT_USER";
const char* mqtt_password = "CAMBIA_MQTT_PASS";
const char* CAM_MODE = "5fps-vga";

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// ==================== PROXY DE VIDEO ====================
const char* ws_host = "CAMBIA_VIDEO_HOST";
const int   ws_port = 443;
const char* ws_path = "/publish";

WebSocketsClient webSocket;

// ==================== PINOUT FREENOVE ESP32-S3 CAM ====================
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5
#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM   13

// ==================== TEMPORIZADO ====================
unsigned long lastCaptureTime  = 0;
const unsigned long captureInterval = 200; // ~5 FPS

unsigned long framesSent      = 0;
unsigned long framesFailed    = 0;
unsigned long lastStatusPrint = 0;
unsigned long lastMqttPublish = 0;
const unsigned long mqttPublishInterval = 30000;

void publishIp() {
    if (!mqttClient.connected()) return;
    String payload = "{\"ip_cam\":\"" + WiFi.localIP().toString() + "\"}";
    mqttClient.publish("rover/telemetry", payload.c_str());
    Serial.println("[MQTT] IP publicada: " + payload);
}

void publishCamStatus() {
    if (!mqttClient.connected()) return;
    bool wsOk = webSocket.isConnected();
    const char* state = wsOk ? "STREAMING" : "CONNECTING";
    char payload[200];
    snprintf(payload, sizeof(payload),
        "{\"state\":\"%s\",\"mode\":\"%s\",\"rssi\":%d,\"ram_kb\":%u,\"frames_sent\":%lu,\"frames_failed\":%lu,\"ws\":%s}",
        state,
        CAM_MODE,
        WiFi.RSSI(),
        ESP.getFreeHeap() / 1024,
        framesSent,
        framesFailed,
        wsOk ? "true" : "false"
    );
    mqttClient.publish("rover/cam/status", payload);
    Serial.printf("[MQTT] Cam status: %s\n", state);
}

void mqttConnect() {
    int attempts = 0;
    while (!mqttClient.connected() && attempts < 3) {
        String clientId = "cam-5fps-vga-";
        clientId += String(random(0xffff), HEX);
        Serial.print("[MQTT] Conectando...");
        if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            Serial.println(" OK");
            publishIp();
        } else {
            Serial.printf(" Fallo (%d). Reintentando...\n", mqttClient.state());
            delay(2000);
        }
        attempts++;
    }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Desconectado.");
            break;
        case WStype_CONNECTED:
            Serial.println("[WS] Conectado al servidor de video.");
            framesSent = 0;
            framesFailed = 0;
            break;
        case WStype_ERROR:
            Serial.println("[WS] Error.");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Camera Marcelo 5fps 640x480 VGA ===");

    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    #ifdef RGB_BUILTIN
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    #else
    rgbLedWrite(48, 0, 0, 0);
    #endif

    // --- Camara ---
    camera_config_t config;
    config.ledc_channel  = LEDC_CHANNEL_0;
    config.ledc_timer    = LEDC_TIMER_0;
    config.pin_d0        = Y2_GPIO_NUM;
    config.pin_d1        = Y3_GPIO_NUM;
    config.pin_d2        = Y4_GPIO_NUM;
    config.pin_d3        = Y5_GPIO_NUM;
    config.pin_d4        = Y6_GPIO_NUM;
    config.pin_d5        = Y7_GPIO_NUM;
    config.pin_d6        = Y8_GPIO_NUM;
    config.pin_d7        = Y9_GPIO_NUM;
    config.pin_xclk      = XCLK_GPIO_NUM;
    config.pin_pclk      = PCLK_GPIO_NUM;
    config.pin_vsync     = VSYNC_GPIO_NUM;
    config.pin_href      = HREF_GPIO_NUM;
    config.pin_sscb_sda  = SIOD_GPIO_NUM;
    config.pin_sscb_scl  = SIOC_GPIO_NUM;
    config.pin_pwdn      = PWDN_GPIO_NUM;
    config.pin_reset     = RESET_GPIO_NUM;
    config.xclk_freq_hz  = 10000000;
    config.pixel_format  = PIXFORMAT_JPEG;
    config.frame_size    = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality  = 10;
    config.fb_count      = 2;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] Error init: 0x%x\n", err);
        return;
    }
    sensor_t* s = esp_camera_sensor_get();
    s->set_hmirror(s, 1);
    s->set_vflip(s, 0);

    // --- WiFi ---
    wifiMulti.addAP(ssid_primary, password_primary);
    wifiMulti.addAP(ssid_secondary, password_secondary);
    Serial.print("[WiFi] Conectando");
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] Conectado. Red: %s | IP: %s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

    // --- MQTT ---
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttConnect();

    // --- OTA ---
    ArduinoOTA.setHostname("cam-5fps-vga");
    ArduinoOTA.onStart([]()   { Serial.println("[OTA] Inicio..."); });
    ArduinoOTA.onEnd([]()     { Serial.println("[OTA] Completado."); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
        Serial.printf("[OTA] %u%%\r", p / (t / 100));
    });
    ArduinoOTA.onError([](ota_error_t e) {
        Serial.printf("[OTA] Error[%u]\n", e);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Listo. Hostname: cam-5fps-vga");

    // --- WebSocket ---
    webSocket.beginSSL(ws_host, ws_port, ws_path);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
}

void loop() {
    ArduinoOTA.handle();

    if (wifiMulti.run() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconectando...");
        delay(500);
        return;
    }

    if (!mqttClient.connected()) mqttConnect();
    mqttClient.loop();

    unsigned long now = millis();

    if (now - lastMqttPublish >= mqttPublishInterval) {
        lastMqttPublish = now;
        publishIp();
    }

    webSocket.loop();

    if (webSocket.isConnected() && (now - lastCaptureTime >= captureInterval)) {
        lastCaptureTime = now;

        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            framesFailed++;
            return;
        }
        bool ok = webSocket.sendBIN(fb->buf, fb->len);
        esp_camera_fb_return(fb);
        if (ok) framesSent++;
        else     framesFailed++;
    }

    if (now - lastStatusPrint >= 5000) {
        lastStatusPrint = now;
        Serial.printf("[DIAG] Env:%lu Fail:%lu RSSI:%d dBm RAM:%uKB Red:%s WS:%s MQTT:%s\n",
                      framesSent, framesFailed,
                      WiFi.RSSI(), ESP.getFreeHeap() / 1024,
                      WiFi.SSID().c_str(),
                      webSocket.isConnected() ? "OK" : "OFF",
                      mqttClient.connected() ? "OK" : "OFF");
    publishCamStatus();
    }
}
