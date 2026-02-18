# Not Implementable as MySQL UDF / MySQL UDF として実装困難な関数

This document lists PostGIS functions that **cannot be implemented** as MySQL UDF plugins due to MySQL's architectural constraints, or that lack the necessary GEOS C API.
This is a reference to prevent revisiting these decisions in the future.

このドキュメントは MySQL UDF プラグインの制約上 **実装不可能な** PostGIS 関数をまとめたもの。
今後、同じ検討を繰り返さないための参照資料。

---

## 1. Set-Returning Functions (SRF) / 集合返却関数

MySQL UDF は単一の値（スカラー値・文字列・BLOB）のみ返却可能。複数行を返す関数は実装できない。

| PostGIS Function | Description | Reason |
|---|---|---|
| **ST_Subdivide** | Recursively splits geometry so each part has ≤ max_vertices | Returns `SETOF geometry` (multiple rows). Internal implementation uses `GEOSClipByRect` but the set-returning behavior cannot be replicated in UDF. |

### Workaround / 回避策
- `STX_Clipbyrect` を個別に呼ぶことで部分的に代替可能
- ストアドプロシージャで再帰分割ロジックを実装する手もある（パフォーマンスは劣る）

---

## 2. Window Functions / ウィンドウ関数

MySQL UDF はウィンドウ関数として登録できない。`OVER (PARTITION BY ...)` 構文と組み合わせられない。

| PostGIS Function | Description | GEOS C API | Reason |
|---|---|---|---|
| **ST_CoverageSimplify** | Simplify polygons in a coverage while maintaining shared edges | `GEOSCoverageSimplifyVW` (3.12+) | Window function — needs access to neighboring rows |
| **ST_CoverageInvalidEdges** | Validate coverage topology and return invalid edges | `GEOSCoverageIsValid` (3.12+) | Window function — needs access to neighboring rows |

### Note
The GEOS C API versions of these functions take a `GeometryCollection` as input (not a window).
A potential future approach: accept a GeometryCollection and return a GeometryCollection of results.
However, the typical PostGIS usage pattern (window function over a table) cannot be replicated.

---

## 3. Aggregate Functions / 集約関数

MySQL UDF は集約関数（`GROUP BY` で使う）として登録可能だが、入力がジオメトリの集約関数は実装が複雑。

| PostGIS Function | Description | GEOS C API | Status |
|---|---|---|---|
| **ST_CoverageUnion** | Union polygons in a coverage by removing shared edges | `GEOSCoverageUnion` (3.8+) | Aggregate function. Could potentially implement as UDF_AGG, but input must be a valid coverage (no gaps/overlaps), limiting practical use. |

### Note
- `ST_CoverageUnion` は単一の GeometryCollection を受け取る GEOS C API 版なら実装可能だが、PostGIS のように `GROUP BY` で集約する使い方は困難
- MySQL の `ST_Union` 集約関数（MySQL 8.0.27+）で部分的に代替可能

---

## 4. No GEOS C API / GEOS C API が存在しない関数

PostGIS が独自に C 言語で実装しており、GEOS C API として公開されていない関数。

| PostGIS Function | Description | Notes |
|---|---|---|
| **ST_Split** | Split a geometry by another geometry | PostGIS proprietary implementation. Splits lines by points/lines/polygon-boundaries, and polygons by lines. Complex logic involving intersection detection and geometry reconstruction. |

### ST_Split について
- GEOS C API には `GEOSSplit` は存在しない（2026-02 時点、GEOS 3.14.1）
- PostGIS は `lwgeom_split.c` で約1500行の独自実装
- 自前実装する場合は相当な開発工数が必要
- TODO に「自前実装が必要な関数」として記載済み

---

## Investigated: 2026-02-18
## GEOS Version: 3.14.1
