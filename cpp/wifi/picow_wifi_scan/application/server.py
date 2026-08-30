import socket
import threading

HTTP_PORT = 8080
TCP_PORT = 4242
BUF = 1024

tcp_client = None


# ---------------- TCP SERVER ----------------
def tcp_server():
    global tcp_client
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", TCP_PORT))
    s.listen(1)

    print("Waiting for Pico TCP client...")
    tcp_client, addr = s.accept()
    print("Pico connected from", addr)


# ---------------- HTTP SERVER ----------------
def http_server():
    global tcp_client

    html_page = b"""\
HTTP/1.1 200 OK
Content-Type: text/html

<!DOCTYPE html>
<html>
<head>
    <title>Pico Control</title>
</head>
<body>
    <h1>Pico Controller</h1>
    <button onclick="send('up')">UP</button><br><br>
    <button onclick="send('down')">DOWN</button><br><br>
    <button onclick="send('left')">LEFT</button>
    <button onclick="send('right')">RIGHT</button>

    <script>
        function send(cmd) {
            fetch('/send', {
                method: 'POST',
                body: cmd
            });
        }
    </script>
</body>
</html>
"""

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", HTTP_PORT))
    s.listen(5)

    print(f"Open browser: http://localhost:{HTTP_PORT}")

    while True:
        conn, _ = s.accept()
        request = conn.recv(BUF)

        if request.startswith(b"POST /send"):
            body = request.split(b"\r\n\r\n", 1)[1].strip()
            print("HTTP command:", body.decode())

            if tcp_client:
                tcp_client.sendall(body + b"\n")

            conn.sendall(b"HTTP/1.1 200 OK\r\n\r\nOK")

        else:
            conn.sendall(html_page)

        conn.close()


# ---------------- MAIN ----------------
threading.Thread(target=tcp_server, daemon=True).start()
http_server()
