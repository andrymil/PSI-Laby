# Używamy lekkiego obrazu Debiana
FROM debian:bullseye-slim

# Instalujemy zależności:
# - gcc: Kompilator C
# - libc6-dev: Standardowe biblioteki C
# - iproute2: Kluczowe dla 'tc' do symulacji gubienia pakietów
# - libssl-dev: Wymagane do obliczania hasha SHA-256 (dla -lssl -lcrypto)
RUN apt-get update && \
    apt-get install -y gcc libc6-dev iproute2 libssl-dev && \
    rm -rf /var/lib/apt/lists/*

COPY . /Client
WORKDIR /Client/

# Kompilujemy program client.c i linkujemy biblioteki OpenSSL
# -lssl i -lcrypto są potrzebne dla funkcji SHA256_*
RUN gcc -o client client.c -lssl -lcrypto

# Domyślne polecenie (zostanie nadpisane przez docker-compose.yml,
# aby dodać 'tc', ale dobrze je tu mieć)
CMD ["./client"]