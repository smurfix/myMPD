---
title: Docker
---

The Docker images are based on [Alpine Linux](https://alpinelinux.org). They are published through the GitHub docker registry [ghcr.io](https://github.com/jcorporation?tab=packages).

There are two images:

- [mympd/mympd](https://github.com/users/jcorporation/packages/container/package/mympd%2Fmympd): the latest stable release
- [mympd/mympd-devel](https://github.com/users/jcorporation/packages/container/package/mympd%2Fmympd-devel): development version

Available architectures:

- x86-64 (amd64)
- arm64 (aarch64)
- armv7
- armv6

Use `docker pull ghcr.io/jcorporation/mympd/mympd:latest` to use the latest stable image.

## Usage

Starts the myMPD docker container:

- Runs the docker container with uid/gid 1000
- Disables SSL
- Listen on port 8080

### Volumes

| VOLUME | DESCRIPTION |
| ------ | ----------- |
| `/run/mpd/socket` | MPD listening socket. |
| `/run/mygpiod/socket` | Optional myGPIOd socket for GPIO scripting support. |
| `/docker/mympd/workdir` | myMPD working directory. Must exist and be writeable by uid 1000. |
| `/docker/mympd/cachedir` | myMPD cache directory. Must exist and be writeable by uid 1000. |
| `/var/lib/mpd/music/` | MPD music directory. Use the same path in the container to enable auto detection. |
| `/var/lib/mpd/playlists/` | MPD playlist directory. Use the same path in the container to enable auto detection. |

You must create the `cachedir` and `workdir` before starting the container.

### Docker Compose

Save this as `docker-compose.yml`:

```yaml
services:
  mympd:
    image: ghcr.io/jcorporation/mympd/mympd
    container_name: mympd
    ports:
      - 8080:8080
    user: 1000:1000
    environment:
      - UMASK_SET=022
      - MYMPD_SSL=false
      - MYMPD_HTTP_PORT=8080
    volumes:
      - /run/mpd:/run/mpd
      ## Optional for myGPIOd support
      ## - /run/mygpiod:/run/mygpiod
      - /docker/mympd/workdir:/var/lib/mympd
      - /docker/mympd/cachedir:/var/cache/mympd
      - /var/lib/mpd/music:/var/lib/mpd/music:ro
      - /var/lib/mpd/playlists:/var/lib/mpd/playlists:ro
    restart: unless-stopped
```

Setup: `docker-compose up -d`

### Docker CLI

```sh
docker run -d \
  --name=mympd \
  -p 8080:8080 \
  -u 1000:1000 \
  -e UMASK_SET=022 \
  -e MYMPD_SSL=false \
  -e MYMPD_HTTP_PORT=8080 \
  -v /run/mpd:/run/mpd \
  ## Optional for myGPIOd support
  ## -v /run/mygpiod:/run/mygpiod \
  -v /docker/mympd/workdir:/var/lib/mympd \
  -v /docker/mympd/cachedir:/var/cache/mympd \
  -v /var/lib/mpd/music:/var/lib/mpd/music:ro \
  -v /var/lib/mpd/playlists:/var/lib/mpd/playlists:ro \
  --restart unless-stopped \
  ghcr.io/jcorporation/mympd/mympd
```

### Logs

```sh
docker logs -f mympd
```

## myMPD configuration

You can configure some basic options of myMPD via startup options or environment variables.

- [Configuration](../020-configuration/index.md)
