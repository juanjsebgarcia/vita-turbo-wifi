#!/usr/bin/env python3
"""vcmd.py <vita-ip> "<command>" - send one command to the plugin's :1339 surface, print the reply."""
import socket, sys

ip, cmd = sys.argv[1], sys.argv[2]
s = socket.create_connection((ip, 1339), timeout=8)
s.sendall(cmd.encode() + b"\n")
data = b""
try:
    while True:
        chunk = s.recv(4096)
        if not chunk:
            break
        data += chunk
except OSError:
    pass
print(data.decode(errors="replace").strip())
