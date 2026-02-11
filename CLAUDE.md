# MySQL Plugin Development Project

## Overview
MySQL 9.6 用のプラグインとしてspatial関数群を開発するプロジェクト。
boost::geometry ライブラリを使用して、MySQLに不足しているGIS関数を追加する。

## Directory Structure
- `mysql960/` - MySQL 9.6 バイナリインストール（実行中、port 3306）
- `mysql960.source/` - MySQL 9.6 ソースコード（ビルド済み）
- `plugins/` - プラグイン開発ディレクトリ
  - `my_calc_plugin/` - サンプルプラグイン（動作確認済み）
- `claude/` - AI関連ファイル（計画、TODO、作業ログ等）
  - `Plans/` - 要件・検討資料
  - `worklogs/` - セッションごとの作業ログ
  - `todo.md` - タスク管理

## Key Paths
- Plugin install dir: `mysql960/lib/plugin/`
- Boost headers: `mysql960.source/extra/boost/boost_1_87_0/`
- MySQL source include: `mysql960.source/include/`
- MySQL binary include: `mysql960/include/`
- MySQL GIS reference: `mysql960.source/sql/gis/` (boost::geometry usage examples)

## Plugin Architecture
- MYSQL_DAEMON_PLUGIN タイプを使用（MYSQL_UDF_PLUGIN は INSTALL PLUGIN でスキップされるため）
- UDF関数は `CREATE FUNCTION ... SONAME '...'` で個別登録
- サンプル実装: `plugins/my_calc_plugin/my_plugin_calc.cc`

## MySQL Connection
```bash
mysql960/bin/mysql --socket=mysql960/data/../mysql960/mysql.sock -u root
# or
mysql960/bin/mysql --socket=mysql960/mysql.sock -u root
```

## Build Commands (example)
```bash
g++ -shared -fPIC -o plugin.so plugin.cc \
  -I mysql960.source/include \
  -I mysql960.source/extra/boost/boost_1_87_0 \
  -L mysql960/lib -lmysqlservices
```

## Conventions
- AI関連ファイルは全て `claude/` フォルダ以下に配置
- 作業ログはセッションごとに `claude/worklogs/` に作成
- タスク管理は `claude/todo.md` で行う
