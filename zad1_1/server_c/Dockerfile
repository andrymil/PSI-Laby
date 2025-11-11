FROM gcc:4.9
COPY . /Server
WORKDIR /Server/
RUN gcc -o server server.c
EXPOSE 8888
CMD ["./server", "8888"]
