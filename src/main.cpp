#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <queue>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"

struct BleData
{
  uint32_t studentId;
  String deviceUuid;
  String macAddress;
  unsigned long detectionTime;
};

void setupWiFi();
void ledFlash(int ledPin);
void alertaError();
void alertaDanger();

#define SCAN_TIME_SECONDS 5
#define TARGET_COMPANY_ID 0x1234
// const char *WIFI_SSID = "Red_C105";
const char *WIFI_SSID = "UTM_Biblioteca";
// const char *WIFI_PASSWORD = "22cpatic";
const char *WIFI_PASSWORD = "";
const char *API_URL = "https://tu-api.com/endpoint";
const char *API_KEY = "tu-api-key";
const char *ID_DEVICE = "1";
const int LED_LOADER = 13;
const int LED_SUCCESS = 25;
const int LED_ERROR = 12;
const int LED_DANGER = 32;
const int ZUMBADOR = 27;

#include <set>
#include <vector>

LiquidCrystal lcd(19, 18, 21, 4, 23, 22);

std::set<uint32_t> ids_etapa1_actual;
std::set<uint32_t> ids_etapa2_actual;
std::set<uint32_t> ids_etapa3_actual;
std::vector<BleData> etapa1_nuevos;
std::vector<BleData> etapa2_nuevos;
std::vector<BleData> etapa3_nuevos;
int etapa = 0;
unsigned long lastEtapaMillis = 0;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (advertisedDevice.haveManufacturerData())
    {
      std::string mData = advertisedDevice.getManufacturerData();
      uint8_t *data = (uint8_t *)mData.data();
      size_t length = mData.length();
      if (length >= 11)
      {
        uint16_t companyId = data[0] | (data[1] << 8);
        if (companyId == TARGET_COMPANY_ID)
        {
          BleData newData;
          uint8_t *payload = data + 2;
          newData.studentId = payload[0] | (payload[1] << 8) | (payload[2] << 16);
          char uuidBuf[17];
          for (int i = 0; i < 8; i++)
          {
            sprintf(&uuidBuf[i * 2], "%02X", payload[3 + i]);
          }
          uuidBuf[16] = '\0';
          newData.deviceUuid = String(uuidBuf);
          newData.macAddress = advertisedDevice.getAddress().toString().c_str();
          newData.detectionTime = millis();

          if (etapa == 0)
          {
            if (ids_etapa1_actual.find(newData.studentId) == ids_etapa1_actual.end())
            {
              ids_etapa1_actual.insert(newData.studentId);
              etapa1_nuevos.push_back(newData);
              Serial.printf("[BLE] Detectado: studentId=%u, uuid=%s, mac=%s, etapa=%d\n",
                            newData.studentId, newData.deviceUuid.c_str(), newData.macAddress.c_str(), etapa);
            }
          }
          else if (etapa == 1)
          {
            if (ids_etapa2_actual.find(newData.studentId) == ids_etapa2_actual.end())
            {
              ids_etapa2_actual.insert(newData.studentId);
              etapa2_nuevos.push_back(newData);
              Serial.printf("[BLE] Detectado: studentId=%u, uuid=%s, mac=%s, etapa=%d\n",
                            newData.studentId, newData.deviceUuid.c_str(), newData.macAddress.c_str(), etapa);
            }
          }
          else if (etapa == 2)
          {
            if (ids_etapa3_actual.find(newData.studentId) == ids_etapa3_actual.end())
            {
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

void setupWiFi()
{
  lcd.clear();
  lcd.print("Conectando WiFi");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);

  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();
  unsigned long lastLedToggle = millis();
  bool ledState = false;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000)
  {
    if (millis() - lastLedToggle >= 250)
    {
      ledState = !ledState;
      digitalWrite(LED_LOADER, ledState ? HIGH : LOW);
      lastLedToggle = millis();
    }

    delay(50);
    Serial.print(".");
  }

  digitalWrite(LED_LOADER, LOW);

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println();
    Serial.println("WiFi conectado");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.print("WiFi Conectado");
    lcd.setCursor(0, 1);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP().toString().substring(0, 11));

    ledFlash(LED_SUCCESS);
    delay(2000);
  }
  else
  {
    Serial.println();
    Serial.println("Falló la conexión WiFi");

    lcd.clear();
    lcd.print("WiFi ERROR");
    lcd.setCursor(0, 1);
    lcd.print("Sin conexion");

    alertaError();
    delay(2000);
  }
}

void enviarAsistencias(const std::vector<BleData> &lista, int etapaNumero)
{
  DynamicJsonDocument doc(1024);
  doc["id_device"] = ID_DEVICE;

  time_t now = time(NULL);
  now -= 6 * 3600;
  struct tm *timeinfo = gmtime(&now);
  char isoTime[25];
  strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

  if (lista.empty())
  {
    Serial.println("[HTTP] No hay dispositivos detectados. Enviando sondeo vacío...");

    lcd.clear();
    lcd.print("Sin dispositivos");
    lcd.setCursor(0, 1);
    lcd.print("Enviando POST...");
    alertaDanger();

    doc["data_time"] = isoTime;
    JsonArray attendances = doc.createNestedArray("attendances");
  }
  else
  {
    JsonArray attendances = doc.createNestedArray("attendances");

    for (const auto &d : lista)
    {
      JsonObject att = attendances.createNestedObject();
      att["id_student"] = d.studentId;
      unsigned long ms = d.detectionTime;
      time_t detectedEpoch = now - ((millis() - ms) / 1000);
      struct tm *detectedTimeinfo = gmtime(&detectedEpoch);
      char detectedIsoTime[25];
      strftime(detectedIsoTime, sizeof(detectedIsoTime), "%Y-%m-%dT%H:%M:%SZ", detectedTimeinfo);
      att["attendance_time"] = detectedIsoTime;
    }
  }

  String payload;
  serializeJson(doc, payload);
  Serial.println("[HTTP] Payload a enviar:");
  Serial.println(payload);

  int httpResponseCode = 0;
  int intento = 1;
  bool enviado = false;
  String url = "https://api-schoolguardian.onrender.com/api/attendance/ping";

  while (!enviado)
  {
    HTTPClient http;
    http.setTimeout(15000);

    lcd.clear();
    lcd.print("POST Intento #");
    lcd.print(intento);
    lcd.setCursor(0, 1);
    lcd.print("Etapa ");
    lcd.print(etapaNumero);
    lcd.print(" - Ping API");

    Serial.println("\n==============================");
    Serial.printf("[Etapa %d] Intento #%d POST a /ping\n", etapaNumero, intento);
    Serial.println("==============================");

    digitalWrite(LED_SUCCESS, HIGH);

    Serial.print("[HTTP] Iniciando conexión a: ");
    Serial.println(url);
    int beginResult = http.begin(url);
    Serial.printf("[HTTP] Resultado de http.begin: %d\n", beginResult);
    http.addHeader("Content-Type", "application/json");

    delay(1000);

    httpResponseCode = http.POST(payload);

    digitalWrite(LED_SUCCESS, LOW);

    if (httpResponseCode > 0)
    {
      Serial.println("[HTTP] Respuesta recibida:");
      String response = http.getString();
      Serial.println(response);

      if (httpResponseCode == 404)
      {
        Serial.println("[HTTP] ERROR 404: Recurso no encontrado. Reintentando...");
        lcd.clear();
        lcd.print("HTTP 404 Error");
        lcd.setCursor(0, 1);
        lcd.print("Reintentando...");
        alertaError();
        delay(2000);
        intento++;
        http.end();
        continue;
      }
      else if (httpResponseCode == 500)
      {
        Serial.println("[HTTP] ERROR 500: Error interno del servidor. Reintentando...");
        lcd.clear();
        lcd.print("Error Servidor");
        lcd.setCursor(0, 1);
        lcd.print("Reintentando...");
        alertaError();
        delay(3000);
        intento++;
        http.end();
        continue;
      }
      else if (httpResponseCode == 502)
      {
        Serial.println("[HTTP] ERROR 502: Bad Gateway - Servidor no disponible. Reintentando...");
        lcd.clear();
        lcd.print("Servidor Caido");
        lcd.setCursor(0, 1);
        lcd.print("Reintentando...");
        alertaError();
        delay(5000); // Espera más tiempo para servidores caídos
        intento++;
        http.end();
        continue;
      }
      else if (httpResponseCode == 503)
      {
        Serial.println("[HTTP] ERROR 503: Servicio no disponible. Reintentando...");
        lcd.clear();
        lcd.print("Servicio");
        lcd.setCursor(0, 1);
        lcd.print("No disponible");
        alertaError();
        delay(5000);
        intento++;
        http.end();
        continue;
      }
      else if (httpResponseCode == 504)
      {
        Serial.println("[HTTP] ERROR 504: Gateway Timeout - Servidor muy lento. Reintentando...");
        lcd.clear();
        lcd.print("Timeout Gateway");
        lcd.setCursor(0, 1);
        lcd.print("Reintentando...");
        alertaError();
        delay(3000);
        intento++;
        http.end();
        continue;
      }
      else if (httpResponseCode >= 400 && httpResponseCode < 500)
      {
        Serial.printf("[HTTP] ERROR %d: Error del cliente. Reintentando...\n", httpResponseCode);
        lcd.clear();
        lcd.print("Error Cliente");
        lcd.setCursor(0, 1);
        lcd.print("Cod: ");
        lcd.print(httpResponseCode);
        alertaError();
        delay(2000);
        intento++;
        http.end();
        continue;
      }
      else if (httpResponseCode >= 500)
      {
        Serial.printf("[HTTP] ERROR %d: Error del servidor. Reintentando...\n", httpResponseCode);
        lcd.clear();
        lcd.print("Error Servidor");
        lcd.setCursor(0, 1);
        lcd.print("Cod: ");
        lcd.print(httpResponseCode);
        alertaError();
        delay(4000);
        intento++;
        http.end();
        continue;
      }

      lcd.clear();
      lcd.print("POST Exitoso!");
      lcd.setCursor(0, 1);
      lcd.print("Etapa ");
      lcd.print(etapaNumero);
      lcd.print(" - ");
      lcd.print(httpResponseCode);
      ledFlash(LED_SUCCESS);

      if (response.indexOf("Ya existe una asistencia para este estudiante") != -1)
      {
        Serial.println("[HTTP] SUGERENCIA: El backend gestiona automáticamente los duplicados.");
      }
      enviado = true;
      delay(1500);
    }
    else
    {
      Serial.printf("[HTTP] ERROR: %d\n", httpResponseCode);

      lcd.clear();
      lcd.print("HTTP ERROR");
      lcd.setCursor(0, 1);
      lcd.print("Etapa ");
      lcd.print(etapaNumero);
      lcd.print(" - ");
      lcd.print(httpResponseCode);
      alertaError();

      if (httpResponseCode == -11)
      {
        Serial.println("[HTTP] Error -11: Conexión perdida. Intentando reconectar WiFi...");
        lcd.clear();
        lcd.print("Reconectando");
        lcd.setCursor(0, 1);
        lcd.print("WiFi...");
        setupWiFi();
      }
      else if (httpResponseCode == -5)
      {
        Serial.println("[HTTP] Error -5: Timeout de lectura. El servidor no respondió a tiempo.");
        Serial.println("[HTTP] SUGERENCIA: Verifica la conexión WiFi y la disponibilidad del servidor.");
        lcd.clear();
        lcd.print("Timeout Error");
        lcd.setCursor(0, 1);
        lcd.print("Servidor lento");
      }
      else
      {
        Serial.println("[HTTP] Error desconocido. Revisa la documentación de HTTPClient.");
        lcd.clear();
        lcd.print("Error desconocido");
        lcd.setCursor(0, 1);
        lcd.print("Ver Serial");
      }

      Serial.println("[HTTP] Reintentando en 2 segundos...");
      delay(2000);
      intento++;
    }
    http.end();
  }
}

void lcdInitialize()
{
  lcd.begin(16, 2);
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("Connect");
}

void ledInitialize()
{
  pinMode(LED_LOADER, OUTPUT);
  pinMode(LED_SUCCESS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  pinMode(LED_DANGER, OUTPUT);
  pinMode(ZUMBADOR, OUTPUT);
  digitalWrite(LED_LOADER, LOW);
  digitalWrite(LED_SUCCESS, LOW);
  digitalWrite(LED_ERROR, LOW);
  digitalWrite(LED_DANGER, LOW);
  digitalWrite(ZUMBADOR, LOW);
}

void alertaError()
{
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_ERROR, HIGH);
    digitalWrite(ZUMBADOR, HIGH);
    delay(200);
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(ZUMBADOR, LOW);
    delay(200);
  }
}

void alertaDanger()
{
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(LED_DANGER, HIGH);
    delay(150);
    digitalWrite(LED_DANGER, LOW);
    delay(150);
  }
}

void ledFlash(int ledPin)
{
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
  }
}

void setup()
{

  Serial.begin(115200);
  lcdInitialize();
  ledInitialize();
  Serial.println("Iniciando ESP32 BLE Scanner");

  lcd.print("Iniciando BLE");

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK)
  {
    Serial.println("Error al inicializar controlador BT");
    while (1)
      ;
  }

  if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK)
  {
    Serial.println("Error al habilitar controlador BT");
    while (1)
      ;
  }

  if (esp_bluedroid_init() != ESP_OK)
  {
    Serial.println("Error al inicializar Bluedroid");
    while (1)
      ;
  }

  if (esp_bluedroid_enable() != ESP_OK)
  {
    Serial.println("Error al habilitar Bluedroid");
    while (1)
      ;
  }

  setupWiFi();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Esperando sincronización NTP...");
  time_t now = time(nullptr);
  int ntpWait = 0;
  while (now < 8 * 3600 * 2 && ntpWait < 30)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    ntpWait++;
  }
  Serial.println("\nHora sincronizada.");

  BLEDevice::init("ESP32-BLE-Receiver");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi desconectado, intentando reconectar...");
    setupWiFi();
  }

  unsigned long now = millis();
  if (etapa == 0 && (now - lastEtapaMillis > 1000))
  {
    Serial.println("[Etapa 1] Escaneando...");

    lcd.clear();
    lcd.print("Sondeo 1");
    lcd.setCursor(0, 1);
    lcd.print("Escaneando BLE...");

    digitalWrite(LED_LOADER, HIGH);

    etapa1_nuevos.clear();
    ids_etapa1_actual.clear();

    BLEDevice::getScan()->start(SCAN_TIME_SECONDS, false);

    unsigned long scanStartTime = millis();
    unsigned long lastLedToggle = millis();
    bool ledState = true;

    Serial.println("ScanStartTime: " + String(scanStartTime));
    Serial.println("LastLedToggle: " + String(lastLedToggle));
    Serial.println("SCAN_TIME_SECONDS: " + String(SCAN_TIME_SECONDS));
    Serial.println("millis(): " + String(millis()));

    digitalWrite(LED_LOADER, LOW);

    if (etapa1_nuevos.size() > 0)
    {
      Serial.printf("[Etapa 1] Se encontraron %d dispositivos\n", etapa1_nuevos.size());
      lcd.clear();
      lcd.print("Sondeo 1: ");
      lcd.print(etapa1_nuevos.size());
      lcd.setCursor(0, 1);
      lcd.print("Enviando POST...");
      ledFlash(LED_SUCCESS);
    }
    else
    {

      Serial.println("[Etapa 1] No se encontraron dispositivos");
      lcd.clear();
      lcd.print("Sondeo 1");
      lcd.setCursor(0, 1);
      lcd.print("Sin dispositivos");

      alertaDanger();
    }

    Serial.println("[Etapa 1] Enviando POST...");
    enviarAsistencias(etapa1_nuevos, 1);
    lastEtapaMillis = millis();
    etapa = 1;
  }
  else if (etapa == 1 && (now - lastEtapaMillis > 10000))
  {
    Serial.println("[Etapa 2] Escaneando...");

    lcd.clear();
    lcd.print("Sondeo 2");
    lcd.setCursor(0, 1);
    lcd.print("Escaneando BLE...");

    digitalWrite(LED_LOADER, HIGH);

    etapa2_nuevos.clear();
    ids_etapa2_actual.clear();

    BLEDevice::getScan()->start(SCAN_TIME_SECONDS, false);

    unsigned long scanStartTime = millis();
    unsigned long lastLedToggle = millis();
    bool ledState = true;

    digitalWrite(LED_LOADER, LOW);

    if (etapa2_nuevos.size() > 0)
    {
      Serial.printf("[Etapa 2] Se encontraron %d dispositivos\n", etapa2_nuevos.size());
      lcd.clear();
      lcd.print("Sondeo 2: ");
      lcd.print(etapa2_nuevos.size());
      lcd.setCursor(0, 1);
      lcd.print("Enviando POST...");
      ledFlash(LED_SUCCESS);
    }
    else
    {
      Serial.println("[Etapa 2] No se encontraron dispositivos");
      lcd.clear();
      lcd.print("Sondeo 2");
      lcd.setCursor(0, 1);
      lcd.print("Sin dispositivos");
      alertaDanger();
    }

    Serial.println("[Etapa 2] Enviando POST...");
    enviarAsistencias(etapa2_nuevos, 2);
    lastEtapaMillis = millis();
    etapa = 2;
  }
  else if (etapa == 2 && (now - lastEtapaMillis > 10000))
  {
    Serial.println("[Etapa 3] Escaneando...");

    lcd.clear();
    lcd.print("Sondeo 3");
    lcd.setCursor(0, 1);
    lcd.print("Escaneando BLE...");

    digitalWrite(LED_LOADER, HIGH);

    etapa3_nuevos.clear();
    ids_etapa3_actual.clear();

    BLEDevice::getScan()->start(SCAN_TIME_SECONDS, false);

    unsigned long scanStartTime = millis();
    unsigned long lastLedToggle = millis();
    bool ledState = true;

    digitalWrite(LED_LOADER, LOW);

    if (etapa3_nuevos.size() > 0)
    {
      Serial.printf("[Etapa 3] Se encontraron %d dispositivos\n", etapa3_nuevos.size());
      lcd.clear();
      lcd.print("Sondeo 3: ");
      lcd.print(etapa3_nuevos.size());
      lcd.setCursor(0, 1);
      lcd.print("Enviando POST...");
      ledFlash(LED_SUCCESS);
    }
    else
    {
      Serial.println("[Etapa 3] No se encontraron dispositivos");
      lcd.clear();
      lcd.print("Sondeo 3");
      lcd.setCursor(0, 1);
      lcd.print("Sin dispositivos");
      alertaDanger();
    }

    Serial.println("[Etapa 3] Enviando POST...");
    enviarAsistencias(etapa3_nuevos, 3);
    lastEtapaMillis = millis();
    etapa = 3;
  }
  else if (etapa == 3 && (now - lastEtapaMillis > 2000))
  {
    Serial.println("\n========================================");
    Serial.println("       ¡SONDEO DE CLASE FINALIZADO!    ");
    Serial.println("========================================");
    Serial.println("Todas las etapas completadas exitosamente.");
    Serial.println("Para repetir el sondeo, reinicie el ESP32.");
    Serial.println("========================================\n");

    lcd.clear();
    lcd.print("SONDEO DE CLASE");
    lcd.setCursor(0, 1);
    lcd.print("FINALIZADO!");

    ledFlash(LED_SUCCESS);
    delay(2000);

    etapa = 4;
  }
}