# 関数チェックリスト

各関数の実行例ブログ記事の進捗管理。

## 空間計測・述語

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Perimeter(geom)` | Polygon/MultiPolygon の周長 | https://sakaik.hateblo.jp/entry/20260219/STX_Perimeter_func |
| `STX_Coveredby(g1, g2)` | g1 が g2 に覆われているか判定 | |
| `STX_Covers(g1, g2)` | g1 が g2 を覆っているか判定 | |
| `STX_Dwithin(g1, g2, dist)` | 2つのジオメトリ間の距離が閾値以内か判定 | |
| `STX_Azimuth(p1, p2)` | p1 から p2 への方位角（ラジアン、北から時計回り） | |
| `STX_Angle(p1, p2, p3)` | p2 における p1-p2-p3 の角度 | |
| `STX_Relate(g1, g2)` | DE-9IM 関係行列を返す | |
| `STX_Relatematch(g1, g2, pattern)` | DE-9IM パターンマッチ判定 | |
| `STX_Npoints(geom)` | 全頂点数を返す（全ジオメトリ型対応） |https://sakaik.hateblo.jp/entry/20260219/STX_NPoints_func |
| `STX_Isring(linestring)` | LineString がリング（閉環・非自己交差）か判定 | |

## ジオメトリ処理

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Project(point, dist, azimuth)` | 指定距離・方位角で点を投影 | |
| `STX_Linelocatepoint(line, point)` | ライン上の最近接点の位置（0.0〜1.0） | |
| `STX_Linesubstring(line, start, end)` | ラインの一部を抽出 | |
| `STX_Closestpoint(point, geom)` | ジオメトリ上の最近接点を返す | |
| `STX_Shortestline(g1, g2)` | 2つのジオメトリ間の最短線分を返す | |
| `STX_Pointonsurface(geom)` | ポリゴン内部の点を返す | |
| `STX_Points(geom)` | 全頂点を MultiPoint として抽出 | |
| `STX_Makepoint(x, y [, srid])` | 座標から POINT を構築 | |
| `STX_Makeline(p1, p2)` / `STX_Makeline(multipoint)` | Point群から LineString を構築 | |
| `STX_Makepolygon(ring [, inner_rings])` | LineString から Polygon を構築 | |
| `STX_Generatepoints(geom, n [, seed])` | ポリゴン内のランダム点を生成 | |
| `STX_Minimumboundingcircle(geom [, segs])` | 最小外接円（Polygon として返却） | |
| `STX_Squaregrid(size, geom)` | バウンディングボックスを覆う矩形グリッド | |
| `STX_Hexgrid(size, geom)` | バウンディングボックスを覆う六角形グリッド | |

## 座標変換

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Translate(geom, dx, dy)` | ジオメトリを平行移動 | |
| `STX_Scale(geom, sx, sy)` | ジオメトリを拡大・縮小 | |
| `STX_Rotate(geom, angle [, center])` | ジオメトリを回転（原点または指定中心） | |
| `STX_Affine(geom, a, b, d, e, xoff, yoff)` | 一般2Dアフィン変換 | |
| `STX_Reverse(geom)` | 頂点の順序を反転 | |
| `STX_Snaptogrid(geom, size [, size_y])` | 座標をグリッドにスナップ | |
| `STX_Removerepeatedpoints(geom [, tol])` | 連続する重複頂点を除去 | |
| `STX_Segmentize(geom, max_length)` | 長い辺を分割（頂点追加） | |

## GEOS ベース関数

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Makevalid(geom)` | 不正ジオメトリの修復 | |
| `STX_Linemerge(geom)` | 接続する LineString の結合 | |
| `STX_Voronoi(geom [, tolerance [, envelope]])` | ボロノイ図 | |
| `STX_Delaunay(geom [, tolerance [, edges_only]])` | ドロネー三角形分割 | |
| `STX_Offsetcurve(geom, dist [, quad_segs [, join [, mitre]]])` | ラインの平行オフセット | |
| `STX_Concavehull(geom, ratio [, allow_holes])` | 凹包 | |
| `STX_Snap(g1, g2, tolerance)` | 頂点を別ジオメトリにスナップ | |
| `STX_Polygonize(geom)` | ラインワークからポリゴン構築 | |
| `STX_Buildarea(geom)` | ラインワークから面構築（内部リングは穴） | |
| `STX_Sharedpaths(g1, g2)` | 2つの線形ジオメトリの共有パス抽出 | |
| `STX_Node(geom)` | ラインのノード化（交差点で分割） | |
| `STX_Simplifypreservetopology(geom, tol)` | トポロジ保持簡略化 | |
| `STX_Unaryunion(geom)` | ジオメトリ構成要素の Union | |
| `STX_Clipbyrect(geom, xmin, ymin, xmax, ymax)` | 矩形による高速クリッピング | |
| `STX_Reduceprecision(geom, gridsize)` | 精度削減（妥当性保証付き） | |
| `STX_Maximuminscribedcircle(geom, tolerance)` | 最大内接円 | |
| `STX_Minimumwidth(geom)` | ジオメトリの最小幅 | |
| `STX_Simplifypolygonhull(geom, frac [, is_outer])` | ポリゴン Hull 簡略化 | |
| `STX_Concavehullofpolygons(geom, ratio [, holes])` | ポリゴン集合の凹包 | |

## 入出力フォーマット変換

| 関数 | 説明 | 実行例URL |
|---|---|---|
| `STX_Asencodedpolyline(geom [, prec])` | Google Encoded Polyline に変換 | |
| `STX_Linefromenccodedpolyline(text [, srid [, prec]])` | Encoded Polyline から LineString を構築 | |
| `STX_Assvg(geom [, rel [, prec]])` | SVG パスデータに変換 | |
| `STX_Askml(geom [, prec])` | KML に変換 | |
| `STX_Asewkt(geom)` | EWKT（Extended WKT）に変換 | |
| `STX_Geomfromewkt(text)` | EWKT からジオメトリを構築 | |
