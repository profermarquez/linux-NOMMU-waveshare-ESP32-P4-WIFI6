# Cambios

## Bluetooth LE (2026-11-27, sobre la v2 con audio)

Fuentes: `source/0003-waveshare-bluetooth.patch` (después del 0002).
Firmware del C6: `firmware/c6-ble/`.  Las imágenes de `firmware/` **no**
lo traen.  Detalle: guía, 11.13.

### Nuevo

| Qué | Estado |
|---|---|
| Kernel con `CONFIG_BT=y`, solo LE; `hci0` se registra cuando una herramienta lo pide | ✅ |
| Firmware del C6 con BLE (`eh_cp_wifi_sta` 3.0.10 de Espressif, solo 4 opciones distintas), grabado **desde Linux**: `eh-wifictl c6-info`, `c6-ota`, `c6-activate` | ✅ |
| `ble_scan` (escanear), `ble_adv` (anunciar) | ✅ 15 dispositivos en 20 s, con el Wi-Fi andando |
| `ble-ap` / `ble-server`: servidor GATT para el celular (Nordic UART + Device Information), emparejamiento "Just Works" | ✅ con Android (conecta y empareja) y con la Mac (servicios, lectura, notificaciones, respuestas) |
| `S40network`: si el C6 no contesta por SDIO al arrancar, reinicia (hasta 2 veces seguidas) | ✅ 10 de 10 reinicios con Wi-Fi |

### Limitaciones conocidas

- **Solo BLE.**  El C6 no tiene Bluetooth clásico ni LE Audio: no puede ser
  un parlante Bluetooth (A2DP) ni un puerto serie clásico (SPP).
- Un celular por vez y una herramienta BLE por vez.
- "Just Works" no tiene PIN: el enlace va cifrado, pero cualquiera cerca se
  puede conectar.
- Alrededor de 1 de cada 9 `reboot` el enlace SDIO con el C6 falla al
  arrancar (ya pasaba antes del BLE).  El reintento lo esquiva; la causa
  sigue sin encontrarse.
- Volver al firmware de fábrica del C6 necesita un adaptador USB-serie en
  sus pads (grabar `otadata-ota0.bin` en `0xd000`).
- `C6_IO9` a masa (jumper) deja al C6 en modo de programación: sin Wi-Fi ni
  Bluetooth.

### Imágenes del C6

| Archivo | SHA-256 |
|---|---|
| `firmware/c6-ble/eh_cp_wifi_sta-bt1.bin` (ranura `ota_1`) | `cfc45bef26bee032a5c5134269367bd5c1e52ba0b68a8d47addfe3c4d4a88ac8` |
| `firmware/c6-ble/otadata-ota0.bin` (`0xd000`, vuelve a `ota_0`) | `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3` |

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
