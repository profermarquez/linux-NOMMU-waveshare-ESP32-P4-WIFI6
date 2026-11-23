# Cambios

## v2 "mejorado" (rama `linux-NOMMU-waveshare-ESP32-P4-WIFI6-mejorado`, 2026-11-23)

Probada en la misma placa que la v1 (ESP32-P4 rev v3.2, 32 MB de flash y
PSRAM, C6 con ESP-Hosted-MCU 3.0.10).  El firmware del C6 **no** cambia.
La tabla de particiones y las direcciones de grabación son las mismas de la v1.

### Nuevo

| Fase | Qué | Estado |
|---|---|---|
| 1 | Pool NOMMU de 10 → 12 MB para programas; el arranque sigue siendo compatible con la Stamp-P4 | ✅ |
| 2 | **microSD** por SPI (spi-gpio + mmc_spi).  Partición `WSDATA` (ext2/ext4) para persistencia y `/home/m5`; partición `WSFILES` (FAT32) en `/mnt/archivos` para intercambiar archivos con la computadora | ✅ |
| 3 | **Hora por NTP** al recibir IP por DHCP (`time-sync`) y **zona horaria** en `config.txt` (`timezone=`, `ntp_server=`) | ✅ |
| 4 | **Gestor de Wi-Fi** `eh-wifid`: varias redes con prioridad, `wifi-connect`, `wifi-scan`, `wifi-forget`, `wifi-reload`, `wifi-status`; **punto de acceso** `ap-on`, `ap-off`, `ap-config`; redes en `wifi.txt` de la microSD | ✅ |
| 5 | **HTTPS con verificación de certificados** (mbedTLS 3.6, TLS 1.2, bundle de CAs): `https-get`, `wget https://…`, `micropython-tls` con `ssl` y módulo `https` | ✅ |
| 6 | **C++** (STL, excepciones, `std::thread`, sockets) compilado desde la Mac, con toolchain aparte | ✅ |
| 7 | Temperatura del chip (LP-TSENS) | ⏸️ En pausa: el driver colgaba el arranque.  Queda desactivado |
| 8 | **GPIO** con libgpiod 2.2: comando `pin`, módulo MicroPython `gpio`, ejemplo en C `gpio-parpadeo`, tabla de pines | ✅ |
| 9 | **I2C** (GPIO7 SDA / GPIO8 SCL → `/dev/i2c-0`, `i2c-tools`) y **SPI** (GPIO21 SCK, 22 MOSI, 23 MISO, 26 CS → `/dev/spidev1.0`, `spidev_test`) | ✅ I2C · ⚠️ SPI sin loopback verificado |

### Limitaciones conocidas

- **Un solo núcleo** (CPU1 sigue apagada) y Linux administra ~14 MB de RAM.
- **TLS:** solo TLS 1.2 (sin TLS 1.3).  Cada `https-get` usa ~1 MB de RAM
  mientras corre, sobre todo para leer el bundle de CAs.  4 en paralelo
  dejaron sin memoria a otros programas.
- **MicroPython `ssl`** solo en `micropython-tls` (el `micropython` normal no
  entra en un bloque de 512 KB con `ssl`).  Un `ssl.SSLContext` sin más **no
  verifica** certificados en MicroPython 1.22: usar el módulo `https` o
  `CERT_REQUIRED` + `load_verify_locations()`.
- **GPIO desde MicroPython** (`gpio.py`) llama a `gpioget`/`gpioset`: ~31 ms
  por operación.  Sirve para LEDs, relés o botones, no para protocolos rápidos.
- **I2C y SPI son por software** (bit-bang).  Con un núcleo, la CPU queda
  ocupada durante cada transferencia.  Velocidades bajas: I2C ~100 kHz, SPI
  ≤1 MHz.
- **SPI:** el loopback MOSI→MISO no se pudo verificar porque el cable de
  prueba tocaba GND.  Con el cable quitado, los pines quedan bien.  Prueba
  pendiente: `spidev_test -D /dev/spidev1.0 -s 100000 -v` con un cable
  **solo** entre GPIO22 y GPIO23 tiene que devolver lo mismo que envía.
- Hubo **un reinicio sin explicación** después de las primeras pruebas de
  SPI.  No se repitió en 4 intentos.
- **Espacio:** el rootfs ocupa 6,49 MB de los 7 MB de su partición (quedan
  ~0,5 MB).  Para agregar mucho más hace falta cambiar la tabla de particiones.
- No mover cables del conector con la placa encendida: un contacto con RUN o
  EN la reinicia.

### Falló o no se hizo

- Fase 7 (temperatura): con el driver compilado dentro del kernel, el
  arranque se colgaba después de `earlycon`.  La sospecha (sin confirmar) es
  el reset del maestro I2C del dominio LP.  Queda desactivado y documentado.
- SDIO de 4 bits, segundo núcleo, IPv6, USB: sin cambios respecto de la v1.

### Imágenes

| Dirección | Archivo | SHA-256 | Cambió |
|---|---|---|---|
| 0x2000 | `bootloader.bin` | `5bd36231c4a258fd5b9efd029901655ce3ab04db3427d09db7f2890a37787820` | no |
| 0x8000 | `partition-table.bin` | `94f1b217ada457065b0e87bbf2be394ac5968003f7197ff6d9229af2a663fc32` | no |
| 0x10000 | `boot-shim.bin` | `35702e0b0e2650c70817f4395e8d0ffda2c2a85ee0a61d306232983cedd32852` | sí (alimentación de la microSD, LDO_VO4) |
| 0x90000 | `Image` | `f9cf0d93c5b969d123d4a57fc89399e28de2e1547a302e677f68a653354d1b89` | sí |
| 0x810000 | `rootfs.squashfs` | `a3c5392483a7dbeb0876e3dcee1f99dfcb4db206af624e61f533b63c952721d0` | sí |
| 0xF10000 | `esp32p4-m5stamp-p4.dtb` | `ba772ebbae4c5435ad493bfc0cc93f873bb321b8825084705f0914395eb14e94` | sí |
| 0xF40000 | `boot.img` | `96c9bb28d5568055452b484d66525c992e72cedf4eb987b516f6f2ab87a8eb39` | no |

Fuente: `source/0001-waveshare-esp32-p4-wifi6-v2.patch` sobre el commit
`69a7312` del proyecto base (verificado: aplica y reproduce el árbol de la v2).

## v1 (rama `main`, 2026-11-15)

Primera versión: Linux 7.2.11 NOMMU, BusyBox, SSH, MicroPython, Wi-Fi
cliente por el C6, `/boot` persistente.  Ver la rama `main`.
