# sqlite - use SQLite from MicroPython on the Waveshare ESP32-P4 board.
#
# MicroPython here has no sqlite3 module, so this calls the sqlite3 command
# (/home/m5/bin/sqlite3) and reads its JSON output.  Each call starts one
# sqlite3 process (~1 MB of RAM while it runs, ~0.1 s).
#
#   import sqlite
#   db = sqlite.Base("/home/m5/datos.db")
#   db.ejecutar("CREATE TABLE IF NOT EXISTS t(id INTEGER PRIMARY KEY, nombre TEXT)")
#   db.ejecutar("INSERT INTO t(nombre) VALUES (?)", ["hola"])
#   filas = db.consultar("SELECT * FROM t")      # list of dicts
#   db.tablas()                                   # ["t"]
#
# "?" placeholders are replaced by quoted literals (strings, numbers, None).
# Errors raise sqlite.Error with sqlite3's own message.
import os
import json

SQLITE3 = "/home/m5/bin/sqlite3"
_n = 0


class Error(Exception):
    pass


def literal(v):
    """SQL literal for a Python value."""
    if v is None:
        return "NULL"
    if v is True:
        return "1"
    if v is False:
        return "0"
    if isinstance(v, (int, float)):
        return repr(v)
    if isinstance(v, bytes):
        return "X'" + "".join("%02x" % b for b in v) + "'"
    return "'" + str(v).replace("'", "''") + "'"


def armar(sql, params=None):
    """Replace each ? outside quotes with a literal from params."""
    if not params:
        return sql
    out = []
    i = 0
    quote = None
    for ch in sql:
        if quote:
            if ch == quote:
                quote = None
            out.append(ch)
        elif ch in "'\"":
            quote = ch
            out.append(ch)
        elif ch == "?":
            if i >= len(params):
                raise Error("faltan parametros para ?")
            out.append(literal(params[i]))
            i += 1
        else:
            out.append(ch)
    if i != len(params):
        raise Error("sobran parametros")
    return "".join(out)


def nombre_valido(n):
    """True for a plain identifier (letters, digits, _), safe to quote."""
    if not n or not (n[0].isalpha() or n[0] == "_"):
        return False
    for c in n:
        if not (c.isalpha() or c.isdigit() or c == "_"):
            return False
    return True


def _leer(ruta):
    try:
        with open(ruta) as f:
            return f.read()
    except OSError:
        return ""


def _borrar(ruta):
    try:
        os.remove(ruta)
    except OSError:
        pass


def _valor(tok):
    if tok == "NULL":
        return None
    if tok[:2] in ("X'", "x'"):
        h = tok[2:-1]
        return bytes(int(h[i:i + 2], 16) for i in range(0, len(h), 2))
    try:
        return int(tok)
    except ValueError:
        try:
            return float(tok)
        except ValueError:
            return tok


def _leer_quote(texto):
    """Parse sqlite3 '.mode quote' output (SQL literals, comma-separated,
    one row per line; strings may contain newlines)."""
    filas, fila, tok = [], [], []
    i, n = 0, len(texto)
    es_texto = False
    while i < n:
        c = texto[i]
        if c == "'" and not tok:
            # quoted string, '' is an escaped quote
            i += 1
            partes = []
            while i < n:
                if texto[i] == "'":
                    if i + 1 < n and texto[i + 1] == "'":
                        partes.append("'")
                        i += 2
                        continue
                    break
                partes.append(texto[i])
                i += 1
            tok = ["".join(partes)]
            es_texto = True
        elif c == ",":
            fila.append(tok[0] if es_texto else _valor("".join(tok)))
            tok, es_texto = [], False
        elif c == "\n":
            if tok or fila:
                fila.append(tok[0] if es_texto else _valor("".join(tok)))
                filas.append(fila)
            fila, tok, es_texto = [], [], False
        elif c != "\r":
            tok.append(c)
        i += 1
    if tok or fila:
        fila.append(tok[0] if es_texto else _valor("".join(tok)))
        filas.append(fila)
    return filas


class Base:
    def __init__(self, ruta, rapido=False):
        # rapido=True: PRAGMA synchronous=OFF (writes ~50x faster on this
        # microSD, but a power cut during a write can corrupt the file).
        self.ruta = ruta
        self.pragmas = "PRAGMA synchronous=%s;\n" % ("OFF" if rapido else "NORMAL")

    def _correr(self, sql, modo):
        global _n
        _n += 1
        base = "/tmp/sqlite-py-%d" % _n
        with open(base + ".sql", "w") as f:
            f.write(self.pragmas)
            f.write(sql)
            f.write(";\n")
        cmd = "%s %s -bail '%s' < %s.sql > %s.out 2> %s.err" % (
            SQLITE3, modo, self.ruta.replace("'", "'\\''"), base, base, base)
        rc = os.system(cmd)
        salida = _leer(base + ".out")
        err = _leer(base + ".err").strip()
        for ext in (".sql", ".out", ".err"):
            _borrar(base + ext)
        if rc != 0 or err:
            raise Error(err or ("sqlite3 termino con codigo %d" % rc))
        return salida

    def consultar(self, sql, params=None):
        """Run a query, return a list of dicts (one per row)."""
        salida = self._correr(armar(sql, params), "-json").strip()
        if not salida:
            return []
        # one JSON array per statement that returned rows; keep the last
        filas = []
        for parte in salida.replace("]\n[", "]\x00[").split("\x00"):
            filas = json.loads(parte)
        return filas

    def consultar_columnas(self, sql, params=None):
        """Like consultar, but returns (columns, rows as lists) in the
        query's column order (MicroPython dicts do not keep the order)."""
        salida = self._correr(armar(sql, params), "-quote -escape off -header")
        filas = _leer_quote(salida)
        if not filas:
            return [], []
        return [str(c) for c in filas[0]], filas[1:]

    def ejecutar(self, sql, params=None):
        """Run statements that return no rows (CREATE, INSERT, UPDATE...)."""
        self._correr(armar(sql, params), "-batch")

    def tablas(self):
        return [f["name"] for f in self.consultar(
            "SELECT name FROM sqlite_master WHERE type='table' "
            "AND name NOT LIKE 'sqlite_%' ORDER BY name")]

    def columnas(self, tabla):
        if not nombre_valido(tabla):
            raise Error("nombre de tabla no valido: " + tabla)
        return self.consultar('PRAGMA table_info("%s")' % tabla)
