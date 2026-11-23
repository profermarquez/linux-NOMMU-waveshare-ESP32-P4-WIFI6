# navegador.py - mini navegador de texto para la terminal (MicroPython).
# Waveshare ESP32-P4-WIFI6, Linux NOMMU v2.  Necesita micropython-tls (HTTPS
# con verificación de certificados, módulo https de la imagen).
#
#   navegar                       pantalla de inicio
#   navegar es.wikipedia.org      abre una página (https:// si no se indica)
#   navegar -w 100 -l 30 URL      ancho y alto de la terminal
#
# Muestra el texto de la página por pantallas y numera los enlaces [n].
# Sin JavaScript, CSS ni imágenes: sirve para páginas de texto (Wikipedia,
# documentación, noticias en versión "lite").  Escribí ? para la ayuda.
import sys
import gc
import socket

try:
    import https
except ImportError:
    print("navegador: necesita micropython-tls (el micropython normal no tiene ssl)")
    sys.exit(1)

VERSION = "1.0"
MAX_BYTES = 300000          # se corta la descarga más allá de esto
TIMEOUT = 20
UA = "navegador-p4/" + VERSION + " (MicroPython; texto)"
BUSCADOR = "https://lite.duckduckgo.com/lite/?q="

INICIO = """Mini navegador de texto - ESP32-P4 (MicroPython)

Escribí una dirección (ej.: es.wikipedia.org) o:
  buscar PALABRAS     busca en DuckDuckGo
  ?                   ayuda

Algunas páginas que funcionan bien en texto:"""
SUGERENCIAS = [
    "https://es.wikipedia.org/wiki/ESP32",
    "https://lite.duckduckgo.com/lite/?q=micropython",
    "https://text.npr.org/",
    "https://lite.cnn.com/",
    "https://news.ycombinator.com/",
    "https://docs.micropython.org/en/latest/",
]

AYUDA = """Comandos (escribilos y Enter):
  Enter          página siguiente         -        página anterior
  N (número)     seguir el enlace [N]     t        volver al principio
  b              volver a la página anterior del historial
  DIRECCIÓN      abrir (también: g DIRECCIÓN)
  buscar TEXTO   buscar en DuckDuckGo
  /TEXTO         buscar en la página (/ solo: siguiente coincidencia)
  l              lista de enlaces          i       datos de la página
  r              recargar                  s ARCH  guardar el texto
  q              salir"""

ENT = {
    "amp": "&", "lt": "<", "gt": ">", "quot": '"', "apos": "'", "nbsp": " ",
    "copy": "(c)", "reg": "(R)", "trade": "(TM)", "mdash": "-", "ndash": "-",
    "hellip": "...", "laquo": "<<", "raquo": ">>", "lsquo": "'", "rsquo": "'",
    "ldquo": '"', "rdquo": '"', "bull": "*", "middot": ".", "deg": "°",
    "euro": "EUR", "iexcl": "¡", "iquest": "¿", "aacute": "á", "eacute": "é",
    "iacute": "í", "oacute": "ó", "uacute": "ú", "ntilde": "ñ", "uuml": "ü",
    "Aacute": "Á", "Eacute": "É", "Iacute": "Í", "Oacute": "Ó", "Uacute": "Ú",
    "Ntilde": "Ñ", "Uuml": "Ü", "ccedil": "ç", "times": "x", "shy": "",
    "zwj": "", "zwnj": "", "thinsp": " ", "ensp": " ", "emsp": " ",
}

BLOQUE = ("p", "div", "section", "article", "header", "footer", "nav", "main",
          "aside", "ul", "ol", "dl", "table", "tr", "form", "blockquote",
          "figure", "figcaption", "address", "center", "details", "summary",
          "dt", "dd", "pre", "tbody", "thead", "caption", "fieldset")
SALTAR = ("script", "style", "svg", "template", "iframe", "select",
          "textarea", "object", "canvas", "math", "button")


# ---------------------------------------------------------------- red ----

def separar(url):
    """url -> (esquema, host, puerto, ruta)."""
    if url.startswith("https://"):
        esq, resto, puerto = "https", url[8:], 443
    elif url.startswith("http://"):
        esq, resto, puerto = "http", url[7:], 80
    else:
        raise ValueError("dirección no válida: " + url)
    i = resto.find("/")
    hp, ruta = (resto, "/") if i < 0 else (resto[:i], resto[i:])
    q = hp.find("?")
    if q >= 0:          # "host?x=1" sin barra
        hp, ruta = hp[:q], "/" + hp[q:]
    if ":" in hp:
        hp, p = hp.split(":", 1)
        puerto = int(p)
    return esq, hp.lower(), puerto, ruta


def resolver(base, href):
    """Dirección absoluta de href relativa a base (sin #fragmento)."""
    href = href.strip()
    i = href.find("#")
    if i >= 0:
        href = href[:i]
    if not href:
        return base
    low = href.lower()
    if low.startswith("http://") or low.startswith("https://"):
        return href
    if ":" in href.split("/")[0]:      # mailto:, javascript:, tel: ...
        return None
    esq, host, puerto, ruta = separar(base)
    raiz = esq + "://" + host
    if not ((esq == "https" and puerto == 443) or (esq == "http" and puerto == 80)):
        raiz += ":%d" % puerto
    if href.startswith("//"):
        return esq + ":" + href
    if href.startswith("/"):
        return raiz + href
    if href.startswith("?"):
        return raiz + ruta.split("?")[0] + href
    dirb = ruta.split("?")[0]
    dirb = dirb[:dirb.rfind("/") + 1]
    partes = []
    for p in (dirb + href).split("/"):
        if p == "..":
            if len(partes) > 1:
                partes.pop()
        elif p != ".":
            partes.append(p)
    return raiz + "/".join(partes)


def descargar_una(url):
    esq, host, puerto, ruta = separar(url)
    addr = socket.getaddrinfo(host, puerto, socket.AF_INET, socket.SOCK_STREAM)[0][-1]
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    trozos = []
    total = 0
    cortado = False
    try:
        s.settimeout(TIMEOUT)
        s.connect(addr)
        if esq == "https":
            try:
                f = https.context().wrap_socket(s, server_hostname=host)
            except ValueError as e:
                raise OSError("certificado rechazado para %s: %s" % (host, str(e).strip()))
            leer = f.read
        else:
            f = s
            leer = s.recv
        pedido = ("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n"
                  "Accept: text/html,text/plain;q=0.9,*/*;q=0.1\r\n"
                  "Accept-Language: es,en;q=0.8\r\nAccept-Encoding: identity\r\n"
                  "Connection: close\r\n\r\n") % (ruta, host, UA)
        f.write(pedido.encode())
        while True:
            try:
                t = leer(2048)
            except OSError:
                if total:      # el servidor cerró sin close_notify
                    break
                raise
            if not t:
                break
            trozos.append(t)
            total += len(t)
            if total > MAX_BYTES:
                cortado = True
                break
        if f is not s:
            f.close()
    finally:
        s.close()
    datos = b"".join(trozos)
    del trozos
    cab, _, cuerpo = datos.partition(b"\r\n\r\n")
    del datos
    lineas = cab.split(b"\r\n")
    try:
        estado = int(lineas[0].split()[1])
    except Exception:
        raise OSError("respuesta HTTP no válida")
    hdr = {}
    for l in lineas[1:]:
        k, _, v = l.partition(b":")
        hdr[k.strip().lower().decode()] = v.strip().decode()
    if "chunked" in hdr.get("transfer-encoding", "").lower():
        cuerpo = deschunk(cuerpo)
    return estado, hdr, cuerpo, cortado


def deschunk(b):
    out = []
    i = 0
    while i < len(b):
        j = b.find(b"\r\n", i)
        if j < 0:
            break
        try:
            n = int(b[i:j].split(b";")[0], 16)
        except ValueError:
            break
        if n == 0:
            break
        out.append(b[j + 2:j + 2 + n])
        i = j + 2 + n + 2
    return b"".join(out)


def descargar(url):
    for _ in range(6):
        estado, hdr, cuerpo, cortado = descargar_una(url)
        loc = hdr.get("location")
        if 300 <= estado < 400 and loc:
            nueva = resolver(url, loc)
            if not nueva:
                raise OSError("redirección no válida: " + loc)
            url = nueva
            continue
        return url, estado, hdr, cuerpo, cortado
    raise OSError("demasiadas redirecciones")


def a_texto(b, hdr):
    ct = hdr.get("content-type", "").lower()
    latin = "iso-8859" in ct or "windows-125" in ct or "latin" in ct
    if not latin and "charset" not in ct:
        cabeza = b[:2048].lower()
        latin = b"iso-8859" in cabeza or b"windows-125" in cabeza
    if not latin:
        try:
            return b.decode()
        except UnicodeError:
            pass
    return "".join([chr(c) for c in b])


# --------------------------------------------------------------- html ----

def entidades(t):
    if "&" not in t:
        return t
    out = []
    i = 0
    while True:
        j = t.find("&", i)
        if j < 0:
            out.append(t[i:])
            break
        out.append(t[i:j])
        k = t.find(";", j, j + 12)
        if k < 0:
            out.append("&")
            i = j + 1
            continue
        nom = t[j + 1:k]
        r = None
        if nom.startswith("#"):
            try:
                n = int(nom[2:], 16) if nom[1:2] in ("x", "X") else int(nom[1:])
                r = chr(n) if n != 160 else " "
            except (ValueError, OverflowError):
                r = None
        else:
            r = ENT.get(nom)
        if r is None:
            out.append("&")
            i = j + 1
        else:
            out.append(r)
            i = k + 1
    return "".join(out)


def atributo(tag, low, nombre):
    """Valor del atributo nombre= en el texto de la etiqueta (o None)."""
    i = 0
    while True:
        i = low.find(nombre, i)
        if i < 0:
            return None
        antes = low[i - 1] if i else " "
        j = i + len(nombre)
        while j < len(low) and low[j] == " ":
            j += 1
        if antes in " \t\n\r\"'/" and j < len(low) and low[j] == "=":
            j += 1
            while j < len(low) and low[j] == " ":
                j += 1
            if j >= len(low):
                return ""
            q = tag[j]
            if q in "\"'":
                k = tag.find(q, j + 1)
                return tag[j + 1:k if k >= 0 else len(tag)]
            k = j
            while k < len(tag) and tag[k] not in " \t\n\r>":
                k += 1
            return tag[j:k]
        i = j


def html_a_texto(html, base):
    """-> (titulo, texto con saltos de línea, lista de enlaces)."""
    low = html.lower()
    out = []
    enlaces = []
    titulo = ""
    pre = 0
    enlace_abierto = None
    n = len(html)
    i = 0
    while i < n:
        j = html.find("<", i)
        if j < 0:
            j = n
        if j > i:
            t = html[i:j]
            if pre:
                out.append(entidades(t).replace(" ", "\xa0"))
            else:
                out.append(entidades(t.replace("\n", " ").replace("\r", " ").replace("\t", " ")))
        if j >= n:
            break
        if low.startswith("<!--", j):
            k = html.find("-->", j + 4)
            i = n if k < 0 else k + 3
            continue
        k = html.find(">", j)
        if k < 0:
            break
        tag = html[j + 1:k]
        tlow = low[j + 1:k]
        i = k + 1
        if not tag or tag[0] in "!?":
            continue
        cierre = tag[0] == "/"
        nombre = tlow[1:] if cierre else tlow
        e = 0
        while e < len(nombre) and nombre[e] not in " \t\n\r/":
            e += 1
        nombre = nombre[:e]
        if not nombre:
            continue
        if not cierre and nombre in SALTAR:
            f = low.find("</" + nombre, i)
            if f < 0:
                break
            g = html.find(">", f)
            i = n if g < 0 else g + 1
            continue
        if nombre == "title" and not cierre:
            f = low.find("</title", i)
            if f >= 0:
                titulo = " ".join(entidades(html[i:f]).split())
                g = html.find(">", f)
                i = n if g < 0 else g + 1
            continue
        if nombre == "head" and cierre:
            out.append("\n")
            continue
        if nombre == "a":
            if cierre:
                if enlace_abierto is not None:
                    out.append("[%d]" % enlace_abierto)
                    enlace_abierto = None
            else:
                if enlace_abierto is not None:
                    out.append("[%d]" % enlace_abierto)
                    enlace_abierto = None
                href = atributo(tag, tlow, "href")
                if href:
                    u = resolver(base, entidades(href))
                    if u:
                        enlaces.append(u)
                        enlace_abierto = len(enlaces)
            continue
        if nombre == "br":
            out.append("\n")
        elif nombre in ("h1", "h2", "h3", "h4", "h5", "h6"):
            out.append("\n\n" if cierre else "\n\n" + "#" * int(nombre[1]) + " ")
        elif nombre == "li":
            if not cierre:
                out.append("\n  * ")
        elif nombre == "hr":
            out.append("\n" + "-" * 30 + "\n")
        elif nombre in ("td", "th"):
            if not cierre:
                out.append("  ")
        elif nombre == "img":
            alt = atributo(tag, tlow, "alt")
            if alt and alt.strip():
                out.append("[img: %s]" % " ".join(entidades(alt).split()))
        elif nombre == "pre":
            pre += -1 if cierre else 1
            if pre < 0:
                pre = 0
            out.append("\n")
        elif nombre == "p" and not cierre:
            out.append("\n\n")
        elif nombre in BLOQUE:
            out.append("\n")
    if enlace_abierto is not None:
        out.append("[%d]" % enlace_abierto)
    return titulo, "".join(out), enlaces


def es_ref(t):
    return len(t) > 2 and t[0] == "[" and t[-1] == "]" and t[1:-1].isdigit()


def unir(texto):
    """Junta viñetas "*" sueltas y referencias [n] con su texto."""
    filas = []
    ult = -1                       # última fila con texto
    for linea in texto.split("\n"):
        t = linea.strip()
        if not t:
            filas.append("")
            continue
        if ult >= 0:
            prev = filas[ult].strip()
            if es_ref(t) or (prev == "*" and t != "*"):
                filas[ult] = (filas[ult].rstrip() + (" " if prev == "*" else "") + t)
                del filas[ult + 1:]
                continue
        filas.append(linea)
        ult = len(filas) - 1
    return filas


def ajustar(texto, ancho):
    """Corta el texto en líneas de hasta ancho columnas."""
    lineas = []
    for linea in unir(texto):
        palabras = linea.split()
        if not palabras:
            if lineas and lineas[-1]:
                lineas.append("")
            continue
        vineta = palabras[0] == "*" and linea.startswith("  * ")
        sangria = "    " if vineta else ""
        actual = "  " if vineta else ""
        vacio = True
        for p in palabras:
            p = p.replace("\xa0", " ")
            lugar = ancho - len(sangria)
            while len(p) > lugar:          # palabra (o URL) más larga que la línea
                if not vacio:
                    lineas.append(actual)
                lineas.append(sangria + p[:lugar])
                p = p[lugar:]
                actual, vacio = sangria, True
            if vacio:
                actual += p
                vacio = False
            elif len(actual) + 1 + len(p) <= ancho:
                actual += " " + p
            else:
                lineas.append(actual)
                actual = sangria + p
        if not vacio:
            lineas.append(actual)
    while lineas and not lineas[-1]:
        lineas.pop()
    return lineas


# ------------------------------------------------------------ interfaz ----

class Pagina:
    def __init__(self, url, titulo, lineas, enlaces, info):
        self.url = url
        self.titulo = titulo
        self.lineas = lineas
        self.enlaces = enlaces
        self.info = info
        self.pos = 0


def normalizar(entrada):
    e = entrada.strip()
    if e.startswith("http://") or e.startswith("https://"):
        return e
    return "https://" + e


def codificar(texto):
    seguro = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~"
    out = []
    for b in texto.encode():
        c = chr(b)
        if c in seguro:
            out.append(c)
        elif c == " ":
            out.append("+")
        else:
            out.append("%%%02X" % b)
    return "".join(out)


def abrir(url, ancho):
    print("Cargando %s ..." % url)
    gc.collect()
    url, estado, hdr, cuerpo, cortado = descargar(url)
    tam = len(cuerpo)
    ct = hdr.get("content-type", "").lower()
    texto = a_texto(cuerpo, hdr)
    del cuerpo
    gc.collect()
    if "html" in ct or (not ct and "<html" in texto[:500].lower()):
        titulo, plano, enlaces = html_a_texto(texto, url)
    elif ct.startswith("text/") or "json" in ct or "xml" in ct:
        titulo, plano, enlaces = "", texto, []
    else:
        titulo, plano, enlaces = "", "(contenido %s, %d bytes: no es texto)" % (ct, tam), []
    del texto
    gc.collect()
    lineas = ajustar(plano, ancho)
    del plano
    gc.collect()
    info = "HTTP %d, %d bytes%s, %s" % (estado, tam, " (CORTADO)" if cortado else "",
                                        ct or "sin tipo")
    if estado >= 400:
        lineas = ["*** Error HTTP %d ***" % estado, ""] + lineas
    return Pagina(url, titulo, lineas, enlaces, info)


def pagina_inicio(ancho):
    lineas = INICIO.split("\n")
    enlaces = []
    for u in SUGERENCIAS:
        enlaces.append(u)
        lineas.append("  [%d] %s" % (len(enlaces), u))
    return Pagina("(inicio)", "Inicio", lineas, enlaces, "")


def mostrar(p, ancho, alto):
    cuerpo = alto - 2
    print("\n" + ("= " + (p.titulo or p.url))[:ancho])
    for l in p.lineas[p.pos:p.pos + cuerpo]:
        print(l)
    for _ in range(cuerpo - len(p.lineas[p.pos:p.pos + cuerpo])):
        print()


def estado_linea(p, alto):
    cuerpo = alto - 2
    fin = min(p.pos + cuerpo, len(p.lineas))
    pc = 100 if not p.lineas else fin * 100 // len(p.lineas)
    return "-- %d-%d de %d (%d%%) %d enlaces -- Enter, N, b, ?, q > " % (
        p.pos + 1 if p.lineas else 0, fin, len(p.lineas), pc, len(p.enlaces))


def principal(args):
    ancho, alto = 80, 24
    url = None
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-w" and i + 1 < len(args):
            ancho = max(30, int(args[i + 1]))
            i += 2
        elif a == "-l" and i + 1 < len(args):
            alto = max(8, int(args[i + 1]))
            i += 2
        elif a in ("-h", "--help"):
            print(AYUDA)
            return
        else:
            url = a
            i += 1
    historial = []
    actual = pagina_inicio(ancho)
    if url:
        try:
            actual = abrir(normalizar(url), ancho)
        except Exception as e:
            print("No pude abrir %s: %s" % (url, e))
    busqueda = ""
    redibujar = True
    while True:
        if redibujar:
            mostrar(actual, ancho, alto)
        redibujar = True
        try:
            cmd = input(estado_linea(actual, alto)).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        cuerpo = alto - 2
        destino = None
        if cmd == "":
            if actual.pos + cuerpo < len(actual.lineas):
                actual.pos += cuerpo
            else:
                print("(fin de la página)")
                redibujar = False
        elif cmd == "-":
            actual.pos = max(0, actual.pos - cuerpo)
        elif cmd == "t":
            actual.pos = 0
        elif cmd == "q":
            return
        elif cmd in ("?", "h", "ayuda"):
            print(AYUDA)
            redibujar = False
        elif cmd == "b":
            if historial:
                actual = historial.pop()
            else:
                print("(no hay páginas anteriores)")
                redibujar = False
        elif cmd == "l":
            for k, u in enumerate(actual.enlaces):
                print("[%d] %s" % (k + 1, u))
            if not actual.enlaces:
                print("(sin enlaces)")
            redibujar = False
        elif cmd == "i":
            print("Dirección: %s\nTítulo:    %s\n%s\nMemoria libre: %d bytes" % (
                actual.url, actual.titulo, actual.info, gc.mem_free()))
            redibujar = False
        elif cmd == "r":
            if actual.url.startswith("http"):
                destino = actual.url
        elif cmd.startswith("s "):
            nombre = cmd[2:].strip()
            try:
                with open(nombre, "w") as f:
                    f.write(actual.url + "\n\n" + "\n".join(actual.lineas) + "\n")
                    for k, u in enumerate(actual.enlaces):
                        f.write("[%d] %s\n" % (k + 1, u))
                print("Guardado en %s" % nombre)
            except OSError as e:
                print("No pude guardar: %s" % e)
            redibujar = False
        elif cmd.startswith("/"):
            if len(cmd) > 1:
                busqueda = cmd[1:].lower()
            if not busqueda:
                redibujar = False
                continue
            encontrado = -1
            for k in range(actual.pos + 1, len(actual.lineas)):
                if busqueda in actual.lineas[k].lower():
                    encontrado = k
                    break
            if encontrado < 0:
                print("(\"%s\" no aparece más abajo)" % busqueda)
                redibujar = False
            else:
                actual.pos = encontrado
        elif cmd.isdigit():
            k = int(cmd)
            if 1 <= k <= len(actual.enlaces):
                destino = actual.enlaces[k - 1]
            else:
                print("(no hay enlace [%d])" % k)
                redibujar = False
        elif cmd.startswith("buscar "):
            destino = BUSCADOR + codificar(cmd[7:].strip())
        elif cmd.startswith("g "):
            destino = normalizar(cmd[2:])
        elif "." in cmd and " " not in cmd:
            destino = normalizar(cmd)
        else:
            print("(no entiendo \"%s\": ? para la ayuda)" % cmd)
            redibujar = False
        if destino:
            try:
                nueva = abrir(destino, ancho)
                if destino != actual.url:
                    historial.append(actual)
                    if len(historial) > 8:      # la memoria es poca
                        historial.pop(0)
                actual = nueva
            except MemoryError:
                gc.collect()
                print("Sin memoria para esa página (es demasiado grande).")
                redibujar = False
            except Exception as e:
                print("No pude abrir %s: %s" % (destino, e))
                redibujar = False
        gc.collect()


principal(sys.argv[1:])
