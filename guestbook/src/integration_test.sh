#!/usr/bin/env bash
# Copyright (C) Ben Lewis, 2024.
# SPDX-License-Identifier: MIT
#
# Integration tests for the guestbook binary.
# Exercises the CGI, CLI, and stdin code paths end-to-end.

set -euo pipefail

GUESTBOOK="${1:?Usage: $0 <path-to-guestbook-binary>}"
PASS=0
FAIL=0
LOGBOOK_DIR=""

cleanup() {
    if [[ -n "$LOGBOOK_DIR" && -d "$LOGBOOK_DIR" ]]; then
        rm -rf "$LOGBOOK_DIR"
    fi
}
trap cleanup EXIT

assert_stdout_contains() {
    local test_name="$1" pattern="$2" stdout="$3"
    if echo "$stdout" | grep -qF "$pattern"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "FAIL [$test_name]: stdout missing '$pattern'" >&2
        echo "  got: $stdout" >&2
    fi
}

assert_stdout_not_contains() {
    local test_name="$1" pattern="$2" stdout="$3"
    if echo "$stdout" | grep -qF "$pattern"; then
        FAIL=$((FAIL + 1))
        echo "FAIL [$test_name]: stdout should not contain '$pattern'" >&2
    else
        PASS=$((PASS + 1))
    fi
}

assert_stderr_contains() {
    local test_name="$1" pattern="$2" stderr_text="$3"
    if echo "$stderr_text" | grep -qF "$pattern"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "FAIL [$test_name]: stderr missing '$pattern'" >&2
        echo "  got: $stderr_text" >&2
    fi
}

assert_exit_code() {
    local test_name="$1" expected="$2" actual="$3"
    if [[ "$actual" -eq "$expected" ]]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "FAIL [$test_name]: expected exit code $expected, got $actual" >&2
    fi
}

# -----------------------------------------------------------------------
# CGI mode tests
# -----------------------------------------------------------------------

echo "--- CGI mode: valid submission redirects to / ---"
stdout=$(echo -n "name=Alice&location=Seattle&message=Hello+World" \
    | CONTENT_LENGTH=47 "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-valid-redirect" "Status: 303 See Other" "$stdout"
assert_stdout_contains "cgi-valid-location" "Location: /" "$stdout"

echo "--- CGI mode: valid submission redirects to HTTP_REFERER (relative path) ---"
stdout=$(echo -n "name=Bob&location=Mars&message=Hi" \
    | CONTENT_LENGTH=33 HTTP_REFERER="/guestbook" "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-referer-redirect" "Location: /guestbook" "$stdout"

echo "--- CGI mode: external HTTP_REFERER falls back to / ---"
stdout=$(echo -n "name=Bob&location=Mars&message=Hi" \
    | CONTENT_LENGTH=33 HTTP_REFERER="http://evil.example.com/phish" "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-referer-external-fallback" "Location: /" "$stdout"

echo "--- CGI mode: same-origin HTTP_REFERER extracts path ---"
stdout=$(echo -n "name=Bob&location=Mars&message=Hi" \
    | CONTENT_LENGTH=33 HTTP_REFERER="http://solar.local/guestbook" HTTP_HOST="solar.local" "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-referer-sameorigin" "Location: /guestbook" "$stdout"

echo "--- CGI mode: HTTP_REFERER with CRLF falls back to / ---"
stdout=$(printf 'name=Bob&location=Mars&message=Hi' \
    | CONTENT_LENGTH=33 HTTP_REFERER=$'/evil\r\nInjected: header' "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-referer-crlf-fallback" "Location: /" "$stdout"

echo "--- CGI mode: scheme-relative HTTP_REFERER (//...) falls back to / ---"
stdout=$(echo -n "name=Bob&location=Mars&message=Hi" \
    | CONTENT_LENGTH=33 HTTP_REFERER="//evil.com/path" "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-referer-scheme-relative-fallback" "Location: /" "$stdout"

echo "--- CGI mode: zero content length returns 400 ---"
stdout=$(echo -n "" | CONTENT_LENGTH=0 "$GUESTBOOK" 2>/dev/null) || true
rc=$?
assert_stdout_contains "cgi-zero-400" "Status: 400 Bad Request" "$stdout"

echo "--- CGI mode: oversized content length returns 413 ---"
stdout=$(echo -n "" | CONTENT_LENGTH=99999 "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-oversize-413" "Status: 413 Payload Too Large" "$stdout"
assert_stdout_contains "cgi-oversize-413-detail" "invalid or exceeds" "$stdout"

echo "--- CGI mode: non-numeric content length returns 413 ---"
stdout=$(echo -n "" | CONTENT_LENGTH=abc "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-nonnumeric-413" "Status: 413 Payload Too Large" "$stdout"

echo "--- CGI mode: missing required field (no message) returns 400 ---"
stdout=$(echo -n "name=Alice&location=here" \
    | CONTENT_LENGTH=24 "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-missing-message-400" "Status: 400 Bad Request" "$stdout"
assert_stdout_contains "cgi-missing-message-detail" "name" "$stdout"

echo "--- CGI mode: missing name returns 400 ---"
stdout=$(echo -n "location=here&message=test" \
    | CONTENT_LENGTH=25 "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-missing-name-400" "Status: 400 Bad Request" "$stdout"
assert_stdout_contains "cgi-missing-name-detail" "name" "$stdout"

echo "--- CGI mode: zero content returns 400 with detail ---"
stdout=$(echo -n "" | CONTENT_LENGTH=0 "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-zero-400-detail" "empty" "$stdout"

echo "--- CGI mode: stderr logs when LOGBOOK is not set ---"
stderr_text=$(echo -n "name=Alice&message=Hello" \
    | CONTENT_LENGTH=24 "$GUESTBOOK" 2>&1 1>/dev/null) || true
assert_stderr_contains "cgi-stderr-no-logbook" "LOGBOOK environment variable is not set" "$stderr_text"

echo "--- CGI mode: stderr logs when LOGBOOK dir does not exist ---"
stderr_text=$(echo -n "name=Alice&message=Hello" \
    | CONTENT_LENGTH=24 LOGBOOK=/nonexistent/path "$GUESTBOOK" 2>&1 1>/dev/null) || true
assert_stderr_contains "cgi-stderr-bad-logbook" "LOGBOOK directory does not exist" "$stderr_text"

echo "--- CGI mode: oversized content length logs to stderr ---"
stderr_text=$(echo -n "" | CONTENT_LENGTH=99999 "$GUESTBOOK" 2>&1 1>/dev/null) || true
assert_stderr_contains "cgi-stderr-oversize" "invalid or oversized CONTENT_LENGTH" "$stderr_text"

echo "--- CGI mode: zero content length logs to stderr ---"
stderr_text=$(echo -n "" | CONTENT_LENGTH=0 "$GUESTBOOK" 2>&1 1>/dev/null) || true
assert_stderr_contains "cgi-stderr-zero" "CONTENT_LENGTH is zero" "$stderr_text"

echo "--- CGI mode: validation failure logs to stderr ---"
stderr_text=$(echo -n "name=Alice&location=here" \
    | CONTENT_LENGTH=24 "$GUESTBOOK" 2>&1 1>/dev/null) || true
assert_stderr_contains "cgi-stderr-validation" "form validation failed" "$stderr_text"

# -----------------------------------------------------------------------
# CGI mode with LOGBOOK — writes to the logbook file
# -----------------------------------------------------------------------

echo "--- CGI mode: writes entry to logbook file ---"
LOGBOOK_DIR=$(mktemp -d)
touch "$LOGBOOK_DIR/logbook.current.log"
stdout=$(echo -n "name=Writer&location=Home&message=Logbook+test" \
    | CONTENT_LENGTH=46 LOGBOOK="$LOGBOOK_DIR" "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-logbook-redirect" "Status: 303 See Other" "$stdout"
# Verify the logbook file has content.
logbook_content=$(cat "$LOGBOOK_DIR/logbook.current.log")
assert_stdout_contains "cgi-logbook-author" '"author":"Writer"' "$logbook_content"
assert_stdout_contains "cgi-logbook-message" "Logbook test" "$logbook_content"
rm -rf "$LOGBOOK_DIR"
LOGBOOK_DIR=""

echo "--- CGI mode: creates logbook file on first run ---"
LOGBOOK_DIR=$(mktemp -d)
# Do NOT pre-create the file — ofstream with app mode should create it.
stdout=$(echo -n "name=First&location=Boot&message=Hello+world" \
    | CONTENT_LENGTH=45 LOGBOOK="$LOGBOOK_DIR" "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_contains "cgi-logbook-create" "Status: 303 See Other" "$stdout"
# Verify the logbook file was created and has content.
if [[ -f "$LOGBOOK_DIR/logbook.current.log" ]]; then
    logbook_content=$(cat "$LOGBOOK_DIR/logbook.current.log")
    assert_stdout_contains "cgi-logbook-create-author" '"author":"First"' "$logbook_content"
else
    FAIL=$((FAIL + 1))
    echo "FAIL [cgi-logbook-create-exists]: logbook.current.log was not created" >&2
fi
rm -rf "$LOGBOOK_DIR"
LOGBOOK_DIR=""

# -----------------------------------------------------------------------
# CLI mode tests
# -----------------------------------------------------------------------

echo "--- CLI mode: valid arguments produce entry ---"
stdout=$("$GUESTBOOK" --author "CLIUser" --location "Terminal" --message "Hello from CLI" 2>/dev/null) || true
assert_stdout_contains "cli-valid-author" '"author":"CLIUser"' "$stdout"
assert_stdout_contains "cli-valid-message" "Hello from CLI" "$stdout"

echo "--- CLI mode: output goes to stdout, not CGI headers ---"
assert_stdout_not_contains "cli-no-cgi-headers" "Status:" "$stdout"

echo "--- CLI mode: missing required argument returns non-zero ---"
set +e
"$GUESTBOOK" --author "Alice" 2>/dev/null
rc=$?
set -e
assert_exit_code "cli-missing-args" 2 "$rc"

# -----------------------------------------------------------------------
# CGI mode: no stdout pollution with debug output
# -----------------------------------------------------------------------

echo "--- CGI mode: stdout contains only CGI headers (no debug output) ---"
stdout=$(echo -n "name=Alice&message=Test" \
    | CONTENT_LENGTH=22 "$GUESTBOOK" 2>/dev/null) || true
assert_stdout_not_contains "cgi-no-env-dump" "Environment variables:" "$stdout"
assert_stdout_not_contains "cgi-no-arg-dump" '["' "$stdout"

# -----------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------

echo ""
echo "====================================="
echo "Integration tests: $PASS passed, $FAIL failed"
echo "====================================="

if [[ "$FAIL" -gt 0 ]]; then
    exit 1
fi
