FROM gcc:4.9
COPY . /Client
WORKDIR /Client/
RUN gcc -o client client.c
CMD ["./client"]