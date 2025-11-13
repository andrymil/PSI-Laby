import socket
import struct

PACKET_FORMAT = '!ii'
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)

HOST = "0.0.0.0"
PORT = 8888


class Node:
    """Simple class to represent a tree node."""
    def __init__(self, value, index):
        self.value = value
        self.index = index
        self.left = None
        self.right = None

    def __repr__(self):
        return f"Node(index={self.index}, value={self.value})"


def print_tree_preorder(node, indent=""):
    """Helper function to visualize the reconstructed tree."""
    if node is None:
        return
    print(f"{indent}{node}")
    print_tree_preorder(node.left, indent + "  L: ")
    print_tree_preorder(node.right, indent + "  R: ")


def main():
    nodes = {}
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            s.bind((HOST, PORT))
        except socket.error as e:
            print(f"Error during bind: {e}")
            return

        s.listen()
        print(f"Server listening on {HOST}:{PORT}...")

        conn, addr = s.accept()
        with conn:
            print(f"Connected by {addr}")
            while True:
                data = conn.recv(PACKET_SIZE)
                if not data:
                    break

                if len(data) == PACKET_SIZE:
                    index, value = struct.unpack(PACKET_FORMAT, data)
                    print(f"Received data: index={index}, value={value}")
                    nodes[index] = Node(value, index)
                else:
                    print(f"Received incomplete packet! ({len(data)} bytes)")

    print("\nClient disconnected. Starting tree reconstruction...")

    if not nodes:
        print("No nodes were received.")
        return

    for index, node in nodes.items():
        left_index = 2 * index + 1
        right_index = 2 * index + 2
        if left_index in nodes:
            node.left = nodes[left_index]
        if right_index in nodes:
            node.right = nodes[right_index]

    root = nodes.get(0)
    if root:
        print("\n--- Reconstructed Tree (Pre-order traversal) ---")
        print_tree_preorder(root)
    else:
        print("ERROR: Root node (index 0) not found!")


if __name__ == "__main__":
    main()
