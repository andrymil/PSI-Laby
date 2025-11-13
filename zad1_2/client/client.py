import socket
import os
import hashlib
import struct
import time

# Używamy nazwy usługi 'server' z docker-compose.yml
# Docker sam znajdzie jej IP.
HOST = "server"
PORT = 8888

FILE_SIZE = 10000
CHUNK_SIZE = 100
NUM_CHUNKS = FILE_SIZE // CHUNK_SIZE
FILENAME = "file_to_send.dat"

# Numery specjalne protokołu
PACKET_FIN = -1
PACKET_DONE = -2
CLIENT_TIMEOUT = 2.0 # sekundy

def main():
    # --- 1. Wygeneruj plik i oblicz hash ---
    print(f"Generating random file: {FILENAME} ({FILE_SIZE} bytes)")
    file_data = os.urandom(FILE_SIZE)
    with open(FILENAME, 'wb') as f:
        f.write(file_data)

    local_hash = hashlib.sha256(file_data).hexdigest()
    print(f"Local file hash: {local_hash}\n")

    # Podziel plik na paczki
    chunks = {}
    for i in range(NUM_CHUNKS):
        chunk_data = file_data[i*CHUNK_SIZE : (i+1)*CHUNK_SIZE]
        # Format pakietu: 4-bajty (int) numeru sekw. + 100-bajtów danych
        chunks[i] = struct.pack(f'!i{CHUNK_SIZE}s', i, chunk_data)

    # --- 2. Skonfiguruj gniazdo UDP ---
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(CLIENT_TIMEOUT)

    # --- 3. Wyślij pierwszą partię wszystkich pakietów ---
    print(f"Sending all {NUM_CHUNKS} packets (initial burst)...")
    for i in range(NUM_CHUNKS):
        sock.sendto(chunks[i], (HOST, PORT))
    print("Initial burst sent.")

    # Format nagłówka dla pakietów specjalnych
    header_format = struct.Struct('!i') # 4-bajty int

    # --- 4. Pętla retransmisji (NAK) ---
    while True:
        try:
            # Wyślij FIN, aby zapytać serwer o status
            print("Sending FIN packet (-1)...")
            sock.sendto(header_format.pack(PACKET_FIN), (HOST, PORT))

            # Czekaj na odpowiedź (NAK lub DONE)
            data, addr = sock.recvfrom(4096) # Bufor na listę brakujących

            # Sprawdź odpowiedź
            resp_type = header_format.unpack(data[:4])[0]

            if resp_type == PACKET_DONE:
                print("\nServer sent DONE (-2). File transfer complete.")
                break

            # Jeśli to nie DONE, to zakładamy, że to NAK (lista brakujących)
            num_missing = resp_type
            if num_missing > 0:
                print(f"Server sent NAK. Missing {num_missing} packets.")
                # Rozpakuj resztę pakietu, aby uzyskać listę
                missing_list_format = f'!{num_missing}i'
                missing_seqs = struct.unpack(missing_list_format, data[4:])

                print(f"Retransmitting packets: {missing_seqs}")
                for seq_num in missing_seqs:
                    if seq_num in chunks:
                        sock.sendto(chunks[seq_num], (HOST, PORT))
                    else:
                        print(f"Error: Server requested non-existent packet {seq_num}")

            elif num_missing == 0:
                 print("\nServer sent NAK with 0 missing. File transfer complete.")
                 # Serwer mógł wysłać NAK z 0 zanim otrzymał nasze FIN,
                 # a my otrzymaliśmy go zamiast DONE. Tak czy inaczej, jest OK.
                 break

        except socket.timeout:
            print(f"Timeout ({CLIENT_TIMEOUT}s) waiting for server ACK/NAK.")
            # Nasz FIN lub odpowiedź serwera (NAK/DONE) zostały zgubione.
            # Pętla po prostu powtórzy wysłanie FIN.
            continue
        except Exception as e:
            print(f"An error occurred: {e}")
            break

    sock.close()
    print("Client shutting down.")
    print(f"--- VERIFICATION ---")
    print(f"Client local hash: {local_hash}")
    print(f"(Compare this with the hash printed by the server console)")

if __name__ == "__main__":
    main()