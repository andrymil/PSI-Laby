import socket
import struct
import hashlib
import sys

PORT = 8888
FILE_SIZE = 10000
CHUNK_SIZE = 100
NUM_CHUNKS = FILE_SIZE // CHUNK_SIZE
PACKET_DATA_SIZE = CHUNK_SIZE + 4 # 4b nagłówek + 100b dane

# Numery specjalne protokołu
PACKET_FIN = -1
PACKET_DONE = -2

def main():
    # Bufor na cały plik (używamy bytearray dla wydajnej modyfikacji)
    file_buffer = bytearray(FILE_SIZE)
    # Lista flag śledząca otrzymane pakiety
    received_chunks = [False] * NUM_CHUNKS
    total_received = 0

    # --- 1. Konfiguracja gniazda UDP ---
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Umożliwia ponowne użycie adresu
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Bindowanie do wszystkich interfejsów (kluczowe dla Dockera)
        sock.bind(('0.0.0.0', PORT))
    except Exception as e:
        print(f"Błąd konfiguracji gniazda: {e}")
        sys.exit(1)

    print(f"Serwer (Python) nasłuchuje na 0.0.0.0:{PORT} (UDP)\n")

    # Zmienna do przechowania adresu klienta
    client_addr = None

    # --- 2. Główna pętla serwera ---
    while True:
        try:
            # Używamy bufora o rozmiarze większym niż NAK
            # (4b count + 4b * NUM_CHUNKS = 40004B, ale recvfrom ma limity)
            # 8192 powinno wystarczyć na NAK dla ~2000 pakietów
            data, addr = sock.recvfrom(8192)

            # Zapamiętujemy adres klienta przy pierwszym kontakcie
            if client_addr is None:
                client_addr = addr
                print(f"Nawiązano połączenie z: {addr}")

            if len(data) < 4:
                continue # Zbyt mały pakiet

            # Dekodujemy numer sekwencyjny (network byte order)
            seq_num = struct.unpack('!i', data[:4])[0]

            if 0 <= seq_num < NUM_CHUNKS:
                # To jest pakiet danych
                if len(data) == PACKET_DATA_SIZE:
                    if not received_chunks[seq_num]:
                        # Kopiujemy dane (pomijając 4b nagłówka)
                        start = seq_num * CHUNK_SIZE
                        end = start + CHUNK_SIZE
                        file_buffer[start:end] = data[4:]

                        received_chunks[seq_num] = True
                        total_received += 1
                        print(f"Otrzymano pakiet {seq_num}. Łącznie: {total_received}/{NUM_CHUNKS}")

            elif seq_num == PACKET_FIN:
                # Klient pyta o status
                print("Otrzymano FIN (-1). Sprawdzanie statusu...")

                # Znajdź brakujące pakiety
                missing_list = []
                for i in range(NUM_CHUNKS):
                    if not received_chunks[i]:
                        missing_list.append(i)

                missing_count = len(missing_list)

                if missing_count == 0:
                    # Mamy wszystko, wyślij DONE i zakończ
                    print("Wszystkie pakiety otrzymane. Wysyłanie DONE (-2).")
                    done_pkt = struct.pack('!i', PACKET_DONE)
                    # Wyślij kilka razy na wypadek, gdyby DONE się zgubił
                    sock.sendto(done_pkt, client_addr)
                    sock.sendto(done_pkt, client_addr)
                    break # Wyjście z pętli while(1)
                else:
                    # Przygotuj pakiet NAK (Negative Acknowledgment)
                    print(f"Brakuje {missing_count} pakietów. Wysyłanie listy NAK.")

                    # Format: 4b (ilość) + 4b * N (numery pakietów)
                    # Używamy '!' (network), 'i' (ilość) oraz f'{missing_count}i' (lista)
                    nak_format = f'!i{missing_count}i'

                    # Tworzymy listę argumentów do spakowania
                    args_to_pack = [missing_count] + missing_list

                    nak_pkt = struct.pack(nak_format, *args_to_pack)
                    sock.sendto(nak_pkt, client_addr)

        except Exception as e:
            print(f"Wystąpił błąd w pętli serwera: {e}")
            break

    # --- 3. Oblicz hash ---
    print("\nRekonstrukcja zakończona.")

    # Oblicz hash SHA-256
    server_hash = hashlib.sha256(file_buffer).hexdigest()

    print("--- WERYFIKACJA --- (Porównaj z hashem klienta)")
    print(f"Server hash (Python): {server_hash}")

    sock.close()
    print("Serwer (Python) kończy działanie.")

if __name__ == "__main__":
    main()