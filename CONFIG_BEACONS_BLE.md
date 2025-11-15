# 📡 Configuración de Beacons BLE - BovinoIOT

## 🎯 Problema Resuelto

Antes el sistema detectaba **TODOS los dispositivos BLE** cercanos (iPhones, smartwatches, etc.).
Ahora solo detecta **TUS beacons** usando filtros UUID y Company ID.

---

## ⚙️ Configuración en `config.h`

### **Opción 1: Filtrar por UUID de Servicio** (RECOMENDADO)

Edita las siguientes líneas en `include/config.h`:

```cpp
// UUID de servicio de tus beacons (CAMBIA ESTO)
#define BEACON_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// Activar filtrado por UUID
constexpr bool FILTER_BY_UUID = true;
```

### **Opción 2: Filtrar por Company ID**

Si tus beacons usan manufacturer data con Company ID personalizado:

```cpp
// Company ID de tus beacons (CAMBIA ESTO)
#define TARGET_COMPANY_ID 0x1234

// Desactivar filtrado por UUID
constexpr bool FILTER_BY_UUID = false;
```

---

## 🔍 ¿Cómo Saber el UUID de Mis Beacons?

### **Método 1: Usar App BLE Scanner (Android/iOS)**

1. Descarga "nRF Connect" o "BLE Scanner"
2. Escanea tus beacons
3. Busca "Services" o "Service UUID"
4. Copia el UUID completo (ejemplo: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`)

### **Método 2: Modo Debug (Temporal)**

1. **Desactiva el filtro temporalmente:**
   ```cpp
   constexpr bool FILTER_BY_UUID = false;
   ```

2. **Sube el código** y abre Serial Monitor

3. **Busca en el log:**
   ```
   [BLE] [DETECTADO] MAC=dc:0d:30:2c:e8:c6, RSSI=-59 dBm
   [BLE] Service UUID: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   ```

4. **Copia el UUID** y configúralo en `config.h`

5. **Reactiva el filtro:**
   ```cpp
   constexpr bool FILTER_BY_UUID = true;
   ```

---

## 📋 UUIDs Comunes

| Tipo de Beacon | UUID de Servicio | Company ID |
|----------------|------------------|------------|
| **iBeacon (Apple)** | Manufacturer Data | `0x004C` |
| **Eddystone (Google)** | `0000FEAA-0000-1000-8000-00805F9B34FB` | `0xFEAA` |
| **Custom ESP32** | Personalizado | Personalizado |
| **Nordic Semi** | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | `0x0059` |

---

## 🛠️ Configuración de Tus Beacons ESP32

Si estás programando tus propios beacons con ESP32, usa este código:

```cpp
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEBeacon.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"  // ⬅️ MISMO que en config.h
#define BEACON_UUID "8ec76ea3-6668-48da-9866-75be8bc86f4d"   // UUID del beacon individual

void setup() {
  BLEDevice::init("Animal_Tag_001");  // Nombre del beacon
  
  BLEServer *pServer = BLEDevice::createServer();
  
  // Crear servicio con UUID específico
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Crear characteristic para datos del animal
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    BEACON_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  
  // Configurar manufacturer data con ID del animal
  BLEAdvertisementData advertisementData;
  uint8_t manufacturerData[6];
  
  // Company ID (0x1234)
  manufacturerData[0] = 0x34;
  manufacturerData[1] = 0x12;
  
  // Animal ID (ejemplo: 12345)
  uint32_t animalId = 12345;
  manufacturerData[2] = (animalId >> 0) & 0xFF;
  manufacturerData[3] = (animalId >> 8) & 0xFF;
  manufacturerData[4] = (animalId >> 16) & 0xFF;
  manufacturerData[5] = (animalId >> 24) & 0xFF;
  
  advertisementData.setManufacturerData(std::string((char*)manufacturerData, 6));
  
  pService->start();
  
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAdvertisementData(advertisementData);
  pAdvertising->start();
  
  Serial.println("Beacon iniciado!");
}

void loop() {
  delay(1000);
}
```

---

## ✅ Verificar que Funciona

Después de configurar, deberías ver en el Serial Monitor:

**ANTES (detectaba todo):**
```
[BLE] [DETECTADO] MAC=aa:bb:cc:dd:ee:ff, CompanyID=0x004C  ← iPhone
[BLE] [DETECTADO] MAC=11:22:33:44:55:66, CompanyID=0x006D  ← Smartwatch
[BLE] [DETECTADO] MAC=dc:0d:30:2c:e8:c6, CompanyID=0x1234  ← Tu beacon
```

**DESPUÉS (solo tus beacons):**
```
[BLE] [DETECTADO] MAC=dc:0d:30:2c:e8:c6, CompanyID=0x1234, Animal ID=12345 ✅
```

---

## 🔧 Troubleshooting

### **Problema: No detecta ningún beacon**

1. Verifica que `BEACON_SERVICE_UUID` coincida con el de tus beacons
2. Prueba desactivar el filtro temporalmente:
   ```cpp
   constexpr bool FILTER_BY_UUID = false;
   ```
3. Revisa los logs para ver qué UUIDs se detectan

### **Problema: Detecta demasiados dispositivos**

1. Activa el filtro UUID:
   ```cpp
   constexpr bool FILTER_BY_UUID = true;
   ```
2. Verifica que el UUID esté correcto

### **Problema: Error -11 en API**

✅ **YA SOLUCIONADO** en esta actualización:
- Reducido tamaño del JSON (2048 → 1536 bytes)
- Agregado delay de estabilización de memoria
- Mejor manejo de reconexión WiFi

---

## 📊 Resumen de Cambios

| Antes | Después |
|-------|---------|
| ❌ Detecta todos los BLE | ✅ Solo tus beacons |
| ❌ iPhones como "animales" | ✅ Filtro UUID/Company ID |
| ❌ Error -11 frecuente | ✅ Memoria optimizada |
| ❌ Sin configuración | ✅ Fácil configuración |

---

**¡Todo listo! Configura tu UUID y disfruta del sistema 🚀**
