# Pines GPIO — Waveshare ESP32-P4-WIFI6 (Linux v2)

Conector de 2 × 20 pines, visto desde arriba con el USB-C arriba (fuente:
imagen "Pinout Definition" del wiki de Waveshare, ESP32-P4-WIFI6). Son 27
GPIO; el resto es alimentación y control.

| Izquierda | | Derecha | |
|---|---|---|---|
| GPIO52 | libre (banco 1) | VBUS | 5 V del USB |
| GPIO51 | libre (banco 1) | VSYS | alimentación del sistema |
| GND | | GND | |
| GPIO31 | libre (banco 0) | EN | habilitación (control) |
| GPIO30 | libre (banco 0) | 3V3 | 3,3 V |
| GPIO29 | libre (banco 0) | GPIO20 | libre (banco 0) |
| GPIO28 | libre (banco 0) | GPIO21 | libre (banco 0) |
| GND | | GND | |
| GPIO50 | libre (banco 1) | GPIO22 | libre (banco 0) |
| GPIO49 | libre (banco 1) | GPIO23 | libre (banco 0) |
| GPIO5 | libre (banco 0) | RUN | reset (control) |
| GPIO4 | libre (banco 0) | GPIO26 | libre (banco 0) |
| GND | | GND | |
| GPIO3 | libre (banco 0) | GPIO27 | libre (banco 0) |
| GPIO2 | libre (banco 0) | GPIO32 | libre (banco 1) |
| GPIO8 / SCL | I²C (fase 9) | GPIO33 | libre (banco 1) |
| GPIO7 / SDA | I²C (fase 9) | GPIO46 | libre (banco 1) |
| GND | | GND | |
| GPIO24 / DM | **reservado** (USB) | GPIO47 | libre (banco 1) |
| GPIO25 / DP | **reservado** (USB) | GPIO48 | libre (banco 1) |

- **Niveles de 3,3 V.** No conectar 5 V a un GPIO.
- **Banco 0** = GPIO0–31 (línea = número de GPIO). **Banco 1** = GPIO32–56
  (línea = GPIO − 32). Cada banco es un `gpiochip`; el número `gpiochipN`
  depende del orden de arranque, por eso `pin`, `gpio.py` y `gpio-parpadeo`
  lo buscan en `/sys/bus/platform/devices/500e0000.gpio-bankN/`.
- **Reservados por el kernel** (no se pueden pedir): GPIO0, 1, 6, 9–13 (I²S
  del códec de audio), 14–19 (SDIO del C6: pedir uno cortaría el Wi-Fi) y
  24/25 (par USB). En el banco 1, 39–44 son la microSD (los usa el kernel).
- **LED:** la placa sólo tiene el LED de encendido. Para probar, un LED
  externo: GPIO → resistencia de 330 Ω → ánodo del LED, cátodo → GND.

## Desde la consola

```sh
pin lista                    # pines del conector: nombre, en uso, dirección
pin 20 1                     # GPIO20 en alto (queda así)
pin 20 0
pin 47 leer pullup           # leer con resistencia interna
pin 20 parpadear 10 250      # 10 veces, 250 ms
gpiodetect; gpioinfo -c gpiochip0   # herramientas de libgpiod 2.2
```

## MicroPython

```python
from gpio import Pin
led = Pin(20, Pin.OUT)
led.on(); led.off(); led.toggle()
boton = Pin(47, Pin.IN, Pin.PULL_UP)
boton.value()
```

Cada operación ejecuta `gpioset`/`gpioget` (esta MicroPython no tiene
`ffi`/`ioctl`): decenas de ms por llamada. Sirve para LEDs, relés y
botones; no para señales rápidas.

## C

`gpio-parpadeo` (fuente `linux/waveshare/gpio-ejemplo/gpio-parpadeo.c`,
API de libgpiod 2):

```sh
gpio-parpadeo 20 10 250       # parpadear
gpio-parpadeo -l 47 pullup    # leer
```
