# Solar Server Logger

Drawing direct inspiration from the way [LOW←TECH MAGAZINE](https://solar.lowtechmagazine.com/power/) works, and how they have a power status page, I decided that since this is an art installation with very similar goals, it should have an update mechanism. However, where their solution is a totally realtime mechanism, I've opted for the slightly less realtime but also... less computational approach of updating a quasi-static CSS file. This binary is the generator 

## Building the logger

Build with [Just](https://github.com/casey/just) from the `logger/` directory:

```sh
cd logger
just build
```

Or from the repository root:

```sh
just logger build
```

### Manual build

#### Dependencies

#### Compiler, build system, etc.

- [Meson](https://mesonbuild.com/)
- Clang++ 16

To configure the build correctly, run `CC=clang CXX=clang++ meson setup build src` to ensure Meson uses Clang/++ instead of GCC.

#### Fedora

`libcurl`, `libcurl-devel`, `fmt`, `fmt-devel`

#### Debian

`libcurl4`, `libcurl4-dev`, `libfmt`, `libfmt-dev`

## Deploying the logger

Deploy with Just from the `logger/` directory (requires root):

```sh
cd logger
sudo just deploy
```

Or from the repository root:

```sh
sudo just logger deploy
```

This installs the logger binary and helper script to `/srv/logger/`, copies
the systemd units, and enables the timer.

### Manual deployment

The layout should be the following:

```
/srv/logger:
logger update-status.sh

/etc/systemd/system:
update-status.service update-status.timer
```

```sh
systemctl daemon-reload
systemctl enable --now update-status.timer
```

## Other works

### nlohmann's json.hpp

I'm using [nlohmann's json library](https://github.com/nlohmann/json) to handle json parsing.
