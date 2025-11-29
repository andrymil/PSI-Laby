FROM gcc:12

RUN apt-get update && \
    apt-get install -y iproute2 libssl-dev && \
    rm -rf /var/lib/apt/lists/*

COPY . /Client
WORKDIR /Client/

RUN gcc -o client client.c -lssl -lcrypto

CMD ["./client"]