# STX Spatial Functions リファレンス (Reference)

MySQL 9.6 用プラグイン `spatial_plugin` が提供する空間関数の仕様書。
A specification of spatial functions provided by the `spatial_plugin` plugin for MySQL 9.6.

## 概要 (Overview)

`stx_` プレフィックスの関数群は、MySQL に不足している空間演算機能を boost::geometry ライブラリを用いて実装したものである。`INSTALL PLUGIN` 時に全関数が自動登録される。
The `stx_`-prefixed functions implement spatial operations missing from MySQL, powered by the boost::geometry library. All functions are automatically registered upon `INSTALL PLUGIN`.

### 座標系サポート (Coordinate System Support)

すべての関数は Cartesian（平面直交座標系）と Geographic（地理座標系）の両方に対応する。
All functions support both Cartesian and Geographic coordinate systems.

| 座標系 (CS) | 距離・周長の単位 (Unit) | 対象SRID (Target SRIDs) |
|---|---|---|
| Cartesian | 座標単位・無次元 (coordinate units) | 0 and other non-geographic SRIDs |
| Geographic | メートル (meters) | 4326 (WGS84), 6668 (JGD2011), etc. (~500 SRIDs) |

Geographic 座標系では WGS84 楕円体上の測地線計算（Vincenty 法）を使用する。
Geographic calculations use Vincenty's formulae on the WGS84 ellipsoid.

### 共通仕様 (Common Behavior)

- 引数に `NULL` が含まれる場合、戻り値は `NULL`
  If any argument is `NULL`, the return value is `NULL`.
- ジオメトリ引数は MySQL の内部バイナリ形式（`ST_GeomFromText()` 等の戻り値）で渡す。
  Geometry arguments must be in MySQL internal binary format (i.e. values returned by `ST_GeomFromText()`, etc.).
- 2つのジオメトリを取る関数では、両引数の座標系が一致する必要がある。
  Functions taking two geometries require both to use the same coordinate system.
- 非対応のジオメトリ型が渡された場合はエラーとなる。
  Passing an unsupported geometry type results in an error.

---

## 関数一覧 (Function List)

| 関数名 (Function) | 戻り値型 (Return Type) | 概要 (Description) |
|---|---|---|
| [stx_perimeter](#stx_perimeter) | DOUBLE | ポリゴンの周長 / Perimeter of a polygon |
| [stx_coveredby](#stx_coveredby) | INTEGER | 包含判定 / Tests if geometry is covered by another |
| [stx_covers](#stx_covers) | INTEGER | 包含判定（逆） / Tests if geometry covers another |
| [stx_dwithin](#stx_dwithin) | INTEGER | 距離閾値判定 / Tests if distance is within threshold |
| [stx_azimuth](#stx_azimuth) | DOUBLE | 2点間の方位角 / Azimuth between two points |
| [stx_project](#stx_project) | GEOMETRY | 点の投影 / Projects a point by distance and azimuth |
| [stx_linelocatepoint](#stx_linelocatepoint) | DOUBLE | 線上の最近接位置 / Fraction of closest point on line |
| [stx_linesubstring](#stx_linesubstring) | GEOMETRY | 線の部分抽出 / Extracts a portion of a line |
| [stx_angle](#stx_angle) | DOUBLE | 3点がなす角度 / Angle formed by three points |
| [stx_translate](#stx_translate) | GEOMETRY | 平行移動 / Translates a geometry by dx, dy |
| [stx_scale](#stx_scale) | GEOMETRY | スケール変換 / Scales a geometry by sx, sy |
| [stx_rotate](#stx_rotate) | GEOMETRY | 回転 / Rotates a geometry by angle |
| [stx_reverse](#stx_reverse) | GEOMETRY | 頂点順逆転 / Reverses vertex order |
| [stx_pointonsurface](#stx_pointonsurface) | GEOMETRY | 内部保証点 / Guaranteed interior point |
| [stx_closestpoint](#stx_closestpoint) | GEOMETRY | 最近接点 / Closest point on geometry |
| [stx_relate](#stx_relate) | STRING | DE-9IM 行列 / DE-9IM matrix string |
| [stx_relatematch](#stx_relatematch) | INTEGER | DE-9IM パターン判定 / DE-9IM pattern match |
| [stx_makepoint](#stx_makepoint) | GEOMETRY | 座標から POINT 構築 / Create Point from coordinates |
| [stx_affine](#stx_affine) | GEOMETRY | アフィン変換 / General 2D affine transformation |
| [stx_snaptogrid](#stx_snaptogrid) | GEOMETRY | 座標丸め / Snap coordinates to grid |
| [stx_removerepeatedpoints](#stx_removerepeatedpoints) | GEOMETRY | 重複頂点除去 / Remove consecutive duplicates |
| [stx_segmentize](#stx_segmentize) | GEOMETRY | 線分分割 / Split segments to max length |
| [stx_generatepoints](#stx_generatepoints) | GEOMETRY | ランダム点生成 / Random points in polygon |
| [stx_asencodedpolyline](#stx_asencodedpolyline) | STRING | Encoded Polyline 出力 / Geometry to Encoded Polyline |
| [stx_linefromenccodedpolyline](#stx_linefromenccodedpolyline) | GEOMETRY | Encoded Polyline 入力 / Encoded Polyline to LineString |
| [stx_assvg](#stx_assvg) | STRING | SVG 出力 / Geometry to SVG path data |
| [stx_askml](#stx_askml) | STRING | KML 出力 / Geometry to KML |
| [stx_asewkt](#stx_asewkt) | STRING | EWKT 出力 / Geometry to EWKT |
| [stx_geomfromewkt](#stx_geomfromewkt) | GEOMETRY | EWKT 入力 / EWKT to Geometry |

---

## 関数詳細 (Function Details)

### stx_perimeter

ポリゴンまたはマルチポリゴンの周長を返す。
Returns the perimeter of a Polygon or MultiPolygon.

```sql
stx_perimeter(geometry) -> DOUBLE
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Polygon または MultiPolygon / Polygon or MultiPolygon |

#### 戻り値 (Return Value)

- Cartesian: 座標単位での周長 / Perimeter in coordinate units
- Geographic: メートル単位での測地線周長 / Geodesic perimeter in meters (WGS84)
- Polygon/MultiPolygon 以外の場合は `0.0` / Returns `0.0` for other geometry types

#### 使用例 (Examples)

```sql
-- Cartesian: 10x10 の正方形 → 周長 40
-- Cartesian: 10x10 square -> perimeter 40
SELECT stx_perimeter(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 40.0

-- Geographic (SRID 4326): 赤道付近の 10度 x 10度 → 約4,421km
-- Geographic: 10deg x 10deg near equator -> ~4,421km
SELECT stx_perimeter(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))', 4326));
-- 4421233.315...

-- MultiPolygon: 各ポリゴンの周長の合計
-- MultiPolygon: sum of perimeters of each polygon
SELECT stx_perimeter(
  ST_GeomFromText('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)),
                                 ((20 20,30 20,30 30,20 30,20 20)))'));
-- 80.0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Perimeter()`
- MySQL 標準: なし (not available in MySQL 9.6)

---

### stx_coveredby

第1引数のジオメトリが第2引数のジオメトリに完全に包含されるかを判定する。境界上の点も包含とみなす（`ST_Within` との違い）。
Tests whether geometry1 is completely covered by geometry2. Points on the boundary are considered covered (unlike `ST_Within`).

```sql
stx_coveredby(geometry1, geometry2) -> INTEGER
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 判定対象 / Geometry to test |
| geometry2 | GEOMETRY | 包含する側 / Covering geometry |

#### 戻り値 (Return Value)

- `1`: geometry1 が geometry2 に包含される / geometry1 is covered by geometry2
- `0`: 包含されない / not covered

#### 対応するジオメトリ型の組み合わせ (Supported Type Combinations)

| geometry1 | geometry2 |
|---|---|
| Point | Point, LineString, Polygon, MultiPolygon |
| LineString | LineString, Polygon, MultiPolygon |
| Polygon | Polygon, MultiPolygon |
| MultiPoint | Polygon, MultiPolygon |

#### 使用例 (Examples)

```sql
-- 内部の点 → 1 / Interior point -> 1
SELECT stx_coveredby(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 1

-- 境界上の点 → 1（coveredby は境界を含む）
-- Point on boundary -> 1 (coveredby includes boundary)
SELECT stx_coveredby(
  ST_GeomFromText('POINT(0 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 1

-- 外部の点 → 0 / Exterior point -> 0
SELECT stx_coveredby(
  ST_GeomFromText('POINT(15 15)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_CoveredBy()`
- MySQL 標準: `MBRCoveredBy()`（MBR ベースのみ / MBR-based only, not exact geometry test）

---

### stx_covers

第1引数のジオメトリが第2引数のジオメトリを完全に包含するかを判定する。
Tests whether geometry1 completely covers geometry2.

`stx_coveredby` の引数を入れ替えた関数。
Equivalent to `stx_coveredby` with swapped arguments.

```sql
stx_covers(geometry1, geometry2) -> INTEGER
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 包含する側 / Covering geometry |
| geometry2 | GEOMETRY | 判定対象 / Geometry to test |

#### 戻り値 (Return Value)

- `1`: geometry1 が geometry2 を包含する / geometry1 covers geometry2
- `0`: 包含しない / does not cover

#### 使用例 (Examples)

```sql
-- ポリゴンが内部の点を包含 → 1
-- Polygon covers an interior point -> 1
SELECT stx_covers(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  ST_GeomFromText('POINT(5 5)'));
-- 1
```

#### 備考 (Notes)

内部的に `stx_coveredby(geometry2, geometry1)` と等価。
Internally equivalent to `stx_coveredby(geometry2, geometry1)`.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Covers()`

---

### stx_dwithin

2つのジオメトリ間の最短距離が指定した閾値以内であるかを判定する。
Tests whether the minimum distance between two geometries is within a given threshold.

```sql
stx_dwithin(geometry1, geometry2, distance_threshold) -> INTEGER
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 1つ目のジオメトリ / First geometry |
| geometry2 | GEOMETRY | 2つ目のジオメトリ / Second geometry |
| distance_threshold | DOUBLE | 距離の閾値 / Distance threshold |

- Cartesian: 閾値は座標単位 / Threshold in coordinate units
- Geographic: 閾値はメートル / Threshold in meters

#### 戻り値 (Return Value)

- `1`: 距離が閾値以下 / Distance is within threshold
- `0`: 閾値を超える / Distance exceeds threshold

#### 対応するジオメトリ型 (Supported Geometry Types)

すべてのジオメトリ型の組み合わせに対応。
All geometry type combinations are supported (Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon).

#### 使用例 (Examples)

```sql
-- 2点間の距離 5.0、閾値 5.0 → 以内
-- Distance between points is 5.0, threshold 5.0 -> within
SELECT stx_dwithin(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(3 4)'), 5.0);
-- 1

-- 閾値を下回る → 0
-- Threshold too small -> 0
SELECT stx_dwithin(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(3 4)'), 4.9);
-- 0

-- Geographic: 赤道上で経度1度（約111km）、閾値200km
-- Geographic: 1 degree at equator (~111km), threshold 200km
SELECT stx_dwithin(
  ST_GeomFromText('POINT(0 0)', 4326),
  ST_GeomFromText('POINT(1 0)', 4326), 200000.0);
-- 1
```

#### 備考 (Notes)

`ST_Distance() <= threshold` と同等だが、内部で距離計算と比較を一度に行う。
Equivalent to `ST_Distance() <= threshold`, but performs distance computation and comparison in a single call.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_DWithin()`

---

### stx_azimuth

2つの点の間の方位角（北を基準に時計回り）をラジアンで返す。
Returns the azimuth (bearing) from point1 to point2 in radians, measured clockwise from north.

```sql
stx_azimuth(point1, point2) -> DOUBLE
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| point1 | POINT | 起点 / Origin point |
| point2 | POINT | 終点 / Destination point |

#### 戻り値 (Return Value)

方位角をラジアンで返す（0 ~ 2&pi;）。
Returns azimuth in radians (range: 0 to 2&pi;).

| 方位 (Direction) | ラジアン (Radians) | 度 (Degrees) |
|---|---|---|
| 北 (North) | 0 | 0 |
| 東 (East) | &pi;/2 | 90 |
| 南 (South) | &pi; | 180 |
| 西 (West) | 3&pi;/2 | 270 |

- Cartesian: `atan2(dx, dy)` で計算 / Computed via `atan2(dx, dy)`
- Geographic: Vincenty の逆解法 / Vincenty inverse formula on WGS84 ellipsoid

#### 使用例 (Examples)

```sql
-- Cartesian: 東方向 → π/2
-- Cartesian: eastward -> pi/2
SELECT stx_azimuth(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(1 0)'));
-- 1.5707963267948966

-- Geographic: 東京→大阪 方向
-- Geographic: bearing from Tokyo to Osaka
SELECT DEGREES(stx_azimuth(
  ST_GeomFromText('POINT(35.6762 139.6503)', 4326),
  ST_GeomFromText('POINT(34.6937 135.5023)', 4326)));
-- ~248 (WSW)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Azimuth()`

---

### stx_project

点を指定した距離と方位角の方向に投影（移動）した新しい点を返す。
Returns a new point projected from the input point by a given distance and azimuth.

```sql
stx_project(point, distance, azimuth) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| point | POINT | 起点 / Origin point |
| distance | DOUBLE | 移動距離 / Distance (Geographic: meters, Cartesian: coordinate units) |
| azimuth | DOUBLE | 方位角（ラジアン、北=0、時計回り） / Azimuth in radians (north=0, clockwise) |

#### 戻り値 (Return Value)

投影先の Point ジオメトリ（入力と同じ SRID）。
A Point geometry at the projected location (same SRID as input).

- Cartesian: 三角関数による平面投影 / Planar projection via trigonometry
- Geographic: Vincenty の順解法 / Vincenty direct formula on WGS84 ellipsoid

#### 使用例 (Examples)

```sql
-- Cartesian: 北に 10 / 10 units north
SELECT ST_AsText(stx_project(
  ST_GeomFromText('POINT(0 0)'), 10.0, 0.0));
-- POINT(0 10)

-- Geographic: 東京から北に 1km / 1km north from Tokyo
SELECT ST_AsText(stx_project(
  ST_GeomFromText('POINT(35.6762 139.6503)', 4326), 1000.0, 0.0));
-- POINT(35.685... 139.6503...)

-- stx_azimuth と組み合わせ：A地点からB地点方向に100m移動
-- Combined with stx_azimuth: move 100m from A toward B
SELECT ST_AsText(stx_project(
  ST_GeomFromText('POINT(35.6762 139.6503)', 4326),
  100.0,
  stx_azimuth(
    ST_GeomFromText('POINT(35.6762 139.6503)', 4326),
    ST_GeomFromText('POINT(34.6937 135.5023)', 4326)
  )
));
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Project()`

---

### stx_linelocatepoint

ライン上で指定した点に最も近い位置を、ラインの始点からの比率（0.0 ~ 1.0）で返す。
Returns the fraction (0.0 to 1.0) of the line's total length at the point closest to the given point.

```sql
stx_linelocatepoint(line, point) -> DOUBLE
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| line | LINESTRING | 対象のライン / Target line |
| point | POINT | 位置を求める点 / Point to locate |

#### 戻り値 (Return Value)

始点を 0.0、終点を 1.0 とした比率。点がライン上にない場合は、ラインへの垂線の足の位置を返す。
Fraction from start (0.0) to end (1.0). If the point is not on the line, returns the fraction at the perpendicular foot (nearest point on the line).

#### 使用例 (Examples)

```sql
-- ラインの中央 → 0.5 / Midpoint of line -> 0.5
SELECT stx_linelocatepoint(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'),
  ST_GeomFromText('POINT(5 0)'));
-- 0.5

-- ライン上にない点（垂線の足の位置を返す）
-- Point not on line (returns perpendicular foot position)
SELECT stx_linelocatepoint(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'),
  ST_GeomFromText('POINT(2.5 5)'));
-- 0.25

-- 複数セグメントのライン / Multi-segment line
SELECT stx_linelocatepoint(
  ST_GeomFromText('LINESTRING(0 0, 5 0, 10 0)'),
  ST_GeomFromText('POINT(5 0)'));
-- 0.5
```

#### 備考 (Notes)

`stx_linesubstring` と組み合わせることで、点の近傍でラインを分割する等の操作が可能。
Can be combined with `stx_linesubstring` to split a line near a given point.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineLocatePoint()`

---

### stx_linesubstring

ラインの指定区間（始点・終点を比率で指定）を新しい LineString として返す。
Returns a sub-linestring between the specified start and end fractions.

```sql
stx_linesubstring(line, start_fraction, end_fraction) -> GEOMETRY (LineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| line | LINESTRING | 対象のライン / Target line |
| start_fraction | DOUBLE | 始点の比率 0.0~1.0 / Start fraction (0.0 to 1.0) |
| end_fraction | DOUBLE | 終点の比率 0.0~1.0 / End fraction (0.0 to 1.0, must be > start) |

比率はラインの全長に対する割合。0.0 = 始点、1.0 = 終点。
Fractions represent the proportion of total line length. 0.0 = start, 1.0 = end.

#### 戻り値 (Return Value)

指定区間の LineString ジオメトリ（入力と同じ SRID）。セグメント境界をまたぐ場合は中間の頂点を含む。
A LineString geometry for the specified interval (same SRID as input). Intermediate vertices are included when the interval spans segment boundaries.

#### 使用例 (Examples)

```sql
-- 中央の半分を抽出 / Extract middle half
SELECT ST_AsText(stx_linesubstring(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 0.25, 0.75));
-- LINESTRING(2.5 0,7.5 0)

-- 全体 / Full line
SELECT ST_AsText(stx_linesubstring(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 0.0, 1.0));
-- LINESTRING(0 0,10 0)

-- セグメント境界をまたぐ区間 / Interval crossing segment boundary
SELECT ST_AsText(stx_linesubstring(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 0.25, 0.75));
-- LINESTRING(5 0,10 0,10 5)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineSubstring()`

---

### stx_angle

3つの点 P1, P2, P3 において、P2 を頂点とする角度をラジアンで返す。
Returns the angle at P2 formed by the rays P2→P1 and P2→P3, in radians.

```sql
stx_angle(point1, point2, point3) -> DOUBLE
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| point1 | POINT | 1つ目の点 / First point (P1) |
| point2 | POINT | 頂点 / Vertex point (P2) |
| point3 | POINT | 3つ目の点 / Third point (P3) |

#### 戻り値 (Return Value)

ベクトル P2→P1 から P2→P3 への反時計回りの角度をラジアンで返す（0 ~ 2&pi;）。
Returns the counterclockwise angle from vector P2→P1 to P2→P3 in radians (range: 0 to 2&pi;).

#### 使用例 (Examples)

```sql
-- 直角（π/2 = 90°） / Right angle (pi/2 = 90 degrees)
SELECT stx_angle(
  ST_GeomFromText('POINT(1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(0 1)'));
-- 1.5707963267948966

-- 直線（π = 180°） / Straight line (pi = 180 degrees)
SELECT stx_angle(
  ST_GeomFromText('POINT(1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(-1 0)'));
-- 3.141592653589793

-- 度に変換 / Convert to degrees
SELECT DEGREES(stx_angle(
  ST_GeomFromText('POINT(1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(0 1)')));
-- 90.0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Angle()`

---

### stx_translate

ジオメトリを指定した (dx, dy) だけ平行移動する。
Translates (shifts) a geometry by the given dx, dy offsets.

```sql
stx_translate(geometry, dx, dy) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| dx | DOUBLE | X 方向の移動量 / X offset |
| dy | DOUBLE | Y 方向の移動量 / Y offset |

#### 戻り値 (Return Value)

平行移動後のジオメトリ（入力と同じ型・SRID）。全ジオメトリ型に対応。
The translated geometry (same type and SRID as input). All geometry types are supported.

#### 使用例 (Examples)

```sql
-- 点の移動 / Translate a point
SELECT ST_AsText(stx_translate(
  ST_GeomFromText('POINT(1 2)'), 10, 20));
-- POINT(11 22)

-- ポリゴンの移動 / Translate a polygon
SELECT ST_AsText(stx_translate(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 5, 5));
-- POLYGON((5 5,5 15,15 15,15 5,5 5))
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Translate()`

---

### stx_scale

ジオメトリを原点を基準に (sx, sy) でスケール変換する。
Scales a geometry by sx, sy factors relative to the origin.

```sql
stx_scale(geometry, sx, sy) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| sx | DOUBLE | X 方向のスケール係数 / X scale factor |
| sy | DOUBLE | Y 方向のスケール係数 / Y scale factor |

#### 戻り値 (Return Value)

スケール変換後のジオメトリ（入力と同じ型・SRID）。全ジオメトリ型に対応。
The scaled geometry (same type and SRID as input). All geometry types are supported.

#### 使用例 (Examples)

```sql
-- 点の拡大 / Scale a point
SELECT ST_AsText(stx_scale(
  ST_GeomFromText('POINT(3 4)'), 2, 3));
-- POINT(6 12)

-- ラインの縮小 / Shrink a linestring
SELECT ST_AsText(stx_scale(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 0.5, 0.5));
-- LINESTRING(0 0,5 0,5 5)
```

#### 備考 (Notes)

スケールの基準は原点 (0, 0)。別の点を基準にしたい場合は `stx_translate` で原点に移動し、スケール後に戻す。
Scaling is relative to the origin (0, 0). To scale around a different center, use `stx_translate` to shift to origin, scale, then shift back.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Scale()`

---

### stx_rotate

ジオメトリを原点を中心に指定した角度（ラジアン）だけ回転する。
Rotates a geometry by the given angle (radians) around the origin.

```sql
stx_rotate(geometry, angle) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| angle | DOUBLE | 回転角度（ラジアン、反時計回りが正） / Rotation angle in radians (positive = counterclockwise) |

#### 戻り値 (Return Value)

回転後のジオメトリ（入力と同じ型・SRID）。全ジオメトリ型に対応。
The rotated geometry (same type and SRID as input). All geometry types are supported.

#### 使用例 (Examples)

```sql
-- 点 (1,0) を 90° 回転 → (0,1) / Rotate (1,0) by 90 degrees -> (0,1)
SELECT ST_AsText(stx_rotate(
  ST_GeomFromText('POINT(1 0)'), PI() / 2));
-- POINT(0 1)  (浮動小数点精度による近似 / approximate due to floating point)

-- 点 (1,0) を 180° 回転 → (-1,0) / Rotate 180 degrees -> (-1,0)
SELECT ST_AsText(stx_rotate(
  ST_GeomFromText('POINT(1 0)'), PI()));
-- POINT(-1 0)
```

#### 備考 (Notes)

回転の中心は原点 (0, 0)。別の点を中心に回転したい場合は `stx_translate` で原点に移動し、回転後に戻す。
Rotation center is the origin (0, 0). To rotate around a different center, use `stx_translate` to shift to origin, rotate, then shift back.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Rotate()`

---

### stx_reverse

ジオメトリの頂点順序を逆転する。
Reverses the vertex order of a geometry.

```sql
stx_reverse(geometry) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |

#### 戻り値 (Return Value)

頂点順序を逆転したジオメトリ（入力と同じ型・SRID）。LineString, Polygon（外環・内環）に有効。MultiPoint は順序の概念がないため変化しない。
A geometry with reversed vertex order (same type and SRID). Effective for LineString, Polygon (outer/inner rings). MultiPoint has no ordering so remains unchanged.

#### 使用例 (Examples)

```sql
-- ラインの逆転 / Reverse a linestring
SELECT ST_AsText(stx_reverse(
  ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)')));
-- LINESTRING(2 2,1 1,0 0)

-- Geographic / Geographic linestring
SELECT ST_AsText(stx_reverse(
  ST_GeomFromText('LINESTRING(0 0, 1 0, 1 1)', 4326)));
-- LINESTRING(1 1,1 0,0 0)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Reverse()`

---

### stx_pointonsurface

ポリゴン（またはマルチポリゴン）の内部に位置することが保証された点を返す。
Returns a point guaranteed to lie in the interior of a polygon (or multipolygon).

```sql
stx_pointonsurface(geometry) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Polygon または MultiPolygon / Polygon or MultiPolygon |

#### 戻り値 (Return Value)

ポリゴン内部にある Point ジオメトリ（入力と同じ SRID）。凹型ポリゴンでも内部に位置する点を返す。MultiPolygon の場合は最大面積のポリゴンを使用。
A Point inside the polygon (same SRID as input). Returns an interior point even for concave polygons. For MultiPolygon, uses the largest polygon by area.

#### アルゴリズム (Algorithm)

1. ポリゴン頂点の座標平均（重心近似）を計算
2. 重心がポリゴン内部にあればそれを返す
3. 重心が外部の場合（凹型等）、重心のY座標での水平線スキャンで内部点を探索

#### 使用例 (Examples)

```sql
-- 正方形の重心 / Centroid of a square
SELECT ST_AsText(stx_pointonsurface(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- POINT(5 5)

-- L字型凹ポリゴン（重心は外部だが、内部点を返す）
-- L-shaped concave polygon (centroid is outside, but returns interior point)
SELECT ST_AsText(stx_pointonsurface(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 5 5, 5 10, 0 10, 0 0))')));
-- POINT(2.5 5)

-- 結果は必ずポリゴン内部にある / Result is always inside the polygon
SELECT stx_coveredby(
  stx_pointonsurface(ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 5 5, 5 10, 0 10, 0 0))')),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 5 5, 5 10, 0 10, 0 0))'));
-- 1
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_PointOnSurface()`

---

### stx_closestpoint

第1引数の点から、第2引数のジオメトリ上で最も近い点を返す。
Returns the closest point on geometry2 to the given point (geometry1).

```sql
stx_closestpoint(point, geometry) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| point | POINT | 基準点 / Reference point (must be Point type) |
| geometry | GEOMETRY | 対象ジオメトリ / Target geometry |

#### 戻り値 (Return Value)

第2引数のジオメトリ上（または内部）で最も近い Point。点がポリゴン内部にある場合は入力点そのものを返す。
The closest Point on (or inside) geometry2. If the point is inside a polygon, returns the point itself.

#### 対応するジオメトリ型 (Supported Types for geometry2)

Point, LineString, Polygon, MultiPolygon

#### 使用例 (Examples)

```sql
-- 点からラインへの最近接点 / Closest point on line
SELECT ST_AsText(stx_closestpoint(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('LINESTRING(0 0, 10 0)')));
-- POINT(5 0)

-- 点からポリゴン境界への最近接点 / Closest point on polygon boundary
SELECT ST_AsText(stx_closestpoint(
  ST_GeomFromText('POINT(15 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- POINT(10 5)

-- 点がポリゴン内部 → 自身を返す / Point inside polygon -> returns self
SELECT ST_AsText(stx_closestpoint(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- POINT(5 5)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_ClosestPoint()`

---

### stx_relate

2つのジオメトリの空間関係を DE-9IM（Dimensionally Extended 9-Intersection Model）行列文字列として返す。
Returns the DE-9IM matrix string describing the spatial relationship between two geometries.

```sql
stx_relate(geometry1, geometry2) -> STRING (9 chars)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 1つ目のジオメトリ / First geometry |
| geometry2 | GEOMETRY | 2つ目のジオメトリ / Second geometry |

#### 戻り値 (Return Value)

9文字の DE-9IM 行列文字列。各文字は交差の次元を表す: `F`（空）、`0`（点）、`1`（線）、`2`（面）。
A 9-character DE-9IM matrix string. Each character represents the dimension of the intersection: `F` (empty), `0` (point), `1` (line), `2` (area).

行列の各位置:
Matrix positions: `II IB IE BI BB BE EI EB EE` (I=Interior, B=Boundary, E=Exterior)

#### 対応するジオメトリ型 (Supported Type Combinations)

Point, LineString, Polygon の全組み合わせ。
All combinations of Point, LineString, and Polygon.

#### 使用例 (Examples)

```sql
-- 点がポリゴン内部 / Point inside polygon
SELECT stx_relate(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 0FFFFF212

-- 離散ポリゴン / Disjoint polygons
SELECT stx_relate(
  ST_GeomFromText('POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'),
  ST_GeomFromText('POLYGON((5 5, 6 5, 6 6, 5 6, 5 5))'));
-- FF2FF1212

-- 同一点 / Equal points
SELECT stx_relate(
  ST_GeomFromText('POINT(1 1)'),
  ST_GeomFromText('POINT(1 1)'));
-- 0FFFFFFF2
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Relate()`
- MySQL 標準: なし

---

### stx_relatematch

2つのジオメトリの DE-9IM 行列が指定したパターンに一致するかを判定する。
Tests whether the DE-9IM matrix of two geometries matches a given pattern.

```sql
stx_relatematch(geometry1, geometry2, pattern) -> INTEGER
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 1つ目のジオメトリ / First geometry |
| geometry2 | GEOMETRY | 2つ目のジオメトリ / Second geometry |
| pattern | STRING | DE-9IM パターン文字列（9文字） / DE-9IM pattern string (9 chars) |

パターン文字: `T`（≠F: 交差あり）, `F`（交差なし）, `*`（任意）, `0`/`1`/`2`（次元一致）
Pattern characters: `T` (non-empty intersection), `F` (empty), `*` (any), `0`/`1`/`2` (exact dimension)

#### 戻り値 (Return Value)

- `1`: パターンに一致 / Pattern matches
- `0`: パターンに不一致 / Pattern does not match

#### 代表的なパターン (Common Patterns)

| 関係 (Relation) | パターン (Pattern) |
|---|---|
| Within | `T*F**F***` |
| Contains | `T*****FF*` |
| Intersects | `T********` |
| Disjoint | `FF*FF****` |
| Touches | `FT******* ` or `F**T***** ` or `F***T****` |
| Overlaps (area) | `T*T***T**` |

#### 使用例 (Examples)

```sql
-- Within 判定 / Test within
SELECT stx_relatematch(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  'T*F**F***');
-- 1

-- Disjoint 判定 / Test disjoint
SELECT stx_relatematch(
  ST_GeomFromText('POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'),
  ST_GeomFromText('POLYGON((5 5, 6 5, 6 6, 5 6, 5 5))'),
  'FF*FF****');
-- 1
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_RelateMatch()`
- MySQL 標準: なし

---

### stx_makepoint

座標値から POINT ジオメトリを構築する。
Creates a Point geometry from X and Y coordinate values.

```sql
stx_makepoint(x, y [, srid]) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| x | DOUBLE | X 座標（経度） / X coordinate (longitude) |
| y | DOUBLE | Y 座標（緯度） / Y coordinate (latitude) |
| srid | INTEGER | (任意) SRID。省略時は 0 / (Optional) SRID. Defaults to 0 |

#### 戻り値 (Return Value)

指定した座標の Point ジオメトリ。WKB 形式で直接構築するため高速。
A Point geometry at the specified coordinates. Constructed directly as WKB for efficiency.

#### 使用例 (Examples)

```sql
-- 基本的な使用法 / Basic usage
SELECT ST_AsText(stx_makepoint(139.7, 35.6));
-- POINT(139.7 35.6)

-- SRID 指定 / With SRID
SELECT ST_AsText(stx_makepoint(139.7, 35.6, 4326));
-- POINT(35.6 139.7)  (4326 uses lat,lon display order)

SELECT ST_SRID(stx_makepoint(139.7, 35.6, 4326));
-- 4326
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_MakePoint()`
- MySQL 標準: なし

---

### stx_affine

ジオメトリに一般的な 2D アフィン変換を適用する。
Applies a general 2D affine transformation to a geometry.

```sql
stx_affine(geometry, a, b, d, e, xoff, yoff) -> GEOMETRY
```

変換式 / Transformation: `x' = a*x + b*y + xoff`, `y' = d*x + e*y + yoff`

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| a | DOUBLE | x→x' 係数 / x-to-x' coefficient |
| b | DOUBLE | y→x' 係数 / y-to-x' coefficient |
| d | DOUBLE | x→y' 係数 / x-to-y' coefficient |
| e | DOUBLE | y→y' 係数 / y-to-y' coefficient |
| xoff | DOUBLE | X オフセット / X offset |
| yoff | DOUBLE | Y オフセット / Y offset |

#### 特殊ケース (Special Cases)

| 変換 (Transform) | パラメータ (Parameters) |
|---|---|
| 平行移動 (Translate) | `stx_affine(geom, 1, 0, 0, 1, dx, dy)` |
| スケール (Scale) | `stx_affine(geom, sx, 0, 0, sy, 0, 0)` |
| 回転 (Rotate) | `stx_affine(geom, cos(a), -sin(a), sin(a), cos(a), 0, 0)` |
| 反転 (Reflect Y) | `stx_affine(geom, 1, 0, 0, -1, 0, 0)` |

#### 使用例 (Examples)

```sql
-- せん断変換 / Shear transform
SELECT ST_AsText(stx_affine(
  ST_GeomFromText('POINT(1 0)'), 1, 2, 0, 1, 0, 0));
-- POINT(1 1)

-- 恒等変換 / Identity transform
SELECT ST_AsText(stx_affine(
  ST_GeomFromText('POINT(3 4)'), 1, 0, 0, 1, 0, 0));
-- POINT(3 4)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Affine()`

---

### stx_snaptogrid

ジオメトリの全座標を指定したグリッドサイズに丸める。
Snaps all coordinates of a geometry to a grid of the specified size.

```sql
stx_snaptogrid(geometry, size) -> GEOMETRY
stx_snaptogrid(geometry, size_x, size_y) -> GEOMETRY
```

変換式 / Transformation: `x' = round(x / size) * size`

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| size | DOUBLE | グリッドサイズ（x, y 共通） / Grid cell size (both axes) |
| size_x | DOUBLE | X 方向グリッドサイズ / Grid cell size for X axis |
| size_y | DOUBLE | Y 方向グリッドサイズ / Grid cell size for Y axis |

#### 備考 (Notes)

- `size = 0` の場合、その軸の座標は変更しない / `size = 0` leaves coordinates unchanged for that axis

#### 使用例 (Examples)

```sql
-- 0.5 単位にスナップ / Snap to 0.5 grid
SELECT ST_AsText(stx_snaptogrid(
  ST_GeomFromText('POINT(1.3 2.7)'), 0.5));
-- POINT(1.5 2.5)

-- X と Y で異なるサイズ / Different X and Y sizes
SELECT ST_AsText(stx_snaptogrid(
  ST_GeomFromText('POINT(1.3 2.7)'), 1, 0.5));
-- POINT(1 2.5)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_SnapToGrid()`

---

### stx_removerepeatedpoints

ジオメトリから連続する重複頂点を除去する。
Removes consecutive duplicate vertices from a geometry.

```sql
stx_removerepeatedpoints(geometry) -> GEOMETRY
stx_removerepeatedpoints(geometry, tolerance) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| tolerance | DOUBLE | (任意) 距離がこの値以下の連続頂点を除去 / (Optional) Remove consecutive vertices within this distance |

#### 備考 (Notes)

- 1引数版: 完全一致の連続重複のみ除去 / 1-argument form: removes exact consecutive duplicates only
- 2引数版: tolerance 以内の連続頂点を除去 / 2-argument form: removes consecutive vertices within tolerance
- LineString は最低2点、Polygon のリングは最低4点を保持 / LineString keeps minimum 2 points, Polygon rings keep minimum 4 points

#### 使用例 (Examples)

```sql
-- 完全一致の重複除去 / Remove exact duplicates
SELECT ST_AsText(stx_removerepeatedpoints(
  ST_GeomFromText('LINESTRING(0 0, 0 0, 1 1, 1 1, 2 2)')));
-- LINESTRING(0 0,1 1,2 2)

-- tolerance 指定 / With tolerance
SELECT ST_AsText(stx_removerepeatedpoints(
  ST_GeomFromText('LINESTRING(0 0, 0.1 0, 1 0, 1.05 0, 2 0)'), 0.2));
-- LINESTRING(0 0,1 0,2 0)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_RemoveRepeatedPoints()`

---

### stx_segmentize

ジオメトリのすべての辺を、指定した最大長以下になるよう分割（頂点を挿入）する。
Splits all segments of a geometry by inserting vertices so that no segment exceeds the specified maximum length.

```sql
stx_segmentize(geometry, max_length) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| max_length | DOUBLE | セグメントの最大長 / Maximum segment length (Cartesian: coordinate units, Geographic: meters) |

#### 備考 (Notes)

- 既に max_length 以下のセグメントは変更しない / Segments already shorter than max_length are unchanged
- 分割は均等に行われる（max_length ちょうどではなく、元の辺を等分割） / Segments are split evenly (not at exact max_length boundaries)
- Point / MultiPoint は辺を持たないためそのまま返す / Point/MultiPoint have no segments and are returned as-is
- Geographic 座標系では測地線上の内挿を行う / Geographic coordinates interpolate along geodesics

#### 使用例 (Examples)

```sql
-- 10 単位のラインを最大 3 で分割 / Split 10-unit line at max 3
SELECT ST_AsText(stx_segmentize(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 3));
-- LINESTRING(0 0,2.5 0,5 0,7.5 0,10 0)

-- ポリゴンの辺を分割 / Densify polygon edges
SELECT ST_AsText(stx_segmentize(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 6));
-- POLYGON((0 0,0 5,0 10,5 10,10 10,10 5,10 0,5 0,0 0))
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Segmentize()`

---

### stx_generatepoints

ポリゴンまたはマルチポリゴンの内部にランダムな点を生成し、MultiPoint として返す。
Generates random points inside a Polygon or MultiPolygon and returns them as a MultiPoint.

```sql
stx_generatepoints(geometry, npoints [, seed]) -> GEOMETRY (MultiPoint)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Polygon または MultiPolygon / Polygon or MultiPolygon |
| npoints | INTEGER | 生成する点の数 / Number of points to generate |
| seed | INTEGER | (任意) 乱数シード（再現性用） / (Optional) Random seed for reproducibility |

#### 戻り値 (Return Value)

指定した数の点を含む MultiPoint ジオメトリ（入力と同じ SRID）。
A MultiPoint geometry with the specified number of points (same SRID as input).

#### アルゴリズム (Algorithm)

バウンディングボックス内でランダムな点を生成し、ポリゴン内部にある点のみを採用する棄却法を使用。MultiPolygon の場合は面積比で各ポリゴンに点数を配分。
Uses rejection sampling within the bounding box. For MultiPolygon, distributes points proportional to polygon areas.

#### 使用例 (Examples)

```sql
-- 5点を生成 / Generate 5 points
SELECT ST_AsText(stx_generatepoints(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 5, 42));

-- シード指定で再現性を確保 / Reproducible with same seed
SELECT ST_AsText(stx_generatepoints(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 3, 123))
=
SELECT ST_AsText(stx_generatepoints(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 3, 123));
-- Always identical
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_GeneratePoints()`

---

### stx_asencodedpolyline

LineString ジオメトリを Google Encoded Polyline Algorithm Format の文字列に変換する。
Converts a LineString geometry to a Google Encoded Polyline Algorithm Format string.

```sql
stx_asencodedpolyline(geometry [, precision]) -> STRING
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | LineString ジオメトリ / LineString geometry |
| precision | INTEGER | (任意) 精度（10^precision で丸め）。デフォルト 5 / (Optional) Precision (coordinates multiplied by 10^precision). Default: 5 |

#### 備考 (Notes)

- Encoded Polyline 形式は (latitude, longitude) 順でエンコードする / Encoded Polyline format encodes in (latitude, longitude) order
- Google Maps API との連携に使用 / Used for integration with Google Maps API
- WKB の座標 (x=lon, y=lat) を自動的に (lat, lon) 順に変換してエンコード / Automatically converts WKB (x=lon, y=lat) to (lat, lon) order for encoding

#### 使用例 (Examples)

```sql
-- Google の公式例 / Google's official example
SELECT stx_asencodedpolyline(
  ST_GeomFromText('LINESTRING(-120.2 38.5, -120.95 40.7, -126.453 43.252)'));
-- _p~iF~ps|U_ulLnnqC_mqNvxq`@
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsEncodedPolyline()`

---

### stx_linefromenccodedpolyline

Google Encoded Polyline Algorithm Format の文字列から LineString ジオメトリを構築する。
Creates a LineString geometry from a Google Encoded Polyline Algorithm Format string.

```sql
stx_linefromenccodedpolyline(text [, srid [, precision]]) -> GEOMETRY (LineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| text | STRING | Encoded Polyline 文字列 / Encoded Polyline string |
| srid | INTEGER | (任意) 出力の SRID。デフォルト 4326 / (Optional) Output SRID. Default: 4326 |
| precision | INTEGER | (任意) 精度（10^precision で復元）。デフォルト 5 / (Optional) Precision. Default: 5 |

#### 備考 (Notes)

- デフォルト SRID は 4326（Encoded Polyline は通常 WGS84 座標に使用されるため）/ Default SRID is 4326 (Encoded Polyline is typically used with WGS84 coordinates)
- デコード後の座標は WKB に (x=lon, y=lat) として格納 / Decoded coordinates are stored as (x=lon, y=lat) in WKB

#### 使用例 (Examples)

```sql
-- デコード / Decode
SELECT ST_AsText(stx_linefromenccodedpolyline(
  '_p~iF~ps|U_ulLnnqC_mqNvxq`@', 0));
-- LINESTRING(-120.2 38.5,-120.95 40.7,-126.453 43.252)

-- ラウンドトリップ / Round-trip
SELECT ST_AsText(stx_linefromenccodedpolyline(
  stx_asencodedpolyline(
    ST_GeomFromText('LINESTRING(-120.2 38.5, -120.95 40.7, -126.453 43.252)')),
  0));
-- LINESTRING(-120.2 38.5,-120.95 40.7,-126.453 43.252)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineFromEncodedPolyline()`

---

### stx_assvg

ジオメトリを SVG (Scalable Vector Graphics) パスデータ文字列に変換する。
Converts a geometry to an SVG path data string.

```sql
stx_assvg(geometry [, rel [, precision]]) -> STRING
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| rel | INTEGER | (任意) 0=絶対座標, 1=相対座標。デフォルト 0 / (Optional) 0=absolute, 1=relative coordinates. Default: 0 |
| precision | INTEGER | (任意) 有効桁数。デフォルト 15 / (Optional) Significant digits. Default: 15 |

#### 出力形式 (Output Format)

| ジオメトリ型 (Type) | 出力形式 (Format) |
|---|---|
| Point | `cx="x" cy="y"` (SVG circle 属性 / SVG circle attributes) |
| LineString | `M x y L x y L x y` (絶対) / `M x y l dx dy l dx dy` (相対) |
| Polygon | `M x y L x y ... Z` (閉パス / closed path) |
| Multi* | 各要素を連結 / Concatenation of elements |

#### 備考 (Notes)

- SVG の Y 軸は下向きのため、Y 座標は符号反転される / Y coordinates are negated because SVG Y-axis points downward

#### 使用例 (Examples)

```sql
-- 点 / Point
SELECT stx_assvg(ST_GeomFromText('POINT(1 2)'));
-- cx="1" cy="-2"

-- ライン（絶対座標）/ LineString (absolute)
SELECT stx_assvg(ST_GeomFromText('LINESTRING(0 0, 10 10, 20 0)'));
-- M 0 -0 L 10 -10 L 20 -0

-- ライン（相対座標）/ LineString (relative)
SELECT stx_assvg(ST_GeomFromText('LINESTRING(10 20, 30 40, 50 20)'), 1);
-- M 10 -20 l 20 -20 l 20 20
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsSVG()`
- MySQL 標準: なし

---

### stx_askml

ジオメトリを KML (Keyhole Markup Language) 形式の XML 文字列に変換する。
Converts a geometry to a KML XML string.

```sql
stx_askml(geometry [, precision]) -> STRING
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| precision | INTEGER | (任意) 有効桁数。デフォルト 15 / (Optional) Significant digits. Default: 15 |

#### 出力形式 (Output Format)

| ジオメトリ型 (Type) | 出力形式 (Format) |
|---|---|
| Point | `<Point><coordinates>lon,lat</coordinates></Point>` |
| LineString | `<LineString><coordinates>lon,lat lon,lat ...</coordinates></LineString>` |
| Polygon | `<Polygon><outerBoundaryIs><LinearRing><coordinates>...</coordinates></LinearRing></outerBoundaryIs></Polygon>` |
| Multi* | `<MultiGeometry>...</MultiGeometry>` |

#### 備考 (Notes)

- KML の座標は (longitude, latitude) 順（WKB の内部格納順と同じ）/ KML coordinates use (longitude, latitude) order (same as WKB internal storage)
- GIS ツール（Google Earth 等）との連携に使用 / Used for integration with GIS tools (Google Earth, etc.)

#### 使用例 (Examples)

```sql
-- 点 / Point
SELECT stx_askml(ST_GeomFromText('POINT(10 20)'));
-- <Point><coordinates>10,20</coordinates></Point>

-- Geographic 座標 / Geographic coordinates
SELECT stx_askml(ST_GeomFromText('POINT(35.6 139.7)', 4326));
-- <Point><coordinates>139.7,35.6</coordinates></Point>

-- 精度指定 / Custom precision
SELECT stx_askml(ST_GeomFromText('POINT(1.23456789 9.87654321)'), 4);
-- <Point><coordinates>1.235,9.877</coordinates></Point>
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsKML()`
- MySQL 標準: なし

---

### stx_asewkt

ジオメトリを EWKT (Extended Well-Known Text) 形式の文字列に変換する。SRID プレフィックス付き。
Converts a geometry to an EWKT string with SRID prefix.

```sql
stx_asewkt(geometry) -> STRING
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |

#### 出力形式 (Output Format)

`SRID=<srid>;WKT_TEXT`

EWKT は標準 WKT に SRID 情報を付加したフォーマット。PostGIS で広く使用されている。
EWKT extends standard WKT with SRID information. Widely used in PostGIS.

#### 備考 (Notes)

- EWKT の座標は WKB の内部格納順（Geographic の場合は lon, lat）/ EWKT coordinates follow WKB internal order (lon, lat for Geographic)
- MySQL の `ST_AsText()` は SRID を出力しないため、SRID を保持したいテキスト表現として有用 / Useful when you need text representation that preserves SRID, as MySQL's `ST_AsText()` does not output SRID

#### 使用例 (Examples)

```sql
-- Cartesian
SELECT stx_asewkt(ST_GeomFromText('POINT(1 2)'));
-- SRID=0;POINT(1 2)

-- Geographic
SELECT stx_asewkt(ST_GeomFromText('POINT(35.6 139.7)', 4326));
-- SRID=4326;POINT(139.7 35.6)

-- LineString
SELECT stx_asewkt(ST_GeomFromText('LINESTRING(0 0, 10 10)'));
-- SRID=0;LINESTRING(0 0,10 10)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsEWKT()`
- MySQL 標準: なし

---

### stx_geomfromewkt

EWKT (Extended Well-Known Text) 文字列からジオメトリを構築する。
Creates a geometry from an EWKT string.

```sql
stx_geomfromewkt(text) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| text | STRING | EWKT 文字列 / EWKT string |

#### 入力形式 (Input Format)

`SRID=<srid>;WKT_TEXT` または SRID プレフィックスなしの標準 WKT（SRID は 0 となる）。
`SRID=<srid>;WKT_TEXT` or standard WKT without SRID prefix (defaults to SRID 0).

対応するジオメトリ型 / Supported geometry types: POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON

#### 備考 (Notes)

- SRID プレフィックスは大文字小文字を区別しない / SRID prefix is case-insensitive
- EWKT の座標は WKB 順（Geographic の場合は lon, lat）で指定する / EWKT coordinates use WKB order (lon, lat for Geographic)
- `stx_asewkt` と `stx_geomfromewkt` でラウンドトリップが可能 / Round-trip with `stx_asewkt` is supported

#### 使用例 (Examples)

```sql
-- SRID 付き / With SRID
SELECT ST_AsText(stx_geomfromewkt('SRID=4326;POINT(139.7 35.6)'));
-- POINT(35.6 139.7)

SELECT ST_SRID(stx_geomfromewkt('SRID=4326;POINT(139.7 35.6)'));
-- 4326

-- SRID なし（SRID 0）/ Without SRID (defaults to 0)
SELECT ST_AsText(stx_geomfromewkt('POINT(5 10)'));
-- POINT(5 10)

-- ラウンドトリップ / Round-trip
SELECT ST_AsText(stx_geomfromewkt(
  stx_asewkt(ST_GeomFromText('LINESTRING(0 0, 10 10)'))));
-- LINESTRING(0 0,10 10)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_GeomFromEWKT()`
- MySQL 標準: なし

---

## インストール (Installation)

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';
```

`INSTALL PLUGIN` を実行すると全29関数が自動的に登録される。個別の `CREATE FUNCTION` は不要。
All 29 functions are automatically registered upon `INSTALL PLUGIN`. No separate `CREATE FUNCTION` statements are needed.

### 登録済み関数の確認 (Verifying Registered Functions)

`performance_schema.user_defined_functions` で現在登録されている関数を確認できる。
You can verify the registered functions via `performance_schema.user_defined_functions`.

```sql
SELECT UDF_NAME, UDF_RETURN_TYPE
FROM performance_schema.user_defined_functions
WHERE UDF_NAME LIKE 'stx_%'
ORDER BY UDF_NAME;

+--------------------------------+-----------------+
| UDF_NAME                       | UDF_RETURN_TYPE |
+--------------------------------+-----------------+
| stx_affine                     | char            |
| stx_angle                      | double          |
| stx_asencodedpolyline         | char            |
| stx_asewkt                     | char            |
| stx_askml                      | char            |
| stx_assvg                      | char            |
| stx_azimuth                    | double          |
| stx_closestpoint               | char            |
| stx_coveredby                  | integer         |
| stx_covers                     | integer         |
| stx_dwithin                    | integer         |
| stx_generatepoints             | char            |
| stx_geomfromewkt               | char            |
| stx_linefromenccodedpolyline   | char            |
| stx_linelocatepoint            | double          |
| stx_linesubstring              | char            |
| stx_makepoint                  | char            |
| stx_perimeter                  | double          |
| stx_pointonsurface             | char            |
| stx_project                    | char            |
| stx_relate                     | char            |
| stx_relatematch                | integer         |
| stx_removerepeatedpoints       | char            |
| stx_reverse                    | char            |
| stx_rotate                     | char            |
| stx_scale                      | char            |
| stx_segmentize                 | char            |
| stx_snaptogrid                 | char            |
| stx_translate                  | char            |
+--------------------------------+-----------------+
```

`UDF_RETURN_TYPE` が `char` の関数は、実際にはジオメトリのバイナリ（SRID + WKB）を返す。UDF の仕様上 GEOMETRY 型を直接返せないため `STRING_RESULT` で登録している。`ST_AsText()` 等に渡せばジオメトリとして正しく解釈される。
Functions with `UDF_RETURN_TYPE = char` actually return geometry binary data (SRID + WKB). Due to the UDF specification, GEOMETRY cannot be used as a return type directly, so they are registered as `STRING_RESULT`. The returned values can be passed to `ST_AsText()` or other spatial functions and will be interpreted correctly as geometries.

## アンインストール (Uninstallation)

```sql
UNINSTALL PLUGIN spatial_plugin;
```

全関数が自動的に登録解除される。
All functions are automatically unregistered.
