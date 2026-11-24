# Linux NOMMU para la Waveshare ESP32-P4-WIFI6 — v2 "mejorado"

> **Rama v2.**  Suma a la v1 microSD, NTP/zona horaria, varias redes Wi-Fi y
> punto de acceso, HTTPS, C++, GPIO, I2C y SPI.  Después se agregaron
> programas en la microSD (SQLite, web, IA) y el micrófono: ver **Usar la
> placa** más abajo.  Detalle y límites:
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
| SQLite, servidor web, APIs REST, TensorFlow Lite Micro (en la microSD) | ✅ |
| Micrófono (`grabar`, `tflm-voz`) | ✅ con el firmware de audio (no está en `firmware/`) |
| Parlante | ❌ no configurado |
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
| `source/0002-waveshare-audio.patch` | El micrófono: shim, driver `/dev/ws-audio` y DTB (aplicar después del 0001) |
| `examples/` | Ejemplos: shell, MicroPython (cliente y servidor HTTP, mini navegador `navegador.py`), C (`c-hello`, `c-sysinfo`, `c-gpio`), C++ (`cpp/`), SQLite y visor web de tablas (`sqlite/`), servidor web y API REST en C con Mongoose (`c-api/`), visor de imágenes en la terminal (`ver-imagen/`), TensorFlow Lite Micro (`tflm/`: seno, dígitos, voz), grabador del micrófono (`audio/`), `wifi-ejemplo.txt`.  Guía 11.9–11.12 y la sección de abajo |
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

## Usar la placa: ubicaciones y ejemplos

Lo que sigue se probó en la placa.  La guía completa está en
[docs/GUIA-DE-USO.md](docs/GUIA-DE-USO.md), secciones 11.9 a 11.12.

### Antes de empezar

- **Entrá como `m5`:** `ssh m5@stamp-p4.local`, o la IP de la placa.
  Como `m5`, los programas de `/home/m5/bin` se ejecutan con solo el
  nombre (`grabar`, `tflm-mnist`, `sqlite3`…).
  - Como `root`, usá la ruta completa: `/home/m5/bin/grabar`.
  - Dentro de la carpeta, `./grabar`.
  - `grabar` solo, desde otra carpeta, da `sh: can't execute 'grabar': No
    such file or directory`.
  - Lo mismo pasa con `ssh m5@… 'comando'`: no carga el PATH, así que usá
    la ruta completa.
- **Para pasar archivos, `scp -O`:** sin `-O` el `scp` de la Mac falla,
  porque la placa no tiene `sftp-server`.
- **No corras `df` sobre `/mnt/archivos`:** en una prueba colgó la placa
  y se reinició.  Sobre `/home/m5`, `df` funciona bien.

### Dónde está cada cosa

| Carpeta | Dónde vive | Qué hay |
|---|---|---|
| `/home/m5/bin` | microSD (ext4, `WSDATA`) | los programas: `navegar`, `sqlite3`, `servidor-web`, `api-sqlite`, `ver-imagen`, `tflm-seno`, `tflm-mnist`, `tflm-voz`, `grabar`… |
| `/home/m5` | microSD | bases SQLite (`lecturas.db`, `notas.db`…), grabaciones `.wav` |
| `/home/m5/imagenes` | microSD | dígitos de ejemplo (`sample0.png` … `sample9.png`) e imágenes propias |
| `/home/m5/audio` | microSD | WAV de prueba de 1 s: `yes_1000ms.wav`, `no_1000ms.wav`, `silence_1000ms.wav`, `noise_1000ms.wav` |
| `/home/m5/modelos` | microSD | modelos `.tflite` del ejemplo del seno |
| `/home/m5/www` | microSD | páginas que sirve `servidor-web` |
| `/home/m5/.micropython/lib` | microSD | módulos de MicroPython: `microdot`, `sqlite.py` |
| `/mnt/archivos` | microSD (FAT32, `WSFILES`) | `wifi.txt`; se lee desde la computadora |
| `/tmp`, `/root`, `/home` fuera de `m5` | RAM | **se borra al reiniciar** |

Todo lo de `/home/m5` sobrevive a los reinicios.  Los programas se
instalan desde la Mac con `make -C examples/<carpeta> deploy
PLACA=stamp-p4.local`.

### Bases de datos SQLite

Cada base es un solo archivo; si abrís un nombre que no existe, se crea.
Guardalas en `/home/m5`.  Junto a una base puede aparecer un archivo
`-journal` vacío: lo usa SQLite al escribir, no lo borres.

```sh
sqlite3 /home/m5/mibase.db                                   # consola (.tables, .schema, .quit)
sqlite3 -box /home/m5/lecturas.db "SELECT * FROM lecturas"   # una consulta, en tabla
micropython /home/m5/bin/ver_tablas.py /home/m5/lecturas.db  # visor web en http://stamp-p4.local:8080/
```

```bash
scp -O m5@stamp-p4.local:/home/m5/lecturas.db .              # copiarla a la Mac
```

- En la Mac se abre con `sqlite3`, que viene con macOS, o con DB Browser
  for SQLite.
- `lecturas.db` la crea `api-sqlite` (la API en C).
- `notas.db` la crea `api_microdot.py` (la API en MicroPython).
- Las escrituras son lentas en la microSD: ~1 s cada una con
  `PRAGMA journal_mode=TRUNCATE; PRAGMA synchronous=NORMAL;`.  Agrupalas en
  una transacción (`BEGIN; … COMMIT;`).

### Ver imágenes en la terminal (`ver-imagen`)

```sh
cd /home/m5/imagenes
ls                                  # las imágenes de ejemplo
ver-imagen sample7.png              # en colores (256), el modo por defecto
ver-imagen -m ascii sample3.png     # solo caracteres
ver-imagen -m bloques -w 40 foto.jpg
```

- Lee PNG, JPEG, BMP, GIF, TGA y PGM.
- Una foto de más de 700 000 píxeles no entra en la RAM: achicala antes en
  la Mac (`sips -Z 640 foto.jpg`).

### Reconocer un dígito escrito a mano (`tflm-mnist`)

```sh
tflm-mnist /home/m5/imagenes/sample3.png     # dibujo + dígito + probabilidades
tflm-mnist -q /home/m5/imagenes/sample?.png  # los 10 de ejemplo: los 10 bien
```

Para probar con un dígito tuyo:
1. Escribilo con fibra gruesa sobre papel blanco y sacale una foto.
2. En la Mac, recortala para que el dígito ocupe casi todo el cuadro, y
   achicala:
   ```bash
   sips -Z 280 mi-digito.jpg
   scp -O mi-digito.jpg m5@stamp-p4.local:/home/m5/imagenes/
   ```
3. En la placa:
   ```sh
   tflm-mnist /home/m5/imagenes/mi-digito.jpg
   ```

Una imagen clara (tinta sobre papel) se invierte sola, porque el modelo
espera el dígito blanco sobre negro.  En el dibujo que muestra, el dígito
tiene que verse centrado y grande.

### Grabar con el micrófono (`grabar`)

La placa tiene un micrófono soldado, conectado al códec ES8311.  Para
usarlo hace falta el **firmware con audio**:
- un boot shim, un kernel con `/dev/ws-audio` y un DTB nuevos;
- las fuentes están en `source/0002-waveshare-audio.patch`;
- **las imágenes de `firmware/` de este repositorio todavía no lo traen**
  (ver la guía, 11.12).

Con ese firmware, al arrancar la consola serie muestra el nivel del
micrófono (`ws_audio: mic L rms=…`).

```bash
make -C examples/audio deploy PLACA=stamp-p4.local     # desde la Mac: instala grabar en /home/m5/bin
```

```sh
grabar -s 10 /home/m5/voz.wav     # 10 s, mono, 16 kHz, 16 bits
grabar -n -s 30                   # solo el medidor de nivel, sin archivo
grabar -e -s 5 /home/m5/est.wav   # los dos canales (el derecho repite el izquierdo)
```

Salida real:

```
  1s  rms   171  pico  1839  |#                                       |
  2s  rms   279  pico  1778  |##                                      |
  ...
/home/m5/voz.wav: 10.0 s de audio en 10.0 s, mono, 16 kHz, 16 bits
```

Escucharlo en la Mac:

```bash
scp -O m5@stamp-p4.local:/home/m5/voz.wav . && afplay voz.wav
```

- **Guardá las grabaciones en `/home/m5`** (microSD).  En `/tmp` se
  pierden al reiniciar.
- **No escribas en la microSD mientras grabás:** la escritura ocupa el
  único núcleo por unos segundos y se pierde audio.  `grabar` ya lo evita
  (guarda en RAM, escribe al final y avisa si se perdió algo).
- **Un solo programa a la vez** puede usar el micrófono.

### Decir "yes" o "no" (`tflm-voz`)

```bash
make -C examples/tflm deploy PLACA=stamp-p4.local    # instala tflm-voz y copia los WAV a /home/m5/audio
```

```sh
tflm-voz /home/m5/audio/yes_1000ms.wav /home/m5/audio/no_1000ms.wav
tflm-voz /home/m5/voz.wav        # tu grabación: 16 kHz, 16 bits; mira el primer segundo
tflm-voz -m -v                   # escuchar el micrófono en vivo; Ctrl+C para salir
```

- Con los WAV de ejemplo: "yes" → **yes** (100 %), "no" → **no** (95 %),
  silencio y ruido → **silencio**.
- En vivo clasifica cada 200 ms y usa ~30 % de la CPU.
- Solo conoce "yes" y "no", en inglés.
- **Todavía no probé** que reconozca una voz real hablándole a la placa.

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
