---
layout: page
permalink: /running
title: Running
---

## Overview

On startup myMPD does the following:

- Check and create `cachedir` and `workdir` directories.
  - `cachedir` and `workdir` must exist, if not started as root.
- Reads environment at first startup.
- Binds to the configured http and ssl port.
- Dropping privileges, if started as root.
- Check and create the directories inside `cachedir` and `workdir`.

- **Note:** It is not supported to run myMPD as root. If started as root, myMPD drops privileges to the configured user (default mympd).

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

The default myMPD service unit uses the `DynamicUser=` directive, therefore no static mympd user is created. If you want to change the group membership of this dynamic user, you must add an override.

**Example: add the mympd user to the music group**

```
mkdir /etc/systemd/system/mympd.service.d
echo -e '[Service]\nSupplementaryGroups=music' > /etc/systemd/system/mympd.service.d/music-group.conf
```

- **Note:** The default systemd service unit supports only systemd v235 and above.

### Openrc usage

You must enable and start the service manually. Use `rc-update add mympd` to enable myMPD at startup and `rc-service mympd start` to start myMPD now.

myMPD logs to syslog to facility `daemon`, you can see the live logs with `tail -f /var/log/messages`.

## Manual startup

To start myMPD in the actual console session: `mympd` (myMPD keeps in foreground and logs to the console, press CTRL+C to stop myMPD)

If you use a distribution with systemd (without a static mympd user):

```
systemd-run -t -p DynamicUser=yes -p StateDirectory=mympd -p CacheDirectory=mympd /usr/bin/mympd
```

Description of [Commandline-Options]({{ site.baseurl }}/configuration/).

## Docker

Goto [Docker]({{ site.baseurl }}/installation/docker)

## myMPD configuration

You can configure some basic options of myMPD via startup options or environment variables.

- [Configuration]({{ site.baseurl }}/configuration/)
