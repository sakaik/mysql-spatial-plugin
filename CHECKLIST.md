# 関数チェックリスト

各関数の実行例ブログ記事の進捗管理。

## 空間計測・述語

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Perimeter(geom)` | Polygon/MultiPolygon の周長 | https://sakaik.hateblo.jp/entry/20260219/STX_Perimeter_func |
| `STX_CoveredBy(g1, g2)` | g1 が g2 に覆われているか判定 | https://sakaik.hateblo.jp/entry/20260219/STX_CoveredBy_Covers_func |
| `STX_Covers(g1, g2)` | g1 が g2 を覆っているか判定 | https://sakaik.hateblo.jp/entry/20260219/STX_CoveredBy_Covers_func |
| `STX_Dwithin(g1, g2, dist)` | 2つのジオメトリ間の距離が閾値以内か判定 | |
| `STX_Azimuth(p1, p2)` | p1 から p2 への方位角（ラジアン、北から時計回り） | https://sakaik.hateblo.jp/entry/20260222/STX_Azimuth_func |
| `STX_Angle(p1, p2, p3)` | p2 における p1-p2-p3 の角度 | https://sakaik.hateblo.jp/entry/20260222/STX_Angle_func |
| `STX_Relate(g1, g2)` | DE-9IM 関係行列を返す | |
| `STX_RelateMatch(g1, g2, pattern)` | DE-9IM パターンマッチ判定 | |
| `STX_NPoints(geom)` | 全頂点数を返す（全ジオメトリ型対応） |https://sakaik.hateblo.jp/entry/20260219/STX_NPoints_func |
| `STX_IsRing(linestring)` | LineString がリング（閉環・非自己交差）か判定 | https://sakaik.hateblo.jp/entry/20260220/STX_IsRing_func |

## ジオメトリ処理

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Project(point, dist, azimuth)` | 指定距離・方位角で点を投影 | |
| `STX_LineLocatepoint(line, point)` | ライン上の最近接点の位置（0.0〜1.0） | |
| `STX_LineSubstring(line, start, end)` | ラインの一部を抽出 | |
| `STX_ClosestPoint(point, geom)` | ジオメトリ上の最近接点を返す | |
| `STX_ShortestLine(g1, g2)` | 2つのジオメトリ間の最短線分を返す | |
| `STX_PointonSurface(geom)` | ポリゴン内部の点を返す | |
| `STX_Points(geom)` | 全頂点を MultiPoint として抽出 | |
| `STX_MakePoint(x, y [, srid])` | 座標から POINT を構築 | https://sakaik.hateblo.jp/entry/20260219/STX_MakePoint_func |
| `STX_Makeline(p1, p2)` / `STX_Makeline(multipoint)` | Point群から LineString を構築 | |
| `STX_MakePolygon(ring [, inner_rings])` | LineString から Polygon を構築 | |
| `STX_GeneratePoints(geom, n [, seed])` | ポリゴン内のランダム点を生成 | |
| `STX_MinimumBoundingCircle(geom [, segs])` | 最小外接円（Polygon として返却） | |
| `STX_SquareGrid(size, geom)` | バウンディングボックスを覆う矩形グリッド | |
| `STX_Hexgrid(size, geom)` | バウンディングボックスを覆う六角形グリッド | |

## 座標変換

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Translate(geom, dx, dy)` | ジオメトリを平行移動 | https://sakaik.hateblo.jp/entry/20260220/STX_Translate_func |
| `STX_Translate_latlon(geom, dlat, dlon)` | 緯度経度順で平行移動（Geographic専用）(独自実装) | https://sakaik.hateblo.jp/entry/20260220/STX_Translate_func |
| `STX_Scale(geom, sx, sy)` | ジオメトリを拡大・縮小 | |
| `STX_Rotate(geom, angle [, center])` | ジオメトリを回転（原点または指定中心） | https://sakaik.hateblo.jp/entry/20260221/STX_Rotate_func |
| `STX_Affine(geom, a, b, d, e, xoff, yoff)` | 一般2Dアフィン変換 | |
| `STX_Reverse(geom)` | 頂点の順序を反転 | https://sakaik.hateblo.jp/entry/20260221/STX_Reverse_func |
| `STX_Snaptogrid(geom, size [, size_y])` | 座標をグリッドにスナップ | |
| `STX_RemoveRepeatedPoints(geom [, tol])` | 連続する重複頂点を除去 | |
| `STX_Segmentize(geom, max_length)` | 長い辺を分割（頂点追加） | |

## GEOS ベース関数

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_MakeValid(geom)` | 不正ジオメトリの修復 | |
| `STX_LineMerge(geom)` | 接続する LineString の結合 | |
| `STX_Voronoi(geom [, tolerance [, envelope]])` | ボロノイ図 | |
| `STX_Delaunay(geom [, tolerance [, edges_only]])` | ドロネー三角形分割 | |
| `STX_OffsetCurve(geom, dist [, quad_segs [, join [, mitre]]])` | ラインの平行オフセット | |
| `STX_ConcaveHull(geom, ratio [, allow_holes])` | 凹包 | |
| `STX_Snap(g1, g2, tolerance)` | 頂点を別ジオメトリにスナップ | |
| `STX_Polygonize(geom)` | ラインワークからポリゴン構築 | |
| `STX_BuildArea(geom)` | ラインワークから面構築（内部リングは穴） | |
| `STX_SharedPaths(g1, g2)` | 2つの線形ジオメトリの共有パス抽出 | |
| `STX_Node(geom)` | ラインのノード化（交差点で分割） | |
| `STX_SimplifyPreserveTopology(geom, tol)` | トポロジ保持簡略化 | |
| `STX_UnaryUnion(geom)` | ジオメトリ構成要素の Union | |
| `STX_ClipByRect(geom, xmin, ymin, xmax, ymax)` | 矩形による高速クリッピング | |
| `STX_ReducePrecision(geom, gridsize)` | 精度削減（妥当性保証付き） | |
| `STX_MaximumInscribedCircle(geom, tolerance)` | 最大内接円 | https://sakaik.hateblo.jp/entry/20260221/STX_MaximumInscribedCircle_func |
| `STX_MinimumWidth(geom)` | ジオメトリの最小幅 | |
| `STX_SimplifyPolygonHull(geom, frac [, is_outer])` | ポリゴン Hull 簡略化 | |
| `STX_ConcaveHullOfPolygons(geom, ratio [, holes])` | ポリゴン集合の凹包 | |

## 入出力フォーマット変換

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_AsEncodedPolyline(geom [, prec])` | Google Encoded Polyline に変換 | |
| `STX_LineFromEnccodedPolyline(text [, srid [, prec]])` | Encoded Polyline から LineString を構築 | |
| `STX_AsSvg(geom [, rel [, prec]])` | SVG パスデータに変換 | |
| `STX_AsKml(geom [, prec])` | KML に変換 | |
| `STX_AsEwkt(geom)` | EWKT（Extended WKT）に変換 | |
| `STX_GeomFromEwkt(text)` | EWKT からジオメトリを構築 | |
