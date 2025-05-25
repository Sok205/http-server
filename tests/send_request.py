import socket
import time

HOST = '127.0.0.1'
PORT = 4222

request1 = (
    "GET /hello HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
)

request2 = (
    "GET /world HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Connection: close\r\n"
    "\r\n"
)

with socket.create_connection((HOST, PORT)) as sock:

    sock.sendall(request1.encode())
    response1 = sock.recv(4096).decode()
    print("----- Response 1 -----")
    print(response1)


    time.sleep(1)


    sock.sendall(request2.encode())
    response2 = sock.recv(4096).decode()
    print("----- Response 2 -----")
    print(response2)

