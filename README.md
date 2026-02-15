# STX Spatial Plugin for MySQL

MySQL plugin that adds spatial functions (`stx_*`) powered by [Boost.Geometry](https://www.boost.org/doc/libs/release/libs/geometry/).

Provides GIS functions missing from MySQL, including distance-based queries, spatial relationships (DE-9IM), coordinate transformations, and more. Supports both Cartesian and Geographic (WGS84) coordinate systems.

## Functions (17)

| Function | Description |
|---|---|
| `stx_perimeter(geom)` | Polygon/MultiPolygon perimeter |
| `stx_coveredby(g1, g2)` | Tests if g1 is covered by g2 |
| `stx_covers(g1, g2)` | Tests if g1 covers g2 |
| `stx_dwithin(g1, g2, dist)` | Tests if distance between geometries <= threshold |
| `stx_azimuth(p1, p2)` | Bearing from p1 to p2 (radians, clockwise from north) |
| `stx_project(point, dist, azimuth)` | Project point by distance and bearing |
| `stx_linelocatepoint(line, point)` | Fraction of line length at closest point |
| `stx_linesubstring(line, start, end)` | Extract portion of linestring |
| `stx_angle(p1, p2, p3)` | Angle at p2 formed by p1-p2-p3 |
| `stx_translate(geom, dx, dy)` | Shift geometry by offset |
| `stx_scale(geom, sx, sy)` | Scale geometry by factors |
| `stx_rotate(geom, angle [, center])` | Rotate geometry (origin or specified center) |
| `stx_reverse(geom)` | Reverse vertex order |
| `stx_pointonsurface(geom)` | Interior point of polygon |
| `stx_closestpoint(point, geom)` | Nearest point on geometry to given point |
| `stx_relate(g1, g2)` | DE-9IM relationship matrix |
| `stx_relatematch(g1, g2, pattern)` | Test DE-9IM pattern match |

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

All 17 functions are registered automatically. No `CREATE FUNCTION` needed.

```sql
-- Verify
SELECT * FROM performance_schema.user_defined_functions WHERE UDF_NAME LIKE 'stx_%';
SHOW STATUS LIKE 'spatial_plugin_%';
```

## Usage Examples

```sql
-- Perimeter of a polygon (geographic, result in meters)
SELECT stx_perimeter(ST_GeomFromText('POLYGON((139 35,140 35,140 36,139 36,139 35))', 4326));

-- Is point within 1km of another point?
SELECT stx_dwithin(
    ST_GeomFromText('POINT(139.7 35.7)', 4326),
    ST_GeomFromText('POINT(139.71 35.71)', 4326),
    1000
);

-- Rotate polygon 45 degrees around its centroid
SELECT stx_rotate(
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))'),
    PI()/4,
    ST_GeomFromText('POINT(5 5)')
);

-- DE-9IM spatial relationship
SELECT stx_relate(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))')
);
```

### Note on Return Types

Functions returning geometry use `STRING_RESULT` (MySQL UDF limitation).
The binary data is valid geometry (SRID + WKB) and works directly with other spatial functions:

```sql
SELECT ST_AsText(stx_translate(ST_GeomFromText('POINT(1 2)'), 10, 20));
-- POINT(11 22)
```

## Tests

```bash
make test       # Run test suite (85 tests)
```

## License

[GNU General Public License v2.0](LICENSE)
