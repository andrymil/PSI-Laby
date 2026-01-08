import hashlib
import hmac
import secrets
import struct

from consts import (
    DEFAULT_G,
    DEFAULT_P,
    HEADER_FORMAT,
    MAC_SIZE,
    MSG_TYPE_SECURE_MESSAGE,
)


class ProtocolEngine:
    def __init__(self):
        self.session_key = None  # 32 bytes
        # Parametry DH
        self.p = None
        self.g = None
        self.private_key = None
        self.public_key = None
        self.peer_public_key = None

    def generate_dh_params(self):
        """Dla klienta: Generuje p, g, klucz prywatny i publiczny."""
        self.p = DEFAULT_P
        self.g = DEFAULT_G
        # Klucz prywatny to losowa liczba < p
        self.private_key = secrets.randbelow(self.p - 1) + 1
        # A = g^a mod p
        self.public_key = pow(self.g, self.private_key, self.p)
        return self.p, self.g, self.public_key

    def handle_peer_dh_params(self, p, g, peer_public_key):
        """Dla serwera: Odbiera parametry od klienta."""
        self.p = p
        self.g = g
        self.peer_public_key = peer_public_key
        # Generuje własną parę kluczy
        self.private_key = secrets.randbelow(self.p - 1) + 1
        self.public_key = pow(self.g, self.private_key, self.p)

        self._derive_session_key()
        return self.public_key

    def finalize_handshake(self, peer_public_key):
        """Dla klienta: Odbiera B od serwera i finalizuje klucz."""
        self.peer_public_key = peer_public_key
        self._derive_session_key()

    def _derive_session_key(self):
        """Oblicza wspólny sekret i hashuje go do 32B."""
        # S = B^a mod p (lub A^b mod p)
        shared_secret = pow(self.peer_public_key, self.private_key, self.p)
        # Hashujemy sekret, aby uzyskać bezpieczny klucz sesji 32B
        self.session_key = hashlib.sha256(str(shared_secret).encode()).digest()
        # print(f"[DEBUG] Session Key Derived: {self.session_key.hex()}")

    def xor_encrypt_decrypt(self, data: bytes) -> bytes:
        """Szyfr strumieniowy XOR."""
        if not self.session_key:
            raise Exception("Brak klucza sesji!")

        output = bytearray()
        key_len = len(self.session_key)
        for i, byte in enumerate(data):
            output.append(byte ^ self.session_key[i % key_len])
        return bytes(output)

    def create_packet(self, msg_type, payload: bytes) -> bytes:
        """Tworzy nagłówek i dokleja payload."""
        length = len(payload)
        header = struct.pack(HEADER_FORMAT, msg_type, length)
        return header + payload

    def parse_header(self, header_bytes):
        """Rozpakowuje 5-bajtowy nagłówek."""
        return struct.unpack(HEADER_FORMAT, header_bytes)

    def create_secure_message(self, inner_flag, content: bytes) -> bytes:
        """
        Tworzy SecureMessage (Wariant W1: Encrypt-then-MAC).
        1. Plaintext = Flag (1B) + Content
        2. Ciphertext = XOR(Plaintext)
        3. MAC = HMAC(Ciphertext)
        4. Payload = Ciphertext + MAC
        """
        # 1. Przygotuj dane z flagą
        plaintext = struct.pack("!B", inner_flag) + content

        # 2. Zaszyfruj
        ciphertext = self.xor_encrypt_decrypt(plaintext)

        # 3. Oblicz MAC
        tag = hmac.new(self.session_key, ciphertext, hashlib.sha256).digest()

        # 4. Złóż payload
        payload = ciphertext + tag

        return self.create_packet(MSG_TYPE_SECURE_MESSAGE, payload)

    def decrypt_secure_message(self, payload: bytes):
        """
        Odbiera payload SecureMessage.
        Weryfikuje MAC, deszyfruje i zwraca (inner_flag, data).
        """
        if len(payload) < MAC_SIZE:
            raise ValueError("Payload zbyt krótki (brak MAC)")

        ciphertext = payload[:-MAC_SIZE]
        received_tag = payload[-MAC_SIZE:]

        # 1. Weryfikacja MAC
        calculated_tag = hmac.new(self.session_key, ciphertext, hashlib.sha256).digest()

        # Używamy compare_digest aby uniknąć ataku czasowego
        if not hmac.compare_digest(received_tag, calculated_tag):
            raise ValueError("Błąd weryfikacji MAC! Wiadomość sfałszowana.")

        # 2. Deszyfrowanie
        plaintext = self.xor_encrypt_decrypt(ciphertext)

        # 3. Wyodrębnienie flagi
        inner_flag = plaintext[0]
        data = plaintext[1:]

        return inner_flag, data
