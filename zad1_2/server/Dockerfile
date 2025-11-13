FROM gcc:4.9
COPY . /Server
WORKDIR /Server/
# Kompilujemy nowy program server.c
RUN gcc -o server server.c
CMD ["./server"]