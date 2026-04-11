# Copilot Instructions for solar-server

## Project Overview

Solar Server is a solar-powered web server art installation inspired by
[LOW←TECH MAGAZINE's solar website](https://solar.lowtechmagazine.com/about/the-solar-website/).
It is intended for deployment at events (e.g., ToorCamp) as a self-contained,
off-grid web server demonstrating how little infrastructure is needed to host
a website. The target hardware is a single-board computer (e.g., an Orange Pi)
running Armbian, powered by a solar panel and battery.

## Repository Structure

```
solar-server/
├── guestbook/          # CGI binary for visitor guestbook entries
│   ├── src/
│   │   ├── main.cc     # URL-encoding, form parsing, entry formatting
│   │   └── meson.build
│   ├── response.txt    # Sample CGI response for reference
│   └── README.md
├── logger/             # Status logger that generates a CSS file with live data
│   ├── src/
│   │   ├── main.cc     # Entry point; writes status.css
│   │   ├── curl.cc/hh  # libcurl RAII wrapper
│   │   ├── report.cc/hh   # System uptime + weather → status_report
│   │   ├── weather.cc/hh  # NWS API weather fetching & parsing
│   │   ├── transform.cc/hh # Converts status_report → CSS or JSON
│   │   ├── unit.hh     # Generic dimensioned_value<V,U> template (volts, watts, temp)
│   │   └── meson.build
│   ├── util/
│   │   ├── update-status.sh      # Shell script to run logger and copy CSS
│   │   ├── update-status.service # systemd oneshot service
│   │   └── update-status.timer   # systemd timer (every 5 minutes)
│   └── README.md
├── website/            # Hugo static site
│   ├── hugo.toml       # Site configuration (theme: northwoods)
│   ├── content/blog/   # Blog posts (Hugo TOML front matter)
│   ├── archetypes/     # Hugo archetype templates
│   └── themes/northwoods/  # Custom Hugo theme
│       ├── layouts/        # HTML templates (baseof, home, list, single)
│       │   └── partials/   # status.html, guestbook-form.html, header, footer
│       ├── assets/css/     # main.css, status.css (replaced by logger output)
│       ├── assets/js/      # main.js
│       └── static/svg/     # Weather and battery SVG icons
├── include_ext/        # Vendored header-only C++ libraries
│   ├── argparse.hpp    # p-ranav/argparse (CLI argument parsing)
│   └── json.hpp        # nlohmann/json v3.11.3 (JSON parsing)
└── README.md           # Project-level README
```

## Language & Build Conventions

- **C++ standard**: C++20 (`cpp_std=c++20`).
- **Compiler**: Clang/Clang++ (not GCC). Configure Meson with:
  ```sh
  CC=clang CXX=clang++ meson setup build src
  ```
- **Compiler warnings**: Meson `warning_level=2` (`-Wall -Wextra`) is enabled
  in all `meson.build` files. Code changes **must not** introduce new compiler
  warnings. Always check build output for warnings after making changes.
- **Build system**: [Meson](https://mesonbuild.com/). Each component
  (`guestbook/src`, `logger/src`) has its own `meson.build`.
- **Formatting**: Uses `{fmt}` library (v9) throughout; avoid `<iostream>`
  formatting when `fmt::format` suffices.
- **JSON**: nlohmann/json (header-only, vendored in `include_ext/`).
- **Argument parsing**: p-ranav/argparse (header-only, vendored in `include_ext/`).
- **HTTP**: libcurl via a custom RAII `curl_handle` wrapper.
- **Static site**: Hugo with the custom `northwoods` theme.

## Building

### Logger

```sh
cd logger
CC=clang CXX=clang++ meson setup build src
meson compile -C build
```

### Guestbook

```sh
cd guestbook
CC=clang CXX=clang++ meson setup build src
meson compile -C build
```

### Website (Hugo)

```sh
cd website
hugo
```

## Target Platform

- **OS**: Armbian 12 (Debian-based)
- **Web server**: lighttpd (with CGI support for the guestbook)
- **System packages**: `pkg-config`, `clang`, `libfmt-dev`, `libcurl4-openssl-dev`

## Deployment Layout

```
/srv/logger/       → logger binary + update-status.sh
/www/solar-site/   → Hugo-generated static site
/usr/lib/cgi-bin/  → guestbook CGI binary
/etc/systemd/system/ → update-status.service + update-status.timer
```

The logger runs every 5 minutes via a systemd timer, fetches weather data from
the NWS API, reads system uptime, and overwrites `/www/solar-site/css/status.css`
so the website's status bar updates without JavaScript or server-side rendering.

## Coding Style

- MIT license; copyright header: `// Copyright (C) Ben Lewis, <year>.`
  with `// SPDX-License-Identifier: MIT`.
- Prefer RAII wrappers (see `curl_handle`).
- Use `fmt::format` over `std::cout <<` chains for structured output.
- Keep header-only dependencies vendored in `include_ext/`.
- Use `#pragma once` for include guards.
- Templates and `fmt::formatter` specializations live alongside their types.

## Key Design Decisions

- **CSS-based status updates**: The logger writes a CSS file that the Hugo
  theme links to. CSS `content` and `::after` pseudo-elements display live
  data without any JavaScript. This minimizes power consumption.
- **NWS API**: Weather data comes from the US National Weather Service API
  (no API key required). Location is currently hardcoded to Seattle (KBFI).
- **Guestbook via CGI**: Visitor messages are submitted via an HTML form to a
  CGI endpoint served by lighttpd. Entries are written to a logbook file
  specified by the `LOGBOOK` environment variable.
