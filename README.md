# Linux NOMMU para la Waveshare ESP32-P4-WIFI6 — v2 "mejorado"

> **Rama v2.**  Suma a la v1 microSD, NTP/zona horaria, varias redes Wi-Fi y
> punto de acceso, HTTPS, C++, GPIO, I2C y SPI.  Detalle y límites:
> **[CHANGELOG.md](CHANGELOG.md)**.  La v1 sigue en la rama `main`
> ([docs/VOLVER-A-V1.md](docs/VOLVER-A-V1.md)).

Linux 7.2.11 (sin MMU, riscv32) para la placa
[Waveshare ESP32-P4-WIFI6](https://www.waveshare.com/esp32-p4-wifi6.htm):
shell de BusyBox, SSH, MicroPython y Wi-Fi a través del ESP32-C6 de la placa.

Es un port del proyecto
[m5stamp-p4-c6-linux](https://github.com/tsuyoshi-otake/m5stamp-p4-c6-linux)
v0.5.1 (perfil `m3-lab`), adaptado a esta placa.

> **Estado: experimental.**  Probado en **una** placa (ESP32-P4 rev v3.2,
> 32 MB de flash, 32 MB de PSRAM, C6 con ESP-Hosted-MCU 3.0.10).  Usalo
> bajo tu responsabilidad y hacé un backup de la flash antes de grabar.

## Qué funciona

| | Estado |
|---|---|
| Linux en la consola serie (USB-C, 115200) | ✅ |
| Wi-Fi cliente WPA2 con varias redes (`wifi-connect`, `wifi.txt` en la microSD) | ✅ |
| Wi-Fi punto de acceso (`ap-on`) | ✅ |
| SSH (Dropbear) con clave o contraseña, `scp -O` | ✅ |
| mDNS (`stamp-p4.local`), hora por NTP, zona horaria | ✅ |
| MicroPython 1.22.2 (`python3`), `vi`, `wget` | ✅ |
| HTTPS con verificación (`https-get`, `wget https://`, `micropython-tls`) | ✅ TLS 1.2 |
| Programas en C y C++ compilados aparte (bFLT) | ✅ |
| microSD (persistencia y archivos) | ✅ |
| GPIO (`pin`, libgpiod 2, `gpio.py`) | ✅ |
| I2C (GPIO7/8, `i2c-tools`) | ✅ |
| SPI (GPIO21/22/23/26, `/dev/spidev1.0`) | ⚠️ loopback sin verificar |
| Temperatura del chip | ⏸️ en pausa |
| IPv6, USB OTG, segundo núcleo | ❌ |

Límites importantes:

- **Recursos:** 1 núcleo, ~14 MB de RAM para Linux, y un pool de 12 MB
  para programas.  Cada programa tiene que entrar en un bloque de ~496 KB.
  Una conexión HTTPS usa ~1 MB.
- **Red:** Wi-Fi por SDIO de 1 bit a 5 MHz, unos 150 KB/s.

## Requisitos

- Una **Waveshare ESP32-P4-WIFI6** con:
  - **32 MB de flash** (`esptool flash-id` tiene que decir 32MB);
  - chip **ESP32-P4 rev ≥ v3.0**.
- El **ESP32-C6 de la placa con ESP-Hosted-MCU 3.0.10**, firmware de
  estación (`eh_cp_wifi_sta`).
  - Este paquete **no** graba el C6.
  - Si tu C6 tiene otro firmware, Linux arranca pero **no hay Wi-Fi**.
  - Ver `docs/INSTALAR.md` → "El firmware del ESP32-C6".
- Python con `esptool` ≥ 5 (`pip install "esptool>=5"`).
- El cable USB-C al puerto de la placa (adaptador USB-serie CH343).

## Instalación rápida

```bash
pip install "esptool>=5"
./flash.sh /dev/cu.usbmodemXXXX      # macOS  (o /dev/ttyACM0 en Linux)
```

Después, en la consola serie, ejecutá `wifi-connect` y escribí tu red.  Paso
a paso, backup y recuperación: **[docs/INSTALAR.md](docs/INSTALAR.md)**.

## Contenido

| Ruta | Qué es |
|---|---|
| `firmware/` | Las imágenes probadas en hardware, con `SHA256SUMS` |
| `flash.sh` | Script de grabación (verifica, pide confirmación, graba y verifica) |
| `docs/INSTALAR.md` | Instalación, primer arranque, Wi-Fi, SSH, backup y restauración |
| `docs/GUIA-DE-USO.md` | Guía completa: conectarse, programar, límites, microSD/USB, proyectos |
| `docs/COMPILAR.md` | Cómo compilar las imágenes desde el código fuente |
| `docs/PINES-GPIO.md` | Tabla de pines del conector (GPIO, I2C, SPI, reservados) |
| `docs/VOLVER-A-V1.md` | Cómo volver a la v1 (y de nuevo a la v2) |
| `CHANGELOG.md` | Qué cambió en la v2, qué está limitado y qué falló |
| `source/0001-waveshare-esp32-p4-wifi6-v2.patch` | Los cambios respecto del proyecto base (aplicar sobre el commit `69a7312`) |
| `examples/` | Ejemplos: shell, MicroPython (cliente y servidor HTTP, mini navegador de texto `navegador.py`), C (`c-hello`, `c-sysinfo`, `c-gpio`), C++ (`cpp/`), SQLite y visor web de tablas (`sqlite/`), servidor web y API REST en C con Mongoose (`c-api/`), TensorFlow Lite Micro (`tflm/`), visor de imágenes en la terminal (`ver-imagen/`), `wifi-ejemplo.txt`.  Guía 11.11 |
| `LICENSE` | GPL-2.0, la licencia del proyecto base |

## Imágenes

| Dirección | Archivo | SHA-256 |
|---|---|---|
| 0x2000 | `bootloader.bin` | `5bd36231c4a258fd5b9efd029901655ce3ab04db3427d09db7f2890a37787820` |
| 0x8000 | `partition-table.bin` | `94f1b217ada457065b0e87bbf2be394ac5968003f7197ff6d9229af2a663fc32` |
| 0x10000 | `boot-shim.bin` | `35702e0b0e2650c70817f4395e8d0ffda2c2a85ee0a61d306232983cedd32852` |
| 0x90000 | `Image` | `f9cf0d93c5b969d123d4a57fc89399e28de2e1547a302e677f68a653354d1b89` |
| 0x810000 | `rootfs.squashfs` | `a3c5392483a7dbeb0876e3dcee1f99dfcb4db206af624e61f533b63c952721d0` |
| 0xF10000 | `esp32p4-m5stamp-p4.dtb` | `ba772ebbae4c5435ad493bfc0cc93f873bb321b8825084705f0914395eb14e94` |
| 0xF40000 | `boot.img` (`/boot` de fábrica) | `96c9bb28d5568055452b484d66525c992e72cedf4eb987b516f6f2ab87a8eb39` |

El DTB conserva el nombre del proyecto base, pero es el de esta placa
(`model = "Waveshare ESP32-P4-WIFI6"`).

**Las imágenes no traen contraseñas ni claves.**
- `root` y `m5` vienen bloqueados.
- La placa genera sus propias contraseñas en el primer arranque y las
  guarda en `/boot/credentials.txt`.
- La host key de SSH se genera en cada arranque.

## Versiones

| Componente | Versión |
|---|---|
| Kernel | Linux 7.2.11 NOMMU riscv32 |
| Rootfs | Buildroot 2025.02.15, uClibc-ng, BusyBox 1.37.0, Dropbear 2026.91, MicroPython 1.22.2, mbedTLS 3.6, libgpiod 2.2, i2c-tools |
| Boot shim / bootloader | ESP-IDF v5.5.3 |
| Wi-Fi (P4) | ESP-Hosted-MCU 3.0.10: módulo `esp_hosted_mcu.ko` + daemon `eh-wifi-sta` |
| Wi-Fi (C6, requerido) | ESP-Hosted-MCU 3.0.10 (fw `0x0003000a`, RPC V2) |

## Créditos y licencia

- Proyecto base:
  [tsuyoshi-otake/m5stamp-p4-c6-linux](https://github.com/tsuyoshi-otake/m5stamp-p4-c6-linux),
  GPL-2.0.  Este port se distribuye con la misma licencia (`LICENSE`).
- ESP-Hosted-MCU:
  [espressif/esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu),
  Apache-2.0.
- Linux, BusyBox, uClibc-ng, Dropbear y MicroPython: cada uno con su
  licencia.

**Código fuente (GPL).**  Las imágenes contienen software GPL (Linux,
BusyBox, etc.).  Quien las redistribuya tiene que ofrecer el código fuente
correspondiente.
- `docs/COMPILAR.md` y `source/` permiten reconstruirlas desde las fuentes
  públicas.
- Para entregar todas las fuentes y licencias exactas en un solo paquete,
  generalo con `make legal-info` de Buildroot sobre el mismo build.
