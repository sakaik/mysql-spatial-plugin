#!/bin/bash
# docker-build.sh - Build spatial_plugin .so against a specific MySQL version.
#
# Uses the buildkit produced by extract-buildkit.sh (mysql-buildkits/<VER>/) plus
# the shared boost directory. Produces:
#
#   build-artifacts/<MYSQL_VER>/spatial_plugin-mysql-<MYSQL_VER>-glibc-2.34.so
#   build-artifacts/<MYSQL_VER>/spatial_plugin-mysql-<MYSQL_VER>-glibc-2.39.so
#
# Usage:
#   ./docker-build.sh <MYSQL_VER> [target]
#     target: ol9 (glibc 2.34) | ubuntu2404 (glibc 2.39) | both (default)
set -e

VER="${1:?Usage: $0 <MYSQL_VER> [ol9|ubuntu2404|both]}"
TARGET="${2:-both}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILDKITS_DIR="$PROJECT_ROOT/mysql-buildkits"
BUILDKIT="$BUILDKITS_DIR/$VER"
ARTIFACT_DIR="$PROJECT_ROOT/build-artifacts/$VER"

# --- Preflight ---
if [ ! -d "$BUILDKIT" ]; then
    echo "ERROR: buildkit for MySQL $VER not found at $BUILDKIT"
    echo "       Run: ./extract-buildkit.sh $VER"
    exit 1
fi
if [ ! -f "$BUILDKIT/BOOST_VERSION" ]; then
    echo "ERROR: $BUILDKIT/BOOST_VERSION missing (re-run extract-buildkit.sh)"
    exit 1
fi
BOOST_DIR_NAME="$(cat "$BUILDKIT/BOOST_VERSION")"
BOOST_DIR="$BUILDKITS_DIR/$BOOST_DIR_NAME"
if [ ! -d "$BOOST_DIR" ]; then
    echo "ERROR: shared boost $BOOST_DIR_NAME not found at $BOOST_DIR"
    exit 1
fi
if [ ! -d "$PROJECT_ROOT/geos" ]; then
    echo "ERROR: geos not found at $PROJECT_ROOT/geos"
    exit 1
fi

mkdir -p "$ARTIFACT_DIR"

build_one() {
    local target="$1"
    local glibc image_name dockerfile
    case "$target" in
        ol9)        glibc=2.34; image_name="spatial-plugin-builder-ol9";         dockerfile="Dockerfile.ol9" ;;
        ubuntu2404) glibc=2.39; image_name="spatial-plugin-builder-ubuntu2404";  dockerfile="Dockerfile.ubuntu2404" ;;
        *) echo "ERROR: unknown target: $target"; return 1 ;;
    esac

    local out_name="spatial_plugin-mysql-${VER}-glibc-${glibc}.so"

    if ! docker image inspect "$image_name" >/dev/null 2>&1; then
        echo "==> Building Docker image: $image_name ..."
        docker build -t "$image_name" -f "$SCRIPT_DIR/$dockerfile" "$SCRIPT_DIR"
    fi

    echo "==> Building $out_name (target: $target, glibc $glibc) ..."
    docker run --rm --network=host \
        -v "$BUILDKIT:/build/mysql:ro" \
        -v "$BOOST_DIR:/build/boost:ro" \
        -v "$PROJECT_ROOT/geos:/build/geos:ro" \
        -v "$PROJECT_ROOT/plugins:/build/plugins:ro" \
        -v "$ARTIFACT_DIR:/build/out" \
        "$image_name" bash -c '
set -e

# Step 1: build GEOS static
echo "Building GEOS ..."
mkdir -p /tmp/geos-build
cmake -S /build/geos -B /tmp/geos-build \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    > /dev/null
cmake --build /tmp/geos-build -j$(nproc) > /tmp/geos-build.log 2>&1 || { tail -50 /tmp/geos-build.log; exit 1; }

# Step 2: compile spatial_plugin
echo "Compiling spatial_plugin ..."
g++ -shared -fPIC -std=c++17 -O2 -fvisibility=hidden -fno-lto -DMYSQL_DYNAMIC_PLUGIN \
    -I /build/mysql/build/include \
    -I /build/mysql/include \
    -I /build/mysql \
    -I /build/mysql/build \
    -I /build/boost \
    -I /build/geos/include \
    -I /tmp/geos-build/capi \
    -I /build/geos/capi \
    -I /build/plugins \
    -o /build/out/'"$out_name"' \
    /build/plugins/spatial_plugin/spatial_plugin.cc \
    /build/plugins/gis_lib/wkb_parser.cc \
    -L /build/mysql/build/libservices -lmysqlservices \
    -L /tmp/geos-build/lib -lgeos_c -lgeos \
    -lstdc++ -lm

echo "Done: $out_name"
'
    echo "==> Output: $ARTIFACT_DIR/$out_name ($(du -h "$ARTIFACT_DIR/$out_name" | cut -f1))"
}

case "$TARGET" in
    ol9|ubuntu2404) build_one "$TARGET" ;;
    both)           build_one ol9; build_one ubuntu2404 ;;
    *) echo "ERROR: target must be ol9, ubuntu2404, or both"; exit 1 ;;
esac
