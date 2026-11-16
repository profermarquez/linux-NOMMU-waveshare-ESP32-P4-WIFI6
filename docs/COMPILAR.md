# Compilar las imágenes desde el código fuente

Las imágenes de `firmware/` se compilaron así.  Se necesita **Linux** (o una
VM Linux; en la placa de prueba se usó Ubuntu 24.04 arm64 en OrbStack) y
unos 20 GB libres.

## 1. Código

```bash
mkdir ws && cd ws
# Proyecto base, en el commit exacto
git clone https://github.com/tsuyoshi-otake/m5stamp-p4-c6-linux.git src
cd src
git checkout 69a7312c97d67074abcc197d8454a9cb0a7d7ee0     # v0.5.1
git apply /ruta/a/linux-NOMMU-waveshare-ESP32-P4-WIFI6/source/0001-waveshare-esp32-p4-wifi6.patch
git submodule update --init --recursive                    # si el proyecto base lo pide
cd ..
```

ESP-Hosted-MCU 3.0.10 (lado host) va en `ws/vendor/esp-hosted-mcu-3.0.10`,
al lado de `src`.  Es la ruta por defecto (o usá `WS_VENDOR_DIR=`):

```bash
mkdir -p vendor
git clone https://github.com/espressif/esp-hosted-mcu.git vendor/esp-hosted-mcu-3.0.10
git -C vendor/esp-hosted-mcu-3.0.10 checkout 2e3f6c8bd717d67698319a6a24f9f4218d8db068
```

Ese es el commit del componente `espressif/esp_hosted` 3.0.10 del registro
de ESP-IDF, según su `idf_component.yml`.  En la placa de prueba se usó la
copia que descarga el registro.  Si el clon de git difiere, bajalo con
`idf.py add-dependency "espressif/esp_hosted==3.0.10"` en un proyecto vacío
y copiá `managed_components/espressif__esp_hosted`.

## 2. Herramientas

El script de build recibe primero **la carpeta de fuentes externas**.  Esa
carpeta tiene ESP-IDF (`esp_idf/`), Buildroot y el kernel en las versiones
fijadas por `versions.lock.json` del proyecto base.  Hay dos formas de
tenerla:

- **`--vendor`**: usa los submódulos del propio repositorio.  Es más simple
  (`git submodule update --init --recursive` en `src`).
- **Una carpeta propia**, por ejemplo `$HOME/src-cache`, preparada como
  explica el README del proyecto base.  Es la que se usó para estas
  imágenes.

Además hacen falta los paquetes del sistema para Buildroot
(`build-essential`, `git`, `rsync`, `bc`, `cpio`, `unzip`, `python3`, etc.)
y `dosfstools` + `mtools` (para `boot.img`).

Versiones usadas: Buildroot 2025.02.15, ESP-IDF v5.5.3, Linux 7.2.11.

## 3. Compilar

```bash
cd ws/src
. "$HOME/src-cache/esp_idf/export.sh"        # el ESP-IDF de la carpeta de fuentes
export BOARD=waveshare
bash linux/release-build.sh "$HOME/src-cache" "$HOME/ws-out" --profile m3-lab
# o, con los submódulos:  bash linux/release-build.sh --vendor "$HOME/ws-out" --profile m3-lab
```

Este procedimiento se usó tal cual para compilar `firmware/` en la placa de
prueba.  La variante `--vendor` no se probó para esta placa.

- `BOARD=waveshare` activa todo lo de esta placa: boot shim, DTB, paquetes
  de ESP-Hosted-MCU y overlay.  Sin esa variable se compila la versión para
  M5Stamp-P4.
- Si cambiaste los parches de `esp-hosted-mcu` o `eh-wifi-sta`, el script
  ya los limpia (`dirclean`) en cada build.

## 4. Dónde quedan las imágenes

| Archivo | Ruta |
|---|---|
| `bootloader.bin` | `$HOME/ws-out/boot-shim-c68/build/bootloader/bootloader.bin` |
| `partition-table.bin` | `$HOME/ws-out/boot-shim-c68/build/partition_table/partition-table.bin` |
| `boot-shim.bin` | `$HOME/ws-out/boot-shim-c68/build/stamp_p4_boot_shim.bin` |
| `Image`, `rootfs.squashfs`, `esp32p4-m5stamp-p4.dtb`, `boot.img` | `$HOME/ws-out/buildroot/images/` |

El build no es reproducible bit a bit: lleva fecha, hora y usuario.  Tus
SHA-256 van a ser distintos de los de `firmware/`.

## 5. Programas propios (bFLT)

Ver `examples/` y la sección 4.3 de `GUIA-DE-USO.md`.  La toolchain queda en
`$HOME/ws-out/buildroot/host/bin/`:

```bash
riscv32-linux-gcc -Os -fPIC -Wl,-elf2flt=-r -o prog prog.c
riscv32-linux-flthdr -s 65536 prog
```

## 6. Fuentes y licencias para redistribuir

Para publicar imágenes cumpliendo la GPL, generá el paquete con las fuentes
exactas de cada componente y sus licencias:

```bash
make -C "$HOME/ws-out/buildroot" legal-info     # queda en $HOME/ws-out/buildroot/legal-info/
```

## 7. Qué cambia el parche (resumen)

Todo queda condicionado a `BOARD=waveshare`:

- **Boot shim:**
  - `ws_sdmmc.c`: SDMMC slot 1 con CLK GPIO18, CMD 19, D0–D3 14–17, y
    reset del C6 en GPIO54, activo en alto;
  - sin USB OTG;
  - tabla de particiones de 32 MB;
  - chip rev ≥ v3.
- **DTS:** consola en UART0 (`ttyS0`), `maxcpus=1`, SDIO de 1 bit a 5 MHz.
- **Kernel:** el parche 0072 (bootflash) usa la dirección de datos ROM de
  P4 rev v3 (`0x4ffbffe8`).
- **Paquete `esp-hosted-mcu`:** driver de Linux de ESP-Hosted-MCU, con
  - acceso a registros por CMD52;
  - correcciones para kernel 7.x;
  - nombre de módulo `esp_hosted_mcu`.
- **Paquete `eh-wifi-sta`:** daemon de estación por RPC, con
  - `vfork` en NOMMU;
  - script de enlace bFLT completo;
  - corrección del reloj de las esperas con timeout en LinuxThreads.
- **Overlay `waveshare/`:**
  - `S40network`: deja `/boot` montado, arranca la estación y `udhcpc`;
  - `wifi-connect`, `boot-save`;
  - `inittab` para `ttyS0`.
