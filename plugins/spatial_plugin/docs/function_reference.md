# STX Spatial Functions リファレンス (Reference)

MySQL 9.6 用プラグイン `spatial_plugin` が提供する空間関数の仕様書。
A specification of spatial functions provided by the `spatial_plugin` plugin for MySQL 9.6.

## 概要 (Overview)

`STX_` プレフィックスの関数群は、MySQL に不足している空間演算機能を boost::geometry ライブラリおよび GEOS ライブラリを用いて実装したものである。`INSTALL PLUGIN` 時に全関数が自動登録される。
The `STX_`-prefixed functions implement spatial operations missing from MySQL, powered by the boost::geometry and GEOS libraries. All functions are automatically registered upon `INSTALL PLUGIN`.

### 座標系サポート (Coordinate System Support)

すべての関数は Cartesian（平面直交座標系）と Geographic（地理座標系）の両方に対応する。
All functions support both Cartesian and Geographic coordinate systems.

| 座標系 (CS) | 距離・周長の単位 (Unit) | 対象SRID (Target SRIDs) |
|---|---|---|
| Cartesian | 座標単位 (coordinate units) | 0, projected CRS (UTM, etc.), and other non-geographic SRIDs |
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
| [STX_Perimeter](#stx_perimeter) | DOUBLE | ポリゴンの周長 / Perimeter of a polygon |
| [STX_CoveredBy](#stx_coveredby) | INTEGER | 包含判定 / Tests if geometry is covered by another |
| [STX_Covers](#stx_covers) | INTEGER | 包含判定（逆） / Tests if geometry covers another |
| [STX_Dwithin](#stx_dwithin) | INTEGER | 距離閾値判定 / Tests if distance is within threshold |
| [STX_Azimuth](#stx_azimuth) | DOUBLE | 2点間の方位角 / Azimuth between two points |
| [STX_Project](#stx_project) | GEOMETRY | 点の投影 / Projects a point by distance and azimuth |
| [STX_LineLocatepoint](#stx_linelocatepoint) | DOUBLE | 線上の最近接位置 / Fraction of closest point on line |
| [STX_LineSubstring](#stx_linesubstring) | GEOMETRY | 線の部分抽出 / Extracts a portion of a line |
| [STX_Angle](#stx_angle) | DOUBLE | 3点がなす角度 / Angle formed by three points |
| [STX_Translate](#stx_translate) | GEOMETRY | 平行移動 / Translates a geometry by dx, dy |
| [STX_Translate_latlon](#stx_translate_latlon) | GEOMETRY | 緯度経度順で平行移動（Geographic専用） / Translates by delta_lat, delta_lon (Geographic only) |
| [STX_Scale](#stx_scale) | GEOMETRY | スケール変換 / Scales a geometry by sx, sy |
| [STX_Rotate](#stx_rotate) | GEOMETRY | 回転 / Rotates a geometry by angle |
| [STX_Reverse](#stx_reverse) | GEOMETRY | 頂点順逆転 / Reverses vertex order |
| [STX_PointonSurface](#stx_pointonsurface) | GEOMETRY | 内部保証点 / Guaranteed interior point |
| [STX_ClosestPoint](#stx_closestpoint) | GEOMETRY | 最近接点 / Closest point on geometry |
| [STX_Relate](#stx_relate) | STRING | DE-9IM 行列 / DE-9IM matrix string |
| [STX_RelateMatch](#stx_relatematch) | INTEGER | DE-9IM パターン判定 / DE-9IM pattern match |
| [STX_MakePoint](#stx_makepoint) | GEOMETRY | 座標から POINT 構築 / Create Point from coordinates |
| [STX_Affine](#stx_affine) | GEOMETRY | アフィン変換 / General 2D affine transformation |
| [STX_SnapToGrid](#stx_snaptogrid) | GEOMETRY | 座標丸め / Snap coordinates to grid |
| [STX_RemoveRepeatedPoints](#stx_removerepeatedpoints) | GEOMETRY | 重複頂点除去 / Remove consecutive duplicates |
| [STX_Segmentize](#stx_segmentize) | GEOMETRY | 線分分割 / Split segments to max length |
| [STX_GeneratePoints](#stx_generatepoints) | GEOMETRY | ランダム点生成 / Random points in polygon |
| [STX_AsEncodedPolyline](#stx_asencodedpolyline) | STRING | Encoded Polyline 出力 / Geometry to Encoded Polyline |
| [STX_LineFromEncodedPolyline](#stx_linefromencodedpolyline) | GEOMETRY | Encoded Polyline 入力 / Encoded Polyline to LineString |
| [STX_AsSvg](#stx_assvg) | STRING | SVG 出力 / Geometry to SVG path data |
| [STX_AsKml](#stx_askml) | STRING | KML 出力 / Geometry to KML |
| [STX_AsEwkt](#stx_asewkt) | STRING | EWKT 出力 / Geometry to EWKT |
| [STX_GeomFromEwkt](#stx_geomfromewkt) | GEOMETRY | EWKT 入力 / EWKT to Geometry |
| [STX_MinimumBoundingCircle](#stx_minimumboundingcircle) | GEOMETRY | 最小外接円 / Minimum bounding circle |
| [STX_SquareGrid](#stx_squaregrid) | GEOMETRY | 矩形グリッド生成 / Square grid generation |
| [STX_Hexgrid](#stx_hexgrid) | GEOMETRY | 六角形グリッド生成 / Hexagonal grid generation |
| [STX_MakeValid](#stx_makevalid) | GEOMETRY | 不正ジオメトリ修復 / Repair invalid geometry (GEOS) |
| [STX_LineMerge](#stx_linemerge) | GEOMETRY | ライン結合 / Merge connected LineStrings (GEOS) |
| [STX_Voronoi](#stx_voronoi) | GEOMETRY | ボロノイ図 / Voronoi diagram (GEOS) |
| [STX_Delaunay](#stx_delaunay) | GEOMETRY | ドロネー三角形分割 / Delaunay triangulation (GEOS) |
| [STX_OffsetCurve](#stx_offsetcurve) | GEOMETRY | ラインの平行オフセット / Parallel offset line (GEOS) |
| [STX_ConcaveHull](#stx_concavehull) | GEOMETRY | 凹包 / Concave hull (GEOS) |
| [STX_Snap](#stx_snap) | GEOMETRY | 頂点スナッピング / Snap vertices to another geometry (GEOS) |
| [STX_Polygonize](#stx_polygonize) | GEOMETRY | ラインからポリゴン構築 / Create polygons from linework (GEOS) |
| [STX_BuildArea](#stx_buildarea) | GEOMETRY | ラインから面構築 / Create area from linework (GEOS) |
| [STX_SharedPaths](#stx_sharedpaths) | GEOMETRY | 共有パス抽出 / Shared paths between lineal geometries (GEOS) |
| [STX_Node](#stx_node) | GEOMETRY | ラインのノード化 / Node a set of linestrings (GEOS) |
| [STX_SimplifyPreserveTopology](#stx_simplifypreservetopology) | GEOMETRY | トポロジ保持簡略化 / Topology-preserving simplification (GEOS) |
| [STX_UnaryUnion](#stx_unaryunion) | GEOMETRY | 構成要素の Union / Union of all components (GEOS) |
| [STX_ClipByRect](#stx_clipbyrect) | GEOMETRY | 矩形クリッピング / Fast rectangle clipping (GEOS) |
| [STX_ReducePrecision](#stx_reduceprecision) | GEOMETRY | 精度削減 / Reduce coordinate precision (GEOS) |
| [STX_MaximumInscribedCircle](#stx_maximuminscribedcircle) | GEOMETRY | 最大内接円 / Maximum inscribed circle (GEOS) |
| [STX_MinimumWidth](#stx_minimumwidth) | GEOMETRY | 最小幅 / Minimum width of geometry (GEOS) |
| [STX_SimplifyPolygonHull](#stx_simplifypolygonhull) | GEOMETRY | ポリゴン Hull 簡略化 / Polygon hull simplification (GEOS) |
| [STX_ConcaveHullOfPolygons](#stx_concavehullofpolygons) | GEOMETRY | ポリゴン集合の凹包 / Concave hull of polygon set (GEOS) |
| [STX_NPoints](#stx_npoints) | INTEGER | 全頂点数 / Total number of vertices |
| [STX_Makeline](#stx_makeline) | GEOMETRY | LineString 構築 / Create LineString from points |
| [STX_MakePolygon](#stx_makepolygon) | GEOMETRY | Polygon 構築 / Create Polygon from LineString |
| [STX_Points](#stx_points) | GEOMETRY | 全頂点抽出 / Extract all vertices as MultiPoint |
| [STX_IsRing](#stx_isring) | INTEGER | 閉環判定 / Test if LineString is a ring (GEOS) |
| [STX_ShortestLine](#stx_shortestline) | GEOMETRY | 最短線分 / Shortest line between geometries (GEOS) |
| [STX_dms2deg](#stx_dms2deg) | DOUBLE | 度分秒→十進度変換（独自関数） / DMS to decimal degrees (original) |
| [STX_deg2dms_deg](#stx_deg2dms_deg) | INTEGER | 十進度→度の部分（独自関数） / Degree part of DMS (original) |
| [STX_deg2dms_min](#stx_deg2dms_min) | INTEGER | 十進度→分の部分（独自関数） / Minute part of DMS (original) |
| [STX_deg2dms_sec](#stx_deg2dms_sec) | DOUBLE | 十進度→秒の部分（独自関数） / Second part of DMS (original) |

---

## 関数詳細 (Function Details)

### STX_Perimeter

ポリゴンまたはマルチポリゴンの周長を返す。
Returns the perimeter of a Polygon or MultiPolygon.

```sql
STX_Perimeter(geometry) -> DOUBLE
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Polygon または MultiPolygon / Polygon or MultiPolygon |

#### 戻り値 (Return Value)

- Cartesian: 座標単位での周長 / Perimeter in coordinate units
- Geographic: メートル単位での測地線周長 / Geodesic perimeter in meters (WGS84)
- Polygon/MultiPolygon 以外の場合は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`) / Raises ERROR 3516 for other geometry types

#### 使用例 (Examples)

```sql
-- Cartesian: 10x10 の正方形 → 周長 40
-- Cartesian: 10x10 square -> perimeter 40
SELECT STX_Perimeter(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 40.0

-- Geographic (SRID 4326): 赤道付近の 10度 x 10度 → 約4,421km
-- Geographic: 10deg x 10deg near equator -> ~4,421km
SELECT STX_Perimeter(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))', 4326));
-- 4421233.315...

-- MultiPolygon: 各ポリゴンの周長の合計
-- MultiPolygon: sum of perimeters of each polygon
SELECT STX_Perimeter(
  ST_GeomFromText('MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)),
                                 ((20 20,30 20,30 30,20 30,20 20)))'));
-- 80.0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Perimeter()`
- MySQL 標準: なし (not available in MySQL 9.6)

---

### STX_CoveredBy

第1引数のジオメトリが第2引数のジオメトリに完全に包含されるかを判定する。境界上の点も包含とみなす（`ST_Within` との違い）。
Tests whether geometry1 is completely covered by geometry2. Points on the boundary are considered covered (unlike `ST_Within`).

```sql
STX_CoveredBy(geometry1, geometry2) -> INTEGER
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
SELECT STX_Coveredby(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 1

-- 境界上の点 → 1（coveredby は境界を含む）
-- Point on boundary -> 1 (coveredby includes boundary)
SELECT STX_Coveredby(
  ST_GeomFromText('POINT(0 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 1

-- 外部の点 → 0 / Exterior point -> 0
SELECT STX_Coveredby(
  ST_GeomFromText('POINT(15 15)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_CoveredBy()`
- MySQL 標準: `MBRCoveredBy()`（MBR ベースのみ / MBR-based only, not exact geometry test）

---

### STX_Covers

第1引数のジオメトリが第2引数のジオメトリを完全に包含するかを判定する。
Tests whether geometry1 completely covers geometry2.

`STX_Coveredby` の引数を入れ替えた関数。
Equivalent to `STX_Coveredby` with swapped arguments.

```sql
STX_Covers(geometry1, geometry2) -> INTEGER
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
SELECT STX_Covers(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  ST_GeomFromText('POINT(5 5)'));
-- 1
```

#### 備考 (Notes)

内部的に `STX_Coveredby(geometry2, geometry1)` と等価。
Internally equivalent to `STX_Coveredby(geometry2, geometry1)`.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Covers()`

---

### STX_Dwithin

2つのジオメトリ間の最短距離が指定した閾値以内であるかを判定する。
Tests whether the minimum distance between two geometries is within a given threshold.

```sql
STX_Dwithin(geometry1, geometry2, distance_threshold) -> INTEGER
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
SELECT STX_Dwithin(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(3 4)'), 5.0);
-- 1

-- 閾値を下回る → 0
-- Threshold too small -> 0
SELECT STX_Dwithin(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(3 4)'), 4.9);
-- 0

-- Geographic: 赤道上で経度1度（約111km）、閾値200km
-- Geographic: 1 degree at equator (~111km), threshold 200km
SELECT STX_Dwithin(
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

### STX_Azimuth

2つの点の間の方位角（北を基準に時計回り）をラジアンで返す。
Returns the azimuth (bearing) from point1 to point2 in radians, measured clockwise from north.

```sql
STX_Azimuth(point1, point2) -> DOUBLE
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
SELECT STX_Azimuth(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(1 0)'));
-- 1.5707963267948966

-- Geographic: 東京→大阪 方向
-- Geographic: bearing from Tokyo to Osaka
SELECT DEGREES(STX_Azimuth(
  ST_GeomFromText('POINT(35.6762 139.6503)', 4326),
  ST_GeomFromText('POINT(34.6937 135.5023)', 4326)));
-- ~248 (WSW)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Azimuth()`

---

### STX_Project

点を指定した距離と方位角の方向に投影（移動）した新しい点を返す。
Returns a new point projected from the input point by a given distance and azimuth.

```sql
STX_Project(point, distance, azimuth) -> GEOMETRY (Point)
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
SELECT ST_AsText(STX_Project(
  ST_GeomFromText('POINT(0 0)'), 10.0, 0.0));
-- POINT(0 10)

-- Geographic: 東京から北に 1km / 1km north from Tokyo
SELECT ST_AsText(STX_Project(
  ST_GeomFromText('POINT(35.6762 139.6503)', 4326), 1000.0, 0.0));
-- POINT(35.685... 139.6503...)

-- STX_Azimuth と組み合わせ：A地点からB地点方向に100m移動
-- Combined with STX_Azimuth: move 100m from A toward B
SELECT ST_AsText(STX_Project(
  ST_GeomFromText('POINT(35.6762 139.6503)', 4326),
  100.0,
  STX_Azimuth(
    ST_GeomFromText('POINT(35.6762 139.6503)', 4326),
    ST_GeomFromText('POINT(34.6937 135.5023)', 4326)
  )
));
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Project()`

---

### STX_LineLocatepoint

ライン上で指定した点に最も近い位置を、ラインの始点からの比率（0.0 ~ 1.0）で返す。
Returns the fraction (0.0 to 1.0) of the line's total length at the point closest to the given point.

```sql
STX_LineLocatepoint(line, point) -> DOUBLE
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
SELECT STX_Linelocatepoint(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'),
  ST_GeomFromText('POINT(5 0)'));
-- 0.5

-- ライン上にない点（垂線の足の位置を返す）
-- Point not on line (returns perpendicular foot position)
SELECT STX_LineLocatePoint(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'),
  ST_GeomFromText('POINT(2.5 5)'));
-- 0.25

-- 複数セグメントのライン / Multi-segment line
SELECT STX_LineLocatePoint(
  ST_GeomFromText('LINESTRING(0 0, 5 0, 10 0)'),
  ST_GeomFromText('POINT(5 0)'));
-- 0.5
```

#### 備考 (Notes)

- `STX_LineSubstring` と組み合わせることで、点の近傍でラインを分割する等の操作が可能。
  Can be combined with `STX_LineSubstring` to split a line near a given point.
- 第1引数が LineString 以外、または第2引数が Point 以外の場合は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`)。
  Raises ERROR 3516 if the first argument is not a LineString or the second is not a Point.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineLocatePoint()`

---

### STX_LineSubstring

ラインの指定区間（始点・終点を比率で指定）を新しい LineString として返す。
Returns a sub-linestring between the specified start and end fractions.

```sql
STX_LineSubstring(line, start_fraction, end_fraction) -> GEOMETRY (LineString)
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
SELECT ST_AsText(STX_Linesubstring(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 0.25, 0.75));
-- LINESTRING(2.5 0,7.5 0)

-- 全体 / Full line
SELECT ST_AsText(STX_Linesubstring(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 0.0, 1.0));
-- LINESTRING(0 0,10 0)

-- セグメント境界をまたぐ区間 / Interval crossing segment boundary
SELECT ST_AsText(STX_Linesubstring(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 0.25, 0.75));
-- LINESTRING(5 0,10 0,10 5)
```

#### 備考 (Notes)

- 第1引数が LineString 以外の場合は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`)。
  Raises ERROR 3516 if the first argument is not a LineString.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineSubstring()`

---

### STX_Angle

3つの点 P1, P2, P3 において、P2 を頂点とする角度をラジアンで返す。Geographic 座標系の場合も球面計算は行わず、与えられた座標値を平面上の座標として計算する。
Returns the angle at P2 formed by the rays P2→P1 and P2→P3, in radians. Even for geographic coordinate systems, this function performs planar calculation using the coordinate values as-is, without spherical geometry.

```sql
STX_Angle(point1, point2, point3) -> DOUBLE
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
SELECT STX_Angle(
  ST_GeomFromText('POINT(1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(0 1)'));
-- 1.5707963267948966

-- 直線（π = 180°） / Straight line (pi = 180 degrees)
SELECT STX_Angle(
  ST_GeomFromText('POINT(1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(-1 0)'));
-- 3.141592653589793

-- 度に変換 / Convert to degrees
SELECT DEGREES(STX_Angle(
  ST_GeomFromText('POINT(1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(0 1)')));
-- 90.0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Angle()`

---

### STX_Translate

ジオメトリを指定した (dx, dy) だけ平行移動する。
Translates (shifts) a geometry by the given dx, dy offsets.

```sql
STX_Translate(geometry, dx, dy) -> GEOMETRY
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
SELECT ST_AsText(STX_Translate(
  ST_GeomFromText('POINT(1 2)'), 10, 20));
-- POINT(11 22)

-- ポリゴンの移動 / Translate a polygon
SELECT ST_AsText(STX_Translate(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 5, 5));
-- POLYGON((5 5,5 15,15 15,15 5,5 5))
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Translate()`

---

### STX_Translate_latlon

Geographic 座標系のジオメトリを (delta_lat, delta_lon) で平行移動する。Geographic SRID 専用。
Translates a geometry by (delta_lat, delta_lon). Geographic SRIDs only.

```sql
STX_Translate_latlon(geometry, delta_lat, delta_lon) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ（Geographic SRID のみ） / Input geometry (Geographic SRID only) |
| delta_lat | DOUBLE | 緯度方向の移動量（度） / Latitude offset in degrees |
| delta_lon | DOUBLE | 経度方向の移動量（度） / Longitude offset in degrees |

#### 戻り値 (Return Value)

平行移動後のジオメトリ（入力と同じ型・SRID）。全ジオメトリ型に対応。
The translated geometry (same type and SRID as input). All geometry types are supported.

Cartesian SRID のジオメトリを渡した場合は ERROR 3726 が発生する。その場合は `STX_Translate()` を使用すること。
Passing a Cartesian SRS geometry raises ERROR 3726. Use `STX_Translate()` for Cartesian geometries.

#### 使用例 (Examples)

```sql
-- 東京の点を北に1度、東に2度移動 / Move a point 1° north and 2° east
SELECT ST_AsText(STX_Translate_latlon(
  ST_GeomFromText('POINT(35 135)', 4326), 1, 2));
-- POINT(36 137)

-- 引数は常に (delta_lat, delta_lon) 順。SRID の軸順序に依存しない。
-- Arguments are always (delta_lat, delta_lon) regardless of SRS axis order.
```

#### 対応する他の関数 (Equivalent in Other Systems)

- なし（独自関数）。PostGIS の `ST_Translate()` は常に (dx, dy) 順。
  No direct equivalent. PostGIS `ST_Translate()` always uses (dx, dy) order.

---

### STX_Scale

ジオメトリを原点を基準に (sx, sy) でスケール変換する。
Scales a geometry by sx, sy factors relative to the origin.

```sql
STX_Scale(geometry, sx, sy) -> GEOMETRY
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
SELECT ST_AsText(STX_Scale(
  ST_GeomFromText('POINT(3 4)'), 2, 3));
-- POINT(6 12)

-- ラインの縮小 / Shrink a linestring
SELECT ST_AsText(STX_Scale(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 0.5, 0.5));
-- LINESTRING(0 0,5 0,5 5)
```

#### 備考 (Notes)

スケールの基準は原点 (0, 0)。別の点を基準にしたい場合は `STX_Translate` で原点に移動し、スケール後に戻す。
Scaling is relative to the origin (0, 0). To scale around a different center, use `STX_Translate` to shift to origin, scale, then shift back.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Scale()`

---

### STX_Rotate

ジオメトリを原点を中心に指定した角度（ラジアン）だけ回転する。
Rotates a geometry by the given angle (radians) around the origin.

```sql
STX_Rotate(geometry, angle) -> GEOMETRY
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
SELECT ST_AsText(STX_Rotate(
  ST_GeomFromText('POINT(1 0)'), PI() / 2));
-- POINT(0 1)  (浮動小数点精度による近似 / approximate due to floating point)

-- 点 (1,0) を 180° 回転 → (-1,0) / Rotate 180 degrees -> (-1,0)
SELECT ST_AsText(STX_Rotate(
  ST_GeomFromText('POINT(1 0)'), PI()));
-- POINT(-1 0)
```

#### 備考 (Notes)

回転の中心は原点 (0, 0)。別の点を中心に回転したい場合は `STX_Translate` で原点に移動し、回転後に戻す。
Rotation center is the origin (0, 0). To rotate around a different center, use `STX_Translate` to shift to origin, rotate, then shift back.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Rotate()`

---

### STX_Reverse

ジオメトリの頂点順序を逆転する。
Reverses the vertex order of a geometry.

```sql
STX_Reverse(geometry) -> GEOMETRY
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
SELECT ST_AsText(STX_Reverse(
  ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)')));
-- LINESTRING(2 2,1 1,0 0)

-- Geographic / Geographic linestring
SELECT ST_AsText(STX_Reverse(
  ST_GeomFromText('LINESTRING(0 0, 1 0, 1 1)', 4326)));
-- LINESTRING(1 1,1 0,0 0)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Reverse()`

---

### STX_PointonSurface

ジオメトリの表面上（または内部）に位置することが保証された点を返す。あらゆるジオメトリ型に対応。
Returns a point guaranteed to lie on the surface of the geometry. Works with any geometry type.

```sql
STX_PointonSurface(geometry) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 任意のジオメトリ / Any geometry type |

#### 戻り値 (Return Value)

入力ジオメトリの表面上にある Point（入力と同じ SRID）。Polygon の場合は内部に位置する点、LineString の場合はライン上の点、Point の場合はそのPoint自身を返す。
A Point on the surface of the input geometry (same SRID as input). For Polygon returns an interior point, for LineString a point on the line, for Point returns itself.

#### 使用例 (Examples)

```sql
-- 正方形の内部点 / Interior point of a square
SELECT ST_AsText(STX_PointonSurface(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- POINT(5 5)

-- L字型凹ポリゴン（内部点を返す）
-- L-shaped concave polygon (returns interior point)
SELECT ST_AsText(STX_PointonSurface(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 5 5, 5 10, 0 10, 0 0))')));
-- POINT(2.5 2.5)

-- ライン上の点 / Point on a linestring
SELECT ST_AsText(STX_PointonSurface(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)')));
-- POINT(10 0)

-- Point はそのまま返る / Point returns itself
SELECT ST_AsText(STX_PointonSurface(
  ST_GeomFromText('POINT(5 5)')));
-- POINT(5 5)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_PointOnSurface()`

---

### STX_ClosestPoint

第1引数のジオメトリ上で、第2引数のジオメトリに最も近い点を返す。PostGIS の `ST_ClosestPoint(g1, g2)` と同じ動作。
Returns the closest point on geometry1 to geometry2. Equivalent to PostGIS `ST_ClosestPoint(g1, g2)`.

```sql
STX_ClosestPoint(geometry1, geometry2) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 最近接点を求めるジオメトリ / Geometry on which to find the closest point |
| geometry2 | GEOMETRY | 対象ジオメトリ / Reference geometry |

#### 戻り値 (Return Value)

geometry1 上の geometry2 に最も近い Point。`STX_ShortestLine(g1, g2)` の始点と等価。
The closest Point on geometry1 to geometry2. Equivalent to the start point of `STX_ShortestLine(g1, g2)`.

#### 対応するジオメトリ型 (Supported Geometry Types)

両引数とも任意のジオメトリ型（Point, LineString, Polygon, MultiPolygon 等）を指定可能。
Both arguments accept any geometry type (Point, LineString, Polygon, MultiPolygon, etc.).

#### 使用例 (Examples)

```sql
-- ライン上の点に最も近い点 / Closest point on line to a point
SELECT ST_AsText(STX_ClosestPoint(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'),
  ST_GeomFromText('POINT(5 5)')));
-- POINT(5 0)

-- ポリゴン境界上の最近接点 / Closest point on polygon boundary
SELECT ST_AsText(STX_ClosestPoint(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  ST_GeomFromText('POINT(15 5)')));
-- POINT(10 5)

-- 点がポリゴン内部 → その点自身を返す / Point inside polygon -> returns that point
SELECT ST_AsText(STX_ClosestPoint(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  ST_GeomFromText('POINT(5 5)')));
-- POINT(5 5)

-- ライン同士の最近接点 / Closest point between two linestrings
SELECT ST_AsText(STX_ClosestPoint(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'),
  ST_GeomFromText('LINESTRING(5 5, 5 10)')));
-- POINT(5 0)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_ClosestPoint()`

---

### STX_Relate

2つのジオメトリの空間関係を DE-9IM（Dimensionally Extended 9-Intersection Model）行列文字列として返す。
Returns the DE-9IM matrix string describing the spatial relationship between two geometries.

```sql
STX_Relate(geometry1, geometry2) -> STRING (9 chars)
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
SELECT STX_Relate(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'));
-- 0FFFFF212

-- 離散ポリゴン / Disjoint polygons
SELECT STX_Relate(
  ST_GeomFromText('POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'),
  ST_GeomFromText('POLYGON((5 5, 6 5, 6 6, 5 6, 5 5))'));
-- FF2FF1212

-- 同一点 / Equal points
SELECT STX_Relate(
  ST_GeomFromText('POINT(1 1)'),
  ST_GeomFromText('POINT(1 1)'));
-- 0FFFFFFF2
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Relate()`
- MySQL 標準: なし

---

### STX_RelateMatch

2つのジオメトリの DE-9IM 行列が指定したパターンに一致するかを判定する。
Tests whether the DE-9IM matrix of two geometries matches a given pattern.

```sql
STX_RelateMatch(geometry1, geometry2, pattern) -> INTEGER
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
SELECT STX_Relatematch(
  ST_GeomFromText('POINT(5 5)'),
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  'T*F**F***');
-- 1

-- Disjoint 判定 / Test disjoint
SELECT STX_Relatematch(
  ST_GeomFromText('POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'),
  ST_GeomFromText('POLYGON((5 5, 6 5, 6 6, 5 6, 5 5))'),
  'FF*FF****');
-- 1
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_RelateMatch()`
- MySQL 標準: なし

---

### STX_MakePoint

座標値から POINT ジオメトリを構築する。引数の順序は SRID の空間参照系定義に基づく軸順序に従う。
Creates a Point geometry from coordinate values. Argument order follows the axis order defined by the SRID's spatial reference system.

```sql
STX_MakePoint(coord1, coord2 [, srid]) -> GEOMETRY (Point)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| coord1 | DOUBLE | 第1座標。SRS 定義の第1軸に対応 / First coordinate, corresponding to the first axis of the SRS |
| coord2 | DOUBLE | 第2座標。SRS 定義の第2軸に対応 / Second coordinate, corresponding to the second axis of the SRS |
| srid | INTEGER | (任意) SRID。省略時は 0 / (Optional) SRID. Defaults to 0 |

引数の解釈は SRID の SRS 定義（`information_schema.ST_SPATIAL_REFERENCE_SYSTEMS` の `AXIS[]` エントリ）に基づく軸順序で決まる。MySQL ビルトインの `ST_GeomFromText()` と同じ規則に従う。
The interpretation of arguments follows the axis order defined by the SRS `AXIS[]` entries in `information_schema.ST_SPATIAL_REFERENCE_SYSTEMS`, consistent with MySQL's built-in `ST_GeomFromText()`.

| SRID | 例 (Example) | 第1軸 (1st axis) | coord1 | coord2 |
|---|---|---|---|---|
| 4326 | WGS 84 | Lat, NORTH | 緯度 / latitude | 経度 / longitude |
| 6668 | JGD2011 | Lat, NORTH | 緯度 / latitude | 経度 / longitude |
| 7035 | RGSPM06 (lon-lat) | Lon, EAST | 経度 / longitude | 緯度 / latitude |
| 2451 | 日本平面直角9系 | X, NORTH | 北方向 / northing | 東方向 / easting |
| 3857 | Web Mercator | X, EAST | X | Y |
| 0 | (Cartesian) | — | X | Y |

Geographic SRID では、緯度 [-90, 90]、経度 [-180, 180] の範囲外は ERROR 3617 / 3616 となる。
For geographic SRIDs, latitude must be within [-90, 90] and longitude within [-180, 180], otherwise ERROR 3617 / 3616 is raised.

#### 戻り値 (Return Value)

指定した座標の Point ジオメトリ。WKB 形式で直接構築するため高速。
A Point geometry at the specified coordinates. Constructed directly as WKB for efficiency.

#### 使用例 (Examples)

```sql
-- Cartesian (SRID 0): (x, y) 順 / (x, y) order
SELECT ST_AsText(STX_Makepoint(10, 20));
-- POINT(10 20)

-- Geographic (SRID 4326): (lat, lon) 順 / (lat, lon) order
SELECT ST_AsText(STX_Makepoint(35.6, 139.7, 4326));
-- POINT(35.6 139.7)

SELECT ST_Latitude(STX_Makepoint(35.6, 139.7, 4326));
-- 35.6
SELECT ST_Longitude(STX_Makepoint(35.6, 139.7, 4326));
-- 139.7

-- Geographic with lon-lat axis order (SRID 7035): (lon, lat) 順
SELECT ST_Longitude(STX_Makepoint(2.0, 47.0, 7035));
-- 2.0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_MakePoint()` — 常に (lon, lat) 順。本関数は MySQL の SRS 軸順序に従う点が異なる
  PostGIS always uses (lon, lat) order. This function follows MySQL's SRS axis order instead.
- MySQL 標準: なし

---

### STX_Affine

ジオメトリに一般的な 2D アフィン変換を適用する。
Applies a general 2D affine transformation to a geometry.

```sql
STX_Affine(geometry, a, b, d, e, xoff, yoff) -> GEOMETRY
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
| 平行移動 (Translate) | `STX_Affine(geom, 1, 0, 0, 1, dx, dy)` |
| スケール (Scale) | `STX_Affine(geom, sx, 0, 0, sy, 0, 0)` |
| 回転 (Rotate) | `STX_Affine(geom, cos(a), -sin(a), sin(a), cos(a), 0, 0)` |
| 反転 (Reflect Y) | `STX_Affine(geom, 1, 0, 0, -1, 0, 0)` |

#### 使用例 (Examples)

```sql
-- せん断変換 / Shear transform
SELECT ST_AsText(STX_Affine(
  ST_GeomFromText('POINT(1 0)'), 1, 2, 0, 1, 0, 0));
-- POINT(1 1)

-- 恒等変換 / Identity transform
SELECT ST_AsText(STX_Affine(
  ST_GeomFromText('POINT(3 4)'), 1, 0, 0, 1, 0, 0));
-- POINT(3 4)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Affine()`

---

### STX_SnapToGrid

ジオメトリの全座標を指定したグリッドサイズに丸める。
Snaps all coordinates of a geometry to a grid of the specified size.

```sql
STX_SnapToGrid(geometry, size) -> GEOMETRY
STX_SnapToGrid(geometry, size_x, size_y) -> GEOMETRY
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
SELECT ST_AsText(STX_Snaptogrid(
  ST_GeomFromText('POINT(1.3 2.7)'), 0.5));
-- POINT(1.5 2.5)

-- X と Y で異なるサイズ / Different X and Y sizes
SELECT ST_AsText(STX_Snaptogrid(
  ST_GeomFromText('POINT(1.3 2.7)'), 1, 0.5));
-- POINT(1 2.5)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_SnapToGrid()`

---

### STX_RemoveRepeatedPoints

ジオメトリから連続する重複頂点を除去する。
Removes consecutive duplicate vertices from a geometry.

```sql
STX_RemoveRepeatedPoints(geometry) -> GEOMETRY
STX_RemoveRepeatedPoints(geometry, tolerance) -> GEOMETRY
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
SELECT ST_AsText(STX_Removerepeatedpoints(
  ST_GeomFromText('LINESTRING(0 0, 0 0, 1 1, 1 1, 2 2)')));
-- LINESTRING(0 0,1 1,2 2)

-- tolerance 指定 / With tolerance
SELECT ST_AsText(STX_Removerepeatedpoints(
  ST_GeomFromText('LINESTRING(0 0, 0.1 0, 1 0, 1.05 0, 2 0)'), 0.2));
-- LINESTRING(0 0,1 0,2 0)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_RemoveRepeatedPoints()`

---

### STX_Segmentize

ジオメトリのすべての辺を、指定した最大長以下になるよう分割（頂点を挿入）する。
Splits all segments of a geometry by inserting vertices so that no segment exceeds the specified maximum length.

```sql
STX_Segmentize(geometry, max_length) -> GEOMETRY
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
SELECT ST_AsText(STX_Segmentize(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 3));
-- LINESTRING(0 0,2.5 0,5 0,7.5 0,10 0)

-- ポリゴンの辺を分割 / Densify polygon edges
SELECT ST_AsText(STX_Segmentize(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 6));
-- POLYGON((0 0,0 5,0 10,5 10,10 10,10 5,10 0,5 0,0 0))
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Segmentize()`

---

### STX_GeneratePoints

ポリゴンまたはマルチポリゴンの内部にランダムな点を生成し、MultiPoint として返す。
Generates random points inside a Polygon or MultiPolygon and returns them as a MultiPoint.

```sql
STX_GeneratePoints(geometry, npoints [, seed]) -> GEOMETRY (MultiPoint)
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
SELECT ST_AsText(STX_Generatepoints(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 5, 42));

-- シード指定で再現性を確保 / Reproducible with same seed
SELECT ST_AsText(STX_Generatepoints(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 3, 123))
=
SELECT ST_AsText(STX_Generatepoints(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 3, 123));
-- Always identical
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_GeneratePoints()`

---

### STX_AsEncodedPolyline

LineString ジオメトリを Google Encoded Polyline Algorithm Format の文字列に変換する。
Converts a LineString geometry to a Google Encoded Polyline Algorithm Format string.

```sql
STX_AsEncodedPolyline(geometry [, precision]) -> STRING
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
SELECT STX_Asencodedpolyline(
  ST_GeomFromText('LINESTRING(-120.2 38.5, -120.95 40.7, -126.453 43.252)'));
-- _p~iF~ps|U_ulLnnqC_mqNvxq`@
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsEncodedPolyline()`

---

### STX_LineFromEncodedPolyline

Google Encoded Polyline Algorithm Format の文字列から LineString ジオメトリを構築する。
Creates a LineString geometry from a Google Encoded Polyline Algorithm Format string.

```sql
STX_LineFromEncodedPolyline(text [, srid [, precision]]) -> GEOMETRY (LineString)
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
SELECT ST_AsText(STX_Linefromencodedpolyline(
  '_p~iF~ps|U_ulLnnqC_mqNvxq`@', 0));
-- LINESTRING(-120.2 38.5,-120.95 40.7,-126.453 43.252)

-- ラウンドトリップ / Round-trip
SELECT ST_AsText(STX_Linefromencodedpolyline(
  STX_Asencodedpolyline(
    ST_GeomFromText('LINESTRING(-120.2 38.5, -120.95 40.7, -126.453 43.252)')),
  0));
-- LINESTRING(-120.2 38.5,-120.95 40.7,-126.453 43.252)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineFromEncodedPolyline()`

---

### STX_AsSvg

ジオメトリを SVG (Scalable Vector Graphics) パスデータ文字列に変換する。
Converts a geometry to an SVG path data string.

```sql
STX_AsSvg(geometry [, rel [, precision]]) -> STRING
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
SELECT STX_Assvg(ST_GeomFromText('POINT(1 2)'));
-- cx="1" cy="-2"

-- ライン（絶対座標）/ LineString (absolute)
SELECT STX_Assvg(ST_GeomFromText('LINESTRING(0 0, 10 10, 20 0)'));
-- M 0 -0 L 10 -10 L 20 -0

-- ライン（相対座標）/ LineString (relative)
SELECT STX_Assvg(ST_GeomFromText('LINESTRING(10 20, 30 40, 50 20)'), 1);
-- M 10 -20 l 20 -20 l 20 20
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsSVG()`
- MySQL 標準: なし

---

### STX_AsKml

ジオメトリを KML (Keyhole Markup Language) 形式の XML 文字列に変換する。
Converts a geometry to a KML XML string.

```sql
STX_AsKml(geometry [, precision]) -> STRING
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
SELECT STX_Askml(ST_GeomFromText('POINT(10 20)'));
-- <Point><coordinates>10,20</coordinates></Point>

-- Geographic 座標 / Geographic coordinates
SELECT STX_Askml(ST_GeomFromText('POINT(35.6 139.7)', 4326));
-- <Point><coordinates>139.7,35.6</coordinates></Point>

-- 精度指定 / Custom precision
SELECT STX_Askml(ST_GeomFromText('POINT(1.23456789 9.87654321)'), 4);
-- <Point><coordinates>1.235,9.877</coordinates></Point>
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsKML()`
- MySQL 標準: なし

---

### STX_AsEwkt

ジオメトリを EWKT (Extended Well-Known Text) 形式の文字列に変換する。SRID プレフィックス付き。
Converts a geometry to an EWKT string with SRID prefix.

```sql
STX_AsEwkt(geometry) -> STRING
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
SELECT STX_Asewkt(ST_GeomFromText('POINT(1 2)'));
-- SRID=0;POINT(1 2)

-- Geographic
SELECT STX_Asewkt(ST_GeomFromText('POINT(35.6 139.7)', 4326));
-- SRID=4326;POINT(139.7 35.6)

-- LineString
SELECT STX_Asewkt(ST_GeomFromText('LINESTRING(0 0, 10 10)'));
-- SRID=0;LINESTRING(0 0,10 10)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_AsEWKT()`
- MySQL 標準: なし

---

### STX_GeomFromEwkt

EWKT (Extended Well-Known Text) 文字列からジオメトリを構築する。
Creates a geometry from an EWKT string.

```sql
STX_GeomFromEwkt(text) -> GEOMETRY
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
- `STX_Asewkt` と `STX_Geomfromewkt` でラウンドトリップが可能 / Round-trip with `STX_Asewkt` is supported

#### 使用例 (Examples)

```sql
-- SRID 付き / With SRID
SELECT ST_AsText(STX_Geomfromewkt('SRID=4326;POINT(139.7 35.6)'));
-- POINT(35.6 139.7)

SELECT ST_SRID(STX_Geomfromewkt('SRID=4326;POINT(139.7 35.6)'));
-- 4326

-- SRID なし（SRID 0）/ Without SRID (defaults to 0)
SELECT ST_AsText(STX_Geomfromewkt('POINT(5 10)'));
-- POINT(5 10)

-- ラウンドトリップ / Round-trip
SELECT ST_AsText(STX_Geomfromewkt(
  STX_Asewkt(ST_GeomFromText('LINESTRING(0 0, 10 10)'))));
-- LINESTRING(0 0,10 10)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_GeomFromEWKT()`
- MySQL 標準: なし

---

### STX_MinimumBoundingCircle

任意のジオメトリの全頂点を包含する最小の円（Minimum Bounding Circle）を Polygon として返す。
Returns the smallest circle that encloses all vertices of a geometry, approximated as a Polygon.

```sql
STX_MinimumBoundingCircle(geometry [, segs_per_quarter]) -> GEOMETRY (Polygon)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 任意のジオメトリ / Any geometry type |
| segs_per_quarter | INTEGER | (任意) 四分円あたりのセグメント数。デフォルト 48（全周 192 セグメント） / (Optional) Segments per quarter circle. Default: 48 (192 total) |

#### 戻り値 (Return Value)

入力ジオメトリの全頂点を包含する最小の円を近似した Polygon。入力と同じ SRID を持つ。
A Polygon approximating the minimum enclosing circle of all vertices. Same SRID as input.

#### アルゴリズム (Algorithm)

Welzl のアルゴリズム（期待計算量 O(n)）を使用して最小外接円の中心と半径を求め、指定したセグメント数で円を Polygon に近似する。
Uses Welzl's algorithm (expected O(n)) to find the minimum enclosing circle center and radius, then approximates it as a Polygon with the specified number of segments.

#### 使用例 (Examples)

```sql
-- 正方形の最小外接円 / MBC of a square
-- 対角線 = 10√2, 半径 = 5√2, 面積 ≈ 50π ≈ 157.08
SELECT ROUND(ST_Area(STX_Minimumboundingcircle(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))), 1);
-- 157.1

-- セグメント数を減らした粗い円 / Coarse circle with fewer segments
SELECT ST_NumPoints(ST_ExteriorRing(STX_Minimumboundingcircle(
  ST_GeomFromText('POINT(0 0)'), 4)));
-- 17 (4*4 + 1 closing point)

-- MultiPoint の外接円 / MBC of MultiPoint
SELECT ROUND(ST_Area(STX_Minimumboundingcircle(
  ST_GeomFromText('MULTIPOINT((0 0),(10 0),(10 10),(0 10))'))), 1);
-- 157.1
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_MinimumBoundingCircle()`
- MySQL 標準: なし

---

### STX_SquareGrid

入力ジオメトリのバウンディングボックスを覆う正方形グリッドを GeometryCollection として返す。
Returns a GeometryCollection of square grid cells covering the bounding box of the input geometry.

```sql
STX_SquareGrid(size, geometry) -> GEOMETRY (GeometryCollection)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| size | DOUBLE | セルの辺の長さ / Side length of each square cell |
| geometry | GEOMETRY | バウンディングボックスを決定するジオメトリ / Geometry whose bounding box defines the grid extent |

#### 戻り値 (Return Value)

入力ジオメトリのバウンディングボックスを覆う Polygon（正方形）の GeometryCollection。グリッドは原点 (0,0) にスナップされるため、異なる入力でも同じサイズのグリッドはタイル状に整列する。
A GeometryCollection of square Polygons covering the input's bounding box. Grid is snapped to origin (0,0) so grids of the same size from different inputs will tile seamlessly.

#### 備考 (Notes)

- グリッドは入力ジオメトリのバウンディングボックス（外接矩形）を基準に生成される。凹形状のジオメトリでは、ジオメトリ自体と交差しないがバウンディングボックスとは交差するセルも含まれる / Grid cells are generated based on the bounding box of the input geometry. For concave geometries, cells that intersect the bounding box but not the geometry itself may be included
- 安全上の上限として最大 1,000,000 セルまで生成 / Maximum 1,000,000 cells for safety
- Cartesian / Geographic 両対応（座標空間で動作） / Works in both Cartesian and Geographic coordinate space

#### 使用例 (Examples)

```sql
-- 10x10 の正方形を 5 単位のグリッドで分割 → 4 セル / 10x10 square with size 5 → 4 cells
SELECT ST_AsText(STX_Squaregrid(5,
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- GEOMETRYCOLLECTION(
--   POLYGON((0 0,5 0,5 5,0 5,0 0)),
--   POLYGON((5 0,10 0,10 5,5 5,5 0)),
--   POLYGON((0 5,5 5,5 10,0 10,0 5)),
--   POLYGON((5 5,10 5,10 10,5 10,5 5)))

-- セル数の確認 / Check cell count
SELECT ST_NumGeometries(STX_Squaregrid(5,
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- 4

-- Geographic 座標（度単位のグリッド）/ Geographic coordinates (grid in degrees)
SELECT ST_NumGeometries(STX_Squaregrid(0.01,
  ST_GeomFromText('POLYGON((35.6 139.7, 35.6 139.71, 35.61 139.71, 35.61 139.7, 35.6 139.7))', 4326)));
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_SquareGrid()`
- MySQL 標準: なし

---

### STX_Hexgrid

入力ジオメトリのバウンディングボックスを覆する六角形（フラットトップ）グリッドを GeometryCollection として返す。
Returns a GeometryCollection of flat-top hexagonal grid cells covering the bounding box of the input geometry.

```sql
STX_Hexgrid(size, geometry) -> GEOMETRY (GeometryCollection)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| size | DOUBLE | 六角形の辺の長さ / Edge length of each hexagon |
| geometry | GEOMETRY | バウンディングボックスを決定するジオメトリ / Geometry whose bounding box defines the grid extent |

#### 戻り値 (Return Value)

入力ジオメトリのバウンディングボックスを覆う Polygon（六角形）の GeometryCollection。フラットトップ型の六角形で、隣接する列は半行ずつオフセットされる。グリッドは原点 (0,0) にスナップされる。
A GeometryCollection of hexagonal Polygons covering the input's bounding box. Uses flat-top hexagons with alternating column offsets. Grid is snapped to origin (0,0).

#### 備考 (Notes)

- グリッドは入力ジオメトリのバウンディングボックス（外接矩形）を基準に生成される。凹形状のジオメトリでは、ジオメトリ自体と交差しないがバウンディングボックスとは交差するセルも含まれる / Grid cells are generated based on the bounding box of the input geometry. For concave geometries, cells that intersect the bounding box but not the geometry itself may be included
- 六角形はフラットトップ型（横幅 = 2×size、高さ = √3×size） / Hexagons are flat-top (width = 2×size, height = √3×size)
- 列間隔 = 1.5 × size、行間隔 = √3 × size / Column spacing = 1.5 × size, row spacing = √3 × size
- 安全上の上限として最大 1,000,000 セルまで生成 / Maximum 1,000,000 cells for safety

#### 使用例 (Examples)

```sql
-- 20x20 の領域を辺長 5 の六角形で覆う / Cover 20x20 area with hexagons of edge length 5
SELECT ST_NumGeometries(STX_Hexgrid(5,
  ST_GeomFromText('POLYGON((0 0, 20 0, 20 20, 0 20, 0 0))')));

-- 各セルは 7 点（6 頂点 + 閉合点）/ Each cell has 7 ring points (6 vertices + closing)
SELECT ST_NumPoints(ST_ExteriorRing(ST_GeometryN(STX_Hexgrid(5,
  ST_GeomFromText('POLYGON((0 0, 20 0, 20 20, 0 20, 0 0))')), 1)));
-- 7

-- 結果は GEOMETRYCOLLECTION 型 / Result is a GEOMETRYCOLLECTION
SELECT ST_GeometryType(STX_Hexgrid(5,
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- GEOMCOLLECTION
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_HexagonGrid()`
- MySQL 標準: なし

---

## GEOS-based Functions / GEOS ベース関数

以下の関数は [GEOS](https://libgeos.org/) ライブラリ（静的リンク）を使用して実装されている。
The following functions are implemented using the [GEOS](https://libgeos.org/) library (statically linked).

### STX_MakeValid

不正なジオメトリを修復する。自己交差するポリゴン等を有効なジオメトリに変換する。
Repairs an invalid geometry. Converts self-intersecting polygons etc. into valid geometries.

```sql
STX_MakeValid(geometry) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 修復対象のジオメトリ / Geometry to repair |

#### 使用例 (Examples)

```sql
-- ボウタイ型ポリゴン（自己交差）を修復 / Repair a bowtie polygon
SELECT ST_AsText(STX_Makevalid(
  ST_GeomFromText('POLYGON((0 0, 10 10, 10 0, 0 10, 0 0))')));
-- MULTIPOLYGON(((0 0,5 5,10 0,0 0)),((0 10,10 10,5 5,0 10)))

-- 有効なジオメトリはそのまま返る / Valid geometry is returned unchanged
SELECT ST_AsText(STX_Makevalid(
  ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))')));
-- POLYGON((0 0,10 0,10 10,0 10,0 0))
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_MakeValid()`
- MySQL 標準: なし

---

### STX_LineMerge

MultiLineString 内の接続する LineString を結合する。
Merges connected LineStrings within a MultiLineString.

```sql
STX_LineMerge(geometry) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 結合対象の MultiLineString / MultiLineString to merge |

#### 使用例 (Examples)

```sql
-- 3本の接続するラインを1本に結合 / Merge 3 connected lines into one
SELECT ST_AsText(STX_Linemerge(ST_GeomFromText(
  'MULTILINESTRING((0 0,1 1),(1 1,2 2),(2 2,3 3))')));
-- LINESTRING(0 0,1 1,2 2,3 3)

-- 接続しないラインは MultiLineString のまま / Disconnected lines stay as MultiLineString
SELECT ST_GeometryType(STX_Linemerge(ST_GeomFromText(
  'MULTILINESTRING((0 0,1 1),(5 5,6 6))')));
-- MULTILINESTRING
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_LineMerge()`
- MySQL 標準: なし

---

### STX_Voronoi

入力ジオメトリの頂点からボロノイ図を生成する。
Generates a Voronoi diagram from the vertices of the input geometry.

```sql
STX_Voronoi(geometry [, tolerance [, envelope]]) -> GEOMETRY (GeometryCollection)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 入力ジオメトリ（頂点を使用）/ Input geometry (vertices are used) |
| tolerance | DOUBLE | スナップ許容値（デフォルト: 0.0）/ Snapping tolerance (default: 0.0) |
| envelope | GEOMETRY | クリッピング範囲（デフォルト: 入力の envelope）/ Clipping envelope (default: input's envelope) |

#### 使用例 (Examples)

```sql
-- 3点からボロノイ図を生成 / Voronoi diagram from 3 points
SELECT ST_NumGeometries(STX_Voronoi(
  ST_GeomFromText('MULTIPOINT(0 0, 10 0, 5 10)')));
-- 3
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_VoronoiPolygons()`
- MySQL 標準: なし

---

### STX_Delaunay

入力ジオメトリの頂点からドロネー三角形分割を生成する。
Generates a Delaunay triangulation from the vertices of the input geometry.

```sql
STX_Delaunay(geometry [, tolerance [, edges_only]]) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 入力ジオメトリ（頂点を使用）/ Input geometry (vertices are used) |
| tolerance | DOUBLE | スナップ許容値（デフォルト: 0.0）/ Snapping tolerance (default: 0.0) |
| edges_only | INTEGER | 1: 辺のみ（MULTILINESTRING）、0: 三角形（GeometryCollection）/ 1: edges only, 0: triangles (default) |

#### 使用例 (Examples)

```sql
-- 4点からドロネー三角形を生成 / Delaunay triangulation from 4 points
SELECT ST_NumGeometries(STX_Delaunay(
  ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)')));
-- 2

-- 辺のみモード / Edges only mode
SELECT ST_GeometryType(STX_Delaunay(
  ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'), 0, 1));
-- MULTILINESTRING
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_DelaunayTriangles()`
- MySQL 標準: なし

---

### STX_OffsetCurve

入力ラインから指定距離だけオフセットした平行線を生成する。
Returns a line offset from the input line by the given distance.

```sql
STX_OffsetCurve(geometry, distance [, quad_segs [, join_style [, mitre_limit]]]) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 入力ライン（LineString）/ Input line |
| distance | DOUBLE | オフセット距離。正=左側、負=右側 / Offset distance. Positive=left, negative=right |
| quad_segs | INTEGER | 四分円あたりのセグメント数（デフォルト: 8）/ Segments per quarter circle (default: 8) |
| join_style | INTEGER | 接合スタイル: 1=round(default), 2=mitre, 3=bevel / Join style |
| mitre_limit | DOUBLE | マイター制限（デフォルト: 5.0）/ Mitre limit (default: 5.0) |

#### 使用例 (Examples)

```sql
-- ラインの左側に距離1でオフセット / Offset line to the left by distance 1
SELECT ST_AsText(STX_Offsetcurve(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), 1));
-- LINESTRING(10 1,0 1)

-- 右側にオフセット（負の距離）/ Offset to the right (negative distance)
SELECT ST_AsText(STX_Offsetcurve(
  ST_GeomFromText('LINESTRING(0 0, 10 0)'), -2));
-- LINESTRING(0 -2,10 -2)

-- マイター接合でオフセット / Offset with mitre join
SELECT ST_AsText(STX_Offsetcurve(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 1, 8, 2));
```

#### 備考 (Notes)

- LineString 以外のジオメトリ型を渡した場合は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`)。
  Raises ERROR 3516 if the geometry is not a LineString.

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_OffsetCurve()`
- MySQL 標準: なし

---

### STX_ConcaveHull

入力ジオメトリの頂点を包含する凹型ポリゴン（凹包）を生成する。
Computes the concave hull of a geometry — a polygon that encloses all vertices.

```sql
STX_ConcaveHull(geometry, ratio [, allow_holes]) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 入力ジオメトリ / Input geometry |
| ratio | DOUBLE | 凹度の制御。0.0=最大凹度、1.0=凸包 / Concavity control. 0.0=max concavity, 1.0=convex hull |
| allow_holes | INTEGER | 1: 穴を許可、0: 穴なし（デフォルト）/ 1: allow holes, 0: no holes (default) |

#### 戻り値 (Return Value)

入力の形状に応じて POLYGON、LINESTRING（共線点の場合）、または POINT（同一点の場合）を返す。
Returns POLYGON, LINESTRING (for collinear points), or POINT (for identical points) depending on input.

#### 使用例 (Examples)

```sql
-- 凸包（ratio=1.0）/ Convex hull (ratio=1.0)
SELECT ST_AsText(STX_Concavehull(
  ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'), 1.0));
-- POLYGON((0 0,0 10,10 10,10 0,0 0))

-- 最大凹度（ratio=0.0）/ Maximum concavity (ratio=0.0)
SELECT ST_AsText(STX_Concavehull(
  ST_GeomFromText('MULTIPOINT(0 0, 5 0, 10 0, 10 5, 10 10, 5 10, 0 10, 0 5, 5 1)'), 0.0));
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_ConcaveHull()`
- MySQL 標準: なし

---

### STX_Snap

入力ジオメトリの頂点・セグメントを参照ジオメトリの頂点にスナップする。
Snaps vertices and segments of the input geometry to vertices of the reference geometry.

```sql
STX_Snap(geometry1, geometry2, tolerance) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | スナップ対象のジオメトリ / Geometry to snap |
| geometry2 | GEOMETRY | スナップ先の参照ジオメトリ / Reference geometry to snap to |
| tolerance | DOUBLE | スナップ距離の閾値 / Distance threshold for snapping |

#### 備考 (Notes)

- ヒューリスティクスで安全な位置を判定するため、すべてのスナップが適用されるとは限らない / Uses heuristics, so not all vertices may be snapped
- オーバーレイ演算の前処理に有用 / Useful as preprocessing for overlay operations

#### 使用例 (Examples)

```sql
-- ポリゴンの頂点をグリッド点にスナップ / Snap polygon vertices to grid points
SELECT ST_AsText(STX_Snap(
  ST_GeomFromText('POLYGON((0.1 0.1, 9.9 0.1, 9.9 9.9, 0.1 9.9, 0.1 0.1))'),
  ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'),
  0.5));
-- POLYGON((0 0,10 0,10 10,0 10,0 0))

-- 点を近くの点にスナップ / Snap a point to a nearby point
SELECT ST_AsText(STX_Snap(
  ST_GeomFromText('POINT(0.1 0)'),
  ST_GeomFromText('POINT(0 0)'),
  0.5));
-- POINT(0 0)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Snap()`
- MySQL 標準: なし

---

### STX_Polygonize

ラインワーク（LineString の集まり）からポリゴンを構築する。
Creates polygons from a set of linework (LineStrings forming closed rings).

```sql
STX_Polygonize(geometry) -> GEOMETRY (GeometryCollection)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | ラインワーク（MultiLineString / GeometryCollection）/ Linework (MultiLineString / GeometryCollection) |

#### 備考 (Notes)

- 入力ラインは正しくノード化されている必要がある（`STX_Node` で前処理推奨） / Input lines must be properly noded (preprocess with `STX_Node`)
- 結果は GeometryCollection of Polygons / Result is a GeometryCollection of Polygons

#### 使用例 (Examples)

```sql
-- 4本のラインから正方形ポリゴンを構築 / Build square polygon from 4 lines
SELECT ST_NumGeometries(STX_Polygonize(ST_GeomFromText(
  'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0))')));
-- 1
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Polygonize()`
- MySQL 標準: なし

---

### STX_BuildArea

ラインワークから面的ジオメトリを構築する。内部リングは穴になる。
Creates an areal geometry from linework. Interior rings become holes.

```sql
STX_BuildArea(geometry) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | ラインワーク / Linework (LineString, MultiLineString, etc.) |

#### 備考 (Notes)

- `STX_Polygonize` と異なり、内部リングを穴として処理する / Unlike `STX_Polygonize`, processes interior rings as holes
- 入力は正しくノード化されている必要がある / Input must be properly noded

#### 使用例 (Examples)

```sql
-- 外側+内側リングから穴あきポリゴンを構築 / Build polygon with hole
SELECT ST_Area(STX_Buildarea(ST_GeomFromText(
  'MULTILINESTRING((0 0,10 0,10 10,0 10,0 0),(2 2,8 2,8 8,2 8,2 2))')));
-- 64 (100 - 36)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_BuildArea()`
- MySQL 標準: なし

---

### STX_SharedPaths

2つの線形ジオメトリの共有パス（重複する部分）を抽出する。
Returns shared paths between two lineal geometries.

```sql
STX_SharedPaths(geometry1, geometry2) -> GEOMETRY (GeometryCollection)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | 線形ジオメトリ / Lineal geometry (LineString / MultiLineString) |
| geometry2 | GEOMETRY | 線形ジオメトリ / Lineal geometry (LineString / MultiLineString) |

#### 戻り値 (Return Value)

GeometryCollection を返す。第1要素は同方向の共有パス、第2要素は逆方向の共有パス。
Returns a GeometryCollection: element [1] = same-direction shared paths, element [2] = opposite-direction shared paths.

#### 使用例 (Examples)

```sql
-- 同方向の共有パス / Same-direction shared path
SELECT ST_AsText(STX_Sharedpaths(
  ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'),
  ST_GeomFromText('LINESTRING(0 0, 10 0)')));
-- GEOMETRYCOLLECTION(MULTILINESTRING((0 0,10 0)),GEOMETRYCOLLECTION EMPTY)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_SharedPaths()`
- MySQL 標準: なし

---

### STX_Node

ラインストリングの集まりを完全にノード化する。交差点にノードを追加し、ラインを分割する。
Fully nodes a set of linestrings by adding intersection points and splitting lines.

```sql
STX_Node(geometry) -> GEOMETRY (MultiLineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | ラインの集まり / Collection of linestrings |

#### 備考 (Notes)

- `STX_Polygonize` の前処理に最適 / Ideal preprocessing for `STX_Polygonize`
- 既存のノードは保持し、最小限の新ノードを追加 / Preserves existing nodes, adds minimal new ones

#### 使用例 (Examples)

```sql
-- X 字交差する2本のラインをノード化 / Node two crossing lines
SELECT ST_NumGeometries(STX_Node(ST_GeomFromText(
  'MULTILINESTRING((0 0, 10 10), (0 10, 10 0))')));
-- 4 (each line split at intersection point)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_Node()`
- MySQL 標準: なし

---

### STX_SimplifyPreserveTopology

トポロジを保持しながらジオメトリを簡略化する。MySQL の `ST_Simplify` とは異なり、ポリゴンのリング交差や崩壊を防止する。
Simplifies geometry using Douglas-Peucker while preserving topology. Unlike MySQL's `ST_Simplify`, prevents ring crossings and collapses in polygons.

```sql
STX_SimplifyPreserveTopology(geometry, tolerance) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 簡略化対象のジオメトリ / Geometry to simplify |
| tolerance | DOUBLE | 簡略化の許容距離 / Distance tolerance for simplification |

#### 使用例 (Examples)

```sql
-- ポリゴンの簡略化（トポロジ保持）/ Simplify polygon preserving topology
SELECT ST_NumPoints(ST_ExteriorRing(STX_Simplifypreservetopology(
  ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 2)));
-- 5 (reduced from 7)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_SimplifyPreserveTopology()`
- MySQL 標準: `ST_Simplify()`（トポロジ保持なし）

---

### STX_UnaryUnion

単一ジオメトリの全構成要素を Union する。重複する MultiPolygon の修復等に使用。
Computes the union of all components of a geometry. Useful for dissolving overlapping MultiPolygons.

```sql
STX_UnaryUnion(geometry) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Union 対象のジオメトリ / Geometry to union |

#### 使用例 (Examples)

```sql
-- 重複する2つのポリゴンを Union / Union overlapping polygons
SELECT ST_Area(STX_Unaryunion(ST_GeomFromText(
  'MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)),((5 5,15 5,15 15,5 15,5 5)))')));
-- 175 (200 - 25 overlap)
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_UnaryUnion()`
- MySQL 標準: `ST_Union()`（2つのジオメトリのみ）

---

### STX_ClipByRect

ジオメトリを2Dバウンディングボックスで高速にクリッピングする。
Fast clipping of a geometry by a 2D bounding box.

```sql
STX_ClipByRect(geometry, xmin, ymin, xmax, ymax) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | クリッピング対象 / Geometry to clip |
| xmin | DOUBLE | 矩形の最小 X / Minimum X of clipping rectangle |
| ymin | DOUBLE | 矩形の最小 Y / Minimum Y of clipping rectangle |
| xmax | DOUBLE | 矩形の最大 X / Maximum X of clipping rectangle |
| ymax | DOUBLE | 矩形の最大 Y / Maximum Y of clipping rectangle |

#### 備考 (Notes)

- `ST_Intersection` より高速だが、出力の妥当性は保証されない場合がある / Faster than `ST_Intersection` but output validity is not always guaranteed
- 入力がバウンディングボックスと交差しない場合は空ジオメトリを返す / Returns empty geometry if input is disjoint from the rectangle

#### 使用例 (Examples)

```sql
-- ポリゴンの左下1/4をクリップ / Clip lower-left quarter of polygon
SELECT ST_Area(STX_Clipbyrect(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
  0, 0, 5, 5));
-- 25
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_ClipByBox2D()`
- MySQL 標準: なし

---

### STX_ReducePrecision

ジオメトリの座標精度を削減する。`STX_Snaptogrid` と異なり、結果のジオメトリの妥当性を保証する。
Reduces coordinate precision of a geometry. Unlike `STX_Snaptogrid`, guarantees the validity of the result.

```sql
STX_ReducePrecision(geometry, gridsize) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |
| gridsize | DOUBLE | グリッドサイズ（座標の丸め精度）/ Grid size (coordinate rounding precision) |

#### 備考 (Notes)

- `STX_Snaptogrid` の上位互換。座標を丸めた後にトポロジを修復するため、結果は常に有効なジオメトリとなる / Superior to `STX_Snaptogrid`: repairs topology after rounding, ensuring valid output
- gridsize = 0 の場合は座標を変更しない / gridsize = 0 leaves coordinates unchanged

#### 使用例 (Examples)

```sql
-- 座標を整数に丸め / Round coordinates to integers
SELECT ST_AsText(STX_Reduceprecision(
  ST_GeomFromText('POINT(1.23 4.56)'), 1));
-- POINT(1 5)

-- ポリゴンも妥当性を保持 / Polygon remains valid
SELECT ST_GeometryType(STX_Reduceprecision(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 1));
-- POLYGON
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_ReducePrecision()`
- MySQL 標準: なし

---

### STX_MaximumInscribedCircle

ポリゴン内部に収まる最大の円（最大内接円）の半径を、中心から最近接境界点への LineString として返す。
Returns the radius of the largest inscribed circle as a LineString from the center to the nearest boundary point.

```sql
STX_MaximumInscribedCircle(geometry, tolerance) -> GEOMETRY (LineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Polygon または MultiPolygon / Polygon or MultiPolygon |
| tolerance | DOUBLE | 計算精度（小さいほど精密だが遅い）/ Computation tolerance (smaller = more precise but slower) |

#### 戻り値 (Return Value)

2点の LineString。始点が円の中心、終点が最近接境界点。始点と終点の距離が内接円の半径。
A 2-point LineString. Start point = circle center, end point = nearest boundary point. The distance between them is the inscribed circle radius.

#### 使用例 (Examples)

```sql
-- 10x10 正方形の最大内接円 → 中心 (5,5)、半径 5
-- Maximum inscribed circle of 10x10 square → center (5,5), radius 5
SELECT ST_AsText(STX_Maximuminscribedcircle(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 0.01));
-- LINESTRING(5 5,5 0)  (approximately)

-- 半径の取得 / Get radius
SELECT ST_Length(STX_Maximuminscribedcircle(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 0.01));
-- ~5.0
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_MaximumInscribedCircle()`
- MySQL 標準: なし

---

### STX_MinimumWidth

ジオメトリの最小幅を表す LineString を返す。最小幅とは、ジオメトリを完全に含む平行な2直線間の最短距離。
Returns a LineString representing the minimum width of a geometry — the shortest distance between two parallel lines that fully contain the geometry.

```sql
STX_MinimumWidth(geometry) -> GEOMETRY (LineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象のジオメトリ / Input geometry |

#### 戻り値 (Return Value)

最小幅を表す LineString。長さが最小幅の値。
A LineString representing the minimum width. Its length equals the minimum width value.

#### 使用例 (Examples)

```sql
-- 10x5 長方形の最小幅 → 5
-- Minimum width of 10x5 rectangle → 5
SELECT ST_Length(STX_MinimumWidth(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 0 5, 0 0))')));
-- 5

-- 10x10 正方形の最小幅 → 10
-- Minimum width of 10x10 square → 10
SELECT ST_Length(STX_MinimumWidth(
  ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')));
-- 10
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: なし（GEOS 固有機能）/ None (GEOS-specific feature)
- MySQL 標準: なし

---

### STX_SimplifyPolygonHull

ポリゴンの外形を簡略化して、元のポリゴンを包含（outer hull）または内包（inner hull）するポリゴンを返す。
Simplifies a polygon to a hull that contains (outer) or is contained by (inner) the original polygon.

```sql
STX_SimplifyPolygonHull(geometry, vertex_fraction [, is_outer]) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | Polygon または MultiPolygon / Polygon or MultiPolygon |
| vertex_fraction | DOUBLE | 保持する頂点の割合。0.0=最大簡略化、1.0=変更なし / Fraction of vertices to retain. 0.0=max simplification, 1.0=no change |
| is_outer | INTEGER | 1: 外側 Hull（デフォルト）、0: 内側 Hull / 1: outer hull (default), 0: inner hull |

#### 備考 (Notes)

- outer hull（is_outer=1）: 元のポリゴンを完全に包含する簡略化ポリゴン / Outer hull: simplified polygon that fully contains the original
- inner hull（is_outer=0）: 元のポリゴンに完全に内包される簡略化ポリゴン / Inner hull: simplified polygon fully contained by the original

#### 使用例 (Examples)

```sql
-- fraction=1.0 で全頂点保持 / Keep all vertices with fraction=1.0
SELECT ST_NumPoints(ST_ExteriorRing(STX_Simplifypolygonhull(
  ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 1.0)));
-- 7 (unchanged)

-- fraction=0 で凸包に簡略化 / Simplify to convex hull with fraction=0
SELECT ST_AsText(STX_Simplifypolygonhull(
  ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 0));
-- POLYGON((0 0,10 0,10 10,0 10,0 0))
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_SimplifyPolygonHull()`
- MySQL 標準: なし

---

### STX_ConcaveHullOfPolygons

ポリゴンの集合（MultiPolygon / GeometryCollection）を包含する凹型ポリゴンを生成する。
Computes the concave hull enclosing a set of polygons.

```sql
STX_ConcaveHullOfPolygons(geometry, ratio [, allow_holes]) -> GEOMETRY
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | ポリゴンの集合（MultiPolygon / GeometryCollection）/ Collection of polygons |
| ratio | DOUBLE | 凹度の制御。0.0=最大凹度、1.0=凸包 / Concavity control. 0.0=max concavity, 1.0=convex hull |
| allow_holes | INTEGER | 1: 穴を許可、0: 穴なし（デフォルト）/ 1: allow holes, 0: no holes (default) |

#### 備考 (Notes)

- `STX_Concavehull` と異なり、ポリゴン同士の形状を考慮した凹包を生成する / Unlike `STX_Concavehull`, considers the shapes of individual polygons
- 入力ポリゴンは tight に扱われる（隙間なく密着）/ Input polygons are treated tightly (no gaps)

#### 使用例 (Examples)

```sql
-- 2つのポリゴンの凹包 / Concave hull of 2 polygons
SELECT ST_GeometryType(STX_Concavehullofpolygons(ST_GeomFromText(
  'MULTIPOLYGON(((0 0,5 0,5 5,0 5,0 0)),((10 0,15 0,15 5,10 5,10 0)))'), 0.5));
-- POLYGON

-- 結果の面積は入力の合計面積以上 / Result area >= sum of input areas
SELECT ST_Area(STX_Concavehullofpolygons(ST_GeomFromText(
  'MULTIPOLYGON(((0 0,5 0,5 5,0 5,0 0)),((10 0,15 0,15 5,10 5,10 0)))'), 0.5))
  >= 50;
-- 1
```

#### 対応する他の関数 (Equivalent in Other Systems)

- PostGIS: `ST_ConcaveHull()` (ポリゴン集合に対して / for polygon sets)
- MySQL 標準: なし

---

## インストール (Installation)

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';
```

`INSTALL PLUGIN` を実行すると全51関数が自動的に登録される。個別の `CREATE FUNCTION` は不要。
All 51 functions are automatically registered upon `INSTALL PLUGIN`. No separate `CREATE FUNCTION` statements are needed.

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
| STX_Affine                     | char            |
| STX_Angle                      | double          |
| STX_AsenCodedPolyline          | char            |
| STX_AsEwkt                     | char            |
| STX_AsKml                      | char            |
| STX_AsSvg                      | char            |
| STX_Azimuth                    | double          |
| STX_ClosestPoint               | char            |
| STX_CoveredBy                  | integer         |
| STX_Covers                     | integer         |
| STX_Dwithin                    | integer         |
| STX_GeneratePoints             | char            |
| STX_GeomFromEwkt               | char            |
| STX_HexGrid                    | char            |
| STX_LineFromEncodedPolyline    | char            |
| STX_LineLocatePoint            | double          |
| STX_LineSubstring              | char            |
| STX_MakePoint                  | char            |
| STX_Minimumboundingcircle      | char            |
| STX_Perimeter                  | double          |
| STX_Pointonsurface             | char            |
| STX_Project                    | char            |
| STX_Relate                     | char            |
| STX_RelateMatch                | integer         |
| STX_RemoveRepeatedPoints       | char            |
| STX_Reverse                    | char            |
| STX_Rotate                     | char            |
| STX_Scale                      | char            |
| STX_Segmentize                 | char            |
| STX_SnapToGrid                 | char            |
| STX_SquareGrid                 | char            |
| STX_Translate                  | char            |
| STX_Translate_latlon           | char            |
| STX_Makevalid                  | char            |
| STX_Linemerge                  | char            |
| STX_Voronoi                    | char            |
| STX_Delaunay                   | char            |
| STX_Offsetcurve                | char            |
| STX_Concavehull                | char            |
| STX_Snap                       | char            |
| STX_Polygonize                 | char            |
| STX_Buildarea                  | char            |
| STX_Sharedpaths                | char            |
| STX_Node                       | char            |
| STX_Simplifypreservetopology   | char            |
| STX_Unaryunion                 | char            |
| STX_Clipbyrect                 | char            |
| STX_Reduceprecision            | char            |
| STX_Maximuminscribedcircle     | char            |
| STX_Minimumwidth               | char            |
| STX_Simplifypolygonhull        | char            |
| STX_Concavehullofpolygons      | char            |
| STX_Npoints                    | integer         |
| STX_Makeline                   | char            |
| STX_Makepolygon                | char            |
| STX_Points                     | char            |
| STX_Isring                     | integer         |
| STX_Shortestline               | char            |
| STX_dms2deg                    | double          |
| STX_deg2dms_deg                | integer         |
| STX_deg2dms_min                | integer         |
| STX_deg2dms_sec                | double          |
+--------------------------------+-----------------+
```

`UDF_RETURN_TYPE` が `char` の関数は、実際にはジオメトリのバイナリ（SRID + WKB）を返す。UDF の仕様上 GEOMETRY 型を直接返せないため `STRING_RESULT` で登録している。`ST_AsText()` 等に渡せばジオメトリとして正しく解釈される。
Functions with `UDF_RETURN_TYPE = char` actually return geometry binary data (SRID + WKB). Due to the UDF specification, GEOMETRY cannot be used as a return type directly, so they are registered as `STRING_RESULT`. The returned values can be passed to `ST_AsText()` or other spatial functions and will be interpreted correctly as geometries.

### STX_NPoints

ジオメトリの全頂点数を返す。MySQL の `ST_NumPoints()` は LineString のみ対応だが、本関数は全ジオメトリ型に対応する。
Returns the total number of vertices in any geometry type. Unlike MySQL's `ST_NumPoints()` which only works for LineString, this function supports all geometry types.

```sql
STX_NPoints(geometry) -> INTEGER
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象ジオメトリ（全型対応） / Target geometry (all types) |

#### 使用例 (Examples)

```sql
SELECT STX_Npoints(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)'));
-- 3

SELECT STX_Npoints(ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0),(2 2,4 2,4 4,2 4,2 2))'));
-- 10 (outer 5 + inner 5)
```

---

### STX_Makeline

2つの Point から、または MultiPoint から LineString を構築する。
Creates a LineString from two Points, or from a MultiPoint.

```sql
STX_Makeline(point1, point2) -> GEOMETRY (LineString)
STX_Makeline(multipoint)     -> GEOMETRY (LineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| point1, point2 | GEOMETRY (Point) | 始点と終点 / Start and end points |
| multipoint | GEOMETRY (MultiPoint) | 頂点群（2点以上） / Set of vertices (2 or more) |

#### 使用例 (Examples)

```sql
SELECT ST_AsText(STX_Makeline(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(1 1)')));
-- LINESTRING(0 0,1 1)

SELECT ST_AsText(STX_Makeline(
  ST_GeomFromText('MULTIPOINT((0 0),(1 1),(2 2))')));
-- LINESTRING(0 0,1 1,2 2)
```

#### 備考 (Notes)

- 2引数モードでは両引数が Point であること。1引数モードでは MultiPoint であること。それ以外は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`)。
  In 2-argument mode, both must be Points. In 1-argument mode, must be a MultiPoint. Raises ERROR 3516 otherwise.

---

### STX_MakePolygon

閉じた LineString から Polygon を構築する。オプションで内環（穴）を MultiLineString として指定可能。
Creates a Polygon from a closed LineString (outer ring). Optionally, inner rings (holes) can be specified as a MultiLineString.

```sql
STX_MakePolygon(outer_ring)                -> GEOMETRY (Polygon)
STX_MakePolygon(outer_ring, inner_rings)   -> GEOMETRY (Polygon)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| outer_ring | GEOMETRY (LineString) | 外環（閉じた LineString、4点以上） / Outer ring (closed, 4+ points) |
| inner_rings | GEOMETRY (MultiLineString) | 内環群（オプション） / Inner rings (optional) |

#### 使用例 (Examples)

```sql
SELECT ST_AsText(STX_Makepolygon(
  ST_GeomFromText('LINESTRING(0 0,10 0,10 10,0 10,0 0)')));
-- POLYGON((0 0,0 10,10 10,10 0,0 0))

SELECT ST_AsText(STX_Makepolygon(
  ST_GeomFromText('LINESTRING(0 0,10 0,10 10,0 10,0 0)'),
  ST_GeomFromText('MULTILINESTRING((2 2,4 2,4 4,2 4,2 2))')));
-- POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2))
```

#### 備考 (Notes)

- 外環が LineString 以外、または内環が MultiLineString 以外の場合は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`)。
  Raises ERROR 3516 if the outer ring is not a LineString or inner rings are not a MultiLineString.

---

### STX_Points

ジオメトリの全頂点を MultiPoint として返す。
Extracts all vertices from any geometry type and returns them as a MultiPoint.

```sql
STX_Points(geometry) -> GEOMETRY (MultiPoint)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry | GEOMETRY | 対象ジオメトリ（全型対応） / Target geometry (all types) |

#### 使用例 (Examples)

```sql
SELECT ST_AsText(STX_Points(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)')));
-- MULTIPOINT((0 0),(1 1),(2 2))

SELECT ST_AsText(STX_Points(ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))')));
-- MULTIPOINT((0 0),(10 0),(10 10),(0 10),(0 0))
```

---

### STX_IsRing

LineString がリング（閉じていて自己交差がない）かどうかを判定する。GEOS `GEOSisRing()` を使用。
Returns 1 if the LineString is a ring (closed and simple, i.e., no self-intersections), 0 otherwise. Uses GEOS `GEOSisRing()`.

```sql
STX_IsRing(linestring) -> INTEGER (0 or 1)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| linestring | GEOMETRY (LineString) | 対象 LineString / Target LineString |

#### 使用例 (Examples)

```sql
SELECT STX_Isring(ST_GeomFromText('LINESTRING(0 0,10 0,10 10,0 10,0 0)'));
-- 1 (closed, simple → ring)

SELECT STX_Isring(ST_GeomFromText('LINESTRING(0 0,1 1,2 2)'));
-- 0 (not closed)

SELECT STX_Isring(ST_GeomFromText('LINESTRING(0 0,2 0,0 2,2 2,0 0)'));
-- 0 (closed but self-intersecting)
```

#### 備考 (Notes)

- LineString 以外のジオメトリ型を渡した場合は ERROR 3516 (`ER_UNEXPECTED_GEOMETRY_TYPE`)。
  Raises ERROR 3516 if the geometry is not a LineString.

---

### STX_ShortestLine

2つのジオメトリ間の最短線分を LineString として返す。GEOS `GEOSNearestPoints()` を使用。
Returns the shortest line (LineString) between two geometries. Uses GEOS `GEOSNearestPoints()`.

```sql
STX_ShortestLine(geometry1, geometry2) -> GEOMETRY (LineString)
```

#### 引数 (Arguments)

| 引数 (Arg) | 型 (Type) | 説明 (Description) |
|---|---|---|
| geometry1 | GEOMETRY | ジオメトリ1 / First geometry |
| geometry2 | GEOMETRY | ジオメトリ2 / Second geometry |

#### 使用例 (Examples)

```sql
SELECT ST_AsText(STX_Shortestline(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('LINESTRING(1 1, 2 2)')));
-- LINESTRING(0 0,1 1)

-- The length of the shortest line equals the distance between geometries
SELECT ST_Length(STX_Shortestline(
  ST_GeomFromText('POINT(0 0)'),
  ST_GeomFromText('POINT(3 4)')));
-- 5 (= ST_Distance between the two points)
```

#### 備考 (Notes)

- `STX_ClosestPoint(g1, g2)` は g1 上の最近接**点**（= ShortestLine の始点）を返すが、`STX_ShortestLine` は両方のジオメトリ上の最近接点を結ぶ**線分**を返す。
  `STX_ClosestPoint(g1, g2)` returns the nearest **point** on g1 (= start point of ShortestLine), while `STX_ShortestLine` returns the **line segment** connecting the nearest points on both geometries.

---

### STX_dms2deg

度分秒（DMS）を十進度に変換する。PostGIS・MySQL に同等関数はなく、本プラグイン独自の関数。
Converts degrees, minutes, seconds to decimal degrees. This is an original function with no equivalent in PostGIS or MySQL.

```sql
STX_dms2deg(degrees DOUBLE, minutes DOUBLE, seconds DOUBLE) → DOUBLE
```

#### 引数 (Parameters)

| 引数 | 型 | 説明 |
|---|---|---|
| degrees | DOUBLE | 度（負の値で西経・南緯） / Degrees (negative for west longitude / south latitude) |
| minutes | DOUBLE | 分（小数可） / Minutes (may include decimals) |
| seconds | DOUBLE | 秒（NULL の場合 0 として扱う） / Seconds (NULL treated as 0) |

#### 計算式 (Formula)

`sign(degrees) × (|degrees| + minutes/60 + seconds/3600)`

#### 使用例 (Examples)

```sql
-- 134°32'6" → 134.535
SELECT STX_dms2deg(134, 32, 6);
-- 134.535

-- 負の値（南緯）
SELECT STX_dms2deg(-35, 40, 52.4496);
-- -35.681236

-- 秒を省略（分に小数を含む）
SELECT STX_dms2deg(134, 32.1, NULL);
-- 134.535
```

---

### STX_deg2dms_deg

十進度から度の部分（整数）を返す。本プラグイン独自の関数。
Returns the degree part (integer) of a decimal degree value. Original function.

```sql
STX_deg2dms_deg(decimal_degrees DOUBLE) → INTEGER
```

#### 備考 (Notes)

- 負の値の場合、符号はこの関数の戻り値にのみ付く（min/sec は常に正）。
  For negative values, the sign applies only to this function's return value (min/sec are always positive).

#### 使用例 (Examples)

```sql
SELECT STX_deg2dms_deg(134.535);
-- 134

SELECT STX_deg2dms_deg(-134.535);
-- -134
```

---

### STX_deg2dms_min

十進度から分の部分（整数）を返す。本プラグイン独自の関数。
Returns the minute part (integer) of a decimal degree value. Original function.

```sql
STX_deg2dms_min(decimal_degrees DOUBLE) → INTEGER
```

#### 備考 (Notes)

- 入力が負でも戻り値は常に正。
  Return value is always non-negative, even for negative input.

#### 使用例 (Examples)

```sql
SELECT STX_deg2dms_min(134.535);
-- 32

SELECT STX_deg2dms_min(-134.535);
-- 32
```

---

### STX_deg2dms_sec

十進度から秒の部分（DOUBLE）を返す。本プラグイン独自の関数。
Returns the second part (DOUBLE) of a decimal degree value. Original function.

```sql
STX_deg2dms_sec(decimal_degrees DOUBLE) → DOUBLE
```

#### 備考 (Notes)

- 入力が負でも戻り値は常に正。
  Return value is always non-negative, even for negative input.
- 浮動小数点演算の特性上、微小な誤差が生じる場合がある。
  Due to floating-point arithmetic, minor rounding errors may occur.

#### 使用例 (Examples)

```sql
SELECT STX_deg2dms_sec(134.535);
-- 6.0 (approximately)

-- 東京駅の緯度 35.681236° → 35° 40' 52.4496"
SELECT STX_deg2dms_deg(35.681236) AS deg,
       STX_deg2dms_min(35.681236) AS min,
       STX_deg2dms_sec(35.681236) AS sec;
-- deg=35, min=40, sec≈52.4496
```

#### 関連関数 (Related Functions)

- `STX_dms2deg` — 逆変換（DMS → 十進度） / Reverse conversion (DMS to decimal degrees)
- 3関数を組み合わせることで十進度をDMS表記に変換可能:
  Combine the three functions to convert decimal degrees to DMS notation:
  ```sql
  SELECT CONCAT(
    STX_deg2dms_deg(35.681236), '° ',
    STX_deg2dms_min(35.681236), ''' ',
    ROUND(STX_deg2dms_sec(35.681236), 2), '"'
  );
  -- 35° 40' 52.45"
  ```

---

## アンインストール (Uninstallation)

```sql
UNINSTALL PLUGIN spatial_plugin;
```

全関数が自動的に登録解除される。
All functions are automatically unregistered.
