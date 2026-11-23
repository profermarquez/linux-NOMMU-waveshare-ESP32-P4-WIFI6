# Volver a la v1

La v1 está en la rama `main` de este repositorio.  La v2 usa la **misma tabla
de particiones y las mismas direcciones**, así que volver es grabar las
imágenes de la v1.  El ESP32-C6 no se toca en ningún caso.

## Opción A: con `flash.sh` (deja `/boot` de fábrica)

Borra la configuración de `/boot` (Wi-Fi, claves, programas guardados).  Los
datos de la microSD no se tocan.

```bash
git checkout main
./flash.sh /dev/cu.usbmodemXXXX
```

## Opción B: conservando `/boot`

Graba solo bootloader, tabla, shim, kernel, rootfs y DTB:

```bash
git checkout main
cd firmware && shasum -a 256 -c SHA256SUMS
esptool --chip esp32p4 --port /dev/cu.usbmodemXXXX --baud 115200 \
  --before default-reset --after hard-reset \
  write-flash -z --flash-mode keep --flash-freq keep --flash-size keep \
  0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 boot-shim.bin \
  0x90000 Image 0x810000 rootfs.squashfs 0xF10000 esp32p4-m5stamp-p4.dtb
```

La v1 no usa la microSD.  Si la v2 guardaba tu Wi-Fi solo en la tarjeta,
volvé a configurarlo con `wifi-connect`.

## Volver a la v2

```bash
git checkout linux-NOMMU-waveshare-ESP32-P4-WIFI6-mejorado
./flash.sh /dev/cu.usbmodemXXXX
```

Si hiciste un backup completo de la flash antes de empezar (ver
`INSTALAR.md`), también podés restaurarlo con
`esptool … write-flash 0x0 backup.bin`.
