# STX Spatial Plugin for MySQL

MySQL plugin that adds spatial functions (`STX_*`) powered by [Boost.Geometry](https://www.boost.org/doc/libs/release/libs/geometry/) and [GEOS](https://libgeos.org/).
Our approach: implement functions broadly first, then refine through thorough testing.
Motto: *"Imperfect beats unavailable."*

Provides GIS functions missing from MySQL, including distance-based queries, spatial relationships (DE-9IM), coordinate transformations, I/O format conversions, and more. Supports Cartesian (including projected CRS such as UTM) and Geographic (WGS84, etc.) coordinate systems.

For testing status and known limitations of each function, see [CHECKLIST.md](CHECKLIST.md).

## Functions (62)

### Spatial Measurement & Predicates

| Function | Description |
|---|---|
| `STX_Perimeter(geom)` | Polygon/MultiPolygon perimeter |
| `STX_CoveredBy(g1, g2)` | Tests if g1 is covered by g2 |
| `STX_Covers(g1, g2)` | Tests if g1 covers g2 |
| `STX_Dwithin(g1, g2, dist)` | Tests if distance between geometries <= threshold |
| `STX_Azimuth(p1, p2)` | Bearing from p1 to p2 (radians, clockwise from north) |
| `STX_Angle(p1, p2, p3)` | Angle at p2 formed by p1-p2-p3 |
| `STX_Relate(g1, g2)` | DE-9IM relationship matrix |
| `STX_RelateMatch(g1, g2, pattern)` | Test DE-9IM pattern match |
| `STX_NPoints(geom)` | Total number of vertices (all geometry types) |
| `STX_IsRing(linestring)` | Test if LineString is a ring (closed and simple) |

### Geometry Processing

| Function | Description |
|---|---|
| `STX_Project(point, dist, azimuth)` | Project point by distance and bearing |
| `STX_LineLocatePoint(line, point)` | Fraction of line length at closest point |
| `STX_LineSubstring(line, start, end)` | Extract portion of linestring |
| `STX_ClosestPoint(geom1, geom2)` | Closest point on geom1 to geom2 |
| `STX_ShortestLine(g1, g2)` | Shortest line between two geometries |
| `STX_PointonSurface(geom)` | Interior point of polygon |
| `STX_Points(geom)` | Extract all vertices as MultiPoint |
| `STX_MakePoint(coord1, coord2 [, srid])` | Create a point from coordinates (axis order follows SRS definition) |
| `STX_MakeLine(p1, p2)` / `STX_MakeLine(multipoint)` | Create LineString from points |
| `STX_MakePolygon(ring [, inner_rings])` | Create Polygon from LineString |
| `STX_GeneratePoints(geom, n [, seed])` | Generate random points inside polygon |
| `STX_MinimumBoundingCircle(geom [, segs])` | Minimum bounding circle as polygon |
| `STX_SquareGrid(size, geom)` | Square grid covering bounding box |
| `STX_HexGrid(size, geom)` | Hexagonal grid covering bounding box |

### Coordinate Transformations

| Function | Description |
|---|---|
| `STX_Translate(geom, dx, dy)` | Shift geometry by offset |
| `STX_Translate_latlon(geom, dlat, dlon)` | Shift by lat/lon offsets (Geographic only) (original) |
| `STX_Scale(geom, sx, sy [, center])` | Scale geometry by factors (origin or specified center) |
| `STX_Rotate(geom, angle [, center])` | Rotate geometry (origin or specified center) |
| `STX_Affine(geom, a, b, d, e, xoff, yoff)` | General 2D affine transformation |
| `STX_Reverse(geom)` | Reverse vertex order |
| `STX_SnapToGrid(geom, size [, size_y])` | Snap coordinates to grid |
| `STX_RemoveRepeatedPoints(geom [, tol])` | Remove consecutive duplicate vertices |
| `STX_Segmentize(geom, max_length)` | Split long segments by adding vertices |

### GEOS-based Functions

| Function | Description |
|---|---|
| `STX_MakeValid(geom)` | Repair invalid geometry |
| `STX_LineMerge(geom)` | Merge connected LineStrings |
| `STX_Voronoi(geom [, tolerance [, envelope]])` | Voronoi diagram |
| `STX_Delaunay(geom [, tolerance [, edges_only]])` | Delaunay triangulation |
| `STX_OffsetCurve(geom, dist [, quad_segs [, join [, mitre]]])` | Parallel offset line |
| `STX_ConcaveHull(geom, ratio [, allow_holes])` | Concave hull |
| `STX_Snap(g1, g2, tolerance)` | Snap vertices to another geometry |
| `STX_Polygonize(geom)` | Create polygons from linework |
| `STX_BuildArea(geom)` | Create area from linework (inner rings become holes) |
| `STX_SharedPaths(g1, g2)` | Shared paths between two lineal geometries |
| `STX_Node(geom)` | Fully node a set of linestrings |
| `STX_SimplifyPreserveTopology(geom, tol)` | Topology-preserving simplification |
| `STX_UnaryUnion(geom)` | Union of all components of a geometry |
| `STX_ClipByRect(geom, xmin, ymin, xmax, ymax)` | Fast clipping by rectangle |
| `STX_ReducePrecision(geom, gridsize)` | Reduce coordinate precision (validity preserving) |
| `STX_MaximumInscribedCircle(geom, tolerance)` | Maximum inscribed circle |
| `STX_MinimumWidth(geom)` | Minimum width of a geometry |
| `STX_SimplifyPolygonHull(geom, frac [, is_outer])` | Polygon hull simplification |
| `STX_ConcaveHullOfPolygons(geom, ratio [, holes])` | Concave hull of polygon set |

### I/O Format Conversion

| Function | Description |
|---|---|
| `STX_AsEncodedPolyline(geom [, prec])` | Geometry to Google Encoded Polyline |
| `STX_LineFromEncodedPolyline(text [, srid [, prec]])` | Encoded Polyline to LineString |
| `STX_AsSvg(geom [, rel [, prec]])` | Geometry to SVG path data |
| `STX_AsKml(geom [, prec])` | Geometry to KML |
| `STX_AsEwkt(geom)` | Geometry to EWKT (Extended WKT) |
| `STX_GeomFromEwkt(text)` | EWKT to Geometry |

### Coordinate Utilities

| Function | Description |
|---|---|
| `STX_dms2deg(d, m, s)` | DMS to decimal degrees (original) |
| `STX_deg2dms_deg(d)` | Degree part of decimal degrees (original) |
| `STX_deg2dms_min(d)` | Minute part of decimal degrees (original) |
| `STX_deg2dms_sec(d)` | Second part of decimal degrees (original) |

See [Function Reference](plugins/spatial_plugin/docs/function_reference.md) for full documentation.

## Requirements

Daemon plugins are tied to the exact MySQL version (patch level included), so
each MySQL version needs its own `.so`. Supported MySQL tracks:

- **LTS**: MySQL 9.7.x (a new `.so` is built for each patch release)
- **Innovation**: calendar-versioned releases starting July 2026
  (26.7 → 26.10 → 27.1 → …, quarterly)
- **MySQL 9.6** support ended with plugin **v0.2.0** — 9.6 users should stay on that release

For building from source: MySQL source tree (headers + Boost), g++ with C++17
support, and a MySQL binary installation for the plugin directory. Multi-version
release builds use Docker (see [Build](#build)).

## Directory Structure

```
├── mysql-buildkits/           # per-MySQL-version source + libmysqlservices.a kits (auto-generated)
│   ├── boost_1_87_0/          #   shared Boost dir (populated from the first extracted MySQL source)
│   ├── 9.7.2/                 #   ~300MB per version (source tree minus mysql-test/, plus compiled libmysqlservices.a)
│   └── 26.7.0/
├── build-artifacts/           # per-version .so outputs from docker-build.sh
│   └── 26.7.0/spatial_plugin-mysql-26.7.0-glibc-{2.34,2.39}.so
├── mysql-26.7.0/              # native dev install (MySQL binary; whichever version is current)
├── mysql-26.7.0.source/       #   full source tree kept only for the current dev version
└── plugins/
    ├── gis_lib/               # shared GIS library (WKB parser, geometry types)
    └── spatial_plugin/        # plugin source
        ├── spatial_plugin.cc
        ├── plugin_version.h
        ├── Makefile
        ├── extract-buildkit.sh
        ├── docker-build.sh
        ├── docker-test.sh
        ├── test.sql
        └── docs/function_reference.md
```

`mysql-buildkits/`, `build-artifacts/`, and the `mysql-<version>*/` dev installs
are not committed to this repository. Download the MySQL source tarball you need
from [MySQL Downloads](https://dev.mysql.com/downloads/mysql/); `extract-buildkit.sh`
fetches it automatically when building for a version that has no local buildkit yet.

## Pre-built Binaries

Two variants are built per MySQL version, differing only in target glibc:

| File pattern | Build Environment | Required glibc | Target Platforms |
|---|---|---|---|
| `spatial_plugin-mysql-<VER>-glibc-2.39.so` | Ubuntu 24.04 (glibc 2.39) | glibc 2.38+ | Ubuntu 24.04+, Debian 13+, Fedora 39+ |
| `spatial_plugin-mysql-<VER>-glibc-2.34.so` | Oracle Linux 9 (glibc 2.34) | glibc 2.32+ | OL9, RHEL 9, AlmaLinux/Rocky 9, Ubuntu 22.04+ |

Check the [Releases page](https://github.com/sakaik/mysql-spatial-plugin/releases)
for the current MySQL versions we ship binaries for. Each `.so` reports the MySQL
version it was built against via `SHOW STATUS LIKE 'spatial_plugin_built_for'`;
it must match your server or `INSTALL PLUGIN` will fail.

Pick the binary matching your MySQL version and your system's glibc (check with
`ldd --version`; a lower-glibc binary also runs on newer systems). Copy it as-is
to the MySQL plugin directory and load it by file name:

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin-mysql-26.7.0-glibc-2.34.so';

-- Verify the .so matches your server:
SHOW STATUS LIKE 'spatial_plugin_%';
```

**Legacy assets in v0.3.0** — the two files without a MySQL version in the name
(`spatial_plugin-glibc-2.39.so`, `spatial_plugin-glibc-2.34.so`) were the initial
build for **MySQL 9.7.0**. Newer MySQL versions ship under the versioned name
above. For MySQL 9.6, stay on plugin v0.2.0.

## Build

Native dev build (uses the sibling `mysql-<VER>/` install):

```bash
cd plugins/spatial_plugin
make            # compile spatial_plugin.so
make install    # copy .so to MySQL plugin directory
```

Multi-version release build via Docker (any supported MySQL version):

```bash
cd plugins/spatial_plugin
make build-mysql   MYSQL_VER=26.7.0   # extract buildkit, build both glibc variants, run test suite
make release-mysql MYSQL_VER=26.7.0   # upload .so files to the current GitHub release (--clobber not used)
```

`extract-buildkit.sh` downloads and processes the MySQL source tarball on first
use for a given version (~5 minutes; cached under `mysql-buildkits/.cache/`).
Subsequent `make build-mysql` runs for the same version skip extraction.

## Installation

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';
```

All 62 functions are registered automatically. No `CREATE FUNCTION` needed.

```sql
-- Verify
SELECT * FROM performance_schema.user_defined_functions WHERE UDF_NAME LIKE 'stx_%';
SHOW STATUS LIKE 'spatial_plugin_%';
```

## Usage Examples

```sql
-- Perimeter of a polygon (geographic, result in meters)
SELECT STX_Perimeter(ST_GeomFromText('POLYGON((139 35,140 35,140 36,139 36,139 35))', 4326));

-- Is point within 1km of another point?
SELECT STX_Dwithin(
    ST_GeomFromText('POINT(139.7 35.7)', 4326),
    ST_GeomFromText('POINT(139.71 35.71)', 4326),
    1000
);

-- Rotate polygon 45 degrees around its centroid
SELECT STX_Rotate(
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))'),
    PI()/4,
    ST_GeomFromText('POINT(5 5)')
);

-- DE-9IM spatial relationship
SELECT STX_Relate(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))')
);

-- Convert to KML
SELECT STX_Askml(ST_GeomFromText('POINT(35.6 139.7)', 4326));
-- <Point><coordinates>139.7,35.6</coordinates></Point>

-- EWKT round-trip
SELECT ST_AsText(STX_Geomfromewkt('SRID=4326;POINT(139.7 35.6)'));
-- POINT(35.6 139.7)
```

### Note on Return Types

Functions returning geometry use `STRING_RESULT` (MySQL UDF limitation).
The binary data is valid geometry (SRID + WKB) and works directly with other spatial functions:

```sql
SELECT ST_AsText(STX_Translate(ST_GeomFromText('POINT(1 2)'), 10, 20));
-- POINT(11 22)
```

## Tests

```bash
make test       # Run test suite (361 tests)
```

## License

[GNU General Public License v2.0](LICENSE)
