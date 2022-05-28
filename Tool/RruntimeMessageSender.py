import socket

def SendMessage(message):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.connect((socket.gethostname(), 12345))
        s.send(message.encode())
    except socket.error as msg:
        print(msg)