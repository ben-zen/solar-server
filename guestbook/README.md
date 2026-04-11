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
CC=clang CXX=clang++ meson setup build src
meson compile -C build
```

### Running tests

```sh
meson test -C build -v
```

## Deployment

The automated deployment script (`util/deploy.sh`) handles all of the steps
below.  Run it as root:

```sh
sudo ./util/deploy.sh
```

### Manual deployment

#### Binary

```sh
install -m 755 build/guestbook /usr/lib/cgi-bin/guestbook
```

#### Logbook directory

```sh
mkdir -p /srv/guestbook
chown www-data:www-data /srv/guestbook
touch /srv/guestbook/logbook.current.log
chown www-data:www-data /srv/guestbook/logbook.current.log
```

#### lighttpd

Install the lighttpd configuration that routes `/cgi-bin/guestbook` to the
binary and sets the `LOGBOOK` environment variable:

```sh
install -m 644 util/lighttpd-guestbook.conf /etc/lighttpd/conf-enabled/90-guestbook.conf
systemctl reload lighttpd
```

The configuration also sets `server.errorlog` so that CGI stderr (diagnostic
and error messages from the guestbook binary) is captured in
`/var/log/lighttpd/error.log`.

#### Logrotate

Install the logrotate configuration to keep lighttpd's error log (which
captures guestbook CGI stderr output) from growing unbounded:

```sh
install -m 644 util/logrotate-lighttpd.conf /etc/logrotate.d/lighttpd
```

The logrotate policy (`util/logrotate-lighttpd.conf`):

- **Rotation**: weekly
- **Retention**: 12 weeks
- **Compression**: enabled (`compress`, `delaycompress`)
- **Missing/empty**: tolerant (`missingok`, `notifempty`)
- **Ownership**: recreated as `640 www-data:www-data`
- **Signal**: sends `USR1` to lighttpd so it reopens its log file after
  rotation

Verify the configuration with:

```sh
logrotate -d /etc/logrotate.d/lighttpd
```

## Risks

Well, we've got user generated content here. 