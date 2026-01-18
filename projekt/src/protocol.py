import hashlib
import hmac
import secrets
import struct

from consts import *


class ProtocolEngine:
    def __init__(self):
        self.session_key = None
        self.p = None
        self.g = None
        self.private_key = None
        self.public_key = None
        self.peer_public_key = None

    def generate_dh_params(self):
        self.p = DEFAULT_P
        self.g = DEFAULT_G
        self.private_key = secrets.randbelow(self.p - 1) + 1
        self.public_key = pow(self.g, self.private_key, self.p)
        return self.p, self.g, self.public_key

    def handle_peer_dh_params(self, p, g, peer_public_key):
        self.p = p
        self.g = g
        self.peer_public_key = peer_public_key
        self.private_key = secrets.randbelow(self.p - 1) + 1
        self.public_key = pow(self.g, self.private_key, self.p)

        self._derive_session_key()
        return self.public_key

    def finalize_handshake(self, peer_public_key):
        self.peer_public_key = peer_public_key
        self._derive_session_key()

    def _derive_session_key(self):
        shared_secret = pow(self.peer_public_key, self.private_key, self.p)
        self.session_key = hashlib.sha256(str(shared_secret).encode()).digest()

        print("\n" + "=" * 50)
        print(f"[REPORT] Established SESSION KEY (Hex): {self.session_key.hex()}")
        print("=" * 50 + "\n")

    def xor_encrypt_decrypt(self, data: bytes) -> bytes:
        if not self.session_key:
            raise Exception("Session key missing!")

        output = bytearray()
        key_len = len(self.session_key)
        for i, byte in enumerate(data):
            output.append(byte ^ self.session_key[i % key_len])
        return bytes(output)

    def create_packet(self, msg_type, payload: bytes) -> bytes:
        length = len(payload)
        header = struct.pack(HEADER_FORMAT, msg_type, length)
        return header + payload

    def parse_header(self, header_bytes):
        return struct.unpack(HEADER_FORMAT, header_bytes)

    def create_secure_message(self, inner_flag, content: bytes) -> bytes:
        plaintext = struct.pack("!B", inner_flag) + content

        ciphertext = self.xor_encrypt_decrypt(plaintext)

        print("[REPORT] Sending encrypted message:")
        print(f"  Plaintext (Hex): {plaintext.hex()}")
        print(f"  Ciphertext (Hex): {ciphertext.hex()}")

        tag = hmac.new(self.session_key, ciphertext, hashlib.sha256).digest()

        payload = ciphertext + tag

        return self.create_packet(MSG_TYPE_SECURE_MESSAGE, payload)

    def decrypt_secure_message(self, payload: bytes):
        if len(payload) < MAC_SIZE:
            raise ValueError("Payload too short (missing MAC)")

        ciphertext = payload[:-MAC_SIZE]
        received_tag = payload[-MAC_SIZE:]

        calculated_tag = hmac.new(self.session_key, ciphertext, hashlib.sha256).digest()
        if not hmac.compare_digest(received_tag, calculated_tag):
            raise ValueError("MAC verification failed! Message forged.")

        plaintext = self.xor_encrypt_decrypt(ciphertext)

        inner_flag = plaintext[0]
        data = plaintext[1:]

        return inner_flag, data
