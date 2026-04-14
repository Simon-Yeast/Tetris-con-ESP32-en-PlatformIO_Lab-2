# Tetris en ESP32 — Matriz LED 8×8

Implementación del clásico juego Tetris sobre una matriz LED 8×8 controlada directamente por un ESP32, sin librerías externas de juego ni de display. Desarrollado en C con ESP-IDF y PlatformIO.

---

## Hardware requerido

| Componente | Cantidad |
|---|---|
| ESP32 DevKit (AZ-Delivery V4) | 1 |
| Matriz LED 8×8 (ánodo de fila, cátodo de columna) | 1 |
| Pulsadores normalmente abiertos | 3 |
| Resistencias 330Ω | 9 |

---

## Asignación de pines

### Filas (ánodo — activo LOW)
| GPIO | Fila |
|------|------|
| 23 | 0 |
| 22 | 1 |
| 21 | 2 |
| 19 | 3 |
| 18 | 4 |
| 5  | 5 |
| 17 | 6 |
| 16 | 7 |

### Columnas rojo (cátodo — activo LOW)
| GPIO | Columna |
|------|---------|
| 32 | 0 |
| 33 | 1 |
| 25 | 2 |
| 26 | 3 |
| 27 | 4 |
| 14 | 5 |
| 12 | 6 |
| 13 | 7 |

### Canal verde y botones
| GPIO | Función |
|------|---------|
| 2  | Columna verde (línea completa) |
| 36 | Botón izquierda |
| 34 | Botón derecha |
| 4  | Botón rotar |

---

## Estructura del proyecto

```
tetris_esp32/
├── platformio.ini
└── src/
    └── main.c
```

---

## Compilar y flashear

```bash
# Compilar
pio run

# Flashear
pio run --target upload

# Monitor serie
pio device monitor
```

---

## Controles

| Botón | Acción |
|-------|--------|
| Izquierda (GPIO36) | Mover pieza a la izquierda |
| Derecha (GPIO34) | Mover pieza a la derecha |
| Rotar (GPIO4) | Rotar pieza 90° en sentido horario |

---

## Piezas implementadas

Las 7 piezas estándar del Tetris: **I, O, T, S, Z, J, L**, cada una con sus 4 rotaciones codificadas en una tabla de bitmaps.

---

## Características

- Multiplexión por software a 2 ms por fila (~62 Hz de refresco)
- Detección de colisiones completa, incluyendo wall-kick lateral al rotar
- Eliminación de líneas con animación verde parpadeante
- Animación de muerte al hacer game over
- Generador de números pseudoaleatorios (LCG) sembrado con el timer del sistema
- Debounce por software de 50 ms en los tres botones

---

## platformio.ini

```ini
[env:az-delivery-devkit-v4]
platform = espressif32
board = az-delivery-devkit-v4
framework = espidf
monitor_speed = 115200
```
