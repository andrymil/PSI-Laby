import socket
import struct
import sys
import threading

from consts import (
    HEADER_SIZE,
    INNER_FLAG_DATA,
    INNER_FLAG_END_SESSION,
    MSG_TYPE_CLIENT_HELLO,
    MSG_TYPE_SECURE_MESSAGE,
    MSG_TYPE_SERVER_HELLO,
)
from protocol import ProtocolEngine


class Server:
    def __init__(self, port):
        self.host = "0.0.0.0"
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.bind((self.host, self.port))
        self.sock.listen(5)

        self.clients = {}
        self.lock = threading.Lock()
        self.running = True

    def start(self):
        print(f"Server started on port {self.port}")

        accept_thread = threading.Thread(target=self.accept_loop)
        accept_thread.daemon = True
        accept_thread.start()

        self.command_loop()

    def accept_loop(self):
        while self.running:
            try:
                client_sock, addr = self.sock.accept()
                addr_str = f"{addr[0]}:{addr[1]}"

                engine = ProtocolEngine()

                with self.lock:
                    self.clients[addr_str] = {
                        "socket": client_sock,
                        "engine": engine,
                        "active": True,
                    }

                print(f"\n[+] New connection from {addr_str}")

                client_thread = threading.Thread(
                    target=self.handle_client, args=(addr_str,)
                )
                client_thread.daemon = True
                client_thread.start()

            except OSError:
                break

    def handle_client(self, addr_str):
        client_data = self.clients[addr_str]
        sock = client_data["socket"]
        engine = client_data["engine"]

        try:
            while client_data["active"]:
                header_bytes = sock.recv(HEADER_SIZE)
                if not header_bytes:
                    break

                msg_type, length = engine.parse_header(header_bytes)

                payload = b""
                while len(payload) < length:
                    chunk = sock.recv(length - len(payload))
                    if not chunk:
                        raise ConnectionError("Incomplete payload")
                    payload += chunk

                if msg_type == MSG_TYPE_CLIENT_HELLO:
                    p = struct.unpack("!I", payload[0:4])[0]
                    g = struct.unpack("!I", payload[4:8])[0]
                    A = struct.unpack("!I", payload[8:12])[0]

                    print(f"[{addr_str}] Received ClientHello (p={p}, g={g}, A={A})")

                    B = engine.handle_peer_dh_params(p, g, A)

                    resp_payload = struct.pack("!I", B)
                    packet = engine.create_packet(MSG_TYPE_SERVER_HELLO, resp_payload)
                    sock.sendall(packet)
                    print(f"[{addr_str}] Sent ServerHello (B={B})")

                elif msg_type == MSG_TYPE_SECURE_MESSAGE:
                    try:
                        inner_flag, data = engine.decrypt_secure_message(payload)

                        if inner_flag == INNER_FLAG_DATA:
                            msg_text = data.decode("utf-8")
                            print(f"[{addr_str}] Secure Message: {msg_text}")

                        elif inner_flag == INNER_FLAG_END_SESSION:
                            print(f"[{addr_str}] Received EndSession request.")
                            break

                    except ValueError as e:
                        print(f"[{addr_str}] SECURITY ALERT: {e}")
                        break

                else:
                    print(f"[{addr_str}] Unknown message type: {msg_type}")

        except Exception as e:
            print(f"[{addr_str}] Error: {e}")
        finally:
            self.disconnect_client(addr_str)

    def disconnect_client(self, addr_str):
        with self.lock:
            if addr_str in self.clients:
                client = self.clients[addr_str]
                if client["active"]:
                    client["active"] = False
                    try:
                        client["socket"].close()
                    except:
                        pass
                    print(f"[-] Connection closed for {addr_str}")
                del self.clients[addr_str]

    def send_end_session(self, addr_str):
        with self.lock:
            if addr_str not in self.clients:
                print("Client not found.")
                return
            client = self.clients[addr_str]

        try:
            packet = client["engine"].create_secure_message(INNER_FLAG_END_SESSION, b"")
            client["socket"].sendall(packet)
            print(f"Sent EndSession to {addr_str}")
        except Exception as e:
            print(f"Error sending EndSession: {e}")

        self.disconnect_client(addr_str)

    def command_loop(self):
        print("\n--- Server CLI ---")
        print("Commands: 'list', 'kill <ip:port>', 'exit'")

        while self.running:
            try:
                cmd = input("> ").strip().split()
                if not cmd:
                    continue

                if cmd[0] == "list":
                    with self.lock:
                        print("Active clients:")
                        for addr in self.clients:
                            print(f" - {addr}")

                elif cmd[0] == "kill":
                    if len(cmd) < 2:
                        print("Usage: kill <ip:port>")
                    else:
                        self.send_end_session(cmd[1])

                elif cmd[0] == "exit":
                    self.running = False
                    self.sock.close()
                    sys.exit(0)
                else:
                    print("Unknown command")
            except Exception as e:
                print(f"CLI Error: {e}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python server.py <port>")
        sys.exit(1)
    port = int(sys.argv[1])
    server = Server(port)
    server.start()
