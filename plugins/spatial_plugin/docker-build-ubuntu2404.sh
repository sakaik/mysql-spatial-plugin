#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE_NAME="spatial-plugin-builder-ubuntu2404"
OUTPUT_NAME="spatial_plugin-glibc-2.39.so"

# Build Docker image if not exists
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "Building Docker image: $IMAGE_NAME ..."
    docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile.ubuntu2404" "$SCRIPT_DIR"
fi

echo "Building spatial_plugin for Ubuntu 24.04 (glibc 2.39) ..."

docker run --rm --network=host \
    -v "$PROJECT_ROOT/mysql970.source:/build/mysql970.source:ro" \
    -v "$PROJECT_ROOT/mysql970:/build/mysql970:ro" \
    -v "$PROJECT_ROOT/geos:/build/geos:ro" \
    -v "$PROJECT_ROOT/plugins:/build/plugins:ro" \
    -v "$SCRIPT_DIR:/build/out" \
    "$IMAGE_NAME" bash -c '
set -e

# Step 1: Build GEOS static library
echo "Building GEOS ..."
mkdir -p /tmp/geos-build
cmake -S /build/geos -B /tmp/geos-build \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_BENCHMARKS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    > /dev/null
cmake --build /tmp/geos-build -j$(nproc)

# Step 2: Build spatial_plugin
echo "Compiling spatial_plugin ..."
g++ -shared -fPIC -std=c++17 -O2 -fvisibility=hidden -DMYSQL_DYNAMIC_PLUGIN \
    -I /build/mysql970.source/build/include \
    -I /build/mysql970/include \
    -I /build/mysql970.source/include \
    -I /build/mysql970.source \
    -I /build/mysql970.source/build \
    -I /build/mysql970.source/extra/boost/boost_1_87_0 \
    -I /build/geos/include \
    -I /tmp/geos-build/capi \
    -I /build/geos/capi \
    -I /build/plugins \
    -o /build/out/'"$OUTPUT_NAME"' \
    /build/plugins/spatial_plugin/spatial_plugin.cc \
    /build/plugins/gis_lib/wkb_parser.cc \
    -L /build/mysql970.source/build/libservices -lmysqlservices \
    -L /tmp/geos-build/lib -lgeos_c -lgeos \
    -lstdc++ -lm

echo "Done: $OUTPUT_NAME"
'

echo "Output: $SCRIPT_DIR/$OUTPUT_NAME"
