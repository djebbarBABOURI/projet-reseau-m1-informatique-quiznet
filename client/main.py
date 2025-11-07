import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.bind(("", 0))
sock.settimeout(2)

sock.sendto(b"looking for quiznet servers", ('255.255.255.255', 5555))

while True:
    try:
        data, addr = sock.recvfrom(1024)
        print("Received:", data.decode(), "from", addr)
    except socket.timeout:
        print("Finished receiving responses")
        break
