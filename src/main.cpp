#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <queue>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"

// --- Declaraciones anticipadas ---
struct BleData {
  uint32_t studentId;
  String deviceUuid;
  String macAddress;
  unsigned long detectionTime;
};

// Prototipos de función
void setupWiFi();

// Configuraciones
#define SCAN_TIME_SECONDS  5
#define TARGET_COMPANY_ID  0x1234
const char* WIFI_SSID = "UTM_Biblioteca"; //Kwantec UTM_Biblioteca
const char* WIFI_PASSWORD = "";  //kw4nt3c%%1331
const char* API_URL = "https://tu-api.com/endpoint";
const char* API_KEY = "tu-api-key";

// Variables globales
#include <set>
#include <vector>

std::set<uint32_t> ids_etapa1_actual;
std::set<uint32_t> ids_etapa2_actual;
std::set<uint32_t> ids_etapa3_actual;
std::vector<BleData> etapa1_nuevos;
std::vector<BleData> etapa2_nuevos;
std::vector<BleData> etapa3_nuevos;
int etapa = 0;
unsigned long lastEtapaMillis = 0;

// Clase para manejar dispositivos BLE detectados
// ...existing code...
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveManufacturerData()) {
      std::string mData = advertisedDevice.getManufacturerData();
      uint8_t* data = (uint8_t*)mData.data();
      size_t length = mData.length();
      if (length >= 11) {
        uint16_t companyId = data[0] | (data[1] << 8);
        if (companyId == TARGET_COMPANY_ID) {
          BleData newData;
          uint8_t* payload = data + 2;
          newData.studentId = payload[0] | (payload[1] << 8) | (payload[2] << 16);
          char uuidBuf[17];
          for (int i = 0; i < 8; i++) {
            sprintf(&uuidBuf[i*2], "%02X", payload[3+i]);
          }
          uuidBuf[16] = '\0';
          newData.deviceUuid = String(uuidBuf);
          newData.macAddress = advertisedDevice.getAddress().toString().c_str();
          newData.detectionTime = millis();

          // Usar el set global correspondiente a la etapa actual
          if (etapa == 0) {
            if (ids_etapa1_actual.find(newData.studentId) == ids_etapa1_actual.end()) {
              ids_etapa1_actual.insert(newData.studentId);
              etapa1_nuevos.push_back(newData);
              Serial.printf("[BLE] Detectado: studentId=%u, uuid=%s, mac=%s, etapa=%d\n",
                newData.studentId, newData.deviceUuid.c_str(), newData.macAddress.c_str(), etapa);
            }
          } else if (etapa == 1) {
            if (ids_etapa2_actual.find(newData.studentId) == ids_etapa2_actual.end()) {
              ids_etapa2_actual.insert(newData.studentId);
              etapa2_nuevos.push_back(newData);
              Serial.printf("[BLE] Detectado: studentId=%u, uuid=%s, mac=%s, etapa=%d\n",
                newData.studentId, newData.deviceUuid.c_str(), newData.macAddress.c_str(), etapa);
            }
          } else if (etapa == 2) {
            if (ids_etapa3_actual.find(newData.studentId) == ids_etapa3_actual.end()) {
              ids_etapa3_actual.insert(newData.studentId);
              etapa3_nuevos.push_back(newData);
              Serial.printf("[BLE] Detectado: studentId=%u, uuid=%s, mac=%s, etapa=%d\n",
                newData.studentId, newData.deviceUuid.c_str(), newData.macAddress.c_str(), etapa);
            }
          }
        }
      }
    }
  }
};
// ...existing code...
// Implementación de funciones
void setupWiFi() {
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi conectado");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
    delay(500);
  } else {
    Serial.println();
    Serial.println("Falló la conexión WiFi");
  }
}

// Ya no se usa processDetectedDevice

// Nueva función para enviar POST o PATCH masivo
void enviarAsistencias(const std::vector<BleData>& lista, bool esPost) {
  if (lista.empty()) {
    Serial.println("[HTTP] No hay dispositivos detectados para enviar.");
    return;
  }

  DynamicJsonDocument doc(1024);
  doc["id_device"] = "1";
  JsonArray attendances = doc.createNestedArray("attendances");

  for (const auto& d : lista) {
    JsonObject att = attendances.createNestedObject();
    att["id_student"] = d.studentId;
    unsigned long ms = d.detectionTime;
    time_t now = time(NULL);
    time_t detectedEpoch = now - ((millis() - ms) / 1000);
    detectedEpoch -= 6 * 3600; // Restar 6 horas para UTC-6
    struct tm* timeinfo = gmtime(&detectedEpoch);
    char isoTime[25];
    strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    att["attendance_time"] = isoTime;
  }

  String payload;
  serializeJson(doc, payload);
  Serial.println("[HTTP] Payload a enviar:");
  Serial.println(payload);

  int httpResponseCode = 0;
  int intento = 1;
  bool enviado = false;
  String url;
  while (!enviado) {
    HTTPClient http;
    http.setTimeout(15000); // Timeout de 15 segundos
    if (esPost) {
      url = "https://api-schoolguardian.onrender.com/api/attendance/device";
      Serial.println("\n==============================");
      Serial.printf("[Etapa 1] Intento #%d POST a la API\n", intento);
      Serial.println("==============================");
    } else {
      url = "https://api-schoolguardian.onrender.com/api/attendance/device-status";
      if (etapa == 1) {
        Serial.println("\n==============================");
        Serial.printf("[Etapa 2] Intento #%d PATCH a la API\n", intento);
        Serial.println("==============================");
      } else if (etapa == 2) {
        Serial.println("\n==============================");
        Serial.printf("[Etapa 3] Intento #%d PATCH a la API\n", intento);
        Serial.println("==============================");
      }
    }
    Serial.print("[HTTP] Iniciando conexión a: ");
    Serial.println(url);
    int beginResult = http.begin(url);
    Serial.printf("[HTTP] Resultado de http.begin: %d\n", beginResult);
    http.addHeader("Content-Type", "application/json");
    // http.addHeader("x-api-key", API_KEY); // Si tu API lo requiere

    delay(1000); // Espera 1 segundo antes de enviar

    if (esPost) {
      httpResponseCode = http.POST(payload);
    } else {
      httpResponseCode = http.PATCH(payload);
    }

    if (httpResponseCode > 0) {
      Serial.println("[HTTP] Respuesta recibida:");
      String response = http.getString();
      Serial.println(response);
      // Sugerencia si el POST falla por asistencia ya existente
      if (response.indexOf("Ya existe una asistencia para este estudiante") != -1) {
        Serial.println("[HTTP] SUGERENCIA: Usa PATCH para actualizar la asistencia de este estudiante.");
      }
      enviado = true;
    } else {
      Serial.printf("[HTTP] ERROR: %d\n", httpResponseCode);
      if (httpResponseCode == -11) {
        Serial.println("[HTTP] Error -11: Conexión perdida. Intentando reconectar WiFi...");
        setupWiFi();
      } else if (httpResponseCode == -5) {
        Serial.println("[HTTP] Error -5: Timeout de lectura. El servidor no respondió a tiempo.");
        Serial.println("[HTTP] SUGERENCIA: Verifica la conexión WiFi y la disponibilidad del servidor.");
      } else {
        Serial.println("[HTTP] Error desconocido. Revisa la documentación de HTTPClient.");
      }
      Serial.println("[HTTP] Reintentando en 2 segundos...");
      delay(2000);
      intento++;
    }
    http.end();
  }
}



void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32 BLE Scanner");

  // Configuración robusta de BLE
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
    Serial.println("Error al inicializar controlador BT");
    while(1);
  }

  if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
    Serial.println("Error al habilitar controlador BT");
    while(1);
  }

  if (esp_bluedroid_init() != ESP_OK) {
    Serial.println("Error al inicializar Bluedroid");
    while(1);
  }

  if (esp_bluedroid_enable() != ESP_OK) {
    Serial.println("Error al habilitar Bluedroid");
    while(1);
  }

  setupWiFi();

  // Sincronizar hora con NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Esperando sincronización NTP...");
  time_t now = time(nullptr);
  int ntpWait = 0;
  while (now < 8 * 3600 * 2 && ntpWait < 30) { // Espera hasta que la hora sea válida o 15s
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    ntpWait++;
  }
  Serial.println("\nHora sincronizada.");

  BLEDevice::init("ESP32-BLE-Receiver");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, intentando reconectar...");
    setupWiFi();
  }

  unsigned long now = millis();
  if (etapa == 0 && (now - lastEtapaMillis > 1000)) { // Espera 1s antes de iniciar
    Serial.println("[Etapa 1] Escaneando...");
    etapa1_nuevos.clear();
    ids_etapa1_actual.clear();
    BLEDevice::getScan()->start(SCAN_TIME_SECONDS, false);
    delay(SCAN_TIME_SECONDS * 1000 + 500); // Espera a que termine el escaneo
    Serial.println("[Etapa 1] Enviando POST...");
    enviarAsistencias(etapa1_nuevos, true);
    lastEtapaMillis = millis();
    etapa = 1;
  } else if (etapa == 1 && (now - lastEtapaMillis > 60000)) { // 1 minuto después
    Serial.println("[Etapa 2] Escaneando...");
    etapa2_nuevos.clear();
    ids_etapa2_actual.clear();
    BLEDevice::getScan()->start(SCAN_TIME_SECONDS, false);
    delay(SCAN_TIME_SECONDS * 1000 + 500);
    Serial.println("[Etapa 2] Enviando PATCH...");
    enviarAsistencias(etapa2_nuevos, false);
    lastEtapaMillis = millis();
    etapa = 2;
  } else if (etapa == 2 && (now - lastEtapaMillis > 60000)) { // 1 minuto después
    Serial.println("[Etapa 3] Escaneando...");
    etapa3_nuevos.clear();
    ids_etapa3_actual.clear();
    BLEDevice::getScan()->start(SCAN_TIME_SECONDS, false);
    delay(SCAN_TIME_SECONDS * 1000 + 500);
    Serial.println("[Etapa 3] Enviando PATCH...");
    enviarAsistencias(etapa3_nuevos, false);
    lastEtapaMillis = millis();
    etapa = 3;
  }
  // No repite, si quieres repetir, reinicia el ESP32 o ajusta la lógica
}