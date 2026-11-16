#!/bin/sh
# Graba Linux NOMMU en el ESP32-P4 de una Waveshare ESP32-P4-WIFI6.
#
#   ./flash.sh PUERTO            ej.: ./flash.sh /dev/cu.usbmodem1101   (macOS)
#                                     ./flash.sh /dev/ttyACM0           (Linux)
#
# Requiere esptool >= 5.0 (pip install "esptool>=5").  Solo escribe la flash del
# ESP32-P4: NO toca el ESP32-C6.  Borra lo que haya en el P4 (hacé un backup
# antes: ver docs/INSTALAR.md).
set -eu

PORT=${1:-}
BAUD=${BAUD:-115200}   # 460800 falló en la placa de prueba; 115200 es seguro
ESPTOOL=${ESPTOOL:-esptool}
DIR=$(cd "$(dirname "$0")/firmware" && pwd)

if [ -z "$PORT" ]; then
	echo "Uso: $0 PUERTO   (por ejemplo /dev/cu.usbmodem1101 o /dev/ttyACM0)" >&2
	exit 1
fi
if ! command -v "$ESPTOOL" >/dev/null 2>&1; then
	if command -v esptool.py >/dev/null 2>&1; then ESPTOOL=esptool.py
	else echo 'No encuentro esptool.  Instalalo con: pip install "esptool>=5"' >&2; exit 1; fi
fi

echo "== Verificando las imágenes (SHA-256)"
cd "$DIR"
if command -v sha256sum >/dev/null 2>&1; then sha256sum -c SHA256SUMS
else shasum -a 256 -c SHA256SUMS; fi

echo "== Chip conectado en $PORT"
"$ESPTOOL" --chip esp32p4 --port "$PORT" --baud "$BAUD" flash-id || {
	echo "No pude hablar con el ESP32-P4.  ¿Es el puerto USB-C de la placa?" >&2; exit 1; }

cat <<EOF

Se va a ESCRIBIR la flash del ESP32-P4 en $PORT:
  0x002000  bootloader.bin
  0x008000  partition-table.bin
  0x010000  boot-shim.bin
  0x090000  Image
  0x810000  rootfs.squashfs
  0xF10000  esp32p4-m5stamp-p4.dtb
  0xF40000  boot.img            (/boot de fábrica)
y se va a BORRAR 0xFC0000-0xFC1FFF (bootmeta: elige qué copia de /boot usar).
El firmware actual del P4 se pierde.  El ESP32-C6 no se toca.
La placa tiene que ser una ESP32-P4-WIFI6 con 32 MB de flash y chip rev >= v3.0.
EOF
printf 'Escribí GRABAR para continuar: '
read -r ans
[ "$ans" = "GRABAR" ] || { echo "Cancelado."; exit 1; }

"$ESPTOOL" --chip esp32p4 --port "$PORT" --baud "$BAUD" \
	--before default-reset --after no-reset \
	write-flash -z --flash-mode keep --flash-freq keep --flash-size keep \
	0x2000 bootloader.bin \
	0x8000 partition-table.bin \
	0x10000 boot-shim.bin \
	0x90000 Image \
	0x810000 rootfs.squashfs \
	0xF10000 esp32p4-m5stamp-p4.dtb \
	0xF40000 boot.img

"$ESPTOOL" --chip esp32p4 --port "$PORT" --baud "$BAUD" \
	--before default-reset --after no-reset \
	erase-region 0xFC0000 0x2000

"$ESPTOOL" --chip esp32p4 --port "$PORT" --baud "$BAUD" \
	--before default-reset --after hard-reset \
	verify-flash \
	0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 boot-shim.bin \
	0x90000 Image 0x810000 rootfs.squashfs 0xF10000 esp32p4-m5stamp-p4.dtb

echo
echo "Listo.  Abrí la consola serie a 115200 (ver docs/INSTALAR.md) y seguí con wifi-connect."
