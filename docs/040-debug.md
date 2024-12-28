---
title: Debug
---

Tips to debug problems with myMPD. Try these steps and reproduce the error.

## myMPD logging

### Start myMPD manually

- Stop myMPD
- Set loglevel to debug: `export MYMPD_LOGLEVEL=7`
- Start it in the console `mympd` (same user as the service runs or as root)
- Debug output is printed to the console
- Press Ctrl + C to abort
- Reset loglevel: `unset MYMPD_LOGLEVEL`

!!! note
    Use [systemd-run](030-running.md#manual-startup), if you use a distribution with systemd.

### Get logs from running myMPD

You can set the log level in the Maintenance dialog to `debug` and get the output from your logging service.

- Systemd: `journalctl -fu mympd`
- Syslog: `tail -f /var/log/messages` (or the logfile for facility `daemon`)

## Webbrowser logging

- Open the javascript console or webconsole
- Clear the browsercache and the application cache
- Reload the webpage

## MPD logging

- Set `log_level "verbose"` in mpd.conf and restart mpd
- Look through the mpd log file for any errors

## Recording traffic between MPD and myMPD

### Local socket connection

MPD is listening on `/run/mpd/socket`. Point myMPD to `/run/mpd/socket-debug`.

```sh
socat -t100 -v UNIX-LISTEN:/run/mpd/socket-debug,mode=777,reuseaddr,fork UNIX-CONNECT:/run/mpd/socket
```

### TCP connection

MPD is listening on a tcp port, default is 6600.

```sh
tcpdump -nni any -vvv -s0 -x host <mpd host> and port <mpd port>
```

## myMPD debug build

- Build: `./build.sh debug`
- Run: `valgrind --leakcheck=full debug/bin/mympd`
