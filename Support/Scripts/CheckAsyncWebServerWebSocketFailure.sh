#!/bin/sh
set -eu

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." 2>/dev/null && pwd)
CONFIGURATION=${1:-Debug}
PORT=${SC_ASYNC_WEB_SERVER_FAILURE_PORT:-18931}
TMP_DIR=$(mktemp -d)
SERVER_PID=
CLIENT_PID=

terminate_tree()
{
    for CHILD_PID in $(pgrep -P "$1" 2>/dev/null || true); do
        terminate_tree "$CHILD_PID"
    done
    kill "$1" 2>/dev/null || true
}

cleanup()
{
    touch "$TMP_DIR/stop-client" 2>/dev/null || true
    if [ -n "$CLIENT_PID" ]; then
        wait "$CLIENT_PID" 2>/dev/null || true
    fi
    if [ -n "$SERVER_PID" ]; then
        terminate_tree "$SERVER_PID"
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

cd "$REPO_ROOT"
./SC.sh build compile AsyncWebServer "$CONFIGURATION"

BACKEND_ARGUMENT=
if [ "$(uname -s)" = "Linux" ]; then
    BACKEND_ARGUMENT=--epoll
fi

./SC.sh build run AsyncWebServer "$CONFIGURATION" -- \
    --port "$PORT" --interface 127.0.0.1 --directory "$REPO_ROOT" \
    --clients 3 --websocket-clients 1 $BACKEND_ARGUMENT >"$TMP_DIR/server.log" 2>&1 &
SERVER_PID=$!

ATTEMPTS=0
until curl --fail --silent --show-error --max-time 1 --noproxy '*' \
    "http://127.0.0.1:$PORT/README.md" >"$TMP_DIR/initial-health.body" 2>/dev/null; do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        cat "$TMP_DIR/server.log"
        exit 1
    fi
    ATTEMPTS=$((ATTEMPTS + 1))
    if [ "$ATTEMPTS" -ge 100 ]; then
        cat "$TMP_DIR/server.log"
        echo "AsyncWebServer did not become ready" >&2
        exit 1
    fi
    sleep 0.05
done

python3 - "$PORT" "$TMP_DIR/client-ready" "$TMP_DIR/stop-client" <<'PY' &
import pathlib
import socket
import sys
import time

port = int(sys.argv[1])
ready = pathlib.Path(sys.argv[2])
stop = pathlib.Path(sys.argv[3])
request = (
    b"GET /ws HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Connection: Upgrade\r\n"
    b"Upgrade: websocket\r\n"
    b"Sec-WebSocket-Version: 13\r\n"
    b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
)

with socket.create_connection(("127.0.0.1", port), timeout=2) as connection:
    connection.sendall(request)
    response = connection.recv(4096)
    if not response.startswith(b"HTTP/1.1 101 "):
        raise SystemExit("first WebSocket client was not upgraded")
    ready.touch()
    while not stop.exists():
        time.sleep(0.01)
PY
CLIENT_PID=$!

ATTEMPTS=0
while [ ! -f "$TMP_DIR/client-ready" ]; do
    if ! kill -0 "$CLIENT_PID" 2>/dev/null; then
        wait "$CLIENT_PID"
        exit 1
    fi
    ATTEMPTS=$((ATTEMPTS + 1))
    if [ "$ATTEMPTS" -ge 100 ]; then
        echo "First WebSocket client did not become ready" >&2
        exit 1
    fi
    sleep 0.05
done

STATUS=$(curl --silent --show-error --max-time 2 --noproxy '*' -o "$TMP_DIR/failure.body" -w '%{http_code}' \
    -H 'Connection: Upgrade' -H 'Upgrade: websocket' -H 'Sec-WebSocket-Version: 13' \
    -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' "http://127.0.0.1:$PORT/ws")
if [ "$STATUS" != "500" ] || [ "$(tr -d '\r\n' <"$TMP_DIR/failure.body")" != "WebSocket upgrade failed" ]; then
    cat "$TMP_DIR/server.log"
    echo "AsyncWebServer did not contain the failed WebSocket upgrade" >&2
    exit 1
fi

curl --fail --silent --show-error --max-time 2 --noproxy '*' \
    "http://127.0.0.1:$PORT/README.md" >"$TMP_DIR/final-health.body"
if ! grep -q 'Sane C++ Libraries' "$TMP_DIR/final-health.body"; then
    cat "$TMP_DIR/server.log"
    echo "AsyncWebServer did not serve the request after the failed upgrade" >&2
    exit 1
fi
if ! grep -q 'AsyncWebServer WebSocket upgrade failed' "$TMP_DIR/server.log"; then
    cat "$TMP_DIR/server.log"
    echo "AsyncWebServer did not report the contained upgrade failure" >&2
    exit 1
fi

touch "$TMP_DIR/stop-client"
wait "$CLIENT_PID"
CLIENT_PID=

terminate_tree "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=

echo "AsyncWebServer contained WebSocket admission failure and served a subsequent request ($CONFIGURATION)."
