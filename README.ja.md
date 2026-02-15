# STX Spatial Plugin for MySQL

MySQL に不足している空間関数を追加するプラグインです。[Boost.Geometry](https://www.boost.org/doc/libs/release/libs/geometry/) を利用して実装しています。

距離クエリ、空間関係判定（DE-9IM）、座標変換などの GIS 関数を提供します。Cartesian（平面直交座標系）と Geographic（WGS84 等の地理座標系）の両方に対応しています。

## 関数一覧（17関数）

| 関数 | 説明 |
|---|---|
| `stx_perimeter(geom)` | Polygon/MultiPolygon の周長 |
| `stx_coveredby(g1, g2)` | g1 が g2 に覆われているか判定 |
| `stx_covers(g1, g2)` | g1 が g2 を覆っているか判定 |
| `stx_dwithin(g1, g2, dist)` | 2つのジオメトリ間の距離が閾値以内か判定 |
| `stx_azimuth(p1, p2)` | p1 から p2 への方位角（ラジアン、北から時計回り） |
| `stx_project(point, dist, azimuth)` | 指定距離・方位角で点を投影 |
| `stx_linelocatepoint(line, point)` | ライン上の最近接点の位置（0.0〜1.0） |
| `stx_linesubstring(line, start, end)` | ラインの一部を抽出 |
| `stx_angle(p1, p2, p3)` | p2 における p1-p2-p3 の角度 |
| `stx_translate(geom, dx, dy)` | ジオメトリを平行移動 |
| `stx_scale(geom, sx, sy)` | ジオメトリを拡大・縮小 |
| `stx_rotate(geom, angle [, center])` | ジオメトリを回転（原点または指定中心） |
| `stx_reverse(geom)` | 頂点の順序を反転 |
| `stx_pointonsurface(geom)` | ポリゴン内部の点を返す |
| `stx_closestpoint(point, geom)` | ジオメトリ上の最近接点を返す |
| `stx_relate(g1, g2)` | DE-9IM 関係行列を返す |
| `stx_relatematch(g1, g2, pattern)` | DE-9IM パターンマッチ判定 |

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

全17関数が自動的に登録されます。個別の `CREATE FUNCTION` は不要です。

```sql
-- 確認
SELECT * FROM performance_schema.user_defined_functions WHERE UDF_NAME LIKE 'stx_%';
SHOW STATUS LIKE 'spatial_plugin_%';
```

## 使用例

```sql
-- ポリゴンの周長（Geographic、結果はメートル）
SELECT stx_perimeter(ST_GeomFromText('POLYGON((139 35,140 35,140 36,139 36,139 35))', 4326));

-- 2点間の距離が 1km 以内か？
SELECT stx_dwithin(
    ST_GeomFromText('POINT(139.7 35.7)', 4326),
    ST_GeomFromText('POINT(139.71 35.71)', 4326),
    1000
);

-- ポリゴンを重心を中心に 45 度回転
SELECT stx_rotate(
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))'),
    PI()/4,
    ST_GeomFromText('POINT(5 5)')
);

-- DE-9IM 空間関係
SELECT stx_relate(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))')
);
```

### 戻り値の型について

ジオメトリを返す関数は MySQL UDF の制約により `STRING_RESULT` を使用しています。
バイナリデータの中身は正しいジオメトリ形式（SRID + WKB）であり、他の空間関数にそのまま渡せます：

```sql
SELECT ST_AsText(stx_translate(ST_GeomFromText('POINT(1 2)'), 10, 20));
-- POINT(11 22)
```

## テスト

```bash
make test       # テストスイートを実行（85テスト）
```

## ライセンス

[GNU General Public License v2.0](LICENSE)
