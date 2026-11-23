# api_microdot - a small REST API in MicroPython (Microdot + SQLite).
#
#   micropython /home/m5/bin/api_microdot.py [PUERTO]     (default 8002)
#
#   GET  /api/notas            all notes, newest first
#   POST /api/notas            body {"texto": "..."}  -> the new note
#   GET  /api/notas/ID
#   DELETE /api/notas/ID
#   GET  /api/estado           uptime and free memory of the board
#
# Data in /home/m5/notas.db (microSD).  Same idea as the C api-sqlite, in a
# few lines of Python; slower (each query starts sqlite3, ~0.1 s) but easy
# to change on the board itself.
import sys
from microdot import Microdot
import sqlite

PUERTO = int(sys.argv[1]) if len(sys.argv) > 1 else 8002
db = sqlite.Base("/home/m5/notas.db")
db.ejecutar("PRAGMA journal_mode=TRUNCATE; CREATE TABLE IF NOT EXISTS notas("
            "id INTEGER PRIMARY KEY, texto TEXT NOT NULL,"
            " hora TEXT DEFAULT CURRENT_TIMESTAMP)")
app = Microdot()


def leer_linea(ruta, clave):
    try:
        with open(ruta) as f:
            for l in f:
                if l.startswith(clave):
                    return l.split()[1]
    except OSError:
        pass
    return None


@app.get("/api/notas")
async def listar(req):
    return db.consultar("SELECT * FROM notas ORDER BY id DESC LIMIT 100")


@app.post("/api/notas")
async def crear(req):
    datos = req.json
    if not isinstance(datos, dict) or not isinstance(datos.get("texto"), str):
        return {"error": 'hace falta {"texto": "..."}'}, 400
    filas = db.consultar("INSERT INTO notas(texto) VALUES (?) RETURNING *",
                         [datos["texto"]])
    return filas[0], 201


@app.get("/api/notas/<int:id>")
async def una(req, id):
    filas = db.consultar("SELECT * FROM notas WHERE id = ?", [id])
    return filas[0] if filas else ({"error": "no existe"}, 404)


@app.delete("/api/notas/<int:id>")
async def borrar(req, id):
    filas = db.consultar("DELETE FROM notas WHERE id = ? RETURNING id", [id])
    return {"borrado": id} if filas else ({"error": "no existe"}, 404)


@app.get("/api/estado")
async def estado(req):
    with open("/proc/uptime") as f:
        uptime = float(f.read().split()[0])
    libre = leer_linea("/proc/meminfo", "MemAvailable:")
    return {"uptime_s": int(uptime), "ram_disponible_kb": int(libre) if libre else None}


print("api_microdot en http://0.0.0.0:%d/api/notas  (Ctrl+C para cortar)" % PUERTO)
app.run(port=PUERTO)
