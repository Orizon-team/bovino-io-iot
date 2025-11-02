# Configuración Maestro-Esclavo con ESP-NOW

## 📋 Resumen del Sistema

Este sistema permite que dispositivos ESP32 **sin WiFi (esclavos)** envíen datos a un ESP32 **con WiFi (maestro)** usando **ESP-NOW**, y el maestro se encarga de enviar todo al backend.

---

## 🔧 Configuración en `config.cpp`

### **Paso 1: Configurar el Maestro**

```cpp
// En src/modules/config.cpp

// Modo del dispositivo
DeviceMode CURRENT_DEVICE_MODE = DEVICE_MASTER;  // ← MAESTRO

// MAC del maestro (no importante para el maestro mismo)
uint8_t MASTER_MAC_ADDRESS[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// WiFi (solo para maestro)
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASSWORD = "TU_PASSWORD";
```

### **Paso 2: Obtener MAC del Maestro**

1. Sube el código al ESP32 maestro
2. Abre el Monitor Serial
3. Busca la línea que dice:
   ```
   [ESP-NOW] MAC Address: AA:BB:CC:DD:EE:FF
   ```
4. **Copia esa dirección MAC** (la necesitarás para los esclavos)

---

### **Paso 3: Configurar los Esclavos**

```cpp
// En src/modules/config.cpp

// Modo del dispositivo
DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;  // ← ESCLAVO

// MAC del maestro (pegar la que copiaste)
uint8_t MASTER_MAC_ADDRESS[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
//                                 ↑ Reemplazar con la MAC real del maestro

// WiFi NO ES NECESARIO para esclavos (puedes dejarlo vacío)
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
```

---

## 📡 Cómo Funciona

### **Flujo de Datos:**

```
┌─────────────────┐
│  ESP32 ESCLAVO  │ (Sin WiFi)
│  Zona: Bebedero │
└────────┬────────┘
         │
         │ ESP-NOW (cada 30 seg)
         │
         ▼
┌─────────────────┐
│  ESP32 MAESTRO  │ (Con WiFi)
│  Zona: Comedero │
└────────┬────────┘
         │
         │ HTTP/API (cada 5 min)
         │
         ▼
┌─────────────────┐
│     Backend     │
│  (Render/Cloud) │
└─────────────────┘
```

### **Esclavo:**
1. Escanea beacons BLE de ganado
2. Cada 30 segundos envía datos al maestro vía ESP-NOW
3. **NO necesita WiFi**
4. Muestra en LCD: "Enviando... A maestro"

### **Maestro:**
1. Escanea beacons BLE de su propia zona
2. Recibe datos de todos los esclavos vía ESP-NOW
3. Cada 5 minutos sincroniza TODO al backend vía WiFi/HTTP
4. Muestra en LCD: "Sincronizando Espere..."

---

## 🎯 Configuración Recomendada

### **Ejemplo con 3 Dispositivos:**

#### **Dispositivo 1: Comedero (MAESTRO)**
```cpp
const char* DEVICE_ID = "IOT_ZONA_001";
const char* DEVICE_LOCATION = "Comedero Norte";
ZoneType CURRENT_ZONE_TYPE = ZONE_FEEDER;
DeviceMode CURRENT_DEVICE_MODE = DEVICE_MASTER;

const char* WIFI_SSID = "MiWiFi";
const char* WIFI_PASSWORD = "12345678";
```

#### **Dispositivo 2: Bebedero (ESCLAVO)**
```cpp
const char* DEVICE_ID = "IOT_ZONA_002";
const char* DEVICE_LOCATION = "Bebedero Sur";
ZoneType CURRENT_ZONE_TYPE = ZONE_WATERER;
DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;

// MAC del maestro (la que obtuviste del Dispositivo 1)
uint8_t MASTER_MAC_ADDRESS[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
```

#### **Dispositivo 3: Pastoreo (ESCLAVO)**
```cpp
const char* DEVICE_ID = "IOT_ZONA_003";
const char* DEVICE_LOCATION = "Pastoreo Este";
ZoneType CURRENT_ZONE_TYPE = ZONE_PASTURE;
DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;

// Misma MAC del maestro
uint8_t MASTER_MAC_ADDRESS[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
```

---

## ⚙️ Parámetros Ajustables

```cpp
// En config.h

// Intervalo de envío esclavo → maestro (30 segundos)
constexpr unsigned long ESPNOW_SEND_INTERVAL = 30000;

// Intervalo de sincronización maestro → backend (5 minutos)
constexpr unsigned long SYNC_INTERVAL = 300000;

// Máximo de esclavos soportados
constexpr int MAX_SLAVES = 10;

// Canal ESP-NOW (1-13)
constexpr int ESPNOW_CHANNEL = 1;
```

---

## 🔍 Verificar Funcionamiento

### **En el Monitor Serial del Maestro:**

```
[ESP-NOW] Inicializando como MAESTRO...
[ESP-NOW] ✅ Maestro inicializado correctamente
[ESP-NOW] MAC Address: AA:BB:CC:DD:EE:FF

[ESP-NOW] 📨 Mensaje recibido de 11:22:33:44:55:66
[ESP-NOW] 🐄 Datos: Device=IOT_ZONA_002, Zona=Bebedero Sur, Animal ID=1234

[SYNC] ━━━━━ Sincronización ━━━━━
[SYNC] Animales detectados localmente: 3
[MAESTRO] 📨 Mensajes de esclavos: 5
[SYNC] ✓ Datos enviados al backend
```

### **En el Monitor Serial del Esclavo:**

```
[ESP-NOW] Inicializando como ESCLAVO...
[ESP-NOW] ✅ Esclavo inicializado correctamente
[ESP-NOW] MAC Address: 11:22:33:44:55:66
[ESP-NOW] Maestro configurado: AA:BB:CC:DD:EE:FF

[ESP-NOW] ━━━━━ Enviando a Maestro ━━━━━
[ESP-NOW] Animales detectados: 2
[ESP-NOW] ✅ Mensaje enviado a maestro (Animal ID: 1234)
[ESP-NOW] ✅ Mensaje enviado a maestro (Animal ID: 5678)
```

---

## ⚠️ Solución de Problemas

### **Esclavo no envía datos:**
1. Verifica que `MASTER_MAC_ADDRESS` sea correcta
2. Asegúrate de que ambos ESP32 estén en el mismo canal WiFi
3. Mantén distancia < 100 metros entre dispositivos

### **Maestro no recibe datos:**
1. Verifica que el maestro esté inicializado correctamente
2. Revisa el Monitor Serial para ver si hay mensajes ESP-NOW
3. Verifica que los esclavos tengan la MAC correcta del maestro

### **WiFi no conecta en maestro:**
1. Verifica SSID y password en `config.cpp`
2. El maestro puede funcionar sin WiFi (solo ESP-NOW) temporalmente
3. Los datos se guardarán en buffer offline hasta que WiFi regrese

---

## ✅ Checklist de Configuración

- [ ] Configurar maestro con `DEVICE_MASTER`
- [ ] Subir código al maestro y obtener su MAC
- [ ] Configurar esclavos con `DEVICE_SLAVE`
- [ ] Pegar MAC del maestro en `MASTER_MAC_ADDRESS` de cada esclavo
- [ ] Configurar WiFi solo en el maestro
- [ ] Asignar IDs únicos a cada dispositivo (`IOT_ZONA_001`, `002`, etc.)
- [ ] Asignar nombres descriptivos (`Comedero Norte`, `Bebedero Sur`, etc.)
- [ ] Verificar que todos los esclavos envían datos al maestro
- [ ] Verificar que el maestro sincroniza al backend

---

## 🎓 Conceptos Clave

**ESP-NOW:** Protocolo de comunicación peer-to-peer de Espressif que funciona sin WiFi router. Permite comunicación directa entre ESP32.

**Ventajas:**
- ✅ No necesita router WiFi
- ✅ Muy bajo consumo de energía
- ✅ Rápido (latencia < 10ms)
- ✅ Alcance hasta 200m en campo abierto

**Limitaciones:**
- ❌ Máximo 250 bytes por mensaje
- ❌ Requiere estar en el mismo canal WiFi
- ❌ No es seguro por defecto (puedes habilitar encriptación)
