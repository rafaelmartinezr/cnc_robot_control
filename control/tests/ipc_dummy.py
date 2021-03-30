import socket
import time
from os import path
import struct

def connect_to_c():
    socket_address = '/home/nvidia/pef_pr21/sock_bf'

    # Establish communication with C process
    new_socket = socket.socket(family=socket.AF_UNIX, type=socket.SOCK_STREAM, proto=0)

    while(not path.exists(socket_address)):
        time.sleep(1)

    try:
        new_socket.connect(socket_address)
    except OSError as err:
        print("Error connecting to motors process - " + str(err))
        exit()

    return new_socket

def get_motor_pos(s):
    command = 4

    n_bytes = 1
    send_bytes = bytearray(n_bytes + 1)

    struct.pack_into('c',send_bytes,0,bytes([n_bytes]))
    struct.pack_into('c',send_bytes,1,bytes([command]))

    s.send(send_bytes)

    recv_data = s.recv(256)

    return recv_data

def decode_position(recv_data):
    n_bytes = struct.unpack('c',recv_data[0:1])[0]
    command = struct.unpack('c',recv_data[1:2])[0]

    position = struct.unpack('d',recv_data[2:10])[0]

    sec = struct.unpack('q',recv_data[10:18])[0]
    nsec = struct.unpack('q',recv_data[18:26])[0]
    time_ns = sec*10^9 + nsec

    return position,time_ns

def main():
    sock = connect_to_c()

    while True:
        time.sleep(0.1)
        pos_data = get_motor_pos(sock)
        pos, _ = decode_position(pos_data)

if __name__ == '__main__':
    main()
