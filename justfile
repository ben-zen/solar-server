set dotenv-load := false

just := just_executable()

mod logger
mod guestbook
mod website
mod bbs 'shell'

# Default recipe: build all components
default: build

# Build all components
build:
    {{ just }} logger build
    {{ just }} guestbook build
    {{ just }} website build
    {{ just }} bbs build

# Run all tests
test:
    {{ just }} guestbook test
    {{ just }} bbs test

# Remove all build artifacts
clean:
    {{ just }} logger clean
    {{ just }} guestbook clean
    {{ just }} website clean
    {{ just }} bbs clean

# Deploy everything to the local machine (requires root)
deploy: deploy-lighttpd
    {{ just }} logger deploy
    {{ just }} guestbook deploy
    {{ just }} website deploy
    {{ just }} bbs deploy

# Install and configure lighttpd for the solar-server project (requires root)
deploy-lighttpd:
    install -d /etc/lighttpd/conf-enabled
    install -d /srv/guestbook/logbook
    install -d /var/log/lighttpd
    install -m 644 lighttpd/solar-server.conf /etc/lighttpd/conf-enabled/90-solar-server.conf
    install -m 644 lighttpd/logrotate.conf /etc/logrotate.d/lighttpd
    systemctl restart lighttpd
