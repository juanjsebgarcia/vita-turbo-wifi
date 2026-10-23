#!/usr/bin/env python3
"""udpsend.py <ip> <port> <mbps> <secs> - paced UDP blast, 1400-byte datagrams, u32 big-endian sequence."""
import socket, struct, sys, time

ip, port, mbps, secs = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 << 20)
size = 1400
pad = b"\0" * (size - 4)
interval = size * 8 / (mbps * 1e6)
t0 = time.perf_counter()
seq = 0
while time.perf_counter() - t0 < secs:
    due = int((time.perf_counter() - t0) / interval)
    while seq <= due:
        try:
            s.sendto(struct.pack(">I", seq) + pad, (ip, port))
            seq += 1
        except BlockingIOError:
            break
    time.sleep(0.0005)
print(f"sent {seq} datagrams ({seq * size * 8 / secs / 1e6:.1f} Mbps offered)")
