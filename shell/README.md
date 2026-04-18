# Solar Server Shell

A BBS-style interactive shell for the solar server, designed for use as a
login shell over telnet, SSH, or a dial-up modem connection.

## Features

- **Welcome banner** with programmable title and info lines
- **Sign the guestbook** — invokes the guestbook binary via fork/exec
  (same entry format as the web guestbook)
- **Read recent guestbook entries** — view the latest visitor messages
- **Read sysop messages** — view messages from the server administrator
- **Extensible** — new commands can be added by creating a `command` struct
- **Separated presentation** — swap the `text_renderer` for a different
  `renderer` implementation to change the UI without touching command logic
- **Configuration** — supports both CLI arguments and a `~/.solarshrc` dotfile

## Building

```sh
just bbs build
```

Or from the `shell/` directory:

```sh
cd shell
just build
```

### Running tests

```sh
just bbs test
```

### Manual build

#### Dependencies

- [Meson](https://mesonbuild.com/)
- Clang++ 16+
- `libfmt` (v9+)

```sh
cd shell
CC=clang CXX=clang++ meson setup build src
meson compile -C build
```

## Usage

```sh
./build/solar-shell \
    --logbook /srv/guestbook/logbook \
    --messages /srv/shell/messages \
    --guestbook-bin /usr/lib/cgi-bin/guestbook \
    --title "Solar Server BBS" \
    --info "Powered by sunlight" \
    --info "ToorCamp 2025"
```

### Command-line options

| Flag              | Description                                      | Default                          |
|-------------------|--------------------------------------------------|----------------------------------|
| `--config`        | Path to configuration file                        | `~/.solarshrc`                   |
| `--logbook`       | Path to the guestbook logbook directory           | `/srv/guestbook/logbook`         |
| `--messages`      | Path to the sysop messages directory              | `/srv/shell/messages`            |
| `--guestbook-bin` | Path to the guestbook binary                      | `guestbook`                      |
| `--title`         | Banner title                                      | `Solar Server BBS`               |
| `--info`          | Extra info line (repeatable)                      | *(none)*                         |
| `--location`      | Default location for the guestbook sign prompt    | *(none)*                         |
| `--width`         | Terminal width for formatting                     | `0` (auto-detect; fallback `72`) |
| `--max-entries`   | Max recent guestbook entries to display           | `10`                             |

CLI arguments always override values from the configuration file.

### Configuration file (~/.solarshrc)

The shell reads `~/.solarshrc` on startup (or a path given by `--config`).
The format is simple `key = value`, one per line.  Lines starting with `#`
are comments.  The `info` key may appear multiple times.

```
# Solar shell configuration
title = Solar Server BBS
info = Powered by sunlight
info = ToorCamp 2025
logbook = /srv/guestbook/logbook
messages = /srv/shell/messages
guestbook-bin = /usr/lib/cgi-bin/guestbook
location = ToorCamp
max-entries = 20
width = 80
```

### As a login shell

To use as a BBS login shell, set the user's shell to the installed binary:

```sh
sudo usermod -s /usr/local/bin/solar-shell bbs
```

## Deployment

```sh
sudo just bbs deploy
```

This installs the binary to `/usr/local/bin/solar-shell` and creates the
sysop messages directory at `/srv/shell/messages`.

### Admin messages

Place `.txt` or `.md` files in the messages directory
(`/srv/shell/messages`).  They will be displayed to users when they choose
"Read messages from the sysop".  Files are sorted by filename (descending),
so a timestamp prefix (e.g., `2025-06-15_welcome.txt`) works well.

## Architecture

The shell is designed with a clear separation between presentation and
business logic:

- **`renderer`** (abstract) — defines how output is displayed to the user.
  The `text_renderer` implementation uses plain text with ANSI escape codes,
  suitable for any terminal.  Untrusted input (guestbook entries) is
  sanitized to strip ANSI escape sequences and control characters.
- **`command`** — encapsulates a single user action with a key, label, and
  execute function.  New features are added by creating new commands.
- **`shell`** — orchestrates the command loop, connecting the renderer to the
  registered commands.  Handles EOF/disconnect gracefully.
- **`guestbook_io`** — reads guestbook entries from the logbook directory and
  invokes the guestbook binary via fork/exec for writing new entries.  This
  avoids duplicating the entry format logic from the guestbook component.
