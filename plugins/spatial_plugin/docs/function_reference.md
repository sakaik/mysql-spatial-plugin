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

## インストール (Installation)

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';
```

`INSTALL PLUGIN` を実行すると全12関数が自動的に登録される。個別の `CREATE FUNCTION` は不要。
All 12 functions are automatically registered upon `INSTALL PLUGIN`. No separate `CREATE FUNCTION` statements are needed.

### 登録済み関数の確認 (Verifying Registered Functions)

`performance_schema.user_defined_functions` で現在登録されている関数を確認できる。
You can verify the registered functions via `performance_schema.user_defined_functions`.

```sql
SELECT UDF_NAME, UDF_RETURN_TYPE
FROM performance_schema.user_defined_functions
WHERE UDF_NAME LIKE 'stx_%'
ORDER BY UDF_NAME;

+---------------------------+-----------------+
| UDF_NAME                  | UDF_RETURN_TYPE |
+---------------------------+-----------------+
| stx_angle                 | double          |
| stx_azimuth               | double          |
| stx_coveredby             | integer         |
| stx_covers                | integer         |
| stx_dwithin               | integer         |
| stx_linelocatepoint       | double          |
| stx_linesubstring         | char            |
| stx_perimeter             | double          |
| stx_project               | char            |
| stx_rotate                | char            |
| stx_scale                 | char            |
| stx_translate             | char            |
+---------------------------+-----------------+
```

`UDF_RETURN_TYPE` が `char` の関数は、実際にはジオメトリのバイナリ（SRID + WKB）を返す。UDF の仕様上 GEOMETRY 型を直接返せないため `STRING_RESULT` で登録している。`ST_AsText()` 等に渡せばジオメトリとして正しく解釈される。
Functions with `UDF_RETURN_TYPE = char` actually return geometry binary data (SRID + WKB). Due to the UDF specification, GEOMETRY cannot be used as a return type directly, so they are registered as `STRING_RESULT`. The returned values can be passed to `ST_AsText()` or other spatial functions and will be interpreted correctly as geometries.

## アンインストール (Uninstallation)

```sql
UNINSTALL PLUGIN spatial_plugin;
```

全関数が自動的に登録解除される。
All functions are automatically unregistered.
