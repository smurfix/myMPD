---
layout: page
permalink: /running
title: Running
---

## Startup script

The installation process installs a LSB compatible startup script / systemd unit for myMPD.

| INIT SYSTEM | SCRIPT |
| ----------- | ------ |
| open-rc | `/etc/init.d/mympd` |
| systemd | `/usr/lib/systemd/system/mympd` or `/lib/systemd/system/mympd` |
| sysVinit | `/etc/init.d/mympd` |
{: .table .table-sm}

### Systemd usage

You must enable and start the service manually. Use `systemctl enable mympd` to enable myMPD at startup and `systemctl start mympd` to start myMPD now.

myMPD logs to STDERR, you can see the live logs with `journalctl -fu mympd`.

### Openrc usage

You must enable and start the service manually. Use `rc-update add mympd` to enable myMPD at startup and `rc-service mympd start` to start myMPD now.

myMPD logs to syslog to facility `daemon`, you can see the live logs with `tail -f /var/log/messages`.

## Manual startup

To start myMPD in the actual console session: `mympd` (myMPD keeps in foreground and logs to the console, press CTRL+C to stop myMPD)

Description of [Commandline-Options]({{ site.baseurl }}/configuration/).

## Docker

Goto [Docker]({{ site.baseurl }}/installation/docker)
