#!/bin/sh
# bench.sh - reproduce the README numbers from a computer on the same LAN.
#
#   VITA_IP=<vita> HOST_IP=<this computer> tools/bench/bench.sh <label> [tcp|udp|wake|all]
#
# tcp : 10 s TCP downlink into RAM on the Vita (512 KB window), no memory card
# udp : 10 s paced UDP blast at 100 Mbps; the Vita counts what arrives = link capacity
# wake: 30 pings 2 s apart - the radio gets to doze between them, so this shows
#       power-save wake latency (100 ms pings keep the radio awake and hide it)
#
# Needs the plugin running on the Vita, python3 and nc here, and the Vita's
# vitacompanion FTP (:1337) to read the result from the plugin log. Switch
# modes first, e.g.:  tools/bench/vcmd.py $VITA_IP "profile maxperf"  then
# "confirm" once the link is back (same for "ht40 on").
set -e
: "${VITA_IP:?set VITA_IP}"; : "${HOST_IP:?set HOST_IP (this computer)}"
D=$(cd "$(dirname "$0")" && pwd); L=${1:-run}; WHAT=${2:-all}
LOG="ftp://$VITA_IP:1337/ur0:/data/wifiturbo/wifiturbo.log"
PORT=$(( 6000 + $(date +%s) % 2000 ))

tcp() {
  nc -l "$PORT" < /dev/zero > /dev/null 2>&1 & NC=$!; sleep 1
  python3 "$D/vcmd.py" "$VITA_IP" "speedtest $HOST_IP $PORT 10 512" > /dev/null
  kill $NC 2>/dev/null || true; sleep 1
  curl -s "$LOG" | grep 'speedtest:' | tail -1 | awk -v l="$L" '{printf "%-18s TCP  %.1f Mbps\n", l, $4*8/10/1e6}'
}
udp() {
  python3 "$D/vcmd.py" "$VITA_IP" "udpsink 5400 10" > /dev/null & V=$!; sleep 1.5
  python3 "$D/udpsend.py" "$VITA_IP" 5400 100 11 > /dev/null; wait $V; sleep 1
  curl -s "$LOG" | grep 'udpsink:' | tail -1 | awk -v l="$L" '{b=$4;u=$10; printf "%-18s UDP  %.1f Mbps\n", l, b*8/u}'
}
wake() {
  T=$(mktemp); ping -c 30 -i 2 "$VITA_IP" > "$T" 2>&1 || true
  python3 "$D/pingstat.py" "$L wake" "$T" 30; rm -f "$T"
}
case "$WHAT" in tcp) tcp ;; udp) udp ;; wake) wake ;; all) tcp; udp; wake ;; esac
