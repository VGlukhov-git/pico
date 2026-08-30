import socket, os

PORT = 4242
BUF = 2048

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("", PORT))
s.listen(1)

print("Waiting for Pico...")
conn, addr = s.accept()
print("Connected", addr)

for i in range(10):
    data = os.urandom(BUF)
    conn.sendall(data)
    rx = b""
    while len(rx) < BUF:
        rx += conn.recv(BUF - len(rx))
    print("OK iteration", i+1)

conn.close()
s.close()
