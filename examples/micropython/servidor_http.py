# servidor_http.py - servidor HTTP mínimo (MicroPython, asyncio).
# Uso: python3 servidor_http.py [puerto]   y abrir http://stamp-p4.local:8080/
# /      -> página con la hora y la memoria libre
# /api   -> el mismo dato en JSON
import asyncio, json, sys, time, gc

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080

def estado():
    with open("/proc/uptime") as f:
        up = int(float(f.read().split()[0]))
    pool = {}
    with open("/sys/kernel/board/nommu_pool") as f:
        for line in f:
            k, _, v = line.partition(" ")
            if k in ("total", "used", "largest_free"):
                pool[k] = int(v) // 1024
    gc.collect()
    return {"uptime_s": up, "hora": time.time(), "pool_kb": pool}

async def atender(reader, writer):
    try:
        linea = await reader.readline()
        while True:  # descartar encabezados
            h = await reader.readline()
            if not h or h == b"\r\n":
                break
        partes = linea.split()
        ruta = partes[1].decode() if len(partes) > 1 else "/"
        e = estado()
        if ruta == "/api":
            cuerpo, tipo = json.dumps(e), "application/json"
        else:
            cuerpo = ("<html><body><h1>Waveshare ESP32-P4</h1>"
                      "<p>Encendida hace %d s. Pool libre: %d KB de %d KB.</p>"
                      "<p><a href='/api'>/api</a></p></body></html>"
                      % (e["uptime_s"], e["pool_kb"]["total"] - e["pool_kb"]["used"], e["pool_kb"]["total"]))
            tipo = "text/html; charset=utf-8"
        cuerpo = cuerpo.encode()
        writer.write(b"HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n" % (tipo.encode(), len(cuerpo)))
        writer.write(cuerpo)
        await writer.drain()
    except Exception as ex:
        print("error:", ex)
    finally:
        writer.close()
        await writer.wait_closed()

async def main():
    await asyncio.start_server(atender, "0.0.0.0", PORT)
    print("escuchando en el puerto", PORT)
    while True:
        await asyncio.sleep(3600)

asyncio.run(main())
