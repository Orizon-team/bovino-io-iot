# 🔧 Solución Error -11 (SSL Timeout) - BovinoIOT

## ❌ Problema

```
[API] ❌ Error de conexión: -11
[API] Reconectando WiFi...
```

Este error ocurre cuando el ESP32 **NO tiene suficiente memoria RAM** para establecer una conexión HTTPS/SSL.

---

## 🎯 Causa Raíz

### **1. Portal WiFi Activo = Consumo de RAM**

El portal de configuración (modo `WIFI_AP_STA`) consume ~25-30KB de RAM:
- Servidor web (WebServer)
- DNS Server (para captive portal)
- Buffers de WiFi en modo dual (AP + Station)

### **2. Cliente SSL Necesita Memoria Contigua**

`WiFiClientSecure` (HTTPS) necesita:
- ~12-15KB de heap **contiguo** para handshake SSL/TLS
- Buffers de certificados y cifrado

### **3. Fragmentación del Heap**

Cuando el portal está activo:
```
Memoria total: ~320KB
- Portal WiFi: ~30KB
- BLE Scanner: ~20KB
- JSON buffers: ~2KB
- Sistema: ~40KB
-------------------
Disponible: ~228KB (pero fragmentado)
```

El problema: aunque hay 228KB "disponibles", no hay un bloque **contiguo** de 15KB para SSL.

---

## ✅ Soluciones Implementadas

### **Solución 1: Cerrar Portal Después de Configurar**

**Antes:**
```cpp
// El portal se mantenía activo en el loop()
if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
    wifiManager.loop(); // ← Consume RAM constantemente
}
```

**Después:**
```cpp
// Se cierra el portal después de conectar WiFi
if (wifiConnected) {
    if (wifiManager.isPortalActive()) {
        Serial.println("[INIT] Cerrando portal para liberar memoria...");
        wifiManager.stopConfigPortal();  // ← Libera ~30KB
    }
}

// Portal deshabilitado en loop() - ahorrando RAM
/*
if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
    wifiManager.loop(); // COMENTADO
}
*/
```

**Beneficio:** Libera ~30KB de RAM

---

### **Solución 2: Reducir Tamaño del JSON**

**Antes:**
```cpp
DynamicJsonDocument doc(2048);  // 2KB reservados
```

**Después:**
```cpp
DynamicJsonDocument doc(1536);  // 1.5KB reservados
```

**Beneficio:** Ahorra 512 bytes

---

### **Solución 3: Delays de Estabilización**

```cpp
// Dar tiempo para que el heap se reorganice
if (attempt > 1) {
    Serial.println("[API] Esperando estabilización de heap...");
    delay(2000);  // ← Permite que el garbage collector trabaje
}
```

**Beneficio:** Permite desfragmentación del heap

---

### **Solución 4: Logging de Memoria**

```cpp
Serial.printf("[API] Memoria libre: %d bytes\n", ESP.getFreeHeap());
Serial.printf("[INIT] Memoria libre después de cerrar portal: %d bytes\n", ESP.getFreeHeap());
```

**Beneficio:** Diagnóstico en tiempo real

---

## 📊 Resultados Esperados

### **Antes (con portal activo):**
```
[API] Intento #1 - POST a: https://...
[API] ❌ Error de conexión: -11
[API] Intento #2 - POST a: https://...
[API] ❌ Error de conexión: -11
[API] Intento #3 - POST a: https://...
[API] ❌ Error de conexión: -11
```

### **Después (portal cerrado):**
```
[INIT] Cerrando portal de configuración para liberar memoria...
[INIT] Memoria libre después de cerrar portal: 185432 bytes

[API] Intento #1/3
[API] Memoria libre: 185432 bytes
[API] POST a: https://...
[API] Código HTTP: 200
[API] ✓ Detecciones enviadas correctamente
```

---

## 🔍 Cómo Verificar que Funciona

### **1. Memoria al Inicio**
```
[INIT] ✓ WiFi conectado: 192.168.100.83
[INIT] Cerrando portal de configuración para liberar memoria...
[INIT] Memoria libre después de cerrar portal: XXXXX bytes  ← Debe ser >180KB
```

### **2. Memoria Antes de API**
```
[API] Intento #1/3
[API] Memoria libre: XXXXX bytes  ← Debe ser >180KB
[API] POST a: https://...
```

### **3. Respuesta Exitosa**
```
[API] Código HTTP: 200
[API] Respuesta:
{"success":true,...}
[API] ✓ Detecciones enviadas correctamente
```

---

## 🛠️ Troubleshooting

### **Si Sigue Fallando:**

#### **Opción A: Verificar Memoria Disponible**
```cpp
// En setup(), después de inicializar WiFi:
Serial.printf("[DEBUG] Heap total: %d bytes\n", ESP.getHeapSize());
Serial.printf("[DEBUG] Heap libre: %d bytes\n", ESP.getFreeHeap());
Serial.printf("[DEBUG] Heap mínimo: %d bytes\n", ESP.getMinFreeHeap());
```

#### **Opción B: Desactivar BLE Temporalmente**
```cpp
// En main.cpp, comentar temporalmente:
// bleScanner.performScan();  // ← Libera ~20KB
```

#### **Opción C: Reducir Beacons por Request**
Si tienes muchos animales (>5), enviar en lotes:

```cpp
// En sendDetections():
const int MAX_BEACONS_PER_REQUEST = 5;
int count = 0;
std::map<uint32_t, BeaconData> batch;

for (const auto& pair : beacons) {
    batch[pair.first] = pair.second;
    count++;
    
    if (count >= MAX_BEACONS_PER_REQUEST) {
        sendDetectionsBatch(batch);  // Enviar lote
        batch.clear();
        count = 0;
        delay(2000);  // Pausa entre lotes
    }
}
```

---

## ⚡ Recomendaciones

### **Para Producción:**

1. **Mantén el portal cerrado** durante operación normal
2. **Usa el botón de reset** (GPIO 27) para reconfigurar
3. **Monitorea la memoria** periódicamente
4. **Limita beacons a 5-10** por zona para evitar problemas

### **Memoria Recomendada:**

| Estado | Memoria Libre | Estado |
|--------|---------------|--------|
| > 180KB | ✅ Óptimo | Sin problemas |
| 150-180KB | ⚠️ Moderado | Puede fallar ocasionalmente |
| < 150KB | ❌ Crítico | Error -11 garantizado |

---

## 📝 Notas Adicionales

### **¿Por Qué No HTTP en Lugar de HTTPS?**

El backend requiere HTTPS. Para usar HTTP:
```cpp
// En config.cpp, cambiar:
const char* API_URL = "http://bovino-io-backend.onrender.com/detections/ingest";
```

Pero esto **no se recomienda** por seguridad.

### **¿El Portal Se Puede Reactivar?**

Sí, tienes 3 opciones:

1. **Botón de Reset** (GPIO 27 por 3 segundos) → Resetea y abre portal
2. **Descomentar en loop():**
   ```cpp
   if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
       wifiManager.loop();
   }
   ```
3. **Forzar desde código:**
   ```cpp
   wifiManager.startConfigPortal();
   ```

---

**¡Todo listo! El error -11 debería estar resuelto. 🚀**
