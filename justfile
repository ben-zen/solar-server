set dotenv-load := false

just := just_executable()

mod logger
mod guestbook
mod website

# Default recipe: build all components
default: build

# Build all components
build:
    {{ just }} logger build
    {{ just }} guestbook build
    {{ just }} website build

# Run all tests
test:
    {{ just }} guestbook test

# Remove all build artifacts
clean:
    {{ just }} logger clean
    {{ just }} guestbook clean
    {{ just }} website clean

# Deploy everything to the local machine (requires root)
deploy: deploy-lighttpd
    {{ just }} logger deploy
    {{ just }} guestbook deploy
    {{ just }} website deploy

# Install and configure lighttpd for the solar-server project (requires root)
deploy-lighttpd:
    install -d /etc/lighttpd/conf-enabled
    install -d /srv/guestbook/logbook
    install -m 644 lighttpd/solar-server.conf /etc/lighttpd/conf-enabled/90-solar-server.conf
    systemctl restart lighttpd
