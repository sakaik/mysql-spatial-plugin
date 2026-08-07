#!/bin/bash
# docker-test.sh - Run test.sql against spatial_plugin .so files inside the
# official Docker Hub MySQL image for a specific MySQL version.
#
# Usage:
#   ./docker-test.sh <MYSQL_VER> <so_file> [so_file2 ...]
#     e.g. ./docker-test.sh 26.7.0 \
#             build-artifacts/26.7.0/spatial_plugin-mysql-26.7.0-glibc-2.34.so \
#             build-artifacts/26.7.0/spatial_plugin-mysql-26.7.0-glibc-2.39.so
# Note: `set -e` is intentionally NOT used — we want to always finalize (rm the
# container) and continue to the next .so on failure. Errors are captured per
# step and rolled into the final overall exit code.

VER="${1:?Usage: $0 <MYSQL_VER> <so_file> [so_file2 ...]}"
shift
[ "$#" -ge 1 ] || { echo "ERROR: at least one .so file required"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE="mysql:${VER}"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "==> Pulling $IMAGE ..."
    docker pull "$IMAGE"
fi

# Detect the image's glibc so we can skip .so files that require a newer one.
# (The official MySQL image is OL9-based → glibc 2.34, so a glibc-2.39 .so
#  cannot be dlopen'd there. We test compatible .so files and report the
#  rest as SKIPPED with a clear reason.)
IMAGE_GLIBC=$(docker run --rm --network=host --entrypoint bash "$IMAGE" -c \
    "ldd --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+$'" 2>/dev/null)
echo "==> $IMAGE glibc = ${IMAGE_GLIBC:-unknown}"

# Compare X.Y version strings numerically (major*1000 + minor).
ver_num() { local v="$1"; local maj="${v%%.*}"; local min="${v##*.}"; echo $((10#$maj * 1000 + 10#$min)); }

overall=0
for SO_PATH in "$@"; do
    SO_NAME="$(basename "$SO_PATH")"
    ABS_SO="$(cd "$(dirname "$SO_PATH")" && pwd)/$SO_NAME"
    if [ ! -f "$ABS_SO" ]; then
        echo "[FAIL] $SO_NAME: file not found ($SO_PATH)"
        overall=1
        continue
    fi

    # Find the .so's maximum GLIBC requirement; skip if the image is too old.
    SO_MAX_GLIBC=$(objdump -T "$ABS_SO" 2>/dev/null | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sed 's/GLIBC_//' | sort -u -V | tail -1)
    if [ -n "$IMAGE_GLIBC" ] && [ -n "$SO_MAX_GLIBC" ] && \
       [ "$(ver_num "$SO_MAX_GLIBC")" -gt "$(ver_num "$IMAGE_GLIBC")" ]; then
        echo
        echo "[SKIP] $SO_NAME: requires glibc $SO_MAX_GLIBC, but $IMAGE has $IMAGE_GLIBC"
        echo "       (the .so is built for newer distros; it cannot be loaded in this test image)"
        continue
    fi

    echo
    echo "================================================================"
    echo "  Testing $SO_NAME against $IMAGE"
    echo "================================================================"

    CONTAINER_NAME="spatial-test-${VER//./_}-$$"

    # --network=host is used (nested-container env forbids the default bridge's
    # sysctl setup). To avoid clashing with a native mysqld on 3306, bind the
    # container's mysqld to a private port; test traffic still uses the socket
    # via `docker exec`.
    # Mount the .so at /tmp/, not /var/lib/mysql-files/ — the image entrypoint
    # runs `chown -R mysql:mysql /var/lib/mysql-files/` and fails on a read-only mount.
    docker run --rm -d --name "$CONTAINER_NAME" --network=host \
        -e MYSQL_ALLOW_EMPTY_PASSWORD=1 \
        -v "$ABS_SO:/tmp/$SO_NAME:ro" \
        "$IMAGE" --port=13306 > /dev/null

    # Wait for the FINAL mysqld to accept connections on our chosen TCP port.
    # (During first-time init, the image's entrypoint runs a "temporary server"
    #  that binds to a unix socket only — pinging via socket returns success too
    #  early, then the temp server shuts down and our next queries hit a dead
    #  server. Forcing TCP to the port we asked for skips the temp server.)
    ready=0
    for i in $(seq 1 90); do
        if docker exec "$CONTAINER_NAME" mysqladmin ping -uroot -h 127.0.0.1 -P 13306 --protocol=TCP --silent 2>/dev/null; then
            ready=1; break
        fi
        sleep 2
    done
    if [ $ready -eq 0 ]; then
        echo "[FAIL] $SO_NAME: mysqld did not become ready"
        docker logs "$CONTAINER_NAME" 2>&1 | tail -40
        docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
        overall=1
        continue
    fi

    # Discover the plugin_dir the running server actually uses, and drop the .so there.
    # (The mysql image's exact path varies between 9.x and 26.x; asking the server is safest.)
    PLUGIN_DIR=$(docker exec "$CONTAINER_NAME" mysql -uroot -Nse "SELECT @@plugin_dir;" 2>/dev/null | tr -d '\r')
    if [ -z "$PLUGIN_DIR" ]; then
        echo "[FAIL] $SO_NAME: could not read @@plugin_dir"
        docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
        overall=1
        continue
    fi
    docker exec "$CONTAINER_NAME" cp "/tmp/$SO_NAME" "${PLUGIN_DIR}/$SO_NAME"

    rc=0
    if ! docker exec "$CONTAINER_NAME" mysql -uroot -e "INSTALL PLUGIN spatial_plugin SONAME '$SO_NAME';"; then
        echo "[FAIL] $SO_NAME: INSTALL PLUGIN failed"
        docker logs "$CONTAINER_NAME" 2>&1 | tail -30
        rc=1
    else
        MYSQL_VER_ACTUAL=$(docker exec "$CONTAINER_NAME" mysql -uroot -Nse "SELECT VERSION();" 2>/dev/null | tr -d '\r')
        BUILT_FOR=$(docker exec "$CONTAINER_NAME" mysql -uroot -Nse "SHOW STATUS LIKE 'spatial_plugin_built_for';" 2>/dev/null | awk '{print $2}' | tr -d '\r')
        echo "  server: $MYSQL_VER_ACTUAL   plugin built_for: $BUILT_FOR"

        # Copy test.sql into the container instead of piping via stdin — some
        # bash/docker combinations lose stdout when combining `docker exec -i`,
        # stdin redirection, and `$(...)` capture together.
        docker cp "$SCRIPT_DIR/test.sql" "$CONTAINER_NAME:/tmp/test.sql"
        OUT=$(docker exec "$CONTAINER_NAME" bash -c "mysql -uroot -t < /tmp/test.sql" 2>&1)
        echo "$OUT" | grep -E "\[FAIL\]|Total:|Result:|ALL PASSED" || echo "  (no summary lines; OUT length=${#OUT}, first: ${OUT:0:120})"
        if echo "$OUT" | grep -q "ALL PASSED"; then
            echo "[OK] $SO_NAME : ALL PASSED"
        else
            echo "[FAIL] $SO_NAME : test.sql did not report ALL PASSED"
            rc=1
        fi
    fi

    docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
    [ $rc -ne 0 ] && overall=1
done

echo
if [ $overall -eq 0 ]; then
    echo "==> MySQL $VER test: ALL BINARIES PASSED"
else
    echo "==> MySQL $VER test: FAILURES PRESENT"
fi
exit $overall
