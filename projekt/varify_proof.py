KEY_HEX = "tutaj_wklej_klucz_z_logow"

CIPHER_HEX = "tutaj_wklej_ciphertext_z_logow"


def xor_decrypt(key_hex, cipher_hex):
    key = bytes.fromhex(key_hex)
    cipher = bytes.fromhex(cipher_hex)

    decrypted = bytearray()
    key_len = len(key)

    print("DANE DO SPRAWOZDANIA:")
    print(f"Klucz: {key_hex}")
    print(f"Szyfrogram: {cipher_hex}")
    print("-" * 30)
    print("Odszyfrowywanie bajt po bajcie (XOR):")

    for i, byte in enumerate(cipher):
        k = key[i % key_len]
        res = byte ^ k
        decrypted.append(res)
        print(
            f"Bajt {i}: {hex(byte)} XOR {hex(k)} = {hex(res)} ('{chr(res)}' jeśli drukowny)"
        )

    print("-" * 30)
    print(f"Odszyfrowany HEX: {decrypted.hex()}")
    # Flaga 01 to DATA, reszta to tekst
    print(f"Odszyfrowany TEKST: {decrypted[1:].decode('utf-8', errors='ignore')}")


if __name__ == "__main__":
    xor_decrypt(KEY_HEX, CIPHER_HEX)
