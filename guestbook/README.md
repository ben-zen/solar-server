# `guestbook`, for leaving greetings

## What's in a log entry?

    Author:     _someone_
    Location:   _somewhere_
    Time:       Collected from the system time
    Status:     Output from `logger`
    Message:    _message contents_

## Methods of writing to the guestbook

### CGI

The guestbook runs as a CGI binary under lighttpd.  Visitors submit messages
through an HTML form; the binary URL-decodes the POST body, validates the
fields, formats the entry with JSON front matter, and appends it to a logbook
file.

Input limits:

| Field      | Max length |
|------------|-----------|
| POST body  | 8 192 B   |
| `name`     | 200 chars |
| `location` | 200 chars |
| `message`  | 2 000 chars |

### BBS/Telnet/AX.25

## Building

```sh
just guestbook build
```

Or from the `guestbook/` directory:

```sh
cd guestbook
just build
```

### Running tests

```sh
just guestbook test
```

## Deployment

Deploy all components at once from the repository root (requires root):

```sh
sudo just deploy
```

This installs the guestbook binary, lighttpd configuration (including
`server.errorlog`), logrotate configuration, and creates the logbook
directory at `/srv/guestbook/logbook`.

Or deploy only the guestbook binary:

```sh
sudo just guestbook deploy
```

### lighttpd

The central lighttpd configuration (`lighttpd/solar-server.conf`) is
installed by `just deploy-lighttpd` and handles:

- CGI routing (`/cgi-bin/` → `/usr/lib/cgi-bin/`)
- `LOGBOOK` environment variable (set to `/srv/guestbook/logbook`)
- `server.errorlog` (at `/var/log/lighttpd/error.log`) so that CGI stderr
  output (diagnostic and error messages from the guestbook binary) is captured

### Logrotate

The logrotate configuration (`lighttpd/logrotate.conf`) is installed by
`just deploy-lighttpd` to `/etc/logrotate.d/lighttpd`:

- **Rotation**: weekly
- **Retention**: 12 weeks
- **Compression**: enabled (`compress`, `delaycompress`)
- **Missing/empty**: tolerant (`missingok`, `notifempty`)
- **Ownership**: recreated as `640 www-data:www-data`
- **Reload**: runs `systemctl reload lighttpd` so lighttpd reopens its log
  file after rotation

Verify the configuration with:

```sh
logrotate -d /etc/logrotate.d/lighttpd
```

## Risks

Well, we've got user generated content here. 