# STX Spatial Plugin for MySQL

MySQL plugin that adds spatial functions (`STX_*`) powered by [Boost.Geometry](https://www.boost.org/doc/libs/release/libs/geometry/).
Our approach: implement functions broadly first, then refine through thorough testing.
Motto: *"Imperfect beats unavailable."*

Provides GIS functions missing from MySQL, including distance-based queries, spatial relationships (DE-9IM), coordinate transformations, I/O format conversions, and more. Supports both Cartesian and Geographic (WGS84) coordinate systems.

## Functions (32)

### Spatial Measurement & Predicates

| Function | Description |
|---|---|
| `STX_Perimeter(geom)` | Polygon/MultiPolygon perimeter |
| `STX_Coveredby(g1, g2)` | Tests if g1 is covered by g2 |
| `STX_Covers(g1, g2)` | Tests if g1 covers g2 |
| `STX_Dwithin(g1, g2, dist)` | Tests if distance between geometries <= threshold |
| `STX_Azimuth(p1, p2)` | Bearing from p1 to p2 (radians, clockwise from north) |
| `STX_Angle(p1, p2, p3)` | Angle at p2 formed by p1-p2-p3 |
| `STX_Relate(g1, g2)` | DE-9IM relationship matrix |
| `STX_Relatematch(g1, g2, pattern)` | Test DE-9IM pattern match |

### Geometry Processing

| Function | Description |
|---|---|
| `STX_Project(point, dist, azimuth)` | Project point by distance and bearing |
| `STX_Linelocatepoint(line, point)` | Fraction of line length at closest point |
| `STX_Linesubstring(line, start, end)` | Extract portion of linestring |
| `STX_Closestpoint(point, geom)` | Nearest point on geometry to given point |
| `STX_Pointonsurface(geom)` | Interior point of polygon |
| `STX_Makepoint(x, y [, srid])` | Create a point from coordinates |
| `STX_Generatepoints(geom, n [, seed])` | Generate random points inside polygon |
| `STX_Minimumboundingcircle(geom [, segs])` | Minimum bounding circle as polygon |
| `STX_Squaregrid(size, geom)` | Square grid covering bounding box |
| `STX_Hexgrid(size, geom)` | Hexagonal grid covering bounding box |

### Coordinate Transformations

| Function | Description |
|---|---|
| `STX_Translate(geom, dx, dy)` | Shift geometry by offset |
| `STX_Scale(geom, sx, sy)` | Scale geometry by factors |
| `STX_Rotate(geom, angle [, center])` | Rotate geometry (origin or specified center) |
| `STX_Affine(geom, a, b, d, e, xoff, yoff)` | General 2D affine transformation |
| `STX_Reverse(geom)` | Reverse vertex order |
| `STX_Snaptogrid(geom, size [, size_y])` | Snap coordinates to grid |
| `STX_Removerepeatedpoints(geom [, tol])` | Remove consecutive duplicate vertices |
| `STX_Segmentize(geom, max_length)` | Split long segments by adding vertices |

### I/O Format Conversion

| Function | Description |
|---|---|
| `STX_Asencodedpolyline(geom [, prec])` | Geometry to Google Encoded Polyline |
| `STX_Linefromenccodedpolyline(text [, srid [, prec]])` | Encoded Polyline to LineString |
| `STX_Assvg(geom [, rel [, prec]])` | Geometry to SVG path data |
| `STX_Askml(geom [, prec])` | Geometry to KML |
| `STX_Asewkt(geom)` | Geometry to EWKT (Extended WKT) |
| `STX_Geomfromewkt(text)` | EWKT to Geometry |

See [Function Reference](plugins/spatial_plugin/docs/function_reference.md) for full documentation.

## Requirements

- MySQL 8.0 or later
- g++ with C++17 support
- MySQL source tree (for headers and Boost.Geometry)
- MySQL binary installation (for `libmysqlservices` and plugin directory)

## Directory Structure

```
├── mysql960.source/        # MySQL source tree (headers + Boost)
├── mysql960/               # MySQL binary installation
└── plugins/
    ├── gis_lib/            # Shared GIS library (WKB parser, geometry types)
    └── spatial_plugin/     # Plugin source
        ├── spatial_plugin.cc
        ├── plugin_version.h
        ├── Makefile
        ├── test.sql
        └── docs/
            └── function_reference.md
```

The `mysql960.source/` and `mysql960/` directories are not included in this repository.
Download and extract them from [MySQL Downloads](https://dev.mysql.com/downloads/mysql/):

- **Source**: `mysql-9.6.0.tar.gz` — extract and build (`cmake` + `make`), then rename to `mysql960.source/`
- **Binary**: `mysql-9.6.0-linux-glibc2.28-x86_64.tar.xz` — extract and rename to `mysql960/`

Both directories should be placed at the repository root (siblings of `plugins/`).

## Pre-built Binary

A pre-built `spatial_plugin.so` is included for **MySQL 9.6.x on Linux (x86_64)**.
The binary is tied to the MySQL version it was compiled against and cannot be used with other versions.
To use a different MySQL version, rebuild from source (see below).

## Build

```bash
cd plugins/spatial_plugin
make            # Compile spatial_plugin.so
make install    # Copy .so to MySQL plugin directory
```

## Installation

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';
```

All 32 functions are registered automatically. No `CREATE FUNCTION` needed.

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
make test       # Run test suite (149 tests)
```

## License

[GNU General Public License v2.0](LICENSE)
