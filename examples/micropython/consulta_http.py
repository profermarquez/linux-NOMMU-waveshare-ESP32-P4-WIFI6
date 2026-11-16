# consulta_http.py - pide una página por HTTP con sockets (MicroPython).
# Uso: python3 consulta_http.py [host] [ruta]
# Nota: no hay módulo ssl en esta placa, así que solo HTTP (no HTTPS).
import socket, sys

host = sys.argv[1] if len(sys.argv) > 1 else "example.com"
path = sys.argv[2] if len(sys.argv) > 2 else "/"

# El kernel no tiene IPv6: quedarse con la primera dirección IPv4.
addr = None
for fam, typ, proto, _, sa in socket.getaddrinfo(host, 80):
    if fam == socket.AF_INET:
        addr = sa
        break
if addr is None:
    print("sin dirección IPv4 para", host)
    sys.exit(1)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(addr)
s.send(b"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n" % (path.encode(), host.encode()))
data = b""
while True:
    chunk = s.recv(1024)
    if not chunk:
        break
    data += chunk
s.close()
head, _, body = data.partition(b"\r\n\r\n")
print(head.split(b"\r\n")[0].decode())
print("bytes de contenido:", len(body))
t = body.find(b"<title>")
if t >= 0:
    print("título:", body[t + 7:body.find(b"</title>")].decode())
