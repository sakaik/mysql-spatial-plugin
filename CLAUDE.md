# MySQL Plugin Development Project

## Overview
MySQL 9.6 用のプラグインとしてspatial関数群を開発するプロジェクト。
boost::geometry ライブラリを使用して、MySQLに不足しているGIS関数を追加する。
関数名は `stx_` プレフィックス（ST eXtended）で統一。

## Directory Structure
- `mysql960/` - MySQL 9.6 バイナリインストール（実行中、port 3306）
- `mysql960.source/` - MySQL 9.6 ソースコード（ビルド済み）
- `plugins/` - プラグイン開発ディレクトリ
  - `gis_lib/` - 再利用可能なGISライブラリ（WKBパーサー、型定義等）
  - `spatial_plugin/` - メインプラグイン（stx_* 関数群）
  - `my_calc_plugin/` - サンプルプラグイン（動作確認済み）
- `claude/` - AI関連ファイル（計画、TODO、作業ログ等）※別Gitリポジトリ（ローカルのみ）

## Key Paths
- Plugin install dir: `mysql960/lib/plugin/`
- Boost headers: `mysql960.source/extra/boost/boost_1_87_0/`
- MySQL source include: `mysql960.source/include/`
- MySQL binary include: `mysql960/include/`
- MySQL GIS reference: `mysql960.source/sql/gis/` (boost::geometry usage examples)
- Function reference: `plugins/spatial_plugin/docs/function_reference.md`

## Plugin Architecture
- MYSQL_DAEMON_PLUGIN タイプを使用（MYSQL_UDF_PLUGIN は INSTALL PLUGIN でスキップされるため）
- UDF は `udf_registration` サービスで init 時に自動登録、deinit 時に自動解除
  - 個別の `CREATE FUNCTION` は不要
- 登録済み関数の確認: `SELECT * FROM performance_schema.user_defined_functions WHERE UDF_NAME LIKE 'stx_%';`

## MySQL Connection
```bash
mysql960/bin/mysql --socket=mysql960/mysql.sock -u root -p'MySQL9.6'
```

## Build / Test
```bash
cd plugins/spatial_plugin
make                # コンパイル
make install        # .so を plugin ディレクトリにコピー
make reload         # install + UNINSTALL/INSTALL PLUGIN（再ロード）
make test           # test.sql 実行（44テスト）
```

## Technical Notes
- MySQL WKB は Geographic SRID (4326等) でも (lon, lat) 順で格納 → boost geographic point と同順のため swap 不要
- Geographic 座標系の判定: `geographic_srids.h`（information_schema から自動生成、約500 SRID）
- Vincenty 法による測地線計算: `bg::formula::vincenty_inverse` / `vincenty_direct`
- STRING_RESULT で返す関数（stx_project, stx_linesubstring）は実際にはジオメトリバイナリ（SRID + WKB）を返す

## Git
- メインリポジトリ: `dev/`（リモート push 対象）
- claude リポジトリ: `dev/claude/`（ローカルのみ、.gitignore で除外）

## Conventions
- AI関連ファイルは全て `claude/` フォルダ以下に配置
- 作業ログはセッションごとに `claude/worklogs/` に作成
- タスク管理は `claude/todo.md` で行う
