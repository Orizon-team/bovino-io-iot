# 🔄 Resetear Configuración del ESP32

## Opción 1: Usando el Monitor Serial

1. Sube el firmware normalmente
2. Abre el Monitor Serial
3. Cuando veas el portal iniciarse, el código automáticamente detectará que no hay configuración
4. El dispositivo abrirá el portal de configuración inicial

## Opción 2: Borrar Flash Completa (Limpieza Total)

Ejecuta este comando en PowerShell desde la carpeta del proyecto:

```powershell
C:\Users\uzieltzab\.platformio\penv\Scripts\platformio.exe run --target erase
```

Luego sube el firmware de nuevo:

```powershell
C:\Users\uzieltzab\.platformio\penv\Scripts\platformio.exe run --target upload
```

## Opción 3: Código Temporal para Limpiar (Agregarlo al setup())

Agrega estas líneas AL INICIO de `setup()` en `main.cpp`, sube el código, luego bórralas y vuelve a subir:

```cpp
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // ⚠️ TEMPORAL: LIMPIAR TODA LA CONFIGURACIÓN
    wifiManager.clearAllConfig();
    Serial.println("✓ Configuración limpiada - Reiniciando...");
    delay(2000);
    ESP.restart();
    // ⚠️ FIN TEMPORAL - Borrar estas líneas después de usarlas
    
    // ... resto del código normal
}
```

## ¿Qué se limpia?

- ✅ Credenciales WiFi (SSID y Password)
- ✅ Modo del dispositivo (MAESTRO/ESCLAVO)
- ✅ MAC del maestro (si es ESCLAVO)
- ✅ Todos los datos de Preferences

## Resultado

Después de limpiar la configuración, el dispositivo:
1. Iniciará sin configuración
2. Abrirá automáticamente el portal de configuración
3. Te pedirá configurar como MAESTRO o ESCLAVO desde cero
