#!/bin/sh
# estado.sh - resumen del estado de la placa (BusyBox sh).
# Uso: sh estado.sh   (o chmod +x y ./estado.sh)
echo "Hora:      $(date)"
echo "Encendida: $(cut -d. -f1 /proc/uptime) s"
ip=$(ifconfig ethsta0 2>/dev/null | sed -n 's/.*inet addr:\([0-9.]*\).*/\1/p')
echo "IP Wi-Fi:  ${ip:-sin IP}"
set -- $(free | sed -n 2p)
echo "Memoria:   $3 KB usados / $2 KB total"
while read -r k v; do
	case "$k" in used|total|largest_free) echo "Pool NOMMU $k: $((v / 1024)) KB" ;; esac
done < /sys/kernel/board/nommu_pool
df -h /boot | sed -n 2p | awk '{print "/boot:     " $3 " usados, " $4 " libres"}'
if ping -c 1 8.8.8.8 >/dev/null 2>&1 || ping 8.8.8.8 >/dev/null 2>&1; then echo "Internet:  OK"; else echo "Internet:  sin salida"; fi
