# Contributing

Thanks for helping make Wi-Fi on the PS Vita less painful. Bug reports,
measurements from your own setup, translations and code are all welcome.

## Ground rules

This plugin patches a live kernel driver on a console people care about, so a
few rules are non-negotiable:

- **Never strand the console.** Anything that can drop the link goes through
  the auto-revert harness (`user/safety.c`): apply, then revert automatically
  unless the user confirms. Reverts must work with the network down.
- **Keep boot gentle.** Boot may restore saved settings with checked byte
  patches and driver flags, but must never hook the driver or send chip
  commands while the console is starting up.
- **Verify before you patch.** Every instruction patch or hook checks the
  original bytes at the target first and refuses on a mismatch (another
  firmware, another plugin). See `kernel/ht40patch.c` for the pattern.
- **Keep hooks trivial.** No logging, locks or syscalls inside hot-path hooks;
  plain loads and stores, then continue.
- **No hard-coded networks.** No SSIDs, gateways, access points or personal
  data in code, logs or docs. Everything is discovered at runtime.

## Building

```sh
make            # VitaSDK if $VITASDK is set, otherwise the pinned Docker image
make clean
```

Fix any new warnings in the code you touch. The build must stay clean.

## Testing on a Vita

You need HENkaku/Ensō on firmware 3.60 and
[vitacompanion](https://github.com/devnoname120/vitacompanion)
([my fork](https://github.com/juanjsebgarcia/vitacompanion)), which provides
FTP on :1337 and commands on :1338.

Turn on *Developer mode (remote control)* on the Wi-Fi Turbo Settings page so
the plugin's port 1339 is open for the commands below.

```sh
make deploy VITA_IP=<vita>     # upload to ur0:tai/ and reboot
make log    VITA_IP=<vita>     # fetch the user and kernel logs
tools/bench/vcmd.py <vita> status
```

If a change misbehaves, **hold L while the Vita boots** to skip all plugins,
then fix `ur0:tai/config.txt` or the module over FTP.

When you report a result, include the firmware, your access point model, the
Wi-Fi channel and width, the relevant log lines, and how you measured. The
scripts in `tools/bench/` produce the numbers used in the README.

**Redact the logs before you post them.** They record your network name
(SSID), your access points' MAC addresses (BSSIDs, shown as
`aa:bb:cc:dd:ee:ff`) and sometimes IP addresses. Public Wi-Fi maps can turn a
BSSID into a rough location. Replace them with placeholders such as `<ssid>`,
`<ap1>`, `<ap2>`. Keep different access points distinct, because roaming
reports depend on telling them apart. Keep the signal levels, channels and
timings as they are.

## Translations

The Settings page strings live in `settings_app/wifiturbo_i18n.c`, one column
per system language, with English as the fallback. The HUD
(`kernel/hud_i18n.c`) is drawn with an ASCII bitmap font, so it only carries
Latin-script translations; other languages show English there.

## Pull requests

- One logical change per PR, with a clear description of what changed and how
  you tested it (on hardware if it touches the driver).
- Commit messages in the existing style: `wifiturbo: <what changed>`,
  `docs: <what changed>`.
- By contributing you agree that your contribution is licensed under the MIT
  licence of this project.
