import socket
import time
import select
from datetime import datetime

HOST = '127.0.0.1'
PORT = 4222

# --- your existing GET requests ---
request1 = (
    "GET /hello HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
)

request2 = (
    "GET /hello HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Connection: close\r\n"
    "\r\n"
)

# --- new requests ---
# 1) PUT to replace the resource
put_body = '{"message": "Hello from PUT"}'
request_put = (
    "PUT /hello HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Content-Type: application/json\r\n"
    f"Content-Length: {len(put_body)}\r\n"
    "Connection: close\r\n"
    "\r\n"
    f"{put_body}"
)


# 3) DELETE to remove the resource
request_delete = (
    "DELETE /hello HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Connection: close\r\n"
    "\r\n"
)


def recv_with_timeout(sock, timeout=5):
    """Receive data with timeout to avoid blocking forever."""
    ready = select.select([sock], [], [], timeout)
    if ready[0]:
        try:
            data = sock.recv(4096)
            if not data:
                print("Server closed connection (recv returned empty)")
                return None
            return data.decode('utf-8', errors='replace')
        except ConnectionResetError:
            print("Connection reset by peer")
            return None
        except Exception as e:
            print(f"Receive error: {e}")
            return None
    else:
        print(f"Timeout waiting for response ({timeout}s)")
        return None


def send_and_print(request_str, description):
    """Open a fresh connection, send the request, and print response."""
    print(f"\n--- {description} ---")
    try:
        with socket.create_connection((HOST, PORT)) as sock:
            print(f"Connected for {description}")
            sock.sendall(request_str.encode())
            response = recv_with_timeout(sock)
            if response:
                print(f"----- Response ({description}) -----")
                print(response)
            else:
                print(f"No response for {description}")
    except ConnectionRefusedError:
        print(f"Cannot connect to {HOST}:{PORT} for {description}")
    except Exception as e:
        print(f"Error during {description}: {e}")


if __name__ == "__main__":
    time_begin = datetime.now()
    send_and_print(request1, "Request 1 (GET keep-alive)")
    time.sleep(0.5)
    send_and_print(request_put, "Request 2 (PUT)")
    send_and_print(request_delete, "Request 3 (DELETE)")
    send_and_print(request2, "Request 4 (GET close)")
    time_end = datetime.now()

    print(f"\n=== All tests completed in {time_end - time_begin} ===")
