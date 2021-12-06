---
layout: page
permalink: /configuration/
title: Configuration
---

myMPD has no single configuration file. Most of the options are configureable through the settings dialog in the web ui.

## Command line options

You can set some basic options with command line options. All these options have sane default values and should not be changed for default usage.

The `workdir` option is useful if you want to run more then one instance of myMPD on the same host.

| OPTION | DESCRIPTION |
| ------ | ----------- |
| -c, --config | creates config and exits (default directory: `/var/lib/mympd/config/`) |
| -h, --help | displays this help |
| -u, --user `<username>`| username to drop privileges to (default: `mympd`) |
| -s, --syslog | enable syslog logging (facility: daemon) |
| -w, --workdir `<path>` | working directory (default: `/var/lib/mympd`) |
| -a, --cachedir `<path>` | cache directory (default: `/var/cache/mympd`) |
| -p, --pin | sets a pin for myMPD settings |
{: .table .table-sm }

- Setting a pin is only supported with compiled in ssl support

## Configuration files

At first startup (if there is no ẁorking directory) myMPD tries to autodetect the MPD connection and reads some environment variables.

After first startup all environment variables are ignored and the files in the directory `/var/lib/mympd/config/` should be edited.

| FILE | TYPE | ENVIRONMENT | DEFAULT | DESCRIPTION |
| ---- | ---- | ----------- | ------- | ----------- |
| acl | string | MYMPD_ACL | | ACL to access the myMPD webserver: [ACL]({{ site.baseurl }}/configuration/acl), allows all hosts in the default configuration |
| http_host | string | MYMPD_HTTP_HOST | 0.0.0.0 | IP address to listen on |
| http_port | number | MYMPD_HTTP_PORT | 80 | Port to listen on. Redirects to `ssl_port` if `ssl` is set to `true` |
| loglevel | number | MYMPD_LOGLEVEL | 5 | [Logging]({{ site.baseurl }}/configuration/logging) - this environment variable is always used |
| lualibs | string | MYMPD_LUALIBS | all | [Scripting]({{ site.baseurl }}/references/scripting) |
| scriptacl | string | MYMPD_SCRIPTACL | +127.0.0.1 | ACL to access the myMPD script backend: [ACL]({{ site.baseurl }}/configuration/acl), allows only local connections in the default configuration. The acl above must also grant access. |
| ssl | boolean | MYMPD_SSL | true | `true` = enables https, `false` = disables https |
| ssl_port | number | MYMPD_SSL_PORT | 443 | Port to listen to https traffic |
| ssl_san | string | MYMPD_SSL_SAN | | Additional SAN for certificate creation |
| custom_cert | boolean | MYMPD_CUSTOM_CERT | false | `true` = use custom ssl key and certificate |
| ssl_cert | string | MYMPD_SSL_CERT | | Path to custom ssl certificate file |
| ssl_key | string | MYMPD_SSL_KEY | | Path to custom ssl key file |
| pin_hash | string | N/A | | SHA256 hash of pin, create it with `mympd -p` |
{: .table .table-sm }

- More details on [SSL]({{ site.baseurl }}/configuration/ssl)

You can use `mympd -c` to create the initial configuration in the `/var/lib/mympd/config/` directory.

### MPD autodetection

myMPD tries to autodetect the mpd connection at first startup.

1. Searches for a valid `mpd.conf` file and reads all interesting settings
2. Uses the default MPD environment variables
3. Tries `/run/mpd/socket` and `/var/run/mpd/socket`

| ENVIRONMENT | DEFAULT | DESCRIPTION |
| ----------- | ------- | ----------- |
| MPD_HOST | `/run/mpd/socket` | MPD host or path to mpd socket |
| MPD_PORT | 6600 | MPD port |
{: .table .table-sm}

This is done after droping privileges to the mympd user.
