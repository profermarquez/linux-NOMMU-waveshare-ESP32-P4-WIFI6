# C++ en la Waveshare ESP32-P4 (v2)

Programas C++ (STL, excepciones, threads, sockets) para la placa. Se compilan
en Linux (en Windows dentro de WSL2; en macOS `make` lo hace dentro de una VM
Linux) y se copian por SSH; la imagen no cambia (los programas son
estáticos).  Ver el README, **Tu computadora → Compilar los ejemplos**.

## Una sola vez: el compilador C++

La imagen se compila sin C++. El compilador va en un directorio aparte,
armado con la misma configuración (GCC 13.4, uClibc-ng, LinuxThreads,
elf2flt), unos 6 minutos.  Antes hay que haber compilado las imágenes
(`docs/COMPILAR.md`), en el mismo Linux:

```bash
bash RUTA-AL-ARBOL-PARCHEADO/linux/waveshare/sdk/build-cxx-toolchain.sh           # Linux o WSL2
orb -m rv32dev bash RUTA-AL-ARBOL-PARCHEADO/linux/waveshare/sdk/build-cxx-toolchain.sh   # macOS con OrbStack
```

Queda en `~/ws-out/cxx-toolchain` (en macOS, dentro de la VM).

## Compilar, copiar y correr

```bash
cd examples/cpp      # (en el árbol fuente: linux/waveshare/cxx-ejemplos)
make            # compila (en macOS, make se vuelve a ejecutar dentro de la VM)
make deploy     # copia a m5@stamp-p4.local:/home/m5/bin
make run        # copia y corre stl-demo y threads-demo
```

Otra placa o usuario: `make deploy PLACA=192.168.4.1 USUARIO=m5`.

| Programa | Qué muestra |
|---|---|
| `stl-demo` | contenedores, algoritmos, `optional`, punteros inteligentes, excepciones propias y de la biblioteca, destructores durante el unwind |
| `threads-demo` | `std::thread`, `mutex`, `condition_variable` (cola productor/consumidor) |
| `sockets-demo get HOST [PUERTO] [RUTA]` | cliente HTTP con una clase `Socket` (RAII + excepciones) |
| `sockets-demo eco [PUERTO] [MAX]` | servidor eco TCP, un thread por cliente; probar con `nc stamp-p4.local 7000` |

Para HTTPS con verificación de certificados usar `https-get` (fase 5), no
`sockets-demo`.

## Un programa nuevo

Escribir `NOMBRE.cpp` acá y agregar `NOMBRE` a `PROGS` en el `Makefile`.
Las reglas de la placa están en `board.mk`; las importantes:

- **Memoria por programa:** al ejecutar, el programa entero (código, datos y
  pila) se copia a un solo bloque de RAM de 512 KB (496 KB útiles). `make`
  muestra cuánto pide cada uno y avisa si se pasa (`STRICT=1` lo vuelve
  error). Los ejemplos piden 280–310 KB.
- **Usar `<cstdio>` (`printf`), no `<iostream>`:** sólo `iostream` lleva un
  programa a ~543 KB; arranca únicamente cuando hay un bloque de 1 MB libre.
- `-fPIC` es obligatorio (sin él las excepciones cuelgan el programa) y
  `nommu-cxx.o` hace funcionar `std::thread`; `board.mk` ya los pone.
- Pila de 64 KB (`STACK=` para cambiarla). La placa tiene 1 núcleo:
  `hardware_concurrency()` da 1.
