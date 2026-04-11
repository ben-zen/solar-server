# Solar Site: an art installation

This is the basis of a solar-powered web server, intended for deploying at
events as a reminder of how little infrastructure is actually needed to
provide a website. Inspired by
[LOW←TECH MAGAZINE's solar website](https://solar.lowtechmagazine.com/about/the-solar-website/).

## Components

### Website (`website/`)

A [Hugo](https://gohugo.io/) static site using a custom theme called
**northwoods**. The theme displays a live status bar (weather, battery
voltage, uptime) that is driven entirely by CSS — no JavaScript needed for
the data display.

### Logger (`logger/`)

A C++20 binary that collects system uptime and weather data from the
[NWS API](https://www.weather.gov/documentation/services-web-api), then
writes a `status.css` file consumed by the Hugo theme. A systemd timer runs
the logger every 5 minutes to keep the status bar current.

See [`logger/README.md`](logger/README.md) for build and deployment details.

### Guestbook (`guestbook/`)

A C++20 CGI binary served by lighttpd. Visitors submit messages through an
HTML form; entries are URL-decoded, formatted with JSON front matter, and
appended to a logbook file.

See [`guestbook/README.md`](guestbook/README.md) for details.

### Vendored Headers (`include_ext/`)

Header-only libraries shared by the C++ components:

- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 — JSON parsing
- [p-ranav/argparse](https://github.com/p-ranav/argparse) — CLI argument parsing

## Building

Both C++ components use [Meson](https://mesonbuild.com/) and require Clang:

```sh
# Logger
cd logger
CC=clang CXX=clang++ meson setup build src
meson compile -C build

# Guestbook
cd guestbook
CC=clang CXX=clang++ meson setup build src
meson compile -C build

# Website
cd website
hugo
```

## Dependencies

### Armbian 12

* pkg-config
* clang16, clang++16
* libfmt9, libfmt-dev, libfmt-doc†
* libcurl4, libcurl4-openssl-dev, libcurl4-doc†

† Documentation packages are optional.

### Fedora

* `clang`, `meson`, `pkg-config`
* `fmt`, `fmt-devel`, `fmt-doc`†
* `libcurl`, `libcurl-devel`

† Documentation packages are optional. On Fedora, `libcurl-devel` already
includes API docs; `fmt-doc` is a separate package.

## Deployment

The intended deployment layout on the target device:

```
/srv/logger/             → logger binary + update-status.sh
/www/solar-site/         → Hugo-generated static site + CSS
/usr/lib/cgi-bin/        → guestbook CGI binary
/etc/systemd/system/     → update-status.service + update-status.timer
```

### lighttpd

Install lighttpd and enable CGI support:

```sh
apt install lighttpd
lighttpd-enable-mod cgi
```

Add the following to `/etc/lighttpd/conf-enabled/10-cgi.conf` (or verify it
exists after enabling the module):

```lighttpd
server.modules += ( "mod_cgi" )

cgi.assign = ( "" => "" )

alias.url += ( "/cgi-bin/" => "/usr/lib/cgi-bin/" )
```

Set the document root to the Hugo output directory in
`/etc/lighttpd/lighttpd.conf`:

```lighttpd
server.document-root = "/www/solar-site"
```

Set the `LOGBOOK` environment variable for the guestbook CGI so it knows
where to write entries:

```lighttpd
cgi.execute-x-only = "disable"
setenv.add-environment = ( "LOGBOOK" => "/www/solar-site/guestbook" )
```

Then restart lighttpd:

```sh
systemctl restart lighttpd
```

### systemd timer

Enable the status updater with:

```sh
systemctl daemon-reload
systemctl enable --now update-status.timer
```

## License

MIT

