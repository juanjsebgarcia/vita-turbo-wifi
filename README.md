<div align="center">

# ⚡ Wi-Fi Turbo for PS Vita

**The PS Vita's Wi-Fi, fixed: faster, quicker to respond, and it reconnects by itself.**

A taiHEN plugin (kernel + user module) for HENkaku/Ensō on firmware 3.60–3.68.

![Headline figures](assets/headline.png)

</div>

> [!IMPORTANT]
> **Firmware 3.60 is tested. 3.65 and 3.68 are in alpha** (from
> [v0.2.0-alpha](https://github.com/juanjsebgarcia/vita-turbo-wifi/releases)):
> they should work, but haven't been confirmed on a console yet. If you try
> it, please [open an issue](https://github.com/juanjsebgarcia/vita-turbo-wifi/issues)
> with your logs. Other firmware versions are not supported: don't install it
> there.

---

## Why

The Vita's Wi-Fi works, but Sony set it up for 2011:

- **It sleeps between packets.** Power save is on by default, so the first
  packet after any pause waits for the radio to wake up: **~100 ms, often 200+**.
  You feel that in game streaming and remote shells.
- **It uses half the channel.** The radio handles 40 MHz channels, but the
  driver only ever asks for 20 MHz, which caps the link at 72 Mbps.
- **It gives up at boot.** If the auto-join misses your network once, the Vita
  just sits there offline until you open Settings and tap Connect.
- **It gets dropped when idle.** Many access points disconnect quiet clients
  after a few minutes, and the Vita doesn't always come back.

Wi-Fi Turbo fixes each of these, measured on real hardware.

## Headline figures

Stock means Sony's defaults (power save on, 20 MHz). Turbo means the
**Max performance** profile plus **40 MHz**. Same Vita, same access point,
modes interleaved so drifting radio conditions don't favour either side.

| | Stock | Turbo | Change |
|---|---|---|---|
| UDP throughput (link capacity) | 42.8 Mbps | **62.5 Mbps** | **+46%** |
| TCP throughput | 33.9 Mbps | **40.6 Mbps** | **+20%** |
| Wake-up latency, median | ~99 ms | **7.4 ms** | **13× faster** |
| Wake-up latency, p95 | ~205 ms | **~20 ms** | **~10× faster** |
| Link rate reported by the AP | 72 Mbps | **150 Mbps** | **2×** |
| Back online after a cold boot | waits for you | **~30 s, hands-free** | |
| Back online after a drop | waits for you | **~3 s** | |

Throughput is the average of 4 interleaved runs per mode, 10 s each, received
straight into RAM on the Vita (no memory card in the path). Wake-up latency is
two runs of 30 pings sent 2 s apart, which gives the radio time to doze; that's the
situation where power save hurts. See [Measuring it yourself](#measuring-it-yourself).

## Features

- 🚀 **40 MHz channels (HT40).** Unlocks the wide channel the radio already
  supports. The access point reports the Vita at **150 Mbps** instead of 72.
  Best on a quiet channel; see the [tips](#tips-for-the-best-wi-fi-performance).
- ⚡ **Power profiles.** *Max performance* turns power save off (13× faster
  wake-up), *Balanced* is Sony's stock behaviour, and *Battery saver* sleeps
  through more beacons for longer battery life.
- 🔌 **Auto-connect at boot.** If the Vita's own auto-join gives up, Wi-Fi
  Turbo triggers the system's own "connect to my saved networks", so the Vita
  is online about 30 s after power-on with no input.
- 🔁 **Auto-reconnect on drop.** Lost the link? It comes back in about 3 s.
- 💓 **Idle keep-alive.** A tiny heartbeat to your router every 45 s, so access
  points don't disconnect a quiet Vita.
- 🔋 **Keep Wi-Fi on.** Stops the Vita dropping into standby (deep sleep), so
  downloads and remote sessions don't die. The screen still dims and turns off
  as normal, but while this is on **the power button only turns the screen
  off**: the Vita stays awake and connected. Turn it off when you want the
  Vita to sleep, or to save battery.
- 📡 **Live HUD.** An on-screen overlay with network, signal (dBm and %),
  **SNR**, channel, access point and active profile, read live from the
  driver.
- 🧭 **Smart Mesh Roaming**. For mesh and multi-AP setups:
  on a weak signal it rescans every channel and joins the strongest access
  point of the same network.
- 🎛️ **Native Settings page.** Everything lives in *Settings → Network → Wi-Fi
  Turbo*, in your system language.
- 🛡️ **Safe by design.** Every patch checks its target first, the driver is
  never hooked during boot, and any change is undone by switching it back in
  Settings, even with Wi-Fi down.
  See [Safety](#safety).

## Use cases

### 📦 Downloading large files
Moving files onto the Vita over Wi-Fi (for example with VitaShell's FTP
server) runs over TCP, which was **about 20% faster** with Turbo in our tests.
How much you gain depends on the app's receive buffer, since the Vita's own TCP
stack tops out around 40 Mbps. *Keep Wi-Fi on* also stops long transfers dying
when the screen sleeps, and auto-reconnect picks up again after a drop.

### 🎮 Streaming games with Moonlight
Game streaming is all about steady latency. Power save adds 80–400 ms
whenever traffic pauses; *Max performance* removes that, and 40 MHz halves
the airtime of every frame. Pair it with
[vita-moonlight](https://github.com/xyzz/vita-moonlight)
([my fork](https://github.com/juanjsebgarcia/vita-moonlight)).

### 🛠️ Wi-Fi driven development
Building homebrew with
[vitacompanion](https://github.com/devnoname120/vitacompanion)
([my fork](https://github.com/juanjsebgarcia/vitacompanion)), with its FTP
uploads, remote launch, input injection and reboots, means your dev loop runs
over Wi-Fi. Wi-Fi Turbo keeps that loop alive: it reconnects after reboots and
drops on its own, keeps the console awake, and makes every round trip snappier.

## Screenshots

| Live HUD | Settings page |
|---|---|
| ![HUD overlay](assets/hud.jpg) | ![Wi-Fi Turbo settings](assets/settings.jpg) |

| Power profiles | Built into Settings → Network |
|---|---|
| ![Power profiles](assets/settings-profile.jpg) | ![Wi-Fi Turbo in the Network menu](assets/settings-entry.jpg) |

*Network names and access point addresses are redacted in these screenshots.*

## Options

### Settings → Network → Wi-Fi Turbo

| Option | What it does | Default |
|---|---|---|
| **Keep Wi-Fi on** | Prevents automatic standby (deep sleep) and sends a small idle keep-alive, so the console and the link stay up. The screen still turns off, but the power button only turns the screen off rather than sleeping the Vita. | On |
| **Wi-Fi HUD overlay** | Shows the live overlay (network, signal, SNR, channel, AP, profile). | Off |
| **Auto-reconnect on drop** | Reconnects automatically whenever the link is lost, including at boot when the Vita's own auto-join gives up. | On |
| **Smart Mesh Roaming** | On sustained weak signal, rescans and joins the strongest AP of the same network. | Off |
| **40 MHz** | Lets the Vita negotiate 40 MHz channels. Needs an access point with 2.4 GHz set to 40 MHz. | Off |
| **Power profile** | *Max performance*, *Balanced* or *Battery saver* (below). | Balanced |
| **Developer mode (remote control)** | Opens the remote-control port 1339 on your network, for scripting and testing (below). | Off |

### Power profiles

| Profile | Radio behaviour | Wake-up latency (median) | Best for |
|---|---|---|---|
| **Max performance** | Power save off | **~8 ms** | Streaming, remote play, development |
| **Balanced** | Sony's stock power save | ~88 ms | Everyday use |
| **Battery saver** | Power save, wakes for every 3rd beacon | ~194 ms | Long idle sessions on battery |

Changing the profile or the 40 MHz option briefly reconnects Wi-Fi (about 5 s)
so the new settings take effect. All settings are remembered and restored at
boot, before the Vita first joins your network. To undo a change, just switch
it back in Settings; that works even if Wi-Fi is down, so a setting your
network doesn't like can never leave you stuck.

### Tips for the best Wi-Fi performance

- **Turn Bluetooth off when you don't need it.** The Vita's Wi-Fi and
  Bluetooth share one 2.4 GHz radio and take turns on the air, so Bluetooth
  activity costs Wi-Fi airtime and adds latency. All the figures in this
  README were measured with Bluetooth off.
- **Set your access point's 2.4 GHz radio to 40 MHz** if you want the
  *40 MHz* option to have any effect; the Vita can only use what the AP offers.
- **In a congested 2.4 GHz area, 40 MHz can be slower.** The band only has room
  for three non-overlapping 20 MHz channels, and a 40 MHz channel takes up two
  of them. With lots of neighbouring networks or other 2.4 GHz devices nearby,
  the wider channel picks up more interference, so throughput and latency can
  get worse instead of better. If that happens, turn *40 MHz* off.
- **Use *Max performance* for streaming and remote play**, where steady
  latency matters more than battery life.

### Remote control (port 1339)

Turn on **Developer mode (remote control)**, the last entry on the Wi-Fi Turbo
page, and the plugin listens on TCP port **1339** on your LAN, handy for
scripting and testing. It's **off by default**: with it off, nothing listens.
It sits alongside [vitacompanion](https://github.com/devnoname120/vitacompanion)
([my fork](https://github.com/juanjsebgarcia/vitacompanion)),
which uses ports 1337 and 1338. Send one command per connection, for example
`tools/bench/vcmd.py <vita-ip> status`.

| Command | Description |
|---|---|
| `status` | Network, state, signal, SNR, noise floor and channel |
| `profile maxperf \| balanced \| maxbatt` | Switch power profile (then `confirm`) |
| `ht40 on \| off` | Toggle 40 MHz (then `confirm`) |
| `confirm` / `revert` | Keep or undo a pending remote change |
| `keepwifi on \| off`, `hud on \| off`, `autoreconnect on \| off` | The Settings toggles |
| `devmode off` | Close the remote-control port (turn it back on in Settings) |
| `roam on \| off`, `roam now`, `roam status` | Smart Mesh Roaming |
| `icm` | Ask the system to connect to your saved networks now |
| `speedtest <host-ip> <port> [secs] [rcvbuf_kb]` | TCP download into RAM from a host on your LAN |
| `udpsink <port> [secs]` | Count a UDP stream: raw link capacity and loss |
| `log dump`, `log level <level>` | Read or tune the plugin log |
| `help` | List all commands |

Profile and 40 MHz changes sent over port 1339 revert by themselves after 45 s
unless you send `confirm`. A remote change that broke the link would otherwise
leave you no way to reach the Vita and undo it. Changes made in Settings don't
need confirming.

> Port 1339 is unauthenticated, like vitacompanion's ports. Use it on networks
> you trust.

## Languages

The Settings page follows your system language. Supported:

Japanese, English (US), English (UK), French, Spanish, German, Italian, Dutch,
Portuguese (Portugal), Portuguese (Brazil), Russian, Korean,
Chinese (Traditional), Chinese (Simplified), Finnish, Swedish, Danish,
Norwegian, Polish and Turkish.

The HUD uses a compact ASCII font, so it's translated for the Latin-script
languages and shows English for the others.

## Disclaimer

This software is provided **"as is", without warranty of any kind**, express or
implied, including but not limited to the warranties of merchantability,
fitness for a particular purpose and non-infringement. It modifies how system
software on your console behaves. **You use it entirely at your own risk.** The
author is not responsible for any damage, data loss, bricked or unusable
systems, lost warranty, or any other harm resulting from installing or using
it. Keep backups, and make sure you know how to boot without plugins (hold
**L** at boot) before you install. See the [MIT License](LICENSE) for the full
terms.

## Install

**Requirements:** a PS Vita or PS TV on firmware **3.60** (tested), **3.65 or
3.68** (alpha, from v0.2.0-alpha), with HENkaku or Ensō (taiHEN). Other
firmware versions are not supported. For the 40 MHz
option, an access point with its 2.4 GHz radio set to 40 MHz channel width.

1. Download `wifiturbo_kernel.skprx` and `wifiturbo.suprx` from the latest
   [release](https://github.com/juanjsebgarcia/vita-turbo-wifi/releases).
2. Copy both files to `ur0:tai/` on the Vita, for example with VitaShell over
   FTP or USB.
3. Open `ur0:tai/config.txt` and add the lines below. If a `*KERNEL`, `*main`
   or `*NPXS10015` section already exists, add the line under that section
   instead of creating a second one.

   ```
   *KERNEL
   ur0:tai/wifiturbo_kernel.skprx

   *main
   ur0:tai/wifiturbo.suprx

   *NPXS10015
   ur0:tai/wifiturbo.suprx
   ```

   (`*NPXS10015` adds the Settings page.)

   > If you also have a `ux0:tai/config.txt`, taiHEN reads that one instead
   > and ignores `ur0:tai/config.txt`. Add the lines to whichever file your
   > other plugins are in, keeping the `ur0:tai/...` paths.

4. Reboot. Find the options under *Settings → Network → Wi-Fi Turbo*.

**To update:** replace both files and reboot. **To uninstall:** remove the
lines from `config.txt`, reboot, then delete the two files and
`ur0:data/wifiturbo/`.

**If anything goes wrong:** hold **L** while the Vita boots to skip all
plugins, then remove the lines from `config.txt`.

## Build

```sh
make                       # uses $VITASDK if set, otherwise a pinned VitaSDK Docker image
make deploy VITA_IP=<vita> # upload to ur0:tai/ over FTP and reboot (needs vitacompanion)
make log    VITA_IP=<vita> # fetch the plugin logs
make config                # print the config.txt lines
```

Output: `build/wifiturbo_kernel.skprx` and `build/wifiturbo.suprx`.

## Measuring it yourself

`tools/bench/` has the scripts behind the numbers above. Run them from a
computer on the same network:

```sh
export VITA_IP=<vita> HOST_IP=<this computer>
tools/bench/vcmd.py $VITA_IP "profile maxperf"   # link drops ~5 s, then:
tools/bench/vcmd.py $VITA_IP confirm
tools/bench/bench.sh turbo all                    # TCP, UDP and wake-up latency
```

- **TCP:** the Vita downloads from your computer into RAM for 10 s, with a
  512 KB receive window. No memory card is involved.
- **UDP:** your computer sends a paced 100 Mbps stream; the Vita counts what
  arrives. That's the link's real capacity.
- **Wake-up latency:** 30 pings, 2 s apart. Pings 100 ms apart keep the radio
  awake and would hide power save.

Alternate stock and turbo runs to cancel out radio drift. The Vita's own TCP
stack tops out around 40 Mbps, so UDP shows the 40 MHz gain best.

## Safety

This plugin changes how a kernel driver behaves, so it's built to fail safe:

- **Easy undo.** Every change is undone by switching it back in Settings,
  which works with Wi-Fi down.
- **Checked patches.** Each driver patch verifies the original instructions
  first and refuses to apply on a mismatch, such as another firmware or a
  conflicting plugin. The one call into the system shell is located by a
  signature that must match exactly once. A feature whose check fails logs why
  and turns itself off.
- **Gentle boot.** At boot the plugin restores your settings with a few
  checked byte patches and a driver flag, before the first Wi-Fi join. It never
  hooks the driver while the console is starting up.
- **Small hooks.** The few driver hooks are tiny, only active when needed, and
  stay off the driver's busy command path.
- **Escape hatch.** Holding L at boot skips every plugin.

## Contributing

Bug reports, measurements from different access points, translations and code
are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md). If you attach logs, redact
your network name and access point MAC addresses first.

## Credits

Built on [taiHEN](https://github.com/yifanlu/taiHEN),
[VitaSDK](https://vitasdk.org) and
[vitacompanion](https://github.com/devnoname120/vitacompanion)
([my fork](https://github.com/juanjsebgarcia/vitacompanion)). Thanks to
the PS Vita homebrew community.

## License

[MIT](LICENSE) © 2026 Juan Garcia Alvite
