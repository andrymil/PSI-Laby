# Format nagłówka: Type (1B) | Length (4B) -> Struct format '!BI'
HEADER_FORMAT = "!BI"
HEADER_SIZE = 5

# Typy wiadomości (Outer)
MSG_TYPE_CLIENT_HELLO = 0x01
MSG_TYPE_SERVER_HELLO = 0x02
MSG_TYPE_SECURE_MESSAGE = 0x03

# Flagi wewnętrzne (Inner - wewnątrz szyfrogramu)
INNER_FLAG_DATA = 0x01
INNER_FLAG_END_SESSION = 0x02

# Rozmiar MAC (SHA256)
MAC_SIZE = 32

# Parametry DH (4 bajty = max 4294967295)
DEFAULT_P = 4294967291  # Największa liczba pierwsza 32-bitowa
DEFAULT_G = 2
