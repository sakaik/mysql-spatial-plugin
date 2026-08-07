#!/bin/bash
# extract-buildkit.sh - Prepare a minimal build kit for a given MySQL version.
#
# Downloads the MySQL source tarball, runs cmake + make mysqlservices inside a
# disposable Oracle Linux 9 container (WITH_LTO=OFF so the .a links with any
# recent GCC), then copies the minimal artifacts needed to build spatial_plugin:
#
#   mysql-buildkits/<VER>/
#     include/          <- from mysql-<VER>/include/
#     build-include/    <- from mysql-<VER>/build/include/  (cmake-generated)
#     libmysqlservices.a
#     BOOST_VERSION     <- e.g. "boost_1_87_0" (name of the boost dir this MySQL bundles)
#
#   mysql-buildkits/<boost_X_Y_Z>/     <- shared across versions using the same boost
#
# Usage: ./extract-buildkit.sh <MYSQL_VER>
#   e.g. ./extract-buildkit.sh 26.7.0
#        ./extract-buildkit.sh 9.7.2
set -e

VER="${1:?Usage: $0 <MYSQL_VER>  (e.g. 26.7.0, 9.7.2)}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILDKITS_DIR="$PROJECT_ROOT/mysql-buildkits"
CACHE_DIR="$BUILDKITS_DIR/.cache"
IMAGE_NAME="spatial-plugin-mysqlservices"

MAJOR_MINOR="$(echo "$VER" | cut -d. -f1-2)"
TARBALL_URL="https://dev.mysql.com/get/Downloads/MySQL-${MAJOR_MINOR}/mysql-${VER}.tar.gz"
TARBALL="$CACHE_DIR/mysql-${VER}.tar.gz"
TARGET_DIR="$BUILDKITS_DIR/${VER}"

if [ -d "$TARGET_DIR" ]; then
    echo "Buildkit for MySQL $VER already exists at:"
    echo "  $TARGET_DIR"
    echo "Delete it first if you want to re-extract."
    exit 0
fi

mkdir -p "$CACHE_DIR"

# --- 1. Download tarball (curl follows the dev.mysql.com -> cdn.mysql.com redirect) ---
if [ ! -f "$TARBALL" ]; then
    echo "==> Downloading mysql-${VER}.tar.gz (~430MB) ..."
    curl -fL --progress-bar -o "$TARBALL.tmp" "$TARBALL_URL"
    mv "$TARBALL.tmp" "$TARBALL"
else
    echo "==> Using cached tarball: $TARBALL"
fi

# --- 2. Extract to /tmp (unique per PID; cleaned on exit) ---
EXTRACT_DIR="/tmp/mysql-buildkit-${VER}-$$"
cleanup() { rm -rf "$EXTRACT_DIR"; }
trap cleanup EXIT

mkdir -p "$EXTRACT_DIR"
echo "==> Extracting tarball ..."
tar -xzf "$TARBALL" -C "$EXTRACT_DIR"

SRC_DIR="$EXTRACT_DIR/mysql-${VER}"
if [ ! -d "$SRC_DIR" ]; then
    SRC_DIR="$(find "$EXTRACT_DIR" -maxdepth 1 -mindepth 1 -type d | head -1)"
fi
[ -d "$SRC_DIR" ] || { echo "ERROR: extracted source dir not found"; exit 1; }
echo "    Source: $SRC_DIR"

# --- 3. Detect bundled boost version ---
BOOST_DIR_NAME="$(ls "$SRC_DIR/extra/boost/" 2>/dev/null | grep '^boost_' | head -1)"
[ -n "$BOOST_DIR_NAME" ] || { echo "ERROR: no boost found under $SRC_DIR/extra/boost/"; exit 1; }
echo "    Bundled boost: $BOOST_DIR_NAME"

# --- 4. Ensure shared boost copy exists ---
SHARED_BOOST="$BUILDKITS_DIR/$BOOST_DIR_NAME"
if [ ! -d "$SHARED_BOOST" ]; then
    echo "==> Copying $BOOST_DIR_NAME to shared location: $SHARED_BOOST"
    cp -a "$SRC_DIR/extra/boost/$BOOST_DIR_NAME" "$SHARED_BOOST"
fi

# --- 5. Build extractor image if not present ---
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "==> Building extractor image: $IMAGE_NAME ..."
    docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile.mysqlservices" "$SCRIPT_DIR"
fi

# --- 6. Configure + build mysqlservices inside the container ---
echo "==> Configuring cmake and building mysqlservices (WITH_LTO=OFF) ..."
docker run --rm --network=host \
    -v "$SRC_DIR:/src" \
    -u "$(id -u):$(id -g)" \
    -e "BOOST_DIR_NAME=$BOOST_DIR_NAME" \
    "$IMAGE_NAME" bash -c '
set -e
cd /src
mkdir -p build && cd build
cmake .. \
    -DDOWNLOAD_BOOST=0 \
    -DWITH_BOOST="/src/extra/boost/$BOOST_DIR_NAME" \
    -DWITH_CURL=0 \
    -DWITH_LTO=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    > /tmp/cmake.log 2>&1 || { tail -80 /tmp/cmake.log; exit 1; }
make -j"$(nproc)" GenError mysqlservices
'

# --- 7. Assemble the buildkit ---
# The plugin's #include chain reaches into sql/, plugin/, and other subdirs
# of the source tree (e.g. mysql/plugin.h pulls in sql/sql_plugin.h). Copy the
# whole source tree minus obviously-huge or irrelevant dirs, then preserve the
# cmake-generated build/include/ and build/libservices/libmysqlservices.a.
echo "==> Assembling buildkit at $TARGET_DIR ..."
mkdir -p "$TARGET_DIR"
rsync -a \
    --exclude='mysql-test' \
    --exclude='extra/boost*' \
    --exclude='.git*' \
    --exclude='build' \
    "$SRC_DIR/" "$TARGET_DIR/"

mkdir -p "$TARGET_DIR/build/libservices"
cp -a "$SRC_DIR/build/include" "$TARGET_DIR/build/include"
cp "$SRC_DIR/build/libservices/libmysqlservices.a" "$TARGET_DIR/build/libservices/"

echo "$BOOST_DIR_NAME" > "$TARGET_DIR/BOOST_VERSION"

# --- 8. Report ---
BK_SIZE=$(du -sh "$TARGET_DIR" | cut -f1)
BOOST_SIZE=$(du -sh "$SHARED_BOOST" | cut -f1)
echo
echo "==================================================================="
echo "  Buildkit ready: mysql-buildkits/${VER}  ($BK_SIZE)"
echo "  Boost (shared): mysql-buildkits/${BOOST_DIR_NAME}  ($BOOST_SIZE)"
echo "  Cached tarball: $TARBALL  (delete to reclaim disk)"
echo "==================================================================="
