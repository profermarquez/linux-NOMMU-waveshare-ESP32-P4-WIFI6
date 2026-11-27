# Guía de uso: Linux en la Waveshare ESP32-P4-WIFI6

Revisada el 2026-11-15 en una placa de prueba (por SSH) y contra el
código fuente.  En las tablas:

- **Verificado**: lo probé en la placa.
- **Teórico**: sale del código o de la documentación, sin prueba en la placa.

> **v2 (2026-11-23).**  Esta rama agrega microSD, NTP y zona horaria,
> varias redes Wi-Fi y punto de acceso, HTTPS, C++, GPIO, I2C y SPI.  Lo
> nuevo está en la **sección 11**.  La tabla de la sección 6 ya está al día.
> Las secciones 1 a 5 y 7 a 10 describen la v1: donde dicen "no hay", mirá
> primero la sección 11.  Cambios y límites: `CHANGELOG.md`.  Volver a la v1:
> `docs/VOLVER-A-V1.md`.

---

## 1. Resumen de una página

**Qué es.**  Un Linux real (kernel 7.2.11) corriendo en el chip ESP32-P4 de
la placa.  Tiene una shell como la de cualquier Linux (BusyBox), SSH,
MicroPython y Wi-Fi a través del ESP32-C6 de la placa.  Podés:

- conectarte desde tu computadora;
- copiar programas y correrlos;
- escribir scripts;
- levantar servicios de red chicos.

**Qué no es.**  No es una Raspberry Pi ni una PC chica:

- **No tiene MMU** (unidad de manejo de memoria).
  - Los programas comparten un único espacio de memoria física; no hay
    memoria virtual ni swap.
  - Un programa no puede crecer más allá del bloque contiguo libre más
    grande (hoy, unos 2–3 MB).
  - No existe `fork()`, solo `vfork()`.  Muchos programas de Linux no
    compilan o no funcionan tal cual.
- **Memoria chica.**  Linux ve unos 16 MB de RAM.  Los programas de
  usuario se reparten un *pool* de 10 MB, que comparten con el sistema.
- **No hay `apt`, ni `gcc`, ni `pip` en la placa.**  Los programas en C se
  compilan en otra computadora (Linux, con la toolchain de Buildroot) y se copian ya
  compilados.  Instalar programas nuevos significa recompilar la imagen
  con Buildroot y grabarla.
- **Formato de los binarios.**  Son **bFLT** (no ELF), enlazados contra
  **uClibc-ng**.  No corre ningún binario de RISC-V "normal" bajado de
  internet.
- **Un solo núcleo**, de los dos que tiene el chip (`maxcpus=1`).  RISC-V
  de 32 bits, sin unidad de punto flotante visible para Linux
  (`rv32imac`): los cálculos con `float` se emulan por software y son
  lentos.
- **Red Wi-Fi lenta.**  Por SSH se midieron unos **140 KB/s de subida y
  170 KB/s de bajada**, porque el C6 está conectado por SDIO de 1 bit a
  5 MHz.
- **Casi nada se guarda solo.**  Solo `/boot` (256 KB) es persistente, y
  únicamente después de correr `boot-save`.  Todo lo demás vive en RAM y
  se borra al reiniciar.

**Para qué sirve bien**: aprender Linux embebido, scripts y servicios de red
chicos, recolectar datos y mandarlos a un servidor, prototipos en
MicroPython o C.

**Para qué no sirve todavía**: tocar hardware (GPIO, I2C, SPI no están
habilitados en Linux), HTTPS, programas grandes (Node, CPython, Java),
almacenamiento grande (microSD y USB todavía no funcionan).

---

## 2. Cómo conectarte

### 2.1 Por SSH (lo normal)

Desde una terminal de tu computadora:

```bash
ssh m5@<IP-de-la-placa>          # usuario normal (clave SSH o contraseña)
ssh root@<IP-de-la-placa>   # root: solo con clave
```

- Para entrar con clave, agregá tu clave pública (ver `docs/INSTALAR.md`, paso 5.3).
- **La placa cambia su "huella" SSH (host key) en cada arranque.**  Después
  de reiniciarla, tu computadora muestra *REMOTE HOST IDENTIFICATION HAS CHANGED*.
  Es normal en este sistema.  Borrá la huella vieja y volvé a conectar:

  ```bash
  ssh-keygen -R <IP-de-la-placa>
  ```

  
- **Límite de conexiones.**  `inetd` acepta como mucho **20 conexiones SSH
  por minuto**; si lo pasás, deja de escuchar 60 s.  Además, si falta
  memoria, la conexión se corta con `Connection reset by peer`.  Si te
  pasa, esperá un minuto y reintentá.

### 2.2 Por consola serie (cuando no hay red)

Conectá el cable al USB-C de la placa (es un adaptador USB-serie CH343):

```bash
python -m serial.tools.miniterm PUERTO 115200
```

- **Abrir el puerto reinicia la placa**: es el auto-reset del adaptador.
  Esperá unos 20 s y apretá Enter en `Please press Enter to activate this
  console`.  Quedás como **root** sin contraseña.
- Para salir de miniterm: `Ctrl+]`.  Con minicom: `Ctrl+A`, después `X`.
- Esta consola es independiente del Wi-Fi y del USB: siempre funciona.

### 2.3 Si la IP cambia

La IP la da el router por DHCP.

1. **Por nombre (mDNS)**: la placa anuncia `stamp-p4.local` (verificado
   desde una Mac en la misma red):
   ```bash
   ssh m5@stamp-p4.local
   ```
2. **Por consola serie**: `ifconfig ethsta0` muestra la IP (`inet addr:`).
3. **En el router**: buscá el dispositivo con MAC de la placa (`ifconfig ethsta0`, campo `HWaddr`).
   Podés reservarle esa IP en el router para que no cambie.

---

## 3. Cambiar la red Wi-Fi y las contraseñas

### 3.1 Red Wi-Fi

> v2: además de lo de abajo, hay `wifi-connect`, `wifi-scan`,
> `wifi-status`, `wifi-forget` y `wifi.txt` en la microSD (sección 11.2).

Por SSH como root, o en la consola serie:

```sh
wifi-connect
```

1. Te pide el SSID (respeta mayúsculas y minúsculas) y la contraseña, que
   no se muestra mientras la escribís.
2. La guarda en `/boot/wifi-sta.conf` y la graba en la flash con
   `boot-save`.
3. Reinicia la conexión y te muestra la IP nueva.

> **Ojo:** si lo hacés por SSH y la red nueva no es la misma, se corta tu
> sesión SSH.  Hacelo por consola serie, o sabiendo que vas a tener que
> buscar la IP nueva.  `wifi-connect` está corregido en la imagen
> instalada, pero **no se probó de punta a punta** (la red actual se cargó
> con otro método).

Si preferís hacerlo a mano (como root):

```sh
vi /boot/wifi-sta.conf      # dos líneas: ssid=MiRed  y  psk=MiClave
boot-save                   # graba /boot en la flash
/etc/init.d/S40network restart
```

Solo funciona como **cliente** (se conecta a un router).  El modo punto de
acceso (que la placa cree su propia red) no está implementado en esta
versión.

### 3.2 Contraseñas de usuario

- **Las contraseñas se aplican en cada arranque desde
  `/boot/credentials.txt`.**  Por eso `passwd` sola **no alcanza**: el
  cambio se pierde al reiniciar.
- Para cambiar la contraseña de `m5` (o la de root), como root:

  ```sh
  vi /boot/credentials.txt     # cambiá la línea password=  (o root_password=)
  boot-save
  reboot
  ```

- `root` por SSH entra **solo con clave**, aunque tenga contraseña.  La
  contraseña de root sirve en la consola serie (que hoy entra sin pedirla)
  y con `su`.
- Las claves SSH se instalan en cada arranque desde
  `/boot/authorized_keys` (para m5) y `/boot/root_authorized_keys` (para
  root).  Para agregar otra clave: agregá la línea al archivo y corré
  `boot-save`.

---

## 4. Cómo programar

Los ejemplos están en `examples/` y se
probaron en la placa el 2026-11-15.

### 4.1 Script de shell

`examples/shell/estado.sh` muestra la hora, el tiempo encendida, la IP, la
memoria, el pool NOMMU, el espacio en `/boot` y si hay Internet.

```bash
scp -O examples/shell/estado.sh m5@<IP-de-la-placa>:/tmp/
ssh m5@<IP-de-la-placa> 'sh /tmp/estado.sh'
```

Salida real:

```
IP Wi-Fi:  <IP-de-la-placa>
Memoria:   7168 KB usados / 16696 KB total
Pool NOMMU total: 10240 KB
Pool NOMMU used: 6176 KB
Pool NOMMU largest_free: 3152 KB
/boot:     102.0K usados, 150.0K libres
Internet:  OK
```

La shell es BusyBox `ash` (`/bin/sh`): sirve casi todo lo de `sh`, pero no
hay `bash`.

### 4.2 MicroPython con red

**Cliente HTTP** (`examples/micropython/consulta_http.py`), con sockets:

```bash
scp -O examples/micropython/consulta_http.py m5@<IP-de-la-placa>:/tmp/
ssh m5@<IP-de-la-placa> 'python3 /tmp/consulta_http.py example.com /'
```

```
HTTP/1.1 200 OK
bytes de contenido: 577
título: Example Domain
```

Hay que elegir la dirección IPv4 a mano: el kernel no tiene IPv6, y
`getaddrinfo` puede devolver primero una IPv6 (`OSError: 97`).  El ejemplo
ya lo hace.

**Servidor HTTP** (`examples/micropython/servidor_http.py`), con `asyncio`:
- `/` devuelve una página con el tiempo encendida y la memoria libre;
- `/api` devuelve lo mismo en JSON.

```bash
scp -O examples/micropython/servidor_http.py m5@<IP-de-la-placa>:/tmp/
ssh m5@<IP-de-la-placa> 'python3 /tmp/servidor_http.py 8080'
# en otra terminal de tu computadora, o en el navegador:
curl http://stamp-p4.local:8080/api
```

Respuesta real: `{"uptime_s": 1691, "pool_kb": {"largest_free": 1644, "used": 5852, "total": 10240}, ...}`

Para cortarlo: `Ctrl+C`.

### 4.3 Programa en C (compilado en otra computadora)

`examples/c-sysinfo/sysinfo.c` muestra el nombre, el kernel y la RAM libre,
y prueba reservar memoria.

```bash
# 1. compilar (con la toolchain de Buildroot, ver docs/COMPILAR.md)
cd examples/c-sysinfo && make      # en la máquina Linux con la toolchain (docs/COMPILAR.md)
# 2. copiar y ejecutar
scp -O examples/c-sysinfo/sysinfo m5@<IP-de-la-placa>:/tmp/
ssh m5@<IP-de-la-placa> '/tmp/sysinfo 2048; /tmp/sysinfo 4096'
```

```
stamp-p4 7.2.11 riscv32
encendida 1686 s, RAM libre 4600 KB de 16696 KB
malloc(2048 KB) OK
...
malloc(4096 KB) falló: no hay un bloque contiguo así de grande
```

Reglas para compilar (ya están en el `Makefile`):

| Regla | Por qué |
|---|---|
| `-Os -fPIC -Wl,-elf2flt=-r` | `-elf2flt` sin `=-r` compila pero el programa da *Segmentation fault* (faltan reubicaciones). |
| `flthdr -s 65536 programa` | la pila por defecto (4 KB) no alcanza ni para `printf`. |
| solo C, no C++ | la toolchain actual no incluye `g++`. |

Para que un programa quede guardado después de reiniciar: copialo a
`/boot/programs/` (como root) y corré `boot-save`.  Hay unos 150 KB libres.

---

## 5. Pasar archivos entre tu computadora y la placa

| Qué | Cómo |
|---|---|
| Computadora → placa | `scp -O archivo m5@<IP-de-la-placa>:/tmp/` |
| Placa → computadora | `scp -O m5@<IP-de-la-placa>:/tmp/archivo .` |
| Sin scp | `cat archivo \| ssh m5@<IP-de-la-placa> 'cat > /tmp/archivo'` |
| Placa baja de internet (solo HTTP) | `wget http://servidor/archivo -O /tmp/archivo` |

- **`-O` es obligatorio** en `scp`: la placa no tiene `sftp-server`.
- Destinos posibles:
  - `/tmp` (RAM; **compartido con `/home` y `/root`: 1.9 MB en total**);
  - `/boot` (256 KB, persistente con `boot-save`).
- Velocidad medida: ~140 KB/s de subida, ~170 KB/s de bajada.
- No hay `rsync`, `curl` ni `sftp`.  `wget` no hace HTTPS.

---

## 6. Tabla de capacidades

Estado en la **v2**.  "Verificado" = probado en la placa.

| Capacidad | Estado | Explicación |
|---|---|---|
| Wi-Fi cliente (WPA2) | ✅ Funciona | Varias redes con prioridad (`eh-wifid`).  Verificado. |
| Wi-Fi punto de acceso | ✅ Funciona | `ap-on` / `ap-off` / `ap-config`.  Sección 11.2. |
| SSH con clave / contraseña | ✅ Funciona | Dropbear por inetd.  Host key nueva en cada arranque. |
| scp | ⚠️ Limitado | Solo con `scp -O`.  Sin sftp. |
| mDNS (`stamp-p4.local`) | ✅ Funciona | Verificado desde una Mac. |
| Hora por NTP y zona horaria | ✅ Funciona | Al recibir IP.  `config.txt` en la microSD.  Sección 11.3. |
| HTTP cliente | ✅ Funciona | `wget` y sockets de MicroPython. |
| HTTPS / TLS | ✅ Funciona (TLS 1.2) | `https-get`, `wget https://…`, `micropython-tls`.  Verifica certificado y nombre.  ~1 MB de RAM por conexión.  Sección 11.4. |
| IPv6 | ❌ No | Desactivado en el kernel. |
| MicroPython 1.22.2 | ✅ Funciona | `micropython` (sin `ssl`) y `micropython-tls` (con `ssl`). |
| Programas en C (bFLT) | ✅ Funciona | Compilados en la VM. |
| C++ | ✅ Funciona | STL, excepciones, threads, sockets.  Toolchain aparte.  Sección 11.5. |
| Persistencia de archivos | ✅ Funciona | `/boot` (256 KB, `boot-save`) y **microSD**. |
| microSD | ✅ Funciona | Por SPI (spi-gpio), junto con el Wi-Fi.  Sección 11.1. |
| USB (pendrive, disco, red) | ❌ No | Ver sección 8.2. |
| GPIO desde Linux | ✅ Funciona | `pin`, libgpiod 2.2, `gpio.py`, ejemplo en C.  Verificado con un LED.  Sección 11.6. |
| I2C | ✅ Funciona | Por software en GPIO7/8, `/dev/i2c-0`.  Verificado: `i2cdetect` ve el códec ES8311 (0x18). |
| SPI | ⚠️ Parcial | Por software, `/dev/spidev1.0`.  Transfiere; loopback MOSI→MISO sin verificar. |
| UART extra | ❌ No | Solo la consola `ttyS0`. |
| LED / botones | ⚠️ Externos | La placa solo tiene el LED de encendido.  Un LED o botón en un GPIO libre sí funciona. |
| Sensor de temperatura del chip | ⏸️ En pausa | El driver colgaba el arranque; desactivado (comando `temperatura` presente, sin sensor). |
| Deep sleep / LP core | ❓ No probado | |
| Segundo núcleo | ❌ No | Linux corre en 1 núcleo. |
| tcpdump | ✅ Instalado | Para mirar tráfico de red. |

---

## 7. Límites prácticos

| Límite | Valor medido | Qué significa |
|---|---|---|
| RAM total que ve Linux | 16.3 MB | El resto de los 32 MB de PSRAM no está asignado a Linux (no investigado). |
| Pool NOMMU para programas | 10 MB en 2 regiones (4 + 6 MB) | Ahí viven todos los programas, incluidos los del sistema. |
| Usado al estar quieta | ~6–7 MB | Wi-Fi, SSH, mdnsd, etc. |
| Bloque libre más grande | ~1.6–3 MB | **Límite real de un programa**: `malloc` de 2 MB funcionó y el de 4 MB falló. |
| `/tmp` + `/home` + `/root` | 1.9 MB en total | Son el mismo disco en RAM: si llenás uno, se llenan todos. |
| `/boot` | 150 KB libres | Persistente con `boot-save`. |
| Sesiones SSH simultáneas | **3–4** | Con 6 a la vez: 4 entraron (3 con "Segmentation fault" al cerrar) y 2 fueron rechazadas por memoria. |
| Conexiones SSH por minuto | 20 | Pasado eso, inetd deja de aceptar 60 s. |

> **Visto en la prueba:** con el servidor MicroPython de la sección 4.2
> corriendo y varias sesiones abiertas, SSH rechazó conexiones nuevas
> (`Connection reset by peer`) durante varios minutos.  El kernel negaba
> memoria a cada Dropbear nuevo (`user_denied` subió de 52 a 72).  Se
> recuperó solo al liberarse memoria.  Si dejás programas grandes
> corriendo, dejá margen para poder entrar.  La consola serie siempre
> funciona.
>
> Y un detalle: `pkill -f nombre` también mata tu propia sesión si
> `nombre` aparece en el comando que escribiste.  Usá `ps` + `kill PID`.

**Si te quedás sin memoria**:
- Lo normal es que el programa nuevo no arranque (`Connection reset by
  peer` en SSH, `can't allocate` o *Segmentation fault*).  El kernel
  protege una reserva para el sistema (`user_reserve_total`), así que
  normalmente la placa sigue viva.
- Mirá el estado:
  ```sh
  cat /sys/kernel/board/nommu_pool   # used, largest_free, user_denied
  ```
- Liberá: cortá tus programas (`ps`, `kill PID`) y borrá archivos de
  `/tmp`.

**Si la placa se cuelga**:

1. Probá la consola serie (abrirla ya la reinicia).
2. Apretá el botón **RST** de la placa.
3. Desenchufá y volvé a enchufar el USB-C.

En los tres casos se pierde lo que estaba en RAM.  Lo guardado en `/boot`
queda.  Hay un *watchdog* activo que reinicia la placa si el sistema deja
de responder del todo.

**Recuperación total**: ver `docs/INSTALAR.md`, sección "Volver atrás".

---

## 8. microSD y USB

### 8.1 microSD

> v2: **la microSD ya funciona**, por SPI y sin afectar el Wi-Fi (sección
> 11.1).  Lo que sigue es el análisis de la v1.

**Dónde está conectada** (evidencia):

- **Ranura:** GPIO43 (CLK), 44 (CMD), 39–42 (D0–D3).  Fuentes:
  - la wiki de Waveshare (ejemplo SDMMC: `clk = 43; cmd = 44; d0 = 39 …`);
  - el ejemplo `09_sdmmc` de ESP32-P4-Platform;
  - `WAVESHARE_XC.md`.
- **Slot:** esos son exactamente los pines IO-MUX fijos del **slot 0** del
  controlador SDMMC del P4 (`SDMMC_SLOT0_IOMUX_PIN_NUM_*` en ESP-IDF).  El
  ejemplo de Waveshare igual los usa a través del slot 1 con la matriz de
  GPIO.
- **C6:** usa el **slot 1** (GPIO14–19 por la matriz de GPIO).
- **En hardware**, el P4 tiene los dos slots en el mismo controlador y
  ESP-IDF puede usarlos a la vez.
- **Boot-shim:** no toca los pines 39–44; solo configura el slot 1.
- **No verificado:** cómo se alimenta la tarjeta (GPIO o LDO interno).  No
  leí el esquemático (`ESP32-P4-WIFI6-datasheet.pdf` en el sitio de
  Waveshare).

**Qué tiene el kernel**:

- **Sí tiene:** MMC/SD de bloque (`CONFIG_MMC_BLOCK`), el driver `dw_mmc`,
  y FAT/VFAT (`CONFIG_VFAT_FS`).
- **No tiene:** ext4 (solo ext2) ni exFAT.  Las tarjetas de más de 32 GB
  vienen en exFAT: habría que formatearlas en FAT32.
- **El driver `dw_mmc` maneja un solo slot a la vez.**
  - Este proyecto le agregó la opción `snps,slot-id` para elegir el slot,
    pero hay un solo `slot_id` por controlador.
  - Hoy está en 1, el C6.
  - Con el driver actual, **microSD y Wi-Fi no pueden funcionar juntos**.

**Alternativas**:

| Opción | Qué implica | Esfuerzo | Riesgo |
|---|---|---|---|
| A. Elegir al arrancar: DTB alternativo con `snps,slot-id = <0>`, pines del slot 0 y 4 bits | Solo DTB + recompilar.  **Sin Wi-Fi** mientras se usa la SD (sirve para copiar datos por consola serie). | Bajo (1 día) | Bajo |
| B. **Parche multi-slot** en `dw_mmc` (dos `mmc_host` sobre el mismo controlador, alternando el slot por comando) | Desarrollo de kernel.  Hay que serializar los dos hosts y reprogramar el ancho de bus y el reloj en cada cambio.  El driver viejo de Linux (antes de 4.x) lo hacía. | Alto (1–3 semanas) | Medio: un error puede romper el Wi-Fi |
| C. SD por SPI (`mmc_spi`) | El kernel no tiene SPI habilitado ni driver SPI para el P4.  Habría que escribir el driver SPI. | Alto | Medio |
| D. Alternar en caliente: desmontar y cambiar `slot_id` | Muy frágil: el C6 pierde el estado. | Medio | Alto |

**Recomendación:** si solo querés leer o escribir la tarjeta de vez en
cuando, la **A**.  Si querés SD y Wi-Fi a la vez, la **B**: es la única
solución limpia, pero es desarrollo de kernel.

**Clasificación: necesita desarrollo nuevo** (B), o solo configuración si
aceptás no tener Wi-Fi (A).

**Hardware necesario:** una microSD de hasta 32 GB en FAT32.

### 8.2 USB

**Conectores de la placa** (docs de Waveshare, verificado el texto):

| Conector | Va a | Uso hoy |
|---|---|---|
| **USB-C** | Adaptador USB-serie **CH343** → UART0 del P4 (GPIO37/38) | **Consola serie** y grabación. |
| **Conector MX1.25 de 4 pines** ("4PIN USB") | **USB 2.0 OTG High-Speed** del P4 | Nada. |
| USB Full-Speed (OTG 1.1 / Serial-JTAG) del P4 | Según Waveshare no tiene conector propio | Nada.  En M5Stamp era el "pendrive" de `/boot`. |

- **5 V hacia un dispositivo:** la documentación **no dice** si el
  conector de 4 pines entrega 5 V.  No leí el esquemático.  Suponé que
  necesitás alimentar el dispositivo aparte (hub con fuente, o un cable que
  inyecte 5 V).
- **Driver DWC2 actual:**
  - compilado **solo en modo dispositivo** (`CONFIG_USB_DWC2_PERIPHERAL=y`,
    `CONFIG_USB` desactivado, así que no hay modo host);
  - de funciones de dispositivo solo tiene la de almacenamiento masivo
    (`g_mass_storage`, cargado pero sin controlador);
  - el nodo del DTB es el del USB **Full-Speed** (`usb@50040000`) y está
    **desactivado**;
  - **no hay nodo ni soporte para el High-Speed**, que es el que llega al
    conector de 4 pines: no hay inicialización del PHY UTMI ni en el shim
    ni en el kernel.

| Opción | Clasificación | Qué habría que cambiar | Esfuerzo | Riesgo | Hardware extra |
|---|---|---|---|---|---|
| **a. Pendrive (placa como host)** | **Necesita desarrollo nuevo** | Kernel: `CONFIG_USB`, DWC2 host/dual-role, `USB_STORAGE`, `SCSI`, `BLK_DEV_SD`.  DTB: nodo del OTG HS.  Shim o kernel: encender el PHY HS.  El driver DWC2 en host sobre NOMMU con 16 MB es justo de memoria. | Alto (2–4 semanas) | Medio | Cable MX1.25 → USB-A hembra, **5 V externos** para el pendrive |
| **b. Placa como disco para la computadora** | **Necesita desarrollo nuevo** (menor) | Habilitar el OTG HS en modo dispositivo: nodo DTB + PHY.  `g_mass_storage` y `usb-lun` ya existen y funcionaban en M5Stamp. | Medio (1–2 semanas) | Bajo-medio | Cable MX1.25 → USB (A o C) macho hacia la computadora |
| **c. Red por USB (CDC-ECM/RNDIS): SSH por cable** | **Necesita desarrollo nuevo** (lo mismo que b, más) | Lo de b, más `CONFIG_USB_ETH` / `USB_F_ECM` (y RNDIS si hiciera falta), y un script para la IP del lado de la placa.  macOS usa CDC-ECM sin drivers. | Medio (1–2 semanas) | Bajo-medio | El mismo cable que b |

**¿Perdés la consola serie?**  **No.**  La consola va por el USB-C
(CH343 → UART0), que es un camino independiente del USB OTG.  Usar el
conector de 4 pines no la afecta.  Lo único a cuidar es no mover la
consola (`console=ttyS0`) a un puerto USB: si el USB falla, te quedás sin
acceso.

---

## 9. Desarrollo de proyectos

Contexto que vale para todas las áreas:

- Sin MMU, binarios bFLT, uClibc-ng.
- ~10 MB de pool compartido; un programa grande no puede pasar de ~2 MB
  contiguos.
- Un núcleo RISC-V de 32 bits, sin FPU visible.
- Red de ~150 KB/s.
- 256 KB persistentes.

Los paquetes que dicen "compatible NOMMU" salen del `Config.in` de
Buildroot 2025.02: no dependen de `BR2_USE_MMU`.  Que lo sean **no
garantiza** que entren en memoria ni que funcionen: ninguno se probó.

### 9.1 Desarrollo web

| | |
|---|---|
| **Hoy (verificado)** | Servidor HTTP en MicroPython con `asyncio` (ejemplo 4.2, sirve HTML y JSON).  Cliente HTTP con sockets y `wget`.  `websocket` importa (sin probar un servidor WebSocket completo).  `json` funciona. |
| **Recompilando (teórico)** | `httpd` de BusyBox (activar el applet; sirve páginas estáticas y CGI).  `sqlite` (compatible NOMMU).  `wolfssl` (TLS, compatible NOMMU); con eso, un cliente HTTPS en C, o recompilar MicroPython con `ssl` sobre mbedTLS (la librería es compatible, sus programas no).  `libcurl` (compatible; necesita TLS para HTTPS). |
| **Desarrollo propio** | API REST en C o MicroPython; WebSocket servidor (hay que armar el handshake). |
| **No viable** | Node.js, PHP CGI/FPM, CPython (Buildroot los marca "depends on BR2_USE_MMU"), nginx y lighttpd (necesitan MMU).  Bases de datos grandes. |
| **Proyecto ejemplo** | Panel web en `http://stamp-p4.local:8080` que muestre datos guardados por la placa, con una API JSON para leerlos desde otra PC. |

### 9.2 Inteligencia artificial

| | |
|---|---|
| **Hoy (verificado)** | Nada específico.  La placa puede llamar a una API externa por **HTTP**, pero no por **HTTPS**: casi todas las APIs de IA exigen HTTPS. |
| **Recompilando (teórico)** | Con `wolfssl`, un cliente HTTPS en C para usar una API de IA externa.  Es la vía más realista con Linux. |
| **Desarrollo propio** | Inferencia de modelos muy chicos en C (por ejemplo TensorFlow Lite Micro compilado como librería C, o un clasificador hecho a mano).  Sin FPU visible, los modelos tendrían que ser **cuantizados a enteros (int8)**. |
| **No viable (en Linux)** | Las **instrucciones de IA/SIMD del P4 (extensión PIE)**: el kernel informa `rv32imac` y la toolchain no las genera.  Usarlas desde Linux requeriría soporte de compilador y kernel.  ESP-DL (la librería de IA de Espressif) es para ESP-IDF, no para Linux. |
| **Conviene** | Para IA *en* el chip: **ESP-IDF + ESP-DL** (usa PIE, FPU y todo el chip).  Para IA con Linux: que la placa mande datos a un servidor o API que haga la inferencia. |
| **Proyecto ejemplo** | La placa junta datos y los manda a una PC de la red (HTTP), donde un modelo los clasifica y devuelve el resultado. |

### 9.3 Ciencia de datos

Módulos de MicroPython verificados (se importan):

- `os`, `sys`, `time`, `json`, `re`, `socket`, `select`, `asyncio`;
- `binascii`, `hashlib`, `struct`, `math`, `random`, `errno`, `io`;
- `array`, `collections`, `termios`, `gc`, `platform`, `heapq`,
  `websocket`, `machine`.

**No** están: `ssl`, `network`, `requests`/`urequests`, `ulab`, `btree`,
`deflate`, `vfs`, `mip`, `uctypes`, `framebuf`, `cryptolib`.

| | |
|---|---|
| **Hoy (verificado)** | Recolectar datos del sistema o de la red, procesarlos con `math`, `array`, `json` y `struct`, guardarlos en `/tmp` (o en `/boot`, poco) y enviarlos por HTTP (sockets). |
| **Recompilando (teórico)** | **`ulab`** (el "numpy" de MicroPython) se compila *dentro* de MicroPython como módulo de usuario.  Hay que modificar el paquete de MicroPython en Buildroot; no hay paquete listo.  Con float por software va a ser lento. |
| **Desarrollo propio** | Logger con buffer en RAM y envío periódico a un servidor. |
| **No viable** | pandas, numpy y CPython: necesitan MMU y mucha memoria. |
| **Proyecto ejemplo** | Registrar cada minuto la latencia a Internet y la memoria, y mandarlo a una planilla o servidor en la PC para graficarlo. |

### 9.4 C y C++

| | |
|---|---|
| **Hoy (verificado)** | `riscv32-linux-gcc` 13.4 (en la VM) con uClibc-ng y pthreads (LinuxThreads).  Binarios bFLT de ~85 KB para un "hola mundo" estático.  Reglas en la sección 4.3. |
| **Recompilando (teórico)** | **C++**: activar `BR2_TOOLCHAIN_BUILDROOT_CXX` (rehace la toolchain); libstdc++ y STL funcionan en uClibc, con binarios mucho más grandes.  **gdbserver** (el paquete `gdb` es compatible NOMMU).  **strace** (compatible). |
| **Desarrollo propio** | Portar librerías C sueltas; reemplazar `fork()` por `vfork()`+`exec`. |
| **No viable** | Programas que dependen de `fork()` sin `exec`, de `mmap` compartido de archivos o de memoria virtual grande. |
| **Proyecto ejemplo** | Un demonio en C que lea `/proc` y sirva métricas por UDP. |

### 9.5 Otros lenguajes

| Lenguaje | Estado |
|---|---|
| Shell (BusyBox ash) | ✅ Instalado (verificado).  Hay `awk`, `sed`, `bc`. |
| MicroPython | ✅ Instalado (verificado). |
| Lua | Recompilando: paquete `lua` compatible NOMMU (teórico; liviano, buena opción). |
| LuaJIT | Compatible según Buildroot, pero su JIT no soporta RISC-V de 32 bits: no recomendado. |
| CPython | ❌ Buildroot lo marca dependiente de MMU; además pesa decenas de MB. |
| Node.js | ❌ Necesita MMU y cientos de MB. |
| Go | ❌ El compilador de Go no genera código para RISC-V de 32 bits ni para Linux sin MMU. |
| Rust | ❌ en la práctica.  Teóricamente hay targets sin MMU, pero no para Linux NOMMU con uClibc y bFLT; sería un proyecto enorme. |
| Java | ❌ OpenJDK necesita MMU. |

### 9.6 IoT y hardware

| | |
|---|---|
| **Hoy (verificado)** | **Ningún acceso a periféricos desde Linux.**  `/dev/gpiochip0` existe (el kernel tiene `GPIO_CDEV` y el driver de GPIO del P4), pero no hay herramienta en la placa (`gpioset`/`gpioget`).  MicroPython tiene `machine.mem32` (acceso directo a registros, solo root) pero no `machine.Pin`.  Sin I2C, SPI ni UART extra. |
| **Recompilando (teórico)** | **`libgpiod`** (compatible NOMMU) para usar GPIO de los pines libres de la tira de 40 pines.  Antes hay que confirmar qué banco y pines expone el DTB (hoy solo aparece `gpio-bank1`).  MQTT cliente: `paho-mqtt-c` (compatible) o `umqtt.simple` en MicroPython (es un archivo .py, se puede copiar), sin TLS.  El *broker* mosquitto necesita MMU. |
| **Desarrollo propio** | **I2C y SPI**: no hay drivers del P4 en este kernel.  Habría que escribirlos o usar I2C por software sobre GPIO (`i2c-gpio`, más configuración del DTB).  La tira de 40 pines de Waveshare (GPIO7/8 son el I2C por defecto, compartido con el codec de audio) quedaría disponible. |
| **Conviene** | Para manejar mucho hardware: **ESP-IDF** o **MicroPython oficial**, que tienen todos los drivers. |
| **Proyecto ejemplo** | Hoy: un "sensor de red" que publica por MQTT (sin TLS) en un broker de la PC.  Con libgpiod: prender un relé o leer un botón de la tira de pines. |

### 9.7 Redes y servicios

| | |
|---|---|
| **Hoy (verificado)** | Servidor SSH (Dropbear), cliente `ssh`/`dbclient` (túneles con `-L`/`-R`, teórico), `nc`, `telnet`, `wget` (HTTP), `ping`, `traceroute`, `nslookup`, `tcpdump`, `udhcpd` (servidor DHCP), `ntpd`, `iw`, mDNS. |
| **Recompilando (teórico)** | `iperf3` (medir velocidad, compatible NOMMU).  `httpd`/`ftpd`/`tftpd` de BusyBox (servidor de archivos chico). |
| **Desarrollo propio** | Modo **punto de acceso** (ESP-Hosted-MCU lo soporta en el C6; falta un daemon como `eh-wifi-sta` para AP).  Gateway/NAT: el kernel no tiene netfilter.  Hay que activarlo, y sin AP no hay a quién rutear. |
| **No viable** | `dnsmasq`, `hostapd`, `rsync`, `socat`, `tinyproxy` (Buildroot los marca MMU: usan `fork()`). |
| **Proyecto ejemplo** | Un "bastión": la placa queda en la red y con `ssh -R` abre un túnel hacia un servidor tuyo para entrar desde afuera. |

### 9.8 Tres formas de usar esta placa

| | Linux NOMMU (actual) | ESP-IDF / FreeRTOS | MicroPython oficial para ESP32-P4 |
|---|---|---|---|
| **Fuerte en** | Shell, SSH, varios procesos, scripts, servicios de red | Todo el hardware (GPIO, I2C, SPI, cámara, pantalla, USB, SD), velocidad, IA con ESP-DL, tiempo real | Programar rápido en Python con todo el hardware (`machine.Pin`, I2C, SPI, SD, red) |
| **Débil en** | Hardware y memoria; sin TLS; lento | Más difícil (C, compilar y grabar cada cambio) | Un solo programa; sin shell Linux |
| **Conviene para** | Aprender Linux, gateway de datos, servicios de red | Productos, IA en el chip, multimedia | Prototipos con sensores, enseñanza |

Cambiar entre ellas implica **regrabar la flash del P4**: `docs/INSTALAR.md`
explica cómo volver.  El C6 no se toca en ningún caso.

### 9.9 Arquitectura de "dispositivo de borde"

```
[sensores / red local] → [placa: Linux] → HTTP/MQTT → [servidor o PC] → [nube / IA]
```

- **La placa:**
  - junta datos (de la red; de sensores solo si se agrega GPIO/I2C);
  - los filtra y resume (promedios, umbrales);
  - guarda poco en RAM, con un buffer si se cae la red;
  - expone un panel HTTP local;
  - se administra por SSH.
- **El servidor o la PC:**
  - guarda la historia (base de datos);
  - hace los gráficos y el análisis pesado;
  - corre la IA;
  - termina el HTTPS hacia la nube (la placa no tiene TLS).
- **Regla práctica:** todo lo que necesite >1 MB, TLS o cálculo con
  decimales, delegalo.

### 9.10 Cinco proyectos, de más simple a más ambicioso

1. **Monitor de red**: un script de shell que cada minuto haga `ping` y
   anote la latencia; un servidor MicroPython que la muestre en una página.
   *Todo funciona hoy.*
2. **API JSON de la placa**: extender `servidor_http.py` con `/api/red`,
   `/api/memoria`, y un cliente en tu computadora que la consulte.  *Funciona hoy.*
3. **Logger hacia la PC**: la placa manda datos por HTTP o MQTT (`umqtt`
   copiado a `/boot`) a un servidor en tu computadora que los guarda y grafica.
   *Funciona hoy (MQTT sin TLS).*
4. **Programa en C con hilos**: un demonio que mida y sirva datos por UDP,
   con persistencia en `/boot`.  *Funciona hoy, con cuidado de memoria.*
5. **Control de hardware**: agregar `libgpiod` (y revisar el DTB de GPIO)
   para manejar un relé o un botón de la tira de 40 pines, controlado desde
   la API web.  *Necesita recompilar y probar.*

---

## 10. Próximos pasos posibles (de más fácil a más difícil)

| # | Paso | Qué implica | Esfuerzo |
|---|---|---|---|
| 1 | **Hora correcta al arrancar** | Lanzar `ntpd` en `S40network` cuando hay IP. | Muy bajo |
| 2 | **Host key SSH fija** | Guardar la clave del servidor en `/boot` en vez de regenerarla.  Evita el aviso en cada reinicio.  Cambia un diseño de m3-lab. | Bajo |
| 3 | **Herramientas nuevas** (`libgpiod`, `lua`, `sqlite`, `iperf3`, `httpd` de BusyBox) | Activar en Buildroot, recompilar, grabar el rootfs.  Vigilar el tamaño: la partición de rootfs es de 7 MB y hoy usa 4.4 MB. | Bajo-medio |
| 4 | **Más persistencia** | Usar la partición `userdata` (16 MB, en 0x1000000) con JFFS2 o littlefs.  Antes hay que validar el direccionamiento de 4 bytes de la flash (por encima de 16 MB). | Medio |
| 5 | **GPIO usable** | Revisar el DTB (bancos de GPIO), instalar `libgpiod`, documentar qué pines de la tira son libres (sin chocar con SDIO 14–19, el reset del C6 54, la UART 37/38). | Medio |
| 6 | **TLS / HTTPS** | `wolfssl` o mbedTLS, y MicroPython con `ssl`.  Ojo con la memoria. | Medio |
| 7 | **microSD sin Wi-Fi** (opción A) | DTB alternativo con slot 0. | Medio (más probar la alimentación de la tarjeta) |
| 8 | **USB dispositivo / red por USB** | Soporte del OTG High-Speed en DTB y PHY, más gadget ECM. | Alto |
| 9 | **Wi-Fi más rápido** | Resolver el SDIO de 4 bits: errores de CRC en CMD53, probablemente por el timing del firmware del C6.  Sin tocar el C6, se puede probar fase y retardo de muestreo en el P4. | Alto |
| 10 | **Segundo núcleo (SMP)** | Investigar por qué la CPU1 no responde en esta placa. | Alto |
| 11 | **microSD + Wi-Fi juntos** | Parche multi-slot de `dw_mmc` (opción B). | Alto |
| 12 | **USB host (pendrive)** | Ver sección 8.2.a. | Alto |

---

## 11. Novedades de la v2

Todo esto se probó en la placa salvo donde se aclara.

### 11.1 microSD

La ranura se maneja por SPI por software (GPIO39–44, `spi-gpio` + `mmc_spi`),
así que **no compite con el Wi-Fi** (el C6 sigue en SDIO).  Es lenta pero
sirve para configuración y archivos.

La placa solo usa particiones con estas **etiquetas**:

| Etiqueta | Sistema | Dónde | Para qué |
|---|---|---|---|
| `WSDATA` | ext2/ext4 | `/mnt/sd` | Persistencia: `persist/config/` (credenciales, claves SSH, `config.txt`, redes Wi-Fi) y `persist/home/m5/`, que se monta sobre `/home/m5` |
| `WSFILES` | FAT32 | `/mnt/archivos` | Archivos para pasar desde/hacia la computadora; acá va `wifi.txt` |

Una tarjeta sin esas etiquetas **no se monta en escritura**: así no se
pisan datos ajenos.  Para apagar sin perder datos: `poweroff` (desmonta la
tarjeta).

### 11.2 Wi-Fi: varias redes y punto de acceso

```sh
wifi-scan                  # redes cercanas
wifi-connect [SSID]        # guarda una red (pide la clave sin mostrarla) y se conecta
wifi-status                # estado, IP, redes guardadas
wifi-forget SSID           # borra una red
wifi-reload                # vuelve a leer wifi.txt y las redes guardadas
ap-on / ap-off             # punto de acceso (ap-off se niega si no hay red cliente; "ap-off force")
ap-config                  # ver/cambiar SSID, canal (canal=N) y clave del AP
```

**Sin teclado:** desde la computadora, copiá `examples/wifi-ejemplo.txt` a la
raíz de la partición `WSFILES` como `wifi.txt`.  Escribí pares
`ssid=` / `clave=`; la primera red tiene prioridad.  La placa lo lee al
encender (o con `wifi-reload`) y nunca lo modifica.  `wifi.txt` tiene
contraseñas: no lo subas a ningún repositorio.

Con el AP encendido, la placa queda en `192.168.4.1`.

### 11.3 Hora y zona horaria

Al recibir IP por DHCP, `time-sync` pone la hora por NTP.  En
`config.txt` (en `persist/config/` de la microSD, o en `/boot`):

```
ntp_server=pool.ntp.org
timezone=America/Argentina/Cordoba
```

`ntp_server=none` desactiva el NTP.  La zona puede ser un nombre de la tabla
de `time-sync` o una cadena POSIX (`ART3`).

### 11.4 HTTPS

```sh
https-get https://example.com/                 # imprime el cuerpo; -o ARCHIVO, -S cabeceras
wget -O pagina.html https://example.com/       # wget delega https:// en https-get
```

Verifica la cadena de certificados contra el bundle del sistema y el nombre
del servidor.  Códigos de salida: 0 OK, 2 red/DNS, 3 TLS o certificado.
Probado contra badssl.com: rechaza certificados vencidos, autofirmados o con
otro nombre.

MicroPython con TLS:

```python
# micropython-tls   (el micropython normal no tiene ssl)
import https
status, headers, body = https.get("https://example.com/")
```

Límites: solo TLS 1.2.  ~1 MB de RAM por conexión mientras dura.  **Un
`ssl.SSLContext` sin configurar no verifica** en MicroPython 1.22: usá el
módulo `https` o `verify_mode = ssl.CERT_REQUIRED` + `load_verify_locations()`.
Sin la hora correcta (11.3) los certificados se ven inválidos.

### 11.5 C++

Ver `examples/cpp/LEEME.md`.  Se compila con una toolchain C++ aparte (la
imagen no cambia) y se copia por SSH: `make`, `make deploy`, `make run`.
Hay que usar `-fPIC` y las reglas de `board.mk`.  Cada programa tiene que
entrar en ~496 KB (código + datos + pila): `iostream` completo no entra.  Usá
`printf` o `std::string`.

### 11.6 GPIO

Tabla completa: `docs/PINES-GPIO.md`.  Pines de 3,3 V; nunca 5 V.

```sh
pin lista                 # pines del conector
pin 20 1 ; pin 20 0       # salida
pin 47 leer pullup        # entrada con pull-up interno
pin 20 parpadear 10 250   # 10 veces cada 250 ms
```

```python
from gpio import Pin       # en micropython; ~31 ms por operación
led = Pin(20, Pin.OUT); led.toggle()
```

En C: `examples/c-gpio/gpio-parpadeo.c` (libgpiod 2).  Para probar, conectá un
LED con una resistencia de 330 Ω entre el GPIO y GND.  Desconectá la placa
antes de mover cables: un contacto con `RUN` o `EN` la reinicia, y uno con
`GND` cortocircuita el pin si está en alto.

### 11.7 I2C y SPI

| Bus | Pines | Dispositivo |
|---|---|---|
| I2C (software, ~100 kHz) | GPIO7 SDA, GPIO8 SCL | `/dev/i2c-0` |
| SPI (software, ≤1 MHz) | GPIO21 SCK, 22 MOSI, 23 MISO, 26 CS | `/dev/spidev1.0` |

```sh
i2cdetect -y 0                     # el códec de audio ES8311 responde en 0x18
i2cget -y 0 0x18 0xfd              # 0x83 (ID del ES8311)
spidev_test -D /dev/spidev1.0 -s 100000 -p "HOLA" -v
```

El bus I2C ya tiene el códec de la placa (0x18): no uses esa dirección.  Los
sensores I2C comunes (BME280, SHT31, OLED SSD1306) van en GPIO7/8 con 3,3 V.

**SPI, prueba pendiente:** con un cable **solo** entre GPIO22 y GPIO23 (pines
9 y 10 de la fila derecha; el 8 es GND), `spidev_test` tiene que devolver en
RX lo mismo que envía.  En la prueba de la v2 el cable tocaba GND y no se
pudo confirmar.

Con un solo núcleo, la CPU queda ocupada mientras dura cada transferencia
I2C/SPI.

### 11.8 Espacio

El rootfs usa 6,49 MB de su partición de 7 MB.  Quedan ~0,5 MB para
paquetes nuevos; para más hay que cambiar la tabla de particiones (el kernel
tiene 7,5 MB, hoy usa 6,3 MB).  La microSD es el lugar para datos y
programas propios.

### 11.9 Mini navegador de texto (`navegar`)

Un navegador de texto escrito en MicroPython: `examples/micropython/navegador.py`
y el lanzador `examples/micropython/navegar`.  No es parte de la imagen.  Va
en la microSD, en `/home/m5/bin`, así que no hace falta grabar la flash.

**Instalar** (desde la Mac, en la carpeta del repositorio):

```bash
scp -O examples/micropython/navegador.py examples/micropython/navegar m5@stamp-p4.local:/home/m5/bin/
ssh m5@stamp-p4.local 'chmod 755 /home/m5/bin/navegar; echo "export PATH=\"\$HOME/bin:\$PATH\"" >> /home/m5/.profile'
```

**Usar** (entrando como `m5`):

```sh
navegar                       # pantalla de inicio con páginas sugeridas
navegar es.wikipedia.org/wiki/ESP32
navegar -w 100 -l 30 URL      # ancho y alto de tu terminal (por defecto 80x24)
```

| Escribí | Qué hace |
|---|---|
| Enter / `-` | página siguiente / anterior |
| un número | sigue el enlace `[N]` |
| `b` | vuelve a la página anterior |
| una dirección, o `g DIRECCIÓN` | la abre (`https://` si no se indica) |
| `buscar PALABRAS` | busca en DuckDuckGo (versión lite) |
| `/TEXTO` | busca en la página; `/` solo: la siguiente |
| `l`, `i` | lista de enlaces, datos de la página |
| `s ARCHIVO` | guarda el texto y los enlaces |
| `r`, `t`, `?`, `q` | recargar, principio, ayuda, salir |

Probado en la placa con Wikipedia, DuckDuckGo lite, text.npr.org,
lite.cnn.com (267 KB) y Hacker News.  Una página de 100 KB tarda ~30 s; una
de 25 KB, 2–3 s.  Verifica los certificados HTTPS como `https-get`.

Límites:
- sin JavaScript, CSS, imágenes ni formularios (salvo la búsqueda);
- descarga como máximo 300 KB por página (el resto se corta y lo avisa);
- usa un heap de 1,75 MB, porque con más MicroPython no arranca, y conserva
  8 páginas en el historial;
- las páginas que se arman con JavaScript se ven vacías.
- Si entrás como `root`, el PATH no incluye `/home/m5/bin`: usá
  `/home/m5/bin/navegar`.

### 11.10 Escribir tus propios programas en la microSD

No hay `nano` ni un IDE en la placa.  Lo más cómodo es escribir el
programa en la computadora y subirlo a `/home/m5/bin`, que está en la
microSD y sobrevive a los reinicios.

**Dónde se guarda cada cosa** (verificado en `/proc/mounts`):

| Carpeta | Dónde vive | ¿Sobrevive a un reinicio? |
|---|---|---|
| `/home/m5` (y `/home/m5/bin`) | microSD, ext4 (`WSDATA`, `mmcblk0p1`) | sí |
| `/mnt/sd` | la misma partición; `/home/m5` está dentro, en `persist/home` | sí |
| `/mnt/archivos` | microSD, FAT32 (`WSFILES`), se lee desde la computadora | sí |
| `/boot` | flash interna | sí, con `boot-save` |
| `/home` fuera de `m5`, `/tmp`, `/root` | RAM | **no** |

**1. Escribir en la computadora y subir con `scp` (recomendado)**

```bash
scp -O miprograma.py m5@stamp-p4.local:/home/m5/bin/     # subir
scp -O m5@stamp-p4.local:/home/m5/bin/miprograma.py .    # bajar
```

Sin `-O`, el `scp` de la Mac usa SFTP y falla con
`can't execute '/usr/libexec/sftp-server'` y `Connection closed`.

**2. Editar en la placa con `vi`** (el de BusyBox):

```sh
vi /home/m5/bin/miprograma.py
```

| Tecla | Qué hace |
|---|---|
| `i` | empezar a escribir |
| `Esc` | dejar de escribir |
| `:wq` + Enter | guardar y salir |
| `:q!` + Enter | salir sin guardar |

**3. Algo corto, sin editor:**

```sh
cat > /home/m5/bin/hola.py << 'FIN'
print("hola")
for i in range(3):
    print(i)
FIN
```

Para probar línea por línea: `micropython` solo abre el intérprete
interactivo (`>>>`); `Ctrl+D` sale.

**Ejecutar**

```sh
micropython /home/m5/bin/hola.py        # sin red segura
micropython-tls /home/m5/bin/hola.py    # si usa https o ssl
```

Probado el 2026-11-23: un `hola.py` subido con `scp -O` desde la Mac se
ejecutó con `micropython` y respondió `hola desde la placa`.

Para que un programa se ejecute con solo escribir su nombre, hacé un
lanzador de shell como `navegar` y dale permiso de ejecución:

```sh
cat > /home/m5/bin/hola << 'FIN'
#!/bin/sh
exec micropython "$(dirname "$0")/hola.py" "$@"
FIN
chmod 755 /home/m5/bin/hola
```

- Como `m5`, `/home/m5/bin` ya está en el PATH (lo agrega
  `/home/m5/.profile`), así que alcanza con `hola`.
- Como `root`, o desde otra carpeta, usá la ruta completa
  (`/home/m5/bin/hola`).  Dentro de la carpeta, `./hola`.  `hola` solo da
  `sh: can't execute 'hola': No such file or directory`.
- Como root, también podés agregar la carpeta al PATH de la sesión:
  `export PATH=/home/m5/bin:$PATH`.

**Otras formas de pasar archivos**

- Sacar la microSD y copiar desde la computadora a la partición
  `WSFILES`.  En la placa aparece en `/mnt/archivos`.  `WSDATA` es ext4 y
  Windows o macOS no la abren sin software extra.
- `wget` desde una dirección web (http o https).

> **Cuidado:** no corras `df` sobre `/mnt/archivos`.  En una prueba tardó
> más de 2 minutos, la placa dejó de responder y se reinició.  Después el
> kernel avisó que la partición FAT "no se desmontó bien".  La causa
> probable, sin confirmar: contar el espacio libre de una FAT grande por el
> SPI por software ocupa el único núcleo y salta el watchdog.  Sobre
> `/home/m5`, `df` funciona bien.

### 11.11 SQLite, servidores web, APIs e IA (TensorFlow Lite Micro)

Todo esto va en la microSD (`/home/m5/bin` y `/home/m5/.micropython/lib`):
no se graba la flash, no cambia la imagen y no hace falta otro driver.
Probado en la placa el 2026-11-23.

| Programa | Qué es | Lenguaje | RAM al arrancar |
|---|---|---|---|
| `sqlite3` | SQLite 3.54.1: base de datos y consola para ver tablas | C | ~1,1 MB |
| `ver_tablas.py` | visor web de tablas SQLite, puerto 8080 | MicroPython + Microdot | — |
| `sqlite.py` | módulo para usar SQLite desde MicroPython | MicroPython | — |
| `servidor-web` | servidor web de archivos + API JSON, puerto 8000 | C + Mongoose | 226 KB |
| `api-sqlite` | API REST sobre una tabla SQLite, puerto 8001 | C + Mongoose + SQLite | 770 KB |
| `api_microdot.py` | API REST de notas, puerto 8002 | MicroPython + Microdot | — |
| `tflm-seno` | TensorFlow Lite Micro: modelo de ejemplo y `.tflite` propios | C++ | 310 KB |
| `tflm-mnist` | reconoce dígitos escritos a mano (0–9) en una imagen | C++ | 338 KB |

Las bibliotecas de terceros no están en el repositorio: `descargar.sh` las
baja y verifica su SHA-256.

| Biblioteca | Versión | Licencia |
|---|---|---|
| SQLite | 3.54.1 | dominio público |
| Microdot | 2.8.0 | MIT |
| Mongoose | 7.24 | **GPLv2** (o licencia comercial de Cesanta si el programa se distribuye cerrado) |
| stb_image | 2.34 (commit `6b5799b`) | dominio público o MIT |
| TensorFlow Lite Micro | commit `38a8f0f` | Apache-2.0 |

#### Instalar (desde la Mac, en la carpeta del repositorio)

Hace falta la VM de compilación con el compilador C++ (ver
`examples/cpp/LEEME.md` y `docs/COMPILAR.md`).

```bash
orb -m rv32dev sh examples/terceros/descargar.sh     # una vez, ~45 MB
make -C examples/c-api deploy PLACA=stamp-p4.local   # sqlite3, servidor-web, api-sqlite
make -C examples/tflm deploy PLACA=stamp-p4.local    # tflm-seno y los modelos .tflite
ssh m5@stamp-p4.local 'mkdir -p /home/m5/.micropython/lib/microdot'
scp -O examples/terceros/microdot/*.py m5@stamp-p4.local:/home/m5/.micropython/lib/microdot/
scp -O examples/sqlite/sqlite.py m5@stamp-p4.local:/home/m5/.micropython/lib/
scp -O examples/sqlite/ver_tablas.py examples/sqlite/api_microdot.py m5@stamp-p4.local:/home/m5/bin/
```

MicroPython busca módulos en `~/.micropython/lib`, así que `import
microdot` e `import sqlite` funcionan como `m5` sin tocar nada más.

#### SQLite desde la consola

```sh
sqlite3 /home/m5/datos.db
sqlite> CREATE TABLE lecturas(id INTEGER PRIMARY KEY, sensor TEXT, valor REAL,
   ...>   hora TEXT DEFAULT CURRENT_TIMESTAMP);
sqlite> INSERT INTO lecturas(sensor, valor) VALUES ('temp', 23.5);
sqlite> .tables
sqlite> .schema lecturas
sqlite> .mode box
sqlite> SELECT * FROM lecturas;
sqlite> .quit
```

En una sola línea: `sqlite3 -box /home/m5/datos.db "SELECT * FROM lecturas"`,
o `-json` para JSON.  Dentro de SQL los textos van con comillas simples
(`'temp'`).  Con comillas dobles da `no such column`.

**Velocidad de escritura** (medida; la microSD va por SPI por software):

| Configuración | Una escritura |
|---|---|
| por defecto (`journal_mode=DELETE`, `synchronous=FULL`) | ~5,7 s |
| `PRAGMA journal_mode=TRUNCATE; PRAGMA synchronous=NORMAL;` | ~1,2 s |
| `PRAGMA synchronous=OFF;` | 0,06 s, pero un corte de luz puede dañar la base |
| 1000 filas dentro de un `BEGIN; … COMMIT;` | 0,08 s en total |

Leer es rápido (0,05–0,3 s).  Recomendación:
- usar `TRUNCATE` + `NORMAL`, que es lo que usan `api-sqlite` y `sqlite.py`;
- agrupar muchas escrituras en una transacción.

`CURRENT_TIMESTAMP` guarda la hora en UTC; para la hora local usá
`datetime('now','localtime')`.

Compilado sin WAL ni mmap (necesitan memoria compartida, que este kernel
NOMMU no tiene) y sin extensiones.  El resto es SQLite normal, con funciones
JSON incluidas.

#### Ver las tablas en el navegador

```sh
micropython /home/m5/bin/ver_tablas.py /home/m5/datos.db
```

Después abrí `http://stamp-p4.local:8080/` en la computadora:
- lista de tablas con su cantidad de filas;
- cada tabla con sus columnas y los datos, de a 50 filas;
- una página `/sql` para escribir consultas;
- `/api/TABLA` devuelve las filas en JSON.

**No tiene contraseña:** cualquiera en tu red puede abrirlo y ejecutar SQL.
Cortalo con `Ctrl+C` cuando termines.

#### SQLite desde MicroPython

```python
import sqlite
db = sqlite.Base("/home/m5/datos.db")
db.ejecutar("CREATE TABLE IF NOT EXISTS t(id INTEGER PRIMARY KEY, nombre TEXT)")
db.ejecutar("INSERT INTO t(nombre) VALUES (?)", ["it's ok"])  # ? = valor con comillas seguras
print(db.consultar("SELECT * FROM t"))          # lista de dicts
print(db.consultar_columnas("SELECT * FROM t")) # (columnas en orden, filas)
print(db.tablas())
```

- No hay `import sqlite3` nativo en este MicroPython: `sqlite.py` llama al
  comando `sqlite3` y lee su salida.  Cada llamada tarda ~0,1 s más lo que
  tarde la escritura.
- Los errores llegan como `sqlite.Error` con el mensaje de SQLite.
- Probado con comillas, comas, saltos de línea, `NULL` y datos binarios.

#### Servidor web en C (`servidor-web`)

```sh
mkdir -p /home/m5/www
echo '<h1>Hola</h1>' > /home/m5/www/index.html
servidor-web                    # o: servidor-web -p 8000 -d /home/m5/www
```

- Sirve los archivos de `/home/m5/www` en `http://stamp-p4.local:8000/`.
- Además responde:
  - `GET /api/hola`;
  - `GET /api/memoria` (RAM y pool NOMMU);
  - `POST /api/eco` (devuelve el JSON que recibe).
- Probado con 20 pedidos al mismo tiempo: todos respondieron 200, en 0,46 s
  en total.
- Es un solo proceso con un bucle de eventos.  No usa `fork()`, que este
  kernel no tiene.

#### API REST en C con SQLite (`api-sqlite`)

```sh
api-sqlite                      # base /home/m5/lecturas.db, puerto 8001
api-sqlite -b otra.db -p 9000 -d /home/m5/www   # también sirve páginas
```

| Pedido | Qué hace |
|---|---|
| `GET /api/lecturas?limite=N&sensor=S` | lista, las más nuevas primero |
| `GET /api/lecturas/ID` | una |
| `POST /api/lecturas` con `{"sensor":"temp","valor":23.5}` | agrega (201) |
| `DELETE /api/lecturas/ID` | borra |
| `GET /api/resumen` | cantidad, media, mínimo y máximo por sensor |

Desde la computadora:

```bash
curl -d '{"sensor":"temp","valor":23.5}' http://stamp-p4.local:8001/api/lecturas
curl http://stamp-p4.local:8001/api/resumen
```

Medido:
- escribir tarda ~1 s;
- leer, 0,04–0,3 s;
- la primera vez que crea la base tarda unos segundos en empezar a
  responder.

Usa parámetros de SQLite (sin inyección SQL) y SQLite arma el JSON.  Para
tu propia API, copiá `examples/c-api/api-sqlite.c`, agregá rutas en
`atender()` y compilá con `make -C examples/c-api`.

#### API REST en MicroPython (`api_microdot.py`)

```sh
micropython /home/m5/bin/api_microdot.py      # puerto 8002, base /home/m5/notas.db
```

Rutas:
- `GET` y `POST /api/notas` (`{"texto":"..."}`);
- `GET` y `DELETE /api/notas/ID`;
- `GET /api/estado`.

Microdot se escribe como Flask:

```python
from microdot import Microdot
app = Microdot()

@app.get("/api/hola")
async def hola(req):
    return {"mensaje": "hola"}

app.run(port=8002)
```

Es más lento que la versión en C, porque cada consulta arranca `sqlite3`,
pero se puede cambiar en la placa misma con `vi`.  En la prueba, los dos
primeros pedidos de la primera ejecución no respondieron (sin error en el
registro).  Después, todos los pedidos respondieron bien, incluidos textos
con comillas.  No pude reproducirlo.

#### Inteligencia artificial: TensorFlow Lite Micro (`tflm-seno`)

```sh
tflm-seno                                       # modelo de ejemplo (aprox. de sin(x))
tflm-seno /home/m5/modelos/hello_world_float.tflite 0.5 1.57 3   # un .tflite propio
tflm-seno -a 256 mimodelo.tflite 1.0            # arena de 256 KB si el modelo es grande
```

Salida real (modelo int8):

```
seno int8: entrada INT8, salida INT8, arena usado 748 de 8192 bytes
     x   sin(x)   modelo   error   tiempo
  1.57    1.000    1.003   0.003     32 us
...
promedio 35 us por inferencia, error maximo 0.116
```

El modelo float tarda ~155 µs por inferencia, con error máximo 0,039.

- **Modelos propios:** se entrenan en la computadora con TensorFlow y se
  convierten a `.tflite`; en la placa solo se ejecutan.  `tflm-seno` sirve
  para modelos de 1 entrada y 1 salida (float o int8).
- **Operadores incluidos:** FullyConnected, Conv2D, DepthwiseConv2D,
  MaxPool2D, AveragePool2D, Reshape, Softmax, Relu, Relu6, Logistic, Tanh,
  Add, Mul, Quantize, Dequantize y Mean.  Otro operador se agrega en
  `registrar()` de `examples/tflm/tflm-seno.cc`.
- **Para un programa propio**, partí de `tflm-seno.cc`: `make -C
  examples/tflm` arma `build/libtflm.a` una vez, en ~15 s con 4 núcleos.
- **Límites:**
  - un núcleo, sin aceleración vectorial ni NPU;
  - el modelo y su arena tienen que entrar en la RAM libre (~2–5 MB);
  - no hay entrenamiento en la placa;
  - no hay TensorFlow "completo" ni PyTorch.

#### Reconocer dígitos escritos a mano (`tflm-mnist`)

`tflm-mnist` usa el modelo LSTM del ejemplo `mnist_lstm` de TFLM:
- carga la imagen con stb_image (PNG, JPEG, BMP, GIF, PGM…);
- la pasa a gris y a 28×28;
- dibuja en la terminal lo que ve el modelo;
- dice qué dígito es, con la probabilidad de cada uno.

```bash
make -C examples/tflm deploy PLACA=stamp-p4.local    # desde la Mac; copia también los dígitos de ejemplo
```

```sh
cd /home/m5/imagenes
tflm-mnist sample3.png            # dibujo + dígito + probabilidades
tflm-mnist -q sample?.png         # solo "archivo: dígito"
tflm-mnist -f sample9.png         # modelo float en vez de int8
tflm-mnist -i foto.jpg            # forzar invertir (tinta oscura sobre papel)
```

Salida real:

```
digito: 3   (89.5 %, 5.9 ms)
0:  0.0%  1:  0.0%  2:  1.2%  3: 89.5%  4:  0.8%
5:  3.9%  6:  0.0%  7:  0.0%  8:  0.0%  9:  4.7%
```

Probado en la placa el 2026-11-24:

| Prueba | Resultado |
|---|---|
| los 10 dígitos de ejemplo (`sample0`–`sample9`), int8 y float | 10 de 10 bien, en cualquier orden |
| `resized9.png` (10×10, agrandada a 28×28) | 9 |
| JPEG de 28×28 y de 640×480 | 7 (correcto) |
| dígito negro sobre blanco de 280×280 | se invierte solo y da 4 (99,6 %); con `-I` (sin invertir) da 6 |
| tiempo por imagen | int8 6–17 ms, float ~88 ms; los 10 dígitos en 0,09 s en total |

- **El modelo espera un dígito blanco sobre fondo negro, centrado**, como
  MNIST.  Si la imagen es mayormente clara la invierte sola (`-i` fuerza
  invertir, `-I` lo impide).
- Para una foto de papel: recortá el dígito y que ocupe casi todo el
  cuadro.  El programa no busca el dígito dentro de la imagen.
- El LSTM guarda estado entre una inferencia y otra.  `tflm-mnist` lo
  reinicia antes de cada imagen; sin eso, en la primera prueba `sample9` dio
  3 al procesarse después de `sample8`.

#### Ver imágenes en la terminal (`ver-imagen`)

`ver-imagen` abre una imagen guardada en la microSD y la dibuja con
caracteres en la terminal SSH.  Lee PNG, JPEG, BMP, GIF (el primer cuadro),
TGA y PGM/PPM, con stb_image.

```bash
make -C examples/ver-imagen deploy PLACA=stamp-p4.local       # desde la Mac
scp -O foto.jpg m5@stamp-p4.local:/home/m5/imagenes/           # subir imágenes
```

```sh
ver-imagen /home/m5/imagenes/sample7.png            # colores (256), por defecto
ver-imagen -m ascii /home/m5/imagenes/sample7.png   # solo caracteres " .:-=+*#%@"
ver-imagen -m bloques -w 40 foto.jpg                # ░▒▓█, 40 columnas
ver-imagen -m color foto.jpg                        # 24 bits (iTerm y otras)
ver-imagen -n /home/m5/imagenes/*.png               # solo tamaño y canales
```

| Modo | Cómo se ve | Terminal |
|---|---|---|
| `ascii` | `" .:-=+*#%@"` según el brillo | cualquiera |
| `bloques` | `░▒▓█` | UTF-8 |
| `gris` | medio bloque `▀` con 24 grises (2 píxeles por carácter) | 256 colores |
| `256` (por defecto) | medio bloque con la paleta de 256 colores | Terminal de la Mac, iTerm |
| `color` | medio bloque en color de 24 bits | iTerm y la mayoría de las modernas |

- `-w` fija el ancho en columnas.  Por defecto entra en 80 (o
  `$COLUMNS`), y las imágenes chicas, como los dígitos MNIST de 28×28, se
  agrandan.
- `-i` invierte el brillo en `ascii` y `bloques`.
- **Límite:** la imagen se decodifica entera en RAM.  Más de 700 000
  píxeles (por ejemplo 1600×1200) se rechaza con un aviso; achicala antes
  en la computadora, a 640×480 por ejemplo.
- `make deploy` también copia a `/home/m5/imagenes` los dígitos de ejemplo
  de `mnist_lstm` (si bajaste tflite-micro).

Probado en la placa el 2026-11-24 (usa 205 KB de RAM al arrancar):
- PNG y JPEG de 28×28: 0,01 s;
- JPEG de 640×480 reducido a 80×30: 0,14 s;
- rechazo de 1600×1200 con el aviso de arriba;
- error claro con un archivo que no es imagen (`unknown image type`).

Un dígito de 28×28 en colores ocupa 9–12 KB de texto por SSH.

#### Dejar un servidor corriendo

Los servidores se cortan al cerrar la sesión SSH.  Para que sigan:

```sh
nohup servidor-web > /home/m5/servidor-web.log 2>&1 &
```

- Arrancarlos solos al encender no está configurado.
- Cada servidor ocupa RAM mientras corre: con dos o tres a la vez, revisá
  `largest_free` en `/sys/kernel/board/nommu_pool` antes de arrancar
  otro.

### 11.12 Micrófono, parlante y reconocimiento de voz

La placa trae un micrófono analógico soldado, conectado a la entrada del
códec **ES8311** (I2C 0x18).  El ES8311 también tiene una salida que va a
un amplificador (encendido por GPIO53) y al conector **SPK** (MX1.25 de 2
pines, para un parlante de 8 Ω y 2 W).  El códec se comunica con el P4 por
**I2S**: MCLK 13, BCLK 12, LRCK 10, DOUT 9, DIN 11 (pines del BSP de
Waveshare).  Desde el 2026-11-24 Linux graba, y desde el 2026-11-26
reproduce.  Probado en la placa.

**Hace falta otro firmware.**  Las imágenes de `firmware/` de este
repositorio **no** traen el audio.  Se agregan tres piezas:

| Dirección | Pieza | Qué hace |
|---|---|---|
| 0x10000 | boot shim con `ws_audio.c` | configura el ES8311 (micrófono 24 dB, salida volumen 90) e I2S1 en full duplex; mide el nivel del micrófono al arrancar |
| 0x90000 | kernel con el parche 0094 | `/dev/ws-audio`, 16 kHz, 16 bits, estéreo: `read()` graba (el micrófono es el canal izquierdo) y `write()` reproduce; enciende el amplificador solo mientras suena |
| 0xF10000 | DTB con el nodo `audio@500c7000` | incluye `pa-gpios`, el amplificador en GPIO53 |

El rootfs, el bootloader, la tabla de particiones, `/boot` y el C6 no
cambian.  Las fuentes están en `source/0002-waveshare-audio.patch`, que se
aplica sobre la v2.

**Al arrancar**, la consola serie muestra el nivel del micrófono, medido
por el shim antes de Linux:

```
ws_audio: mic L rms=107 peak=326 dc=0 (6400 frames, gain 24 dB)
ws_audio: I2S1 full duplex + ES8311 ADC/DAC ready for Linux: 16 kHz S16_LE stereo, mic gain 24 dB, out volume 90
```

Un micrófono que no funciona da valores cerca de 0 o fijos.

#### Grabar (`grabar`)

```bash
make -C examples/audio deploy PLACA=stamp-p4.local      # desde la Mac
```

```sh
grabar -s 10 /home/m5/voz.wav     # 10 s, mono, 16 kHz; muestra el nivel cada segundo
grabar -n -s 30                   # solo el medidor de nivel, sin archivo
grabar -e -s 5 estereo.wav        # los dos canales (el derecho repite el izquierdo)
```

```bash
scp -O m5@stamp-p4.local:/home/m5/voz.wav . && afplay voz.wav   # escucharlo en la Mac
```

Probado en la placa:
- tres grabaciones seguidas de 10 s dieron 10,0 s de audio en 10,0 s;
- sin cortes: el salto medio entre muestras es 5,5 en los bordes de bloque
  y 5,1 adentro;
- sin clics.

- `grabar` guarda el audio en RAM y crea el archivo **al terminar**.
  Antes de grabar vacía las escrituras pendientes de la microSD (`sync`).
- **Escribir en la microSD mientras se graba hace perder audio:** el diario
  de ext4 ocupa el único núcleo por unos segundos.  En una prueba, 10 s de
  audio tardaron 14 s, y se perdieron unos 4 s.
- Si eso pasa, `grabar` avisa, y el kernel lo anota (`dmesg | grep
  overrun`).

#### Reproducir (`reproducir`)

Conectá el parlante al conector **SPK** con la placa apagada: el cable rojo
al `+` y el negro al `−` (los que están impresos en la placa).  El conector
espera 8 Ω.  Si no sabés la impedancia, dos parlantes en serie son lo más
seguro; nunca en paralelo.

```bash
make -C examples/audio deploy PLACA=stamp-p4.local      # instala grabar y reproducir
```

```sh
reproducir /home/m5/voz.wav               # una grabación
reproducir -v 40 /home/m5/voz.wav         # al 40 % del volumen (por defecto 70 %)
reproducir uno.wav dos.wav                # varias, una tras otra
reproducir -t 440 -s 60                   # un tono de 440 Hz durante 60 s
reproducir -t 1000 -s 300 -v 30           # 5 minutos a 1 kHz, bajito
```

- **Archivos:** WAV PCM de 16 bits, mono o estéreo, de 4 a 48 kHz.  Si no
  es de 16 kHz lo convierte (interpolación lineal): suena a la velocidad
  correcta, con menos calidad.
- **Memoria:** lee el archivo de a poco, así que las grabaciones largas no
  necesitan más memoria.
- **`Ctrl+C`** corta.  El kernel deja sonar el medio segundo que ya tiene en
  la cola y después apaga el amplificador.
- Se puede **grabar y reproducir a la vez** desde dos programas: uno lee
  `/dev/ws-audio` y otro escribe.

Probado en la placa (2026-11-26):

| Prueba | Resultado |
|---|---|
| tono de 10 s (`-t 440 -s 10`) | se escuchó; 10 s en 10,16 s |
| grabar 4 s mientras suena un tono de 1 kHz | el micrófono midió 2105, contra 37 en silencio |
| reproducir esa grabación | 4,0 s en 4,16 s |

- **Igual que al grabar, escribir en la microSD mientras suena hace cortes.**
  La primera prueba, justo después de copiar el programa a la tarjeta,
  tardó 17,7 s en lugar de 10, con un silencio de unos segundos.  Después
  de un `sync` no se repitió.  El kernel lo anota: `dmesg | grep underrun`.

#### Radio por internet (`radio`, `radio-rfi.sh`)

Escucha una radio MP3 por `http://` en el parlante, con un cache de 30 s
contra los cortes de la red.  Viene configurada con **RFI Monde**
(`http://rfimonde64k.ice.infomaniak.ch/rfimonde-64.mp3`, 64 kbps).

```bash
make -C examples/audio deploy PLACA=stamp-p4.local    # desde la Mac: instala radio y radio-rfi.sh
```

Como `root`, al volumen más alto sin distorsión:

```sh
/home/m5/bin/radio-rfi.sh                                  # hasta Ctrl+C
nohup /home/m5/bin/radio-rfi.sh > /tmp/radio.log 2>&1 &    # que siga al cerrar SSH
killall radio                                              # pararla
```

`radio-rfi.sh` pone el DAC del ES8311 en 0 dB (registro `0x32` = `0xbf`,
por I2C), toca al 100 % y al terminar deja el volumen como estaba.  Por
encima de 0 dB el códec solo agrega ganancia digital: más fuerte, pero
saturado y forzando el parlante.

Directo, con otras opciones (también como `m5`):

```sh
radio                              # RFI, volumen 100 %, cache 30 s
radio -v 60 -c 10                  # más bajo, cache de 10 s
radio http://servidor:8000/radio.mp3   # otra radio MP3 por http
```

Salida real:

```
  icy-name:RFI monde 64a
  cache de 256 KB (30 s a 64 kbps)
  cache lleno (30 s), suena
  MP3 22050 Hz, estereo, 64 kbps
  51 s sonando, cache 29 s
```

- **Cache:** guarda el MP3 comprimido en RAM (30 s = 240 KB).  Al empezar
  espera a llenarlo; en la prueba tardó unos 20 s, por la ráfaga inicial
  del servidor.  Por eso suena unos 30 s atrás del vivo.
- **Si se corta la red,** sigue sonando del cache y reconecta en segundo
  plano.  Si el cache se vacía, espera 3 s de audio y sigue.
- **CPU:** el decodificador (minimp3) hace las cuentas de coma flotante por
  software, porque el compilador de la placa no usa la FPU: decodificar usa
  ~67 %, y la radio entera ~86 %.  El cache sirve contra la red, no contra la
  CPU: no copies archivos a la microSD ni corras programas pesados mientras
  suena.  Radios de más de 64 kbps pueden no llegar.
- **Solo `http://`:** TLS no entra en la CPU junto con el decodificador.
- **Por qué no la página `ecouterradioenligne.com`:** pide su dirección de
  audio detrás de un control anti-bots de Cloudflare.  RFI publica sus
  propias radios en Infomaniak.
- **Probado en la placa** (2026-11-26): sonó bien.  Con cache, en 75 s de
  reloj: ~20 s llenando y ~55 s sonando, con el cache estable en 29–30 s.

#### Decir "yes" o "no" (`tflm-voz`)

Es el ejemplo `micro_speech` de TensorFlow Lite Micro, con dos modelos:
- un preprocesador (FFT, banco de filtros, reducción de ruido) que cada
  20 ms da 40 valores;
- un clasificador que mira el último segundo y elige entre silencio,
  desconocido, "yes" y "no".

```bash
make -C examples/tflm deploy PLACA=stamp-p4.local       # también copia WAVs de prueba a /home/m5/audio
```

```sh
tflm-voz /home/m5/audio/yes_1000ms.wav /home/m5/audio/no_1000ms.wav   # archivos WAV de 1 s
tflm-voz -m                     # escuchar el micrófono; Ctrl+C para salir
tflm-voz -m -s 30 -v -u 0.7     # 30 s, mostrando cada resultado, umbral 70 %
```

Probado en la placa con los WAV del ejemplo:

| Archivo | Resultado |
|---|---|
| `yes_1000ms.wav` | **yes** (100 %) |
| `no_1000ms.wav` | **no** (95 %) |
| `silence_1000ms.wav` | **silencio** (34 %, el más alto) |
| `noise_1000ms.wav` | **silencio** (97 %) |

Con el micrófono:
- cada fila de 20 ms tarda 0,41 ms y cada clasificación 62 ms;
- clasifica cada 200 ms, o sea ~30 % de la CPU;
- avisa cuando "yes" o "no" pasa el umbral (80 % por defecto).

**Sin probar todavía:** que reconozca a una persona hablándole a la placa.
En la sala en silencio marcó sobre todo "desconocido" (0,4–0,9), no
"silencio".  Puede ser el nivel del micrófono, distinto al de los datos de
entrenamiento.

#### Límites

- **Un programa graba y uno reproduce a la vez.**  Un segundo programa que
  intenta lo mismo recibe `EBUSY`.
- **Mono:** el ES8311 tiene una sola salida; los dos canales se mezclan.
- **El volumen es por software** (`reproducir -v`).  La ganancia del códec
  queda fija en 90 desde el shim.
- **El modelo de voz solo conoce "yes" y "no"** en inglés.  Para otras
  palabras hay que entrenar un modelo en la computadora.
- **Un `tflm-voz -m -v` mostró 2 pérdidas de audio en 10 s** la primera
  vez; las corridas siguientes no.  No sé la causa.
- Conviene no grabar mientras otro programa escribe mucho en la microSD.

**Volver al sistema sin audio:** grabar el shim de la v2 en 0x10000 y el
`Image` y el DTB de `firmware/` en 0x90000 y 0xF10000 (o `./flash.sh`, que
también borra `/boot`).

**Cómo se llegó acá** (para quien quiera seguir):
- el driver usa un anillo de DMA de 8 bloques de 63,75 ms para cada
  dirección;
- mientras el dispositivo está abierto el transmisor suena siempre
  (silencio si no hay nada que reproducir): en full duplex el receptor toma
  el reloj del transmisor;
- un trabajo del kernel cada 30 ms limpia los bloques ya reproducidos, para
  que vuelva al silencio cuando el programa deja de escribir;
- los problemas que hubo, en orden: el receptor sin reloj, el canal de DMA
  que ESP-IDF dejaba asignado, el valor viejo del FIFO al empezar, y los
  cortes por escribir en la microSD.  Ver los commits de
  `source/0002-waveshare-audio.patch`.

---

## Anexo A. Qué se hizo y por qué funcionó

### A.1 Proyecto base

| | |
|---|---|
| Repositorio | <https://github.com/tsuyoshi-otake/m5stamp-p4-c6-linux> |
| Versión | **v0.5.1** (perfil `m3-lab`) |
| Commit base | `69a7312c97d67074abcc197d8454a9cb0a7d7ee0` ("release: v0.5.1 hardware validation record") |

### A.2 Qué se adaptó a esta placa

| Tema | M5Stamp-P4 (original) | Waveshare (ahora) |
|---|---|---|
| Wi-Fi del C6 | Stamp-AddOn con ESP-Hosted-**NG** | C6 de la placa con ESP-Hosted-**MCU 3.0.10** (otro protocolo): módulo `esp_hosted_mcu.ko` + programa `eh-wifi-sta` |
| Pines SDIO | Los del AddOn | **Slot 1**: CLK GPIO18, CMD 19, D0–D3 14–17 (`boot-shim/main/ws_sdmmc.c`) |
| Reset del C6 | GPIO de M5 | **GPIO54, activo en alto** (1 = en reset).  El shim lo pulsa 10 ms y espera 1.5 s. |
| Ancho y frecuencia SDIO | 4 bits | **1 bit, máximo 5 MHz** (en 4 bits fallan las lecturas en bloque con CRC) |
| Consola | USB-Serial-JTAG / USB | **UART0 = `ttyS0`** (CH343 del USB-C); se saltea el parche 0027 |
| Flash y particiones | 16 MB | **32 MB**, misma distribución + `userdata` de 16 MB en 0x1000000 (sin usar) |
| Chip | P4 rev v1 | **P4 rev v3.2**: el parche 0072 (bootflash) usa la dirección de datos ROM `0x4ffbffe8` de rev v3 |
| Núcleos | 2 | **1** (`maxcpus=1`): el CPU1 no reporta en esta placa |
| USB OTG Full-Speed ("pendrive" `/boot`) | Activo | **Desactivado**: en esta placa esos pines no llegan a un conector |
| `/boot` | Se exporta por USB y se desmonta | Queda **montado** en `/boot`; se guarda con `boot-save` |
| Deep sleep | Apaga el C6 con GPIO42 | Mantiene GPIO54 en alto (C6 en reset) — **no probado** |

Correcciones necesarias en ESP-Hosted-MCU para que corra en este Linux:

- acceso a registros byte a byte por CMD52;
- adaptación al kernel 7.x;
- `vfork` en lugar de `fork`;
- script de enlace bFLT completo;
- reloj de las esperas con timeout: con LinuxThreads, todas vencían al
  instante.

### A.3 Por qué funcionó esto y no linux-waveshare-xc

Un intento anterior con el port `linux-waveshare-xc` (kernel
6.18.35):

- Usaba el driver **ESP-Hosted-NG 1.0.6** (`esp32_sdio`), en **4 bits a
  40 MHz**.
- El C6 se detectaba (`mmc0: new high speed SDIO card`), pero `esp_probe`
  fallaba con **`error -84`** (EILSEQ = error de CRC en los datos).
- Los diagnósticos de ese intento mostraron que:
  - **las lecturas de un byte (CMD52) de los registros 0x60–0x63 daban
    `ret=0`**;
  - pero **la lectura de 4 bytes en bloque (CMD53) del mismo registro daba
    `-84`**, aun forzando PIO y bajando a 10 MHz.

Por qué esto anda:

1. **El firmware del C6 es otro protocolo.**  El C6 tiene **ESP-Hosted-MCU
   3.0.10** (`eh_cp_wifi_sta`), y el driver NG espera el firmware NG.  Aun
   sin el error de CRC, NG nunca habría hablado con este firmware.  Ahora
   el lado Linux usa el **driver y el protocolo MCU** (los mismos de
   Espressif para 3.0.10), así que coinciden.
2. **El error de CRC del bus.**  Las transferencias en bloque fallan con 4
   bits (también lo verifiqué en esta versión: CRC `-84` con las fases de
   muestreo 0–2).  Con **1 bit** funcionan.  Además, el driver lee los
   registros **byte a byte (CMD52)**, que nunca fallaron.  Causa probable,
   no confirmada: el firmware del C6 se compiló con un timing SDIO pensado
   para un host ESP32 (`PSEND_PSAMPLE`), no para Linux.
3. **La frecuencia.**  Se bajó a **5 MHz**, con la que el bus es estable.
4. **Bugs que el intento anterior todavía no había alcanzado**: dirección
   ROM de P4 rev v3 (el kernel se colgaba al iniciar bootflash), consola en
   UART0 y CPU1.

### A.4 Firmware del C6 hoy

- **ESP-Hosted-MCU 3.0.10**, proyecto `eh_cp_wifi_sta`, ESP-IDF v5.5.5.
- El driver de Linux lo informa al arrancar: `slave fw version:
  0x0003000a`, `RPC version negotiated: V2`.
- **No se modificó**: en todo este trabajo no se escribió ni borró la flash
  del C6.
- **No existe backup** de la flash del C6.

### A.5 Dónde está cada cosa

- `firmware/`: las imágenes de este paquete.  En `README.md` están sus
  direcciones de grabación y SHA-256.
- `source/`: los cambios respecto del proyecto base.
- `docs/COMPILAR.md`: cómo reconstruir todo.
- `docs/INSTALAR.md`: backup, instalación y cómo volver atrás.

**No existe un backup "oficial" del C6** (ni del P4): cada usuario tiene
que hacer el suyo antes de grabar.

---

## Anexo B. Inventario verificado en la placa (2026-11-15)

- **CPU:** 1 hart activo, `rv32imac_zicntr_zicsr_zifencei_zihpm_zaamo_zalrsc_zca`,
  `mmu: none`.  El boot-shim informa 400 MHz; Waveshare dice 360 o
  400 MHz según la página.
- **Memoria:** `MemTotal: 16696 kB`.  Pool NOMMU (`/sys/kernel/board/nommu_pool`):
  - `total 10485760`, en 2 regiones (4 MB + 6 MB);
  - en reposo, `used` ≈ 6.2–7 MB y `largest_free` ≈ 2–3 MB;
  - `user_reserve_total` = 2 MB.
- **Almacenamiento:**
  - `mtd0` es el rootfs (7 MB, squashfs de 4.4 MB, solo lectura);
  - `ram0` es un disco ext2 de 2 MB en RAM con `/tmp`, `/home`, `/root` y
    `/var/lib`;
  - `loop0` es `/boot` (FAT de 256 KB);
  - no hay `mmcblk` (la microSD no está activa).
- **Red:**
  - `ethsta0` (Wi-Fi cliente, IP por DHCP), `ethap0` (sin usar), `lo`;
  - escuchan TCP/22 (inetd → dropbear) y UDP/5353 (mdnsd).
- **Procesos del sistema:** `syslogd`, `inetd`, `bootsyncd`, `mdnsd`,
  `eh-wifi-sta` (con `respawn`), `udhcpc`.
- **Comandos:**
  - BusyBox 1.37.0 está dividido en varios binarios: `busybox`,
    `busybox-tools`, `busybox-daemons`, `busybox-udhcpc` y `busybox-su`.
    `busybox-tools --list` no funciona: se identifican por los enlaces.
  - Principales: `sh`, `vi`, `awk`, `sed`, `grep`, `find`, `tar`, `gzip`,
    `unzip`, `xz`, `bc`, `diff`, `patch`, `less`, `top`, `ps`, `free`,
    `ip`, `ifconfig`, `route`, `ping`, `traceroute`, `nslookup`, `wget`,
    `nc`, `telnet`, `ntpd`, `udhcpd`, `i2cdetect`/`i2cget` (sin I2C en el
    kernel), `devmem`, `mke2fs`, `lsusb`, `lsof`, `watch`, `timeout`.
  - Fuera de BusyBox:
    - en `/usr/bin`: `micropython` (con alias `python3`), `tcpdump`,
      `dropbear`, `dbclient`/`ssh`/`scp`;
    - en `/usr/sbin`: `iw`, `wpa_supplicant` (no se usa en esta placa),
      `mdnsd`, `deepsleep`, `autorun`, `boot-save`, `boot-commit`,
      `wifi-connect`, `eh-wifi-sta`, `sysinfo`, `sdio-diag`.
- **Funciones de M5Stamp en esta placa**:

  | Función | Estado | Por qué |
  |---|---|---|
  | `/boot` como pendrive USB | ❌ No funciona | No hay controlador USB activo (`/sys/class/udc` vacío).  `bootsyncd` informa "No USB drive on this board". |
  | `/boot` como carpeta persistente | ✅ Funciona distinto | Se edita en la placa y se guarda con `boot-save`. |
  | Autorun (`main.py`/`main.sh`/`main` en `/boot`) | ❓ Funciona distinto, no probado | Al arrancar, `S05` lo toma de `/boot`.  Se copiaría con `scp` + `boot-save` en vez de por pendrive. |
  | `deepsleep` | ❓ No probado | La interfaz existe (`/sys/kernel/board/deep_sleep`).  El C6 queda en reset por GPIO54.  Al despertar, el arranque normal reinicia el C6 y Linux.  La máscara de GPIO para despertar (`0xfdff`) incluye GPIO14 y 15, que acá son líneas SDIO del C6: no usarlas para despertar. |
  | LP core (`lpcore.py`, `/sys/kernel/board/lp_core`) | ❓ No probado | La interfaz existe; depende de pines LP que en esta placa pueden estar ocupados. |
  | Botón BOOT para cancelar el sueño | ❓ Teórico | El shim usa GPIO35, que es el BOOT también en el P4 de Waveshare (pin de arranque del chip). |
  | Credenciales y claves desde `/boot` | ✅ Funciona | Verificado: claves SSH y contraseñas se aplican al arrancar. |
  | Modo AP Wi-Fi de M5Stamp (wpa_supplicant) | ❌ No | Era para el protocolo NG; acá el C6 se maneja por RPC y solo está el modo cliente. |
