#!/usr/bin/env python3
"""pingstat.py <label> <ping-output-file> <sent> - median/p95/p99/jitter/loss from `ping` output (DUP-aware)."""
import re, statistics, sys

lines = [l for l in open(sys.argv[2]) if "time=" in l]
dups = sum("DUP!" in l for l in lines)
rtt = [float(re.search(r"time=([\d.]+)", l).group(1)) for l in lines if "DUP!" not in l]
sent = int(sys.argv[3])
r = sorted(rtt)
q = lambda p: r[min(len(r) - 1, int(p * len(r)))]
jit = statistics.mean(abs(a - b) for a, b in zip(rtt, rtt[1:])) if len(rtt) > 1 else 0
print(f"{sys.argv[1]:<18} median {q(.5):6.1f}  p95 {q(.95):6.1f}  p99 {q(.99):6.1f}  "
      f"max {r[-1]:6.1f}  jitter {jit:5.1f} ms  loss {100 * (sent - len(rtt)) / sent:4.1f}%  dups {dups}")
