# Configuración de Feasybeacons por UUID

## Problema Resuelto
Los beacons Feasybeacon tienen Company ID `0x004C` (Apple iBeacon) que no se puede cambiar desde la app móvil. La solución es **identificar los beacons por su UUID** en lugar del Company ID.

## Pasos de Configuración

### 1. Obtener el UUID de tus Beacons

Desde la **app móvil de Feasybeacon**:
1. Abre la app y conecta con cada uno de tus 3 beacons
2. Busca el campo **UUID** (formato: `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`)
3. **Anota el UUID** - será el mismo para todos tus beacons si configuraste un grupo

Ejemplo de UUID:
```
FDA50693-A4E2-4FB1-AFCF-C6EB07647825
```

### 2. Configurar Major y Minor para cada Beacon

En la app de Feasybeacon, configura **Major** y **Minor** para identificar cada animal:

#### Opción A: Usar Major como ID del Animal (Recomendado)
- **Beacon 1**: Major = `1`, Minor = `0`
- **Beacon 2**: Major = `2`, Minor = `0`
- **Beacon 3**: Major = `3`, Minor = `0`

Con esta configuración, el sistema extraerá el ID del animal desde el campo **Major**.

#### Opción B: Combinar Major + Minor
- **Beacon 1**: Major = `1000`, Minor = `1`
- **Beacon 2**: Major = `1000`, Minor = `2`
- **Beacon 3**: Major = `1000`, Minor = `3`

El sistema combinará Major y Minor en un ID único de 32 bits.

### 3. Configurar el Código

Edita el archivo `include/config.h`:

```cpp
// UUID de tus beacons (CAMBIA ESTO)
#define BEACON_UUID_1 "FDA50693-A4E2-4FB1-AFCF-C6EB07647825"  // ← Tu UUID aquí

// Si tienes beacons con diferentes UUIDs, agrégalos aquí
#define BEACON_UUID_2 "00000000-0000-0000-0000-000000000000"  // Opcional
#define BEACON_UUID_3 "00000000-0000-0000-0000-000000000000"  // Opcional

// Modo de filtrado
constexpr FilterMode BEACON_FILTER_MODE = FILTER_BY_UUID;  // ✓ YA ESTÁ CONFIGURADO

// Cómo extraer el ID del animal
constexpr AnimalIdSource ANIMAL_ID_SOURCE = USE_MAJOR_ONLY;  // ← CAMBIA SEGÚN OPCIÓN
```

### 4. Opciones de `ANIMAL_ID_SOURCE`

| Opción | Descripción | Rango de IDs |
|--------|-------------|--------------|
| `USE_MAJOR_ONLY` | Solo usa Major | 0 - 65,535 |
| `USE_MINOR_ONLY` | Solo usa Minor | 0 - 65,535 |
| `USE_MAJOR_MINOR` | Combina Major+Minor | 0 - 4,294,967,295 |
| `USE_MAC_ADDRESS` | Usa MAC del beacon | Basado en dirección MAC |

**Recomendación**: `USE_MAJOR_ONLY` para simplicidad.

## Ejemplo de Configuración Completa

### En la App Feasybeacon:
```
Beacon 1:
  UUID:  FDA50693-A4E2-4FB1-AFCF-C6EB07647825
  Major: 101
  Minor: 0
  
Beacon 2:
  UUID:  FDA50693-A4E2-4FB1-AFCF-C6EB07647825
  Major: 102
  Minor: 0
  
Beacon 3:
  UUID:  FDA50693-A4E2-4FB1-AFCF-C6EB07647825
  Major: 103
  Minor: 0
```

### En `config.h`:
```cpp
#define BEACON_UUID_1 "FDA50693-A4E2-4FB1-AFCF-C6EB07647825"
constexpr FilterMode BEACON_FILTER_MODE = FILTER_BY_UUID;
constexpr AnimalIdSource ANIMAL_ID_SOURCE = USE_MAJOR_ONLY;
```

### Resultado Esperado:
```
[BLE] [DETECTADO] MAC=dc:0d:30:2c:e8:01, RSSI=-65 dBm
[BLE] 🔍 Analizando beacon: CompanyID=0x004C, Length=25
[BLE]   ✓ Formato iBeacon detectado
[BLE]   UUID: FDA50693-A4E2-4FB1-AFCF-C6EB07647825
[BLE]   Major=101, Minor=0, TxPower=-59 dBm
[BLE]   ✓ Animal ID=101 (Major)
[BLE] 📡 Beacon: ID=101, RSSI=-65 dBm, Dist=1.23m
```

## Verificación

Después de cargar el código:

1. Abre el **Monitor Serial** (115200 baud)
2. Observa los mensajes de detección:
   ```
   [BLE] ━━━━━ Escaneo #1 [Modo: NORMAL] ━━━━━
   [BLE] [DETECTADO] MAC=...
   [BLE]   ✓ Formato iBeacon detectado
   [BLE]   UUID: ...
   [BLE]   Major=XXX, Minor=YYY
   ```

Si **NO ves tus beacons**, verifica:
- El UUID en `config.h` coincide EXACTAMENTE con el de la app
- `BEACON_FILTER_MODE = FILTER_BY_UUID`
- Los beacons están encendidos y transmitiendo
- El RSSI es mayor a -90 dBm (MIN_RSSI_THRESHOLD)

## Modo Debug (Ver Todos los Beacons)

Para ver **TODOS** los beacons detectados (sin filtrar):

```cpp
constexpr FilterMode BEACON_FILTER_MODE = FILTER_DISABLED;
```

Esto te permitirá ver qué beacons detecta el ESP32 y verificar los UUIDs.

## Modos Alternativos de Filtrado

Si el UUID no funciona, puedes usar:

### Por Prefijo de MAC:
```cpp
#define BEACON_MAC_PREFIX "dc:0d:30:2c:e8"  // Primeros 5 bytes de la MAC
constexpr FilterMode BEACON_FILTER_MODE = FILTER_BY_MAC_PREFIX;
```

### Sin Filtro (Debug):
```cpp
constexpr FilterMode BEACON_FILTER_MODE = FILTER_DISABLED;
```

---

## Resumen

✅ **Ventaja del filtrado por UUID**: No depende del Company ID, funciona con cualquier beacon iBeacon estándar

✅ **Flexible**: Puedes tener múltiples grupos de beacons con diferentes UUIDs

✅ **Compatible**: Funciona con Feasybeacon y cualquier otro beacon compatible con iBeacon
