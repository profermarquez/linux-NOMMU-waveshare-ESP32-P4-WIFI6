#!/bin/sh
# radio-rfi.sh - RFI Monde on the board's speaker, as loud as it goes
# without distortion.
#
#   radio-rfi.sh [SEGUNDOS]        (as root; Ctrl+C or "killall radio" stops it)
#
# Sets the ES8311 DAC volume (register 0x32 over /dev/i2c-0) to 0xbf = 0 dB,
# the highest level that does not clip, plays at 100 % and puts the previous
# value back when it ends.  Above 0xbf the codec adds digital gain: louder,
# but distorted, and hard on a small speaker.
# As m5 (no access to /dev/i2c-0) it plays at the codec's boot level.

RADIO=/home/m5/bin/radio
ANTERIOR=$(i2cget -y 0 0x18 0x32 2>/dev/null)

if [ -n "$ANTERIOR" ]; then
	i2cset -y 0 0x18 0x32 0xbf && echo "codec: volumen del DAC $ANTERIOR -> 0xbf (0 dB, el maximo sin distorsion)"
	trap 'i2cset -y 0 0x18 0x32 "$ANTERIOR"; echo "codec: volumen del DAC vuelto a $ANTERIOR"' EXIT
	trap 'exit 0' INT TERM
else
	echo "aviso: sin acceso a /dev/i2c-0 (entra como root); suena al volumen del arranque" >&2
fi

if [ -n "$1" ]; then
	"$RADIO" -v 100 -s "$1"
else
	"$RADIO" -v 100
fi
