#!/usr/bin/env python3

import hashlib
import socket
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 12345

SECRET_KEY = "Test42"

md5_raw = hashlib.md5(SECRET_KEY.encode()).digest()
target_hash = hashlib.sha1(md5_raw).hexdigest()

print(f"Secret key:  {SECRET_KEY}")
print(f"Target hash: {target_hash}")
print(f"Listening on port {PORT}...")

RANGE_START = "Test00"
RANGE_END = "Test99"

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(("0.0.0.0", PORT))
server.listen(1)

try:
    while True:
        print("\nWaiting for client connection...")
        conn, addr = server.accept()
        print(f"Client connected from {addr}")

        buf = ""
        def recv_line():
            global buf
            while "\n" not in buf:
                data = conn.recv(4096).decode()
                if not data:
                    return None
                buf += data
            line, buf = buf.split("\n", 1)
            return line.strip()

        def send_line(msg):
            conn.sendall((msg + "\n").encode())

        try:
            # Receive HASHRATE
            line = recv_line()
            if line is None:
                print("Client disconnected")
                conn.close()
                continue
            print(f"Client: {line}")

            if line.startswith("HASHRATE"):
                hashrate = line.split()[1]
                print(f"Client hashrate: {hashrate}")
            else:
                print(f"Unexpected message: {line}")
                conn.close()
                continue

            # Send TASK
            task_msg = f"TASK {target_hash} {RANGE_START} {RANGE_END}"
            print(f"Server: {task_msg}")
            send_line(task_msg)

            # Wait for result
            line = recv_line()
            if line is None:
                print("Client disconnected before sending result")
                conn.close()
                continue
            print(f"Client: {line}")

            if line.startswith("FOUND"):
                found_key = line.split(maxsplit=1)[1]
                if found_key == SECRET_KEY:
                    print(f"SUCCESS! Client found the correct key: {found_key}")
                else:
                    print(f"WRONG KEY! Expected '{SECRET_KEY}', got '{found_key}'")
            elif line == "NOT_FOUND":
                print("FAIL: Client did not find the key")
            else:
                print(f"Unexpected response: {line}")

            # Send DONE
            send_line("DONE")
            time.sleep(0.5)

        except Exception as e:
            print(f"Error: {e}")
        finally:
            conn.close()
            print("Client disconnected")

except KeyboardInterrupt:
    print("\nServer shutting down")
finally:
    server.close()
