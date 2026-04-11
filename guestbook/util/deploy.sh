#!/usr/bin/env bash
# Deploy the guestbook CGI binary and its supporting configuration.
# Run as root (or with sudo).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Installing guestbook binary..."
install -m 755 "$REPO_ROOT/build/guestbook" /usr/lib/cgi-bin/guestbook

echo "Creating logbook directory..."
mkdir -p /srv/guestbook
chown www-data:www-data /srv/guestbook
touch /srv/guestbook/logbook.current.log
chown www-data:www-data /srv/guestbook/logbook.current.log

echo "Installing lighttpd configuration..."
install -m 644 "$SCRIPT_DIR/lighttpd-guestbook.conf" /etc/lighttpd/conf-enabled/90-guestbook.conf

echo "Installing logrotate configuration..."
install -m 644 "$SCRIPT_DIR/logrotate-lighttpd.conf" /etc/logrotate.d/lighttpd

echo "Ensuring lighttpd log directory exists..."
mkdir -p /var/log/lighttpd
chown www-data:www-data /var/log/lighttpd

echo "Reloading lighttpd..."
systemctl reload lighttpd || systemctl restart lighttpd

echo "Guestbook deployment complete."
