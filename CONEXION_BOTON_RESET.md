# 🔴 Botón de Reset - Conexión Hardware

## 📋 Componentes Necesarios
- **1x Pulsador (Push Button)** - Normalmente Abierto (NO)
- **1x Resistencia 10kΩ** (opcional, el ESP32 ya tiene pull-up interno)
- **Cables Dupont** macho-macho o macho-hembra según tu protoboard

---

## 🔌 Diagrama de Conexión

### ✅ CONEXIÓN RECOMENDADA (Con Pull-up Interno)
**La más simple, usa el pull-up interno del ESP32:**

```
ESP32                    PULSADOR
┌─────────┐              ┌──┐
│         │              │  │
│ GPIO 27 ├──────────────┤  ├────┐
│         │              │  │    │
│         │              └──┘    │
│         │                      │
│     GND ├──────────────────────┘
│         │
└─────────┘
```

**Instrucciones:**
1. **Terminal 1 del pulsador** → Conectar a **GPIO 27** del ESP32
2. **Terminal 2 del pulsador** → Conectar a **GND** del ESP32

> ⚡ **Importante:** El código usa `INPUT_PULLUP`, por lo que el pin está normalmente en HIGH (3.3V) y va a LOW cuando presionas el botón.

---

### 🔧 CONEXIÓN ALTERNATIVA (Con Resistencia Pull-up Externa)
**Si prefieres una resistencia externa:**

```
          +3.3V
            │
            ├─── 10kΩ ───┐
            │             │
ESP32       │         PULSADOR
┌─────┐     │          ┌──┐
│     │     │          │  │
│ 27  ├─────┴──────────┤  ├────┐
│     │                │  │    │
│ GND ├────────────────┴──┘    │
│     │                         │
└─────┘                         │
                                │
                               GND
```

**Instrucciones:**
1. Un extremo de la **resistencia 10kΩ** a **3.3V** del ESP32
2. Otro extremo de la resistencia a **GPIO 27**
3. **Terminal 1 del pulsador** a **GPIO 27**
4. **Terminal 2 del pulsador** a **GND**

---

## ⚙️ Configuración en el Código

El botón está configurado en `config.h`:

```cpp
constexpr int RESET_BUTTON = 27;  // Pin GPIO27
constexpr unsigned long RESET_BUTTON_HOLD_TIME = 3000;  // 3 segundos
constexpr unsigned long DEBOUNCE_DELAY = 50;  // Anti-rebote 50ms
```

**Puedes cambiar el pin si es necesario**, pero asegúrate de usar un GPIO que:
- ✅ No esté en uso por otros componentes
- ✅ Soporte entrada digital
- ✅ No sea un pin especial (0, 2, 15 pueden causar problemas en el boot)

---

## 🎯 Funcionamiento

### Uso Normal
1. **Presiona y MANTÉN** el botón por **3 segundos completos**
2. El ESP32 mostrará en serial:
   ```
   [Reset] ⏳ Botón presionado - mantén 3 seg...
   [Reset] ⏱️  Presionado: 1000 ms / 3000 ms
   [Reset] ⏱️  Presionado: 2000 ms / 3000 ms
   [Reset] ⏱️  Presionado: 3000 ms / 3000 ms
   [Reset] ✅ RESET CONFIRMADO - Borrando configuración...
   ```
3. El LCD mostrará: `"RESETEANDO" / "Espere..."`
4. Se borrará **toda la configuración** (WiFi, modo, ubicación)
5. El ESP32 se **reiniciará automáticamente**
6. Al iniciar, abrirá el portal de configuración

### Si Sueltas Antes de 3 Segundos
```
[Reset] ❌ Liberado muy pronto (1523 ms)
```
No hará nada, debes mantenerlo presionado 3 segundos completos.

---

## 🛡️ Seguridad Implementada

- ✅ **Anti-rebote (Debouncing):** 50ms para evitar falsas lecturas
- ✅ **Validación de tiempo:** Requiere 3 segundos sostenidos
- ✅ **Feedback en tiempo real:** Mensajes en serial cada segundo
- ✅ **Confirmación visual:** LCD y LEDs indican el proceso
- ✅ **No accidental:** Es difícil presionar 3 segundos por error

---

## 🧪 Prueba del Botón

1. **Sube el código** al ESP32
2. **Abre el Serial Monitor** a 115200 baudios
3. **Presiona el botón** brevemente (< 3 seg) → Debería decir "Liberado muy pronto"
4. **Presiona y MANTÉN** 3+ segundos → Debería resetear y reiniciar

---

## 📊 Pines Utilizados - Resumen Completo

| Pin GPIO | Función           | Tipo          | Notas                          |
|----------|-------------------|---------------|--------------------------------|
| 13       | LED_LOADER        | Salida Digital| LED de carga                   |
| 14       | LED_DANGER        | Salida Digital| LED de peligro                 |
| 15       | ZUMBADOR          | Salida Digital| Buzzer/Zumbador                |
| 21       | LCD_SDA (I2C)     | I2C Data      | Pantalla LCD                   |
| 22       | LCD_SCL (I2C)     | I2C Clock     | Pantalla LCD                   |
| 25       | LED_ERROR         | Salida Digital| LED de error                   |
| 26       | LED_SUCCESS       | Salida Digital| LED de éxito                   |
| **27**   | **RESET_BUTTON**  | **Entrada Digital** | **Botón de reset** ⬅️ NUEVO |

---

## ❓ Preguntas Frecuentes

### ¿Puedo usar otro pin?
Sí, cambia `RESET_BUTTON` en `config.h`. Evita usar pines: 0, 2, 12, 15 (pueden impedir el boot).

### ¿Puedo cambiar el tiempo de presión?
Sí, modifica `RESET_BUTTON_HOLD_TIME` en `config.h` (valor en milisegundos).

### ¿Qué se borra exactamente?
- ✅ Credenciales WiFi
- ✅ Modo (Maestro/Esclavo)
- ✅ MAC del maestro (si es esclavo)
- ✅ Zona y sublocalización

### ¿Puedo usar un pulsador normalmente cerrado (NC)?
Sí, pero debes cambiar la lógica en `checkResetButton()` (invertir LOW/HIGH).

---

## ✅ Ventajas vs Comentar/Descomentar Código

| Método Anterior | Con Botón Físico |
|----------------|------------------|
| ❌ Editar código | ✅ Sin tocar código |
| ❌ Recompilar y subir | ✅ Reset instantáneo |
| ❌ Conectar USB | ✅ Funciona en campo |
| ❌ Abrir IDE | ✅ Solo presionar botón |

---

## 🎓 Ejemplo de Uso Real

**Escenario:** Necesitas reconfigurar un dispositivo que ya está instalado en el campo.

1. **Presiona el botón** (3 segundos)
2. El ESP32 se resetea y reinicia
3. Se abre el portal WiFi automáticamente
4. Conéctate a `BovinoIOT-IOT_ZONA_001`
5. Reconfigura como necesites
6. ¡Listo! Sin necesidad de llevarlo al taller

---

**¡Todo listo para usar! 🚀**
