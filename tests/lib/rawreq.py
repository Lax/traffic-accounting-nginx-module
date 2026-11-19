#!/usr/bin/env python3
"""Send a raw HTTP request (bytes from stdin) to host:port, print response.

Used for requests that curl cannot express safely: HTTP/1.0 without a Host
header, header values containing '|', DEL or control bytes in the URI query.
"""
import socket
import sys


def main():
    host, port = sys.argv[1], int(sys.argv[2])
    data = sys.stdin.buffer.read()
    s = socket.create_connection((host, port), timeout=10)
    s.settimeout(10)
    s.sendall(data)
    chunks = []
    try:
        while True:
            b = s.recv(65536)
            if not b:
                break
            chunks.append(b)
    except socket.timeout:
        pass
    s.close()
    sys.stdout.buffer.write(b"".join(chunks))


if __name__ == "__main__":
    main()
