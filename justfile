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
deploy:
    {{ just }} logger deploy
    {{ just }} guestbook deploy
    {{ just }} website deploy
