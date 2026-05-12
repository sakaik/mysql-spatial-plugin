#!/bin/bash
# Run the spatial_plugin test suite (test.sql) against MySQL 9.7 inside a
# disposable Docker container. Validates that the released .so loads into 9.7
# and that all tests pass.
#
# Usage:
#   ./docker-test-mysql97.sh                 # test both released .so files
#   ./docker-test-mysql97.sh <plugin.so>     # test one specific .so
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_NAME="spatial-plugin-test-mysql97"
MYSQL_DIR="$PROJECT_ROOT/mysql970"

if [ ! -x "$MYSQL_DIR/bin/mysqld" ]; then
    echo "ERROR: $MYSQL_DIR/bin/mysqld not found."
    echo "Extract mysql-9.7.0-linux-glibc2.28-x86_64.tar.xz to $MYSQL_DIR first."
    exit 1
fi

if [ "$#" -ge 1 ]; then
    SO_LIST="$1"
else
    SO_LIST="spatial_plugin-glibc-2.34.so spatial_plugin-glibc-2.39.so"
fi

# Build the test image if needed
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "Building Docker image: $IMAGE_NAME ..."
    docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile.mysql97-test" "$SCRIPT_DIR"
fi

docker run --rm --network=host \
    -e "SO_LIST=$SO_LIST" \
    -v "$MYSQL_DIR:/opt/mysql:ro" \
    -v "$SCRIPT_DIR:/plugin:ro" \
    "$IMAGE_NAME" bash -c '
set -e
BASEDIR=/opt/mysql
SOCK=/tmp/mysql97.sock
MYSQLD="$BASEDIR/bin/mysqld"
MYSQL="$BASEDIR/bin/mysql --socket=$SOCK -u root"

overall=0
for SO in $SO_LIST; do
    echo
    echo "================================================================"
    echo "  Testing $SO against MySQL $($MYSQLD --version | grep -oP "Ver \K[0-9.]+")"
    echo "================================================================"

    rm -rf /tmp/data && mkdir -p /tmp/data
    "$MYSQLD" --no-defaults --basedir="$BASEDIR" --datadir=/tmp/data \
        --initialize-insecure --user=root > /tmp/init.log 2>&1 || { cat /tmp/init.log; exit 1; }

    "$MYSQLD" --no-defaults --basedir="$BASEDIR" --datadir=/tmp/data \
        --socket="$SOCK" --skip-networking --user=root \
        --plugin-dir=/plugin > /tmp/mysqld.log 2>&1 &
    MYSQLD_PID=$!

    # Wait for the server to accept connections
    for i in $(seq 1 60); do
        if $MYSQL -e "SELECT 1" >/dev/null 2>&1; then break; fi
        if ! kill -0 $MYSQLD_PID 2>/dev/null; then echo "mysqld died:"; cat /tmp/mysqld.log; exit 1; fi
        sleep 1
    done

    rc=0
    if ! $MYSQL -e "INSTALL PLUGIN spatial_plugin SONAME '"'"'$SO'"'"';"; then
        echo "[FAIL] INSTALL PLUGIN failed for $SO"
        cat /tmp/mysqld.log
        rc=1
    else
        OUT=$($MYSQL -t < /plugin/test.sql 2>&1)
        echo "$OUT" | grep -E "\[FAIL\]|Total:|Result:" || true
        if echo "$OUT" | grep -q "ALL PASSED"; then
            echo "[OK] $SO : ALL PASSED"
        else
            echo "[FAIL] $SO : test.sql reported failures"
            rc=1
        fi
    fi

    $MYSQL -e "SHUTDOWN" 2>/dev/null || kill $MYSQLD_PID 2>/dev/null || true
    wait $MYSQLD_PID 2>/dev/null || true
    [ $rc -ne 0 ] && overall=1
done

echo
if [ $overall -eq 0 ]; then
    echo "==> MySQL 9.7 test: ALL BINARIES PASSED"
else
    echo "==> MySQL 9.7 test: FAILURES PRESENT"
fi
exit $overall
'
