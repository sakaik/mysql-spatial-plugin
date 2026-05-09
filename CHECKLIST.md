# 関数チェックリスト

各関数の実行例ブログ記事の進捗管理。

## 空間計測・述語

| 関数 | 説明 | 実行例URL |
|---|---|---|
| [`STX_Perimeter(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_perimeter) | Polygon/MultiPolygon の周長 | https://sakaik.hateblo.jp/entry/20260219/STX_Perimeter_func |
| [`STX_CoveredBy(g1, g2)`](plugins/spatial_plugin/docs/function_reference.md#stx_coveredby) | g1 が g2 に覆われているか判定 | https://sakaik.hateblo.jp/entry/20260219/STX_CoveredBy_Covers_func |
| [`STX_Covers(g1, g2)`](plugins/spatial_plugin/docs/function_reference.md#stx_covers) | g1 が g2 を覆っているか判定 | https://sakaik.hateblo.jp/entry/20260219/STX_CoveredBy_Covers_func |
| [`STX_Dwithin(g1, g2, dist)`](plugins/spatial_plugin/docs/function_reference.md#stx_dwithin) | 2つのジオメトリ間の距離が閾値以内か判定 | https://sakaik.hateblo.jp/entry/20260308/STX_DWithin_func |
| [`STX_Azimuth(p1, p2)`](plugins/spatial_plugin/docs/function_reference.md#stx_azimuth) | p1 から p2 への方位角（ラジアン、北から時計回り） | https://sakaik.hateblo.jp/entry/20260222/STX_Azimuth_func |
| [`STX_Angle(p1, p2, p3)`](plugins/spatial_plugin/docs/function_reference.md#stx_angle) | p2 における p1-p2-p3 の角度 | https://sakaik.hateblo.jp/entry/20260222/STX_Angle_func |
| [`STX_Relate(g1, g2)`](plugins/spatial_plugin/docs/function_reference.md#stx_relate) | DE-9IM 関係行列を返す | https://sakaik.hateblo.jp/entry/20260308/STX_Relate_func |
| [`STX_RelateMatch(g1, g2, pattern)`](plugins/spatial_plugin/docs/function_reference.md#stx_relatematch) | DE-9IM パターンマッチ判定 | https://sakaik.hateblo.jp/entry/20260308/STX_RelateMatch_func |
| [`STX_NPoints(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_npoints) | 全頂点数を返す（全ジオメトリ型対応） |https://sakaik.hateblo.jp/entry/20260219/STX_NPoints_func |
| [`STX_IsRing(linestring)`](plugins/spatial_plugin/docs/function_reference.md#stx_isring) | LineString がリング（閉環・非自己交差）か判定 | https://sakaik.hateblo.jp/entry/20260220/STX_IsRing_func |

## ジオメトリ処理

| 関数 | 説明 | 実行例URL |
|---|---|---|
| [`STX_Project(point, dist, azimuth)`](plugins/spatial_plugin/docs/function_reference.md#stx_project) | 指定距離・方位角で点を投影 | https://sakaik.hateblo.jp/entry/20260224/STX_Project_func |
| [`STX_LineLocatePoint(line, point)`](plugins/spatial_plugin/docs/function_reference.md#stx_linelocatepoint) | ライン上の最近接点の位置（0.0〜1.0） | https://sakaik.hateblo.jp/entry/20260322/STX_LineLocatePoint_func |
| [`STX_LineSubstring(line, start, end)`](plugins/spatial_plugin/docs/function_reference.md#stx_linesubstring) | ラインの一部を抽出 | https://sakaik.hateblo.jp/entry/20260308/STX_LineSubstring_func |
| [`STX_ClosestPoint(geom1, geom2)`](plugins/spatial_plugin/docs/function_reference.md#stx_closestpoint) | geom1上のgeom2に最も近い点を返す | https://sakaik.hateblo.jp/entry/20260322/STX_ClosestPoint_func |
| [`STX_ShortestLine(g1, g2)`](plugins/spatial_plugin/docs/function_reference.md#stx_shortestline) | 2つのジオメトリ間の最短線分を返す | https://sakaik.hateblo.jp/entry/20260322/STX_ShortestLine_func |
| [`STX_PointonSurface(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_pointonsurface) | ポリゴン内部の点を返す | https://sakaik.hateblo.jp/entry/20260322/STX_PointOnSurface_func |
| [`STX_Points(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_points) | 全頂点を MultiPoint として抽出 | https://sakaik.hateblo.jp/entry/20260322/STX_Points_func |
| [`STX_MakePoint(x, y [, srid])`](plugins/spatial_plugin/docs/function_reference.md#stx_makepoint) | 座標から POINT を構築 | https://sakaik.hateblo.jp/entry/20260219/STX_MakePoint_func |
| [`STX_MakeLine(p1, p2)`](plugins/spatial_plugin/docs/function_reference.md#stx_makeline) / `STX_MakeLine(multipoint)` | Point群から LineString を構築 | https://sakaik.hateblo.jp/entry/20260322/STX_MakeLine_func |
| [`STX_MakePolygon(ring [, inner_rings])`](plugins/spatial_plugin/docs/function_reference.md#stx_makepolygon) | LineString から Polygon を構築 | https://sakaik.hateblo.jp/entry/20260322/STX_MakePolygon_func |
| [`STX_GeneratePoints(geom, n [, seed])`](plugins/spatial_plugin/docs/function_reference.md#stx_generatepoints) | ポリゴン内のランダム点を生成 | https://sakaik.hateblo.jp/entry/20260322/STX_GeneratePoints_func |
| [`STX_MinimumBoundingCircle(geom [, segs])`](plugins/spatial_plugin/docs/function_reference.md#stx_minimumboundingcircle) | 最小外接円（Polygon として返却） | https://sakaik.hateblo.jp/entry/20260328/STX_MinimumBoundingCircle_func |
| [`STX_SquareGrid(size, geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_squaregrid) | バウンディングボックスを覆う矩形グリッド | https://sakaik.hateblo.jp/entry/20260328/STX_SquareGrid_func |
| [`STX_HexGrid(size, geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_hexgrid) | バウンディングボックスを覆う六角形グリッド | https://sakaik.hateblo.jp/entry/20260328/STX_HexGrid_func |

## 座標変換

| 関数 | 説明 | 実行例URL |
|---|---|---|
| [`STX_Translate(geom, dx, dy)`](plugins/spatial_plugin/docs/function_reference.md#stx_translate) | ジオメトリを平行移動 | https://sakaik.hateblo.jp/entry/20260220/STX_Translate_func |
| [`STX_Translate_latlon(geom, dlat, dlon)`](plugins/spatial_plugin/docs/function_reference.md#stx_translate_latlon) | 緯度経度順で平行移動（Geographic専用）(独自実装) | https://sakaik.hateblo.jp/entry/20260220/STX_Translate_func |
| [`STX_Scale(geom, sx, sy [, center])`](plugins/spatial_plugin/docs/function_reference.md#stx_scale) | ジオメトリを拡大・縮小（基準点指定可） | https://sakaik.hateblo.jp/entry/20260509/STX_Scale |
| [`STX_Rotate(geom, angle [, center])`](plugins/spatial_plugin/docs/function_reference.md#stx_rotate) | ジオメトリを回転（原点または指定中心） | https://sakaik.hateblo.jp/entry/20260221/STX_Rotate_func |
| [`STX_Affine(geom, a, b, d, e, xoff, yoff)`](plugins/spatial_plugin/docs/function_reference.md#stx_affine) | 一般2Dアフィン変換 | https://sakaik.hateblo.jp/entry/20260509/STX_Affine |
| [`STX_Reverse(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_reverse) | 頂点の順序を反転 | https://sakaik.hateblo.jp/entry/20260221/STX_Reverse_func |
| [`STX_SnapToGrid(geom, size [, size_y])`](plugins/spatial_plugin/docs/function_reference.md#stx_snaptogrid) | 座標をグリッドにスナップ | https://sakaik.hateblo.jp/entry/20260509/STX_SnapToGrid |
| [`STX_RemoveRepeatedPoints(geom [, tol])`](plugins/spatial_plugin/docs/function_reference.md#stx_removerepeatedpoints) | 連続する重複頂点を除去 | https://sakaik.hateblo.jp/entry/20260329/STX_RemoveRepeatedPoints_func |
| [`STX_Segmentize(geom, max_length)`](plugins/spatial_plugin/docs/function_reference.md#stx_segmentize) | 長い辺を分割（頂点追加） | https://sakaik.hateblo.jp/entry/20260509/STX_Segmentize |

## GEOS ベース関数

| 関数 | 説明 | 実行例URL |
|---|---|---|
| [`STX_MakeValid(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_makevalid) | 不正ジオメトリの修復 | |
| [`STX_LineMerge(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_linemerge) | 接続する LineString の結合 | |
| [`STX_Voronoi(geom [, tolerance [, envelope]])`](plugins/spatial_plugin/docs/function_reference.md#stx_voronoi) | ボロノイ図 | |
| [`STX_Delaunay(geom [, tolerance [, edges_only]])`](plugins/spatial_plugin/docs/function_reference.md#stx_delaunay) | ドロネー三角形分割 | |
| [`STX_OffsetCurve(geom, dist [, quad_segs [, join [, mitre]]])`](plugins/spatial_plugin/docs/function_reference.md#stx_offsetcurve) | ラインの平行オフセット | |
| [`STX_ConcaveHull(geom, ratio [, allow_holes])`](plugins/spatial_plugin/docs/function_reference.md#stx_concavehull) | 凹包 | |
| [`STX_Snap(g1, g2, tolerance)`](plugins/spatial_plugin/docs/function_reference.md#stx_snap) | 頂点を別ジオメトリにスナップ | |
| [`STX_Polygonize(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_polygonize) | ラインワークからポリゴン構築 | |
| [`STX_BuildArea(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_buildarea) | ラインワークから面構築（内部リングは穴） | |
| [`STX_SharedPaths(g1, g2)`](plugins/spatial_plugin/docs/function_reference.md#stx_sharedpaths) | 2つの線形ジオメトリの共有パス抽出 | |
| [`STX_Node(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_node) | ラインのノード化（交差点で分割） | |
| [`STX_SimplifyPreserveTopology(geom, tol)`](plugins/spatial_plugin/docs/function_reference.md#stx_simplifypreservetopology) | トポロジ保持簡略化 | |
| [`STX_UnaryUnion(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_unaryunion) | ジオメトリ構成要素の Union | |
| [`STX_ClipByRect(geom, xmin, ymin, xmax, ymax)`](plugins/spatial_plugin/docs/function_reference.md#stx_clipbyrect) | 矩形による高速クリッピング | |
| [`STX_ReducePrecision(geom, gridsize)`](plugins/spatial_plugin/docs/function_reference.md#stx_reduceprecision) | 精度削減（妥当性保証付き） | |
| [`STX_MaximumInscribedCircle(geom, tolerance)`](plugins/spatial_plugin/docs/function_reference.md#stx_maximuminscribedcircle) | 最大内接円 | https://sakaik.hateblo.jp/entry/20260221/STX_MaximumInscribedCircle_func |
| [`STX_MinimumWidth(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_minimumwidth) | ジオメトリの最小幅 | |
| [`STX_SimplifyPolygonHull(geom, frac [, is_outer])`](plugins/spatial_plugin/docs/function_reference.md#stx_simplifypolygonhull) | ポリゴン Hull 簡略化 | |
| [`STX_ConcaveHullOfPolygons(geom, ratio [, holes])`](plugins/spatial_plugin/docs/function_reference.md#stx_concavehullofpolygons) | ポリゴン集合の凹包 | |

## 入出力フォーマット変換

| 関数 | 説明 | 実行例URL |
|---|---|---|
| [`STX_AsEncodedPolyline(geom [, prec])`](plugins/spatial_plugin/docs/function_reference.md#stx_asencodedpolyline) | Google Encoded Polyline に変換 | https://sakaik.hateblo.jp/entry/20260222/STX_LineFromEncodedPolyline_STX_AsEncodedPolyline_func |
| [`STX_LineFromEncodedPolyline(text [, srid [, prec]])`](plugins/spatial_plugin/docs/function_reference.md#stx_linefromencodedpolyline) | Encoded Polyline から LineString を構築 | https://sakaik.hateblo.jp/entry/20260222/STX_LineFromEncodedPolyline_STX_AsEncodedPolyline_func |
| [`STX_AsSvg(geom [, rel [, prec]])`](plugins/spatial_plugin/docs/function_reference.md#stx_assvg) | SVG パスデータに変換 | |
| [`STX_AsKml(geom [, prec])`](plugins/spatial_plugin/docs/function_reference.md#stx_askml) | KML に変換 | |
| [`STX_AsEwkt(geom)`](plugins/spatial_plugin/docs/function_reference.md#stx_asewkt) | EWKT（Extended WKT）に変換 | |
| [`STX_GeomFromEwkt(text)`](plugins/spatial_plugin/docs/function_reference.md#stx_geomfromewkt) | EWKT からジオメトリを構築 | |

## 座標ユーティリティ

| 関数 | 説明 | 実行例URL |
|---|---|---|
| [`STX_dms2deg(d, m, s)`](plugins/spatial_plugin/docs/function_reference.md#stx_dms2deg) | 度分秒→十進度変換(独自実装) | https://sakaik.hateblo.jp/entry/20260222/STX_dms2deg_STX_deg2dms_func |
| [`STX_deg2dms_deg(d)`](plugins/spatial_plugin/docs/function_reference.md#stx_deg2dms_deg) | 十進度→度の部分(独自実装) | https://sakaik.hateblo.jp/entry/20260222/STX_dms2deg_STX_deg2dms_func |
| [`STX_deg2dms_min(d)`](plugins/spatial_plugin/docs/function_reference.md#stx_deg2dms_min) | 十進度→分の部分(独自実装) | https://sakaik.hateblo.jp/entry/20260222/STX_dms2deg_STX_deg2dms_func |
| [`STX_deg2dms_sec(d)`](plugins/spatial_plugin/docs/function_reference.md#stx_deg2dms_sec) | 十進度→秒の部分(独自実装) | https://sakaik.hateblo.jp/entry/20260222/STX_dms2deg_STX_deg2dms_func |
