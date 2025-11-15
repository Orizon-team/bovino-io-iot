# ✅ Solución: Detección de Feasybeacons con Company ID de iPhone

## Problema
Los beacons Feasybeacon tienen Company ID `0x004C` (Apple/iPhone) que no se puede cambiar desde la app móvil. El sistema anterior solo detectaba beacons con Company ID personalizado, por lo que no detectaba estos beacons.

## Solución Implementada

### 1. Filtrado por UUID de iBeacon
En lugar de filtrar por Company ID, ahora el sistema puede filtrar por **UUID del iBeacon**, que sí es configurable desde la app Feasybeacon.

### 2. Extracción de ID por Major/Minor
El sistema ahora extrae el ID del animal desde los campos **Major** y **Minor** del iBeacon, con 3 modos disponibles:
- `USE_MAJOR_ONLY`: Solo usa Major (0-65535)
- `USE_MINOR_ONLY`: Solo usa Minor (0-65535)  
- `USE_MAJOR_MINOR`: Combina ambos (0-4,294,967,295)

### 3. Múltiples Modos de Filtrado
Agregamos soporte para diferentes métodos de filtrado:
- ✅ `FILTER_BY_UUID` - **Por UUID de iBeacon (RECOMENDADO)**
- `FILTER_BY_MAC_PREFIX` - Por prefijo de dirección MAC
- `FILTER_BY_NAME_PREFIX` - Por prefijo del nombre
- `FILTER_BY_COMPANY_ID` - Por Company ID (modo anterior)
- `FILTER_DISABLED` - Sin filtro (debug)

## Archivos Modificados

### `include/config.h`
- ✅ Agregado soporte para 3 UUIDs diferentes
- ✅ Nuevo enum `BeaconFilterMode` con opciones de filtrado
- ✅ Nuevo enum `AnimalIdSource` para extraer ID
- ✅ Configuración por defecto: `FILTER_BY_UUID` + `USE_MAJOR_MINOR`

### `src/modules/ble_scanner.cpp`
- ✅ Función `matchesBeaconUUID()` para comparar UUIDs
- ✅ Lógica de filtrado multi-modo en `processDevice()`
- ✅ Parser completo de formato iBeacon en `extractAnimalId()`
- ✅ Extracción de Major/Minor desde manufacturer data
- ✅ Logs detallados para debug

## Configuración Necesaria

### Paso 1: Obtener UUID de tus Beacons
Abre la app Feasybeacon y anota el UUID de tus beacons. Por ejemplo:
```
FDA50693-A4E2-4FB1-AFCF-C6EB07647825
```

### Paso 2: Configurar Major/Minor
En cada beacon configura:
- **Beacon 1**: Major = `1`, Minor = `0`
- **Beacon 2**: Major = `2`, Minor = `0`
- **Beacon 3**: Major = `3`, Minor = `0`

### Paso 3: Editar `config.h`
```cpp
// Cambiar el UUID
#define BEACON_UUID_1 "FDA50693-A4E2-4FB1-AFCF-C6EB07647825"

// Verificar configuración
constexpr BeaconFilterMode BEACON_FILTER_MODE = FILTER_BY_UUID;
constexpr AnimalIdSource ANIMAL_ID_SOURCE = USE_MAJOR_ONLY;
```

### Paso 4: Compilar y Cargar
```bash
platformio run --target upload
```

## Salida Esperada en Monitor Serial

```
[BLE] ━━━━━ Escaneo #1 [Modo: NORMAL] ━━━━━
[BLE] [DETECTADO] MAC=dc:0d:30:2c:e8:01, RSSI=-65 dBm, CompanyID=0x004C, Bytes=25
[BLE] 🔍 Analizando beacon: CompanyID=0x004C, Length=25
[BLE]   ✓ Formato iBeacon detectado
[BLE]   UUID: FDA50693-A4E2-4FB1-AFCF-C6EB07647825
[BLE]   Major=1, Minor=0, TxPower=-59 dBm
[BLE]   ✓ Animal ID=1 (Major)
[BLE] 📡 Beacon: ID=1, RSSI=-65 dBm, Dist=1.23m, MAC=dc:0d:30:2c:e8:01
[BLE] [VACA DETECTADA] Nuevo animal detectado: ID=1, Distancia=1.23m
[BLE] Animales en zona: 1 | Cambios recientes: 1 | Sin cambios: 0
```

## Ventajas

✅ **Compatible con Feasybeacon**: Funciona con Company ID 0x004C
✅ **Flexible**: Soporta múltiples UUIDs
✅ **Estándar iBeacon**: Compatible con cualquier beacon iBeacon
✅ **Filtrado robusto**: Evita detectar otros dispositivos BLE
✅ **Debug mejorado**: Logs detallados para troubleshooting

## Alternativas de Filtrado

Si el UUID no funciona o tienes problemas, puedes cambiar el modo:

### Opción A: Por Prefijo de MAC
```cpp
constexpr BeaconFilterMode BEACON_FILTER_MODE = FILTER_BY_MAC_PREFIX;
#define BEACON_MAC_PREFIX "dc:0d:30:2c:e8"  // Primeros 5 bytes
```

### Opción B: Sin Filtro (Debug)
```cpp
constexpr BeaconFilterMode BEACON_FILTER_MODE = FILTER_DISABLED;
```
Esto mostrará **TODOS** los beacons detectados.

## Troubleshooting

### No detecta beacons
1. Verifica que el UUID en `config.h` coincida EXACTAMENTE con el de la app
2. Verifica que `BEACON_FILTER_MODE = FILTER_BY_UUID`
3. Prueba con `FILTER_DISABLED` para ver todos los beacons
4. Verifica que RSSI > -90 dBm

### IDs incorrectos
1. Verifica Major/Minor en la app Feasybeacon
2. Cambia `ANIMAL_ID_SOURCE` según tu configuración
3. Revisa los logs en Monitor Serial

### Beacons intermitentes
1. Aumenta `MIN_RSSI_THRESHOLD` (ej: -80)
2. Ajusta intervalos de escaneo en `config.h`
3. Verifica batería de los beacons

## Referencias
- **Manual completo**: `CONFIG_FEASYBEACON_UUID.md`
- **Configuración beacons**: `CONFIG_BEACONS_BLE.md`
- **Formato iBeacon**: [Apple iBeacon Specification](https://developer.apple.com/ibeacon/)
