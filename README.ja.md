# STX Spatial Plugin for MySQL

MySQL に不足している空間関数を追加するプラグインです。[Boost.Geometry](https://www.boost.org/doc/libs/release/libs/geometry/) と [GEOS](https://libgeos.org/) を利用して実装しています。
とりあえず粗々でも実装関数を増やし、その後にしっかりとテスト、修正をしていく方針です。
キャッチコピーは「ないよりもあるほうがいい」。

距離クエリ、空間関係判定（DE-9IM）、座標変換、入出力フォーマット変換などの GIS 関数を提供します。Cartesian（投影座標系（UTM等）を含む平面直交座標系）と Geographic（WGS84 等の地理座標系）の両方に対応しています。

## 関数一覧（58関数）

### 空間計測・述語

| 関数 | 説明 |
|---|---|
| `STX_Perimeter(geom)` | Polygon/MultiPolygon の周長 |
| `STX_CoveredBy(g1, g2)` | g1 が g2 に覆われているか判定 |
| `STX_Covers(g1, g2)` | g1 が g2 を覆っているか判定 |
| `STX_Dwithin(g1, g2, dist)` | 2つのジオメトリ間の距離が閾値以内か判定 |
| `STX_Azimuth(p1, p2)` | p1 から p2 への方位角（ラジアン、北から時計回り） |
| `STX_Angle(p1, p2, p3)` | p2 における p1-p2-p3 の角度 |
| `STX_Relate(g1, g2)` | DE-9IM 関係行列を返す |
| `STX_RelateMatch(g1, g2, pattern)` | DE-9IM パターンマッチ判定 |
| `STX_NPoints(geom)` | 全頂点数を返す（全ジオメトリ型対応） |
| `STX_IsRing(linestring)` | LineString がリング（閉環・非自己交差）か判定 |

### ジオメトリ処理

| 関数 | 説明 |
|---|---|
| `STX_Project(point, dist, azimuth)` | 指定距離・方位角で点を投影 |
| `STX_LineLocatepoint(line, point)` | ライン上の最近接点の位置（0.0〜1.0） |
| `STX_LineSubstring(line, start, end)` | ラインの一部を抽出 |
| `STX_ClosestPoint(point, geom)` | ジオメトリ上の最近接点を返す |
| `STX_ShortestLine(g1, g2)` | 2つのジオメトリ間の最短線分を返す |
| `STX_PointonSurface(geom)` | ポリゴン内部の点を返す |
| `STX_Points(geom)` | 全頂点を MultiPoint として抽出 |
| `STX_MakePoint(coord1, coord2 [, srid])` | 座標から POINT を構築（軸順序は SRS 定義に従う） |
| `STX_Makeline(p1, p2)` / `STX_Makeline(multipoint)` | Point群から LineString を構築 |
| `STX_MakePolygon(ring [, inner_rings])` | LineString から Polygon を構築 |
| `STX_GeneratePoints(geom, n [, seed])` | ポリゴン内のランダム点を生成 |
| `STX_MinimumBoundingCircle(geom [, segs])` | 最小外接円（Polygon として返却） |
| `STX_SquareGrid(size, geom)` | バウンディングボックスを覆う矩形グリッド |
| `STX_Hexgrid(size, geom)` | バウンディングボックスを覆う六角形グリッド |

### 座標変換

| 関数 | 説明 |
|---|---|
| `STX_Translate(geom, dx, dy)` | ジオメトリを平行移動 |
| `STX_Translate_latlon(geom, dlat, dlon)` | 緯度経度順で平行移動（Geographic専用）（独自実装） |
| `STX_Scale(geom, sx, sy)` | ジオメトリを拡大・縮小 |
| `STX_Rotate(geom, angle [, center])` | ジオメトリを回転（原点または指定中心） |
| `STX_Affine(geom, a, b, d, e, xoff, yoff)` | 一般2Dアフィン変換 |
| `STX_Reverse(geom)` | 頂点の順序を反転 |
| `STX_Snaptogrid(geom, size [, size_y])` | 座標をグリッドにスナップ |
| `STX_RemoveRepeatedPoints(geom [, tol])` | 連続する重複頂点を除去 |
| `STX_Segmentize(geom, max_length)` | 長い辺を分割（頂点追加） |

### GEOS ベース関数

| 関数 | 説明 |
|---|---|
| `STX_MakeValid(geom)` | 不正ジオメトリの修復 |
| `STX_LineMerge(geom)` | 接続する LineString の結合 |
| `STX_Voronoi(geom [, tolerance [, envelope]])` | ボロノイ図 |
| `STX_Delaunay(geom [, tolerance [, edges_only]])` | ドロネー三角形分割 |
| `STX_OffsetCurve(geom, dist [, quad_segs [, join [, mitre]]])` | ラインの平行オフセット |
| `STX_ConcaveHull(geom, ratio [, allow_holes])` | 凹包 |
| `STX_Snap(g1, g2, tolerance)` | 頂点を別ジオメトリにスナップ |
| `STX_Polygonize(geom)` | ラインワークからポリゴン構築 |
| `STX_BuildArea(geom)` | ラインワークから面構築（内部リングは穴） |
| `STX_SharedPaths(g1, g2)` | 2つの線形ジオメトリの共有パス抽出 |
| `STX_Node(geom)` | ラインのノード化（交差点で分割） |
| `STX_SimplifyPreserveTopology(geom, tol)` | トポロジ保持簡略化 |
| `STX_UnaryUnion(geom)` | ジオメトリ構成要素の Union |
| `STX_ClipByRect(geom, xmin, ymin, xmax, ymax)` | 矩形による高速クリッピング |
| `STX_ReducePrecision(geom, gridsize)` | 精度削減（妥当性保証付き） |
| `STX_MaximumInscribedCircle(geom, tolerance)` | 最大内接円 |
| `STX_MinimumWidth(geom)` | ジオメトリの最小幅 |
| `STX_SimplifyPolygonHull(geom, frac [, is_outer])` | ポリゴン Hull 簡略化 |
| `STX_ConcaveHullOfPolygons(geom, ratio [, holes])` | ポリゴン集合の凹包 |

### 入出力フォーマット変換

| 関数 | 説明 |
|---|---|
| `STX_AsEncodedPolyline(geom [, prec])` | Google Encoded Polyline に変換 |
| `STX_LineFromEncodedPolyline(text [, srid [, prec]])` | Encoded Polyline から LineString を構築 |
| `STX_AsSvg(geom [, rel [, prec]])` | SVG パスデータに変換 |
| `STX_AsKml(geom [, prec])` | KML に変換 |
| `STX_AsEwkt(geom)` | EWKT（Extended WKT）に変換 |
| `STX_GeomFromEwkt(text)` | EWKT からジオメトリを構築 |

### 座標ユーティリティ

| 関数 | 説明 |
|---|---|
| `STX_dms2deg(d, m, s)` | 度分秒→十進度変換(独自実装) |
| `STX_deg2dms_deg(d)` | 十進度→度の部分(独自実装) |
| `STX_deg2dms_min(d)` | 十進度→分の部分(独自実装) |
| `STX_deg2dms_sec(d)` | 十進度→秒の部分(独自実装) |

詳細は [関数リファレンス](plugins/spatial_plugin/docs/function_reference.md) を参照してください。

## 動作要件

- MySQL 8.0 以降
- C++17 対応の g++
- MySQL ソースツリー（ヘッダファイルおよび Boost.Geometry を使用）
- MySQL バイナリインストール（`libmysqlservices` およびプラグインディレクトリを使用）

## ディレクトリ構成

```
├── mysql960.source/        # MySQL ソースツリー（ヘッダ + Boost）
├── mysql960/               # MySQL バイナリインストール
└── plugins/
    ├── gis_lib/            # 共有 GIS ライブラリ（WKB パーサー、型定義）
    └── spatial_plugin/     # プラグインソース
        ├── spatial_plugin.cc
        ├── plugin_version.h
        ├── Makefile
        ├── test.sql
        └── docs/
            └── function_reference.md
```

`mysql960.source/` と `mysql960/` はリポジトリに含まれていません。
[MySQL Downloads](https://dev.mysql.com/downloads/mysql/) からダウンロードして配置してください：

- **ソース**: `mysql-9.6.0.tar.gz` — 展開してビルド（`cmake` + `make`）後、`mysql960.source/` にリネーム
- **バイナリ**: `mysql-9.6.0-linux-glibc2.28-x86_64.tar.xz` — 展開して `mysql960/` にリネーム

両ディレクトリをリポジトリのルート（`plugins/` と同階層）に配置してください。

## ビルド済みバイナリ

**MySQL 9.6.x（Linux x86_64）** 用のビルド済み `spatial_plugin.so` が同梱されています。
このバイナリはコンパイル時の MySQL バージョンに紐づいており、他のバージョンでは使用できません。
異なるバージョンの MySQL で使用する場合は、ソースからリビルドしてください。

## ビルド

```bash
cd plugins/spatial_plugin
make            # spatial_plugin.so をコンパイル
make install    # .so を MySQL プラグインディレクトリにコピー
```

## インストール

```sql
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';
```

全58関数が自動的に登録されます。個別の `CREATE FUNCTION` は不要です。

```sql
-- 確認
SELECT * FROM performance_schema.user_defined_functions WHERE UDF_NAME LIKE 'stx_%';
SHOW STATUS LIKE 'spatial_plugin_%';
```

## 使用例

```sql
-- ポリゴンの周長（Geographic、結果はメートル）
SELECT STX_Perimeter(ST_GeomFromText('POLYGON((139 35,140 35,140 36,139 36,139 35))', 4326));

-- 2点間の距離が 1km 以内か？
SELECT STX_Dwithin(
    ST_GeomFromText('POINT(139.7 35.7)', 4326),
    ST_GeomFromText('POINT(139.71 35.71)', 4326),
    1000
);

-- ポリゴンを重心を中心に 45 度回転
SELECT STX_Rotate(
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))'),
    PI()/4,
    ST_GeomFromText('POINT(5 5)')
);

-- DE-9IM 空間関係
SELECT STX_Relate(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))')
);

-- KML に変換
SELECT STX_Askml(ST_GeomFromText('POINT(35.6 139.7)', 4326));
-- <Point><coordinates>139.7,35.6</coordinates></Point>

-- EWKT のラウンドトリップ
SELECT ST_AsText(STX_Geomfromewkt('SRID=4326;POINT(139.7 35.6)'));
-- POINT(35.6 139.7)
```

### 戻り値の型について

ジオメトリを返す関数は MySQL UDF の制約により `STRING_RESULT` を使用しています。
バイナリデータの中身は正しいジオメトリ形式（SRID + WKB）であり、他の空間関数にそのまま渡せます：

```sql
SELECT ST_AsText(STX_Translate(ST_GeomFromText('POINT(1 2)'), 10, 20));
-- POINT(11 22)
```

## テスト

```bash
make test       # テストスイートを実行（291テスト）
```

## ライセンス

[GNU General Public License v2.0](LICENSE)
