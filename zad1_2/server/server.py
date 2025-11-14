import socket
import struct
import hashlib
import sys

PORT = 8888
FILE_SIZE = 10000
CHUNK_SIZE = 100
NUM_CHUNKS = FILE_SIZE // CHUNK_SIZE
PACKET_DATA_SIZE = CHUNK_SIZE + 4  # 4b nagłówek + 100b dane

# Numery specjalne protokołu
PACKET_FIN = -1
PACKET_DONE = -2


def setup_socket() -> socket.socket | None:
    """Tworzy, konfiguruje i binduje gniazdo serwera UDP."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(('0.0.0.0', PORT))
        print(f"Serwer (Python) nasłuchuje na 0.0.0.0:{PORT} (UDP)\n")
        return sock
    except Exception as e:
        print(f"Błąd konfiguracji gniazda: {e}")
        return None


def handle_fin_packet(sock, client_addr, received_chunks):
    """
    Obsługuje żądanie FIN: sprawdza brakujące pakiety i wysyła NAK lub DONE.
    Zwraca False, jeśli transfer jest zakończony, w przeciwnym razie True.
    """
    print("Otrzymano FIN (-1). Sprawdzanie statusu...")

    # Znajdź brakujące pakiety
    missing_list = [i for i, received in enumerate(received_chunks) if not received]
    missing_count = len(missing_list)

    if missing_count == 0:
        # Mamy wszystko, wyślij DONE i zakończ
        print("Wszystkie pakiety otrzymane. Wysyłanie DONE (-2).")
        done_pkt = struct.pack('!i', PACKET_DONE)
        sock.sendto(done_pkt, client_addr)
        sock.sendto(done_pkt, client_addr) # Wyślij dwa razy dla pewności
        return False  # Sygnalizuje pętli, aby się zakończyła
    else:
        # Przygotuj pakiet NAK (Negative Acknowledgment)
        print(f"Brakuje {missing_count} pakietów. Wysyłanie listy NAK.")

        # Format: 4b (ilość) + 4b * N (numery pakietów)
        nak_format = f'!i{missing_count}i'
        args_to_pack = [missing_count] + missing_list

        nak_pkt = struct.pack(nak_format, *args_to_pack)
        sock.sendto(nak_pkt, client_addr)
        return True  # Sygnalizuje pętli, aby kontynuowała


def calculate_and_print_hash(file_buffer):
    """Oblicza i wyświetla hash SHA-256 dla zrekonstruowanego pliku."""
    print("\nRekonstrukcja zakończona.")
    server_hash = hashlib.sha256(file_buffer).hexdigest()
    print("--- WERYFIKACJA --- (Porównaj z hashem klienta)")
    print(f"Server hash (Python): {server_hash}")


def server_loop(sock, file_buffer, received_chunks):
    """Główna pętla serwera odbierająca pakiety."""
    client_addr = None
    total_received = 0

    while True:
        try:
            data, addr = sock.recvfrom(8192)

            if client_addr is None:
                client_addr = addr
                print(f"Nawiązano połączenie z: {addr}")

            if len(data) < 4:
                continue  # Zbyt mały pakiet

            seq_num = struct.unpack('!i', data[:4])[0]

            if 0 <= seq_num < NUM_CHUNKS:
                # To jest pakiet danych
                if len(data) == PACKET_DATA_SIZE and not received_chunks[seq_num]:
                    start = seq_num * CHUNK_SIZE
                    end = start + CHUNK_SIZE
                    file_buffer[start:end] = data[4:]

                    received_chunks[seq_num] = True
                    total_received += 1
                    print(f"Otrzymano pakiet {seq_num}. Łącznie: {total_received}/{NUM_CHUNKS}")

            elif seq_num == PACKET_FIN:
                # Klient pyta o status
                if not handle_fin_packet(sock, client_addr, received_chunks):
                    break  # Zakończ pętlę, jeśli otrzymaliśmy wszystko

        except Exception as e:
            print(f"Wystąpił błąd w pętli serwera: {e}")
            break


def main():
    """Główna funkcja uruchamiająca serwer."""

    # Inicjalizacja buforów
    file_buffer = bytearray(FILE_SIZE)
    received_chunks = [False] * NUM_CHUNKS

    # Konfiguracja gniazda
    sock = setup_socket()
    if sock is None:
        sys.exit(1)

    try:
        # Uruchomienie głównej pętli
        server_loop(sock, file_buffer, received_chunks)
    finally:
        # Zawsze zamykaj gniazdo
        sock.close()

    # Weryfikacja po zakończeniu pętli
    calculate_and_print_hash(file_buffer)
    print("Serwer (Python) kończy działanie.")


if __name__ == "__main__":
    main()