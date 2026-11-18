#!/bin/sh
set -eu

project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
docker info >/dev/null
logs="$project/docs/assets/linux"
mkdir -p "$logs"

docker run --rm -i \
    --mount "type=bind,src=$project,dst=/src,readonly" \
    --mount "type=bind,src=$logs,dst=/logs" \
    debian:bookworm-slim sh -s <<'LINUX'
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends gcc libc6-dev make python3 strace

mkdir /tmp/lab2
cp /src/parent.c /src/child.c /src/ipc.c /src/ipc.h /src/Makefile /tmp/lab2/
cp -R /src/tests /tmp/lab2/
cd /tmp/lab2

{
    date -u
    cat /etc/os-release
    uname -a
    cc --version
    python3 --version
    strace --version
} > /logs/environment.txt 2>&1

{
    make clean
    make
    make test
} > /logs/tests.log 2>&1
cat /logs/tests.log

printf '/logs/normal-results.txt\n100 2 5\n7.5 2.5\n0 2\n' > /logs/normal.in
strace -f -o /logs/normal.strace ./parent \
    < /logs/normal.in > /logs/normal.stdout 2> /logs/normal.log
printf '0\n' > /logs/normal.exit-code
printf '10\n3\n0\n' | cmp - /logs/normal-results.txt

printf '/logs/zero-results.txt\n20 2\n10 0\n99 3\n' > /logs/zero.in
code=0
strace -f -o /logs/zero.strace ./parent \
    < /logs/zero.in > /logs/zero.stdout 2> /logs/zero.log || code=$?
printf '%s\n' "$code" > /logs/zero.exit-code
test "$code" -eq 2
printf '10\n' | cmp - /logs/zero-results.txt

printf '\nLinux build, tests and both strace runs completed successfully.\n'
LINUX

printf '\nLogs: %s\n' "$logs"
