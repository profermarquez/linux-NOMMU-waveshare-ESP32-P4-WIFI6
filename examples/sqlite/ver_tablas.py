# ver_tablas - web viewer for a SQLite database (MicroPython + Microdot).
#
#   micropython /home/m5/bin/ver_tablas.py /home/m5/datos.db [PUERTO]
#   then open http://stamp-p4.local:8080/ on the computer
#
# Pages:
#   /                   tables with their row counts
#   /tabla/NOMBRE       columns and rows (50 per page, ?pagina=N)
#   /sql                run any SQL statement (form)
#   /api/NOMBRE         the rows as JSON (?limite=N&desde=N)
#
# No password: anyone on your network can open it and run SQL.  Stop it
# with Ctrl+C when you are done.
import sys
from microdot import Microdot, Response
import sqlite

POR_PAGINA = 50

if len(sys.argv) < 2:
    print("uso: ver_tablas.py BASE.db [PUERTO]")
    sys.exit(1)
db = sqlite.Base(sys.argv[1])
PUERTO = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
app = Microdot()

ESTILO = """<style>
body{font-family:sans-serif;margin:1em;}
table{border-collapse:collapse;margin:.5em 0;}
td,th{border:1px solid #999;padding:3px 8px;text-align:left;}
th{background:#ddd;} .err{color:#b00;white-space:pre-wrap;}
textarea{width:100%;max-width:50em;}
</style>"""


def esc(v):
    if v is None:
        return "<i>NULL</i>"
    s = str(v)
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def pagina(titulo, cuerpo):
    html = ("<!doctype html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width'>"
            "<title>%s</title>%s</head><body>"
            "<p><a href='/'>Tablas</a> · <a href='/sql'>SQL</a> · %s</p>"
            "<h2>%s</h2>%s</body></html>") % (esc(titulo), ESTILO, esc(db.ruta), esc(titulo), cuerpo)
    return Response(html, headers={"Content-Type": "text/html; charset=utf-8"})


def tabla_html(cols, filas):
    if not filas:
        return "<p>(sin filas)</p>"
    h = ["<table><tr>"]
    h += ["<th>%s</th>" % esc(c) for c in cols]
    h.append("</tr>")
    for f in filas:
        h.append("<tr>" + "".join("<td>%s</td>" % esc(v) for v in f) + "</tr>")
    h.append("</table>")
    return "".join(h)


def entero(req, nombre, defecto):
    try:
        return max(0, int(req.args.get(nombre, defecto)))
    except ValueError:
        return defecto


@app.route("/")
async def inicio(req):
    try:
        filas = []
        for t in db.tablas():
            if sqlite.nombre_valido(t):
                n = db.consultar('SELECT count(*) AS n FROM "%s"' % t)[0]["n"]
            else:
                n = "?"
            filas.append("<tr><td><a href='/tabla/%s'>%s</a></td><td>%s</td></tr>" % (esc(t), esc(t), n))
        cuerpo = ("<table><tr><th>Tabla</th><th>Filas</th></tr>%s</table>" % "".join(filas)
                  if filas else "<p>La base no tiene tablas.</p>")
    except sqlite.Error as e:
        cuerpo = "<p class='err'>%s</p>" % esc(e)
    return pagina("Tablas", cuerpo)


@app.route("/tabla/<nombre>")
async def ver_tabla(req, nombre):
    if not sqlite.nombre_valido(nombre):
        return pagina("Error", "<p class='err'>Nombre de tabla no válido.</p>")
    p = entero(req, "pagina", 0)
    try:
        cols = db.columnas(nombre)
        total = db.consultar('SELECT count(*) AS n FROM "%s"' % nombre)[0]["n"]
        nombres, filas = db.consultar_columnas('SELECT * FROM "%s" LIMIT %d OFFSET %d'
                                               % (nombre, POR_PAGINA, p * POR_PAGINA))
    except sqlite.Error as e:
        return pagina(nombre, "<p class='err'>%s</p>" % esc(e))
    esquema = tabla_html(["columna", "tipo", "clave", "no nulo"],
                         [[c["name"], c["type"], "sí" if c["pk"] else "", "sí" if c["notnull"] else ""]
                          for c in cols])
    nav = "<p>Filas %d–%d de %d. " % (p * POR_PAGINA + (1 if filas else 0), p * POR_PAGINA + len(filas), total)
    if p > 0:
        nav += "<a href='?pagina=%d'>« anterior</a> " % (p - 1)
    if (p + 1) * POR_PAGINA < total:
        nav += "<a href='?pagina=%d'>siguiente »</a>" % (p + 1)
    nav += " · <a href='/api/%s'>JSON</a></p>" % esc(nombre)
    return pagina(nombre, "<h3>Columnas</h3>" + esquema + "<h3>Datos</h3>" + nav + tabla_html(nombres, filas))


@app.route("/sql", methods=["GET", "POST"])
async def sql(req):
    texto = ""
    resultado = ""
    if req.method == "POST":
        texto = (req.form or {}).get("sql", "")
        if texto.strip():
            try:
                nombres, filas = db.consultar_columnas(texto)
                resultado = tabla_html(nombres, filas) if filas else "<p>Listo (sin filas).</p>"
            except sqlite.Error as e:
                resultado = "<p class='err'>%s</p>" % esc(e)
    form = ("<form method='post'><textarea name='sql' rows='6'>%s</textarea><br>"
            "<button>Ejecutar</button></form>") % esc(texto).replace("<i>NULL</i>", "")
    return pagina("SQL", form + resultado)


@app.route("/api/<nombre>")
async def api(req, nombre):
    if not sqlite.nombre_valido(nombre):
        return {"error": "nombre de tabla no valido"}, 400
    limite = min(entero(req, "limite", 100), 1000)
    desde = entero(req, "desde", 0)
    try:
        return db.consultar('SELECT * FROM "%s" LIMIT %d OFFSET %d' % (nombre, limite, desde))
    except sqlite.Error as e:
        return {"error": str(e)}, 400


print("ver_tablas: %s en http://0.0.0.0:%d/  (Ctrl+C para cortar)" % (db.ruta, PUERTO))
app.run(port=PUERTO)
