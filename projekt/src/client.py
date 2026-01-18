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


class Client:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = None
        self.engine = ProtocolEngine()
        self.connected = False
        self.receive_thread = None

    def connect(self):
        if self.connected:
            print("Already connected.")
            return

        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.connected = True
            self.engine = ProtocolEngine()

            p, g, A = self.engine.generate_dh_params()
            payload = struct.pack("!III", p, g, A)
            packet = self.engine.create_packet(MSG_TYPE_CLIENT_HELLO, payload)
            self.sock.sendall(packet)
            print(f"Sent ClientHello (p={p}, g={g}, A={A})")

            self.receive_thread = threading.Thread(target=self.receive_loop)
            self.receive_thread.daemon = True
            self.receive_thread.start()

        except Exception as e:
            print(f"Connection failed: {e}")
            self.connected = False

    def send_message(self, text):
        if not self.connected or not self.engine.session_key:
            print("Not connected or handshake incomplete.")
            return

        packet = self.engine.create_secure_message(
            INNER_FLAG_DATA, text.encode("utf-8")
        )
        try:
            self.sock.sendall(packet)
        except:
            print("Failed to send message.")
            self.disconnect()

    def disconnect(self):
        if self.connected:
            if self.engine.session_key:
                try:
                    packet = self.engine.create_secure_message(
                        INNER_FLAG_END_SESSION, b""
                    )
                    self.sock.sendall(packet)
                except:
                    pass

            self.connected = False
            try:
                self.sock.close()
            except:
                pass
            print("Disconnected.")

    def receive_loop(self):
        try:
            while self.connected:
                header_bytes = self.sock.recv(HEADER_SIZE)
                if not header_bytes:
                    break

                msg_type, length = self.engine.parse_header(header_bytes)

                payload = b""
                while len(payload) < length:
                    chunk = self.sock.recv(length - len(payload))
                    if not chunk:
                        break
                    payload += chunk

                if msg_type == MSG_TYPE_SERVER_HELLO:
                    B = struct.unpack("!I", payload)[0]
                    print(f"Received ServerHello (B={B})")
                    self.engine.finalize_handshake(B)
                    print(
                        f"*** Secure Connection Established! Key: {self.engine.session_key.hex()[:8]}... ***"
                    )

                elif msg_type == MSG_TYPE_SECURE_MESSAGE:
                    try:
                        inner_flag, data = self.engine.decrypt_secure_message(payload)
                        if inner_flag == INNER_FLAG_END_SESSION:
                            print("\nServer ended the session.")
                            self.connected = False
                            self.sock.close()
                            break
                        elif inner_flag == INNER_FLAG_DATA:
                            print(f"\n[Server]: {data.decode('utf-8')}")
                    except ValueError as e:
                        print(f"Security Error: {e}")
                        self.disconnect()
                        break
        except Exception as e:
            if self.connected:
                print(f"Receive error: {e}")
            self.connected = False

    def start_cli(self):
        print("\n--- Client CLI ---")
        print("Commands: 'connect', 'send <msg>', 'exit'")

        while True:
            try:
                user_input = input("> ").strip()
                cmd_parts = user_input.split(" ", 1)
                cmd = cmd_parts[0]

                if cmd == "connect":
                    self.connect()
                elif cmd == "send":
                    if len(cmd_parts) > 1:
                        self.send_message(cmd_parts[1])
                    else:
                        print("Usage: send <message>")
                elif cmd == "exit":
                    self.disconnect()
                    sys.exit(0)
                else:
                    print("Unknown command")
            except KeyboardInterrupt:
                self.disconnect()
                sys.exit(0)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python client.py <host> <port>")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2])

    client = Client(host, port)
    client.start_cli()
