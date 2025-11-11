import socket
import sys
import io

HOST = 'z34_udp_server'  # The server's hostname or IP address
PORT=8888
size = 1
binary_stream = io.BytesIO()

print("Will send to ", HOST, ":", PORT)

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
  while True:
    # buffer = str( size )
    binary_stream.write("Hello, world!\n".encode('ascii'))
    binary_stream.seek(0)
    stream_data = binary_stream.read()
    # print( "Sending buffer size= ", size, "data= ", stream_data  )
    print( "Sending buffer size= ", size)

    s.sendto( stream_data, (HOST, PORT))
    data = s.recv(size)
    # print('Received', repr(data))
    size = size * 2


print('Client finished.')
