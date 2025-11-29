import socket
import struct
import hashlib
import sys
import math

HOST = "0.0.0.0"
PORT = 8888

FILE_SIZE = 10000
CHUNK_SIZE = 100
NUM_CHUNKS = math.ceil(FILE_SIZE / CHUNK_SIZE)
PACKET_SIZE = CHUNK_SIZE + 4

FIN_PACKET = -1


def setup_socket():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((HOST, PORT))

        print(f"Server listening on {HOST}:{PORT}\n")
        return sock
    except Exception as e:
        print(f"Socket configuration error: {e}")
        return None


def handle_fin_packet(sock, client_addr, received_chunks):
    print("Received FIN. Checking packets...")

    missing_list = [i for i, received in enumerate(received_chunks) if not received]
    missing_count = len(missing_list)

    if missing_count == 0:
        print("All packets received. Sending success.")

        success_pkt = struct.pack('!i', 0)

        sock.sendto(success_pkt, client_addr)
        sock.settimeout(2.0)

        print('Waiting in case client did not get success packet.')
        try:
            while True:
                sock.recvfrom(1024)

                print("Client sent FIN again. Resending success.")
                sock.sendto(success_pkt, client_addr)

        except socket.timeout:
            print("Timeout reached.")
        except Exception as e:
            print(f"Unexpected error: {e}")

        return True
    else:
        print(f"Missing {missing_count} packets. Sending NAK list.")

        args_to_pack = [missing_count] + missing_list
        nak_packet = struct.pack(f'!i{missing_count}i', *args_to_pack)

        sock.sendto(nak_packet, client_addr)

        return False


def calculate_and_print_hash(file_buffer):
    print("\nReconstruction complete.")
    server_hash = hashlib.sha256(file_buffer).hexdigest()
    print("--- VERIFICATION ---")
    print(f"Server hash: {server_hash}")


def server_loop(sock, file_buffer, received_chunks):
    client_addr = None
    total_received = 0

    while True:
        try:
            data, addr = sock.recvfrom(4096)

            if client_addr is None:
                client_addr = addr
                print(f"Connection established with: {addr}")

            if len(data) < 4:
                continue

            seq_number = struct.unpack('!i', data[:4])[0]

            if 0 <= seq_number < NUM_CHUNKS:
                if len(data) == PACKET_SIZE and not received_chunks[seq_number]:
                    start = seq_number * CHUNK_SIZE
                    end = start + CHUNK_SIZE
                    file_buffer[start:end] = data[4:]

                    received_chunks[seq_number] = True
                    total_received += 1
                    print(f"Received packet {seq_number}. Total: {total_received}/{NUM_CHUNKS}")

            elif seq_number == FIN_PACKET:
                if handle_fin_packet(sock, client_addr, received_chunks):
                    break

        except Exception as e:
            print(f"An error occurred in server loop: {e}")
            break


def main():
    file_buffer = bytearray(FILE_SIZE)
    received_chunks = [False] * NUM_CHUNKS

    sock = setup_socket()
    if sock is None:
        sys.exit(1)

    try:
        server_loop(sock, file_buffer, received_chunks)
    finally:
        sock.close()

    calculate_and_print_hash(file_buffer)
    print("Server shutting down.")


if __name__ == "__main__":
    main()
