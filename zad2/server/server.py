import socket
import struct

# Definicja formatu pakietu:
# ! - porządek sieciowy (big-endian)
# i - 4-bajtowy signed integer (dla indeksu)
# i - 4-bajtowy signed integer (dla wartości)
PACKET_FORMAT = '!ii'
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)

HOST = "0.0.0.0"
PORT = 8888

class Node:
    """Prosta klasa do reprezentacji węzła drzewa."""
    def __init__(self, value, index):
        self.value = value
        self.index = index
        self.left = None
        self.right = None

    def __repr__(self):
        return f"Node(index={self.index}, value={self.value})"

def print_tree_preorder(node, indent=""):
    """Pomocnicza funkcja do wizualizacji zrekonstruowanego drzewa."""
    if node is None:
        return
    print(f"{indent}{node}")
    print_tree_preorder(node.left, indent + "  L: ")
    print_tree_preorder(node.right, indent + "  R: ")

def main():
    # Słownik do przechowywania węzłów wg ich indeksów
    nodes = {}

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # Umożliwia ponowne użycie adresu
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        s.bind((HOST, PORT))
        s.listen()
        print(f"Serwer nasłuchuje na {HOST}:{PORT}...")

        conn, addr = s.accept()
        with conn:
            print(f"Połączono z {addr}")

            # Odbieramy dane, dopóki klient nie zamknie połączenia
            while True:
                data = conn.recv(PACKET_SIZE)
                if not data:
                    # Klient zamknął połączenie
                    break

                # Upewniamy się, że odebraliśmy pełny pakiet
                if len(data) == PACKET_SIZE:
                    # Rozpakowujemy dane
                    index, value = struct.unpack(PACKET_FORMAT, data)
                    print(f"Odebrano dane: index={index}, value={value}")

                    # Tworzymy i przechowujemy nowy węzeł
                    nodes[index] = Node(value, index)
                else:
                    print(f"Odebrano niekompletny pakiet! ({len(data)} bajtów)")

    print("\nKlient zakończył połączenie. Rozpoczynam rekonstrukcję drzewa...")

    # --- Rekonstrukcja drzewa ---
    if not nodes:
        print("Nie odebrano żadnych węzłów.")
        return

    # Łączymy węzły na podstawie ich indeksów
    for index, node in nodes.items():
        left_index = 2 * index + 1
        right_index = 2 * index + 2

        if left_index in nodes:
            node.left = nodes[left_index]
        if right_index in nodes:
            node.right = nodes[right_index]

    # Korzeń zawsze ma indeks 0
    root = nodes.get(0)

    if root:
        print("\n--- Zrekonstruowane Drzewo (w kolejności Pre-order) ---")
        print_tree_preorder(root)
    else:
        print("BŁĄD: Nie znaleziono korzenia (indeks 0)!")

if __name__ == "__main__":
    main()