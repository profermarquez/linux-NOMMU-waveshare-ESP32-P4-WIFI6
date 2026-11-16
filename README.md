# Linux NOMMU para la Waveshare ESP32-P4-WIFI6

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
| Wi-Fi cliente WPA2, DHCP, DNS, Internet | ✅ |
| SSH (Dropbear) con clave o contraseña, `scp -O` | ✅ |
| mDNS (`stamp-p4.local`) | ✅ |
| MicroPython 1.22.2 (`python3`), `vi`, `wget` (HTTP) | ✅ |
| Programas en C compilados aparte (bFLT) | ✅ |
| Configuración, claves y programas persistentes en `/boot` (256 KB) | ✅ con `boot-save` |
| Wi-Fi punto de acceso, HTTPS/TLS, IPv6, C++ | ❌ |
| GPIO, I2C, SPI desde Linux | ❌ |
| microSD, USB OTG | ❌ (ver `docs/GUIA-DE-USO.md`, sección 8) |

Límites importantes:

- **Recursos:** 1 núcleo, ~16 MB de RAM para Linux, y un pool de 10 MB
  para programas (un programa puede reservar unos 2 MB).
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
| `source/0001-waveshare-esp32-p4-wifi6.patch` | Los cambios respecto del proyecto base (aplicar sobre el commit `69a7312`) |
| `examples/` | Ejemplos probados: shell, MicroPython (cliente y servidor HTTP), C (`c-hello`, `c-sysinfo`) |
| `LICENSE` | GPL-2.0, la licencia del proyecto base |

## Imágenes

| Dirección | Archivo | SHA-256 |
|---|---|---|
| 0x2000 | `bootloader.bin` | `5bd36231c4a258fd5b9efd029901655ce3ab04db3427d09db7f2890a37787820` |
| 0x8000 | `partition-table.bin` | `94f1b217ada457065b0e87bbf2be394ac5968003f7197ff6d9229af2a663fc32` |
| 0x10000 | `boot-shim.bin` | `cc7b0e01d495d9aeec5026d5c49cad8935c0175c4ac747bcf6c418fa50fce12a` |
| 0x90000 | `Image` | `383b012f1f3cf8040abd8802b811d0d511b0afe8e2482171357defa2d291927e` |
| 0x810000 | `rootfs.squashfs` | `b7daff36a1c9597eb041fddac0c854dba2169e17bec930952f92c4d90ca507a5` |
| 0xF10000 | `esp32p4-m5stamp-p4.dtb` | `441e82fe82d660912593771c6b86fef22f3bc9507b2a45497e95ce51e2148655` |
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
| Rootfs | Buildroot 2025.02.15, uClibc-ng, BusyBox 1.37.0, Dropbear 2026.91, MicroPython 1.22.2 |
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
