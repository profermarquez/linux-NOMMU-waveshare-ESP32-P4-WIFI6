# Instalar Linux en la Waveshare ESP32-P4-WIFI6

## 0. Antes de empezar

- **Placa:** Waveshare ESP32-P4-WIFI6 con 32 MB de flash y ESP32-P4 rev ≥ v3.0.
- **Cable:** USB-C de datos, conectado al puerto USB-C de la placa.  Es un
  adaptador USB-serie CH343:
  - en **macOS** aparece como `/dev/cu.usbmodemXXXX` (o `/dev/cu.wchusbserialXXXX`);
  - en **Linux**, como `/dev/ttyACM0`;
  - en **Windows**, como un puerto `COM` (por ejemplo `COM3`): el número lo
    muestra el Administrador de dispositivos, en "Puertos (COM y LPT)".
    `flash.sh` corre en **Git Bash** (`./flash.sh COM3`); los comandos de
    `esptool` de esta guía también andan en PowerShell.  No se probó en
    Windows.

  Para encontrarlo:
  ```bash
  ls /dev/cu.* /dev/tty.usb* 2>/dev/null    # macOS
  ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null  # Linux
  ```
- **Python y esptool ≥ 5:**
  ```bash
  pip install "esptool>=5"
  esptool version
  ```
  En los ejemplos, `PUERTO` es tu puerto.

### 0.1 Comprobar la placa

```bash
esptool --chip esp32p4 --port PUERTO --baud 115200 flash-id
```

Tiene que mostrar `Chip type: ESP32-P4 (revision v3.x)` y `Detected flash
size: 32MB`.  Si dice 16MB o una revisión v1.x, **no sigas**: estas
imágenes son para 32 MB y rev v3.

### 0.2 Backup de la flash del P4 (recomendado)

Guarda una copia completa (32 MB, unos 50 minutos a 115200), para volver
atrás si hace falta:

```bash
esptool --chip esp32p4 --port PUERTO --baud 115200 read-flash 0 0x2000000 backup-p4-32mb.bin
shasum -a 256 backup-p4-32mb.bin > backup-p4-32mb.bin.sha256
```

Guardalo en un lugar seguro y **no lo publiques**: puede contener tus
contraseñas de Wi-Fi.

### 0.3 El firmware del ESP32-C6

El Wi-Fi depende de que el C6 tenga **ESP-Hosted-MCU 3.0.10** en modo
estación.  Este paquete **no** graba el C6.

- **Cómo saber qué tiene:** después de instalar Linux, mirá el `dmesg`
  (paso 3).  Si aparece `slave fw version: 0x0003000a` y `RPC version
  negotiated: V2`, está bien.
- **Si tiene otro firmware:**
  - El firmware del C6 corresponde al ejemplo `examples/wifi/sta/cp` de
    [esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu) v3.0.10
    (proyecto `eh_cp_wifi_sta`), compilado para `esp32c6` con transporte
    SDIO.
  - El C6 se graba por sus pads UART (`C6_U0RXD`/`C6_U0TXD`), con `C6_IO9`
    a masa al encender, según el FAQ de Waveshare.
  - **Este procedimiento no está probado en este proyecto.**  Grabar el C6
    es irreversible si no tenés un backup de su flash.
- **Para Bluetooth (BLE)** el C6 necesita el mismo firmware compilado con
  BLE.  Ese sí se probó: se graba desde Linux con `eh-wifictl c6-ota`, sin
  cables, y el de fábrica queda en la otra ranura.  Ver la guía, 11.13.
- **`C6_IO9` a masa es solo para grabarlo por cable.**  Si queda así, el C6
  arranca siempre en modo de programación y no hay Wi-Fi.

## 1. Grabar el P4

Desde la carpeta de este paquete:

```bash
./flash.sh PUERTO
```

El script:

1. Comprueba los SHA-256 de `firmware/`.
2. Lee el chip.
3. Te muestra qué va a escribir y pide que escribas `GRABAR`.
4. Graba las 7 imágenes.
5. Borra `bootmeta`, el registro que elige qué copia de `/boot` arrancar.
6. Verifica lo grabado y reinicia.

Tarda unos 12 minutos a 115200.

**Grabación manual** (lo mismo que hace el script):

```bash
cd firmware
esptool --chip esp32p4 --port PUERTO --baud 115200 --before default-reset --after no-reset \
  write-flash -z --flash-mode keep --flash-freq keep --flash-size keep \
  0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 boot-shim.bin \
  0x90000 Image 0x810000 rootfs.squashfs 0xF10000 esp32p4-m5stamp-p4.dtb 0xF40000 boot.img
esptool --chip esp32p4 --port PUERTO --baud 115200 --after hard-reset erase-region 0xFC0000 0x2000
```

Si la grabación falla a mitad, repetila: la placa siempre puede entrar al
modo de grabación.  Si no responde, mantené **BOOT**, apretá y soltá
**RST**, y soltá BOOT.

## 2. Abrir la consola serie

```bash
python -m serial.tools.miniterm PUERTO 115200     # salir: Ctrl+]
# o: screen PUERTO 115200  /  minicom -D PUERTO -b 115200
```

- **Abrir el puerto reinicia la placa**: es el auto-reset del CH343.
- En unos 20 s aparece `Please press Enter to activate this console`.
  Apretá Enter y quedás como **root**.

## 3. Comprobar que el C6 está bien

```sh
dmesg | grep -E 'slave fw version|RPC version'
```

Lo correcto es:

```
esp_hosted_mcu: process_init_event: slave fw version: 0x0003000a
esp_hosted_mcu: diagnose_peer_rpc_version: RPC version negotiated: V2
```

Al arrancar también vas a ver líneas que **no son errores graves**:

- `mmc0: error -110 whilst initialising SDIO card`: es el primer intento;
  después se detecta bien.
- `probe with driver esp_hosted_mcu failed with error -22`: es la función
  2 del C6, que no se usa.

## 4. Conectar al Wi-Fi

En la consola serie:

```sh
wifi-connect
```

1. Escribí el **nombre de la red** (respeta mayúsculas) y la **contraseña**
   (no se muestra).
2. Se guarda primera (máxima prioridad) en `/boot/wifi-networks.conf` y,
   si hay microSD, también en la tarjeta.
3. Al terminar muestra la IP (`inet addr:`).

**Alternativa manual** (como root), útil si `wifi-connect` falla:

```sh
grep -q ' /boot ' /proc/mounts || mount -t vfat -o loop,rw,sync,umask=0077 /tmp/boot.img /boot
umask 077
printf 'ssid=%s\npsk=%s\n' 'TU_RED' 'TU_CLAVE' > /boot/wifi-sta.conf
boot-save
/etc/init.d/S40network restart
sleep 20; ifconfig ethsta0
```

**Sin consola (v2):** escribí las redes en `wifi.txt` en la partición
`WSFILES` de la microSD (ver `examples/wifi-ejemplo.txt` y la sección 11.2
de la guía).

El método manual de arriba sigue sirviendo: la v2 importa
`/boot/wifi-sta.conf` una vez a su lista de redes.

Notas:
- Solo redes **2.4 GHz**, WPA2-PSK.
- Punto de acceso: `ap-on` (v2, ver la guía, sección 11.2).

> `wifi-connect` está en la imagen, pero en la placa de prueba la red se
> cargó con el método manual.  Si `wifi-connect` falla, usá el manual y
> avisá en un *issue*.

## 5. Entrar por SSH

### 5.1 Contraseñas

- En el primer arranque la placa genera contraseñas propias para `m5` y
  `root`.  Las podés ver en la consola serie:
  ```sh
  cat /boot/credentials.txt
  ```
- Para cambiarlas: editá las líneas `password=` (m5) y `root_password=`
  (root) en ese archivo, corré `boot-save` y reiniciá.  `passwd` sola no
  alcanza: las contraseñas se vuelven a aplicar desde ese archivo en cada
  arranque.

### 5.2 Entrar

```bash
ssh m5@stamp-p4.local          # o m5@<IP>
```

`root` por SSH entra **solo con clave**.

### 5.3 Agregar tu clave pública

Desde tu computadora, si todavía no tenés una:

```bash
ssh-keygen -t ed25519
```

Copiá el contenido de `~/.ssh/id_ed25519.pub`.  En la placa, como root:

```sh
grep -q ' /boot ' /proc/mounts || mount -t vfat -o loop,rw,sync,umask=0077 /tmp/boot.img /boot
echo 'ssh-ed25519 AAAA... tu-comentario' >> /boot/authorized_keys       # para m5
echo 'ssh-ed25519 AAAA... tu-comentario' >> /boot/root_authorized_keys  # para root
boot-save
reboot
```

**La host key de SSH cambia en cada arranque** (diseño del proyecto base).
Después de reiniciar, tu computadora va a avisar que la huella cambió.
Borrala y volvé a entrar:

```bash
ssh-keygen -R stamp-p4.local; ssh-keygen -R <IP>
```

### 5.4 Copiar archivos

```bash
scp -O archivo m5@stamp-p4.local:/tmp/    # -O es obligatorio: no hay sftp
```

## 6. Seguridad

- La **consola serie da root sin contraseña**: quien tenga el cable tiene
  acceso total.
- La contraseña del Wi-Fi queda en texto plano en `/boot/wifi-sta.conf`
  (solo legible por root).
- SSH escucha en el puerto 22 de la red Wi-Fi.  Usala en redes de
  confianza y cambiá las contraseñas generadas si las compartiste.

## 7. Volver atrás

- **A tu firmware anterior** (con el backup del paso 0.2):
  ```bash
  shasum -a 256 -c backup-p4-32mb.bin.sha256
  esptool --chip esp32p4 --port PUERTO --baud 115200 write-flash 0x0 backup-p4-32mb.bin
  ```
- **Reinstalar este Linux sin perder `/boot`** (Wi-Fi, claves, programas):
  grabá todo menos `boot.img` y sin borrar `bootmeta`:
  ```bash
  cd firmware
  esptool --chip esp32p4 --port PUERTO --baud 115200 write-flash -z --flash-mode keep --flash-freq keep --flash-size keep \
    0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 boot-shim.bin \
    0x90000 Image 0x810000 rootfs.squashfs 0xF10000 esp32p4-m5stamp-p4.dtb
  ```
- **Volver `/boot` a fábrica**: grabá `boot.img` en 0xF40000 y borrá
  `0xFC0000` (`erase-region 0xFC0000 0x2000`).  Se pierden la red, las
  claves y las contraseñas guardadas.

## 8. Problemas comunes

| Síntoma | Causa probable / solución |
|---|---|
| No aparece el puerto | Cable solo de carga; probá otro.  En macOS puede hacer falta el driver de WCH para CH343. |
| `flash-id` no conecta | Entrá en modo grabación: mantené BOOT, apretá RST, soltá BOOT. |
| `wifi-connect` → `Read-only file system` | `/boot` no está montado.  Montalo (ver la alternativa manual del paso 4). |
| No hay `ethsta0` | El driver del C6 no cargó: mirá `dmesg | grep -i esp`. |
| `ethsta0` sin IP | Revisá SSID y clave (2.4 GHz) y `/run/eh-wifi-sta.log`.  `reason 201: NO_AP_FOUND` = no ve la red. |
| No hay `slave fw version: 0x0003000a` | El C6 tiene otro firmware (paso 0.3). |
| SSH: `REMOTE HOST IDENTIFICATION HAS CHANGED` | Normal después de reiniciar (paso 5.3). |
| SSH: `Connection reset by peer` | Falta memoria o pasaste 20 conexiones por minuto.  Esperá un minuto. |
| La placa no responde | Botón RST, o desconectar y conectar el USB-C. |
