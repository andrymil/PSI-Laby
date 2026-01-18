KEY_HEX = "paste_key_from_logs_here"

CIPHER_HEX = "paste_ciphertext_from_logs_here"


def xor_decrypt(key_hex, cipher_hex):
    key = bytes.fromhex(key_hex)
    cipher = bytes.fromhex(cipher_hex)

    decrypted = bytearray()
    key_len = len(key)

    print("REPORT DATA:")
    print(f"Key: {key_hex}")
    print(f"Ciphertext: {cipher_hex}")
    print("-" * 30)
    print("Byte-by-byte decryption (XOR):")

    for i, byte in enumerate(cipher):
        k = key[i % key_len]
        res = byte ^ k
        decrypted.append(res)
        print(
            f"Byte {i}: {hex(byte)} XOR {hex(k)} = {hex(res)} ('{chr(res)}' if printable)"
        )

    print("-" * 30)
    print(f"Decrypted HEX: {decrypted.hex()}")
    print(f"Decrypted TEXT: {decrypted[1:].decode('utf-8', errors='ignore')}")


if __name__ == "__main__":
    xor_decrypt(KEY_HEX, CIPHER_HEX)
