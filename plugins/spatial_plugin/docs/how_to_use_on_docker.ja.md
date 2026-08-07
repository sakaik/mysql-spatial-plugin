# Docker 上での STX Spatial Plugin の利用方法

## 概要

本プラグインを簡易的に MySQL の Docker 公式イメージで動作させる手順を紹介します。
以下の例では **MySQL 9.7.2（現行 LTS）** を使用します。他のバージョン（例: Innovation
`mysql:26.7.0`）を使う場合は、`image:` と `.so` の MySQL バージョン部分を揃えてください。
デーモンプラグインは MySQL バージョンに厳密に紐づくため、両者は完全一致が必須です。

対応 MySQL 版の一覧と .so ファイル名は
[Releases ページ](https://github.com/sakaik/mysql-spatial-plugin/releases) を参照してください。

## 前提条件

- Docker がインストール済みであり実行可能な状態になっていること


## 手順
## MySQL dockerプラグイン動作環境用意
### 作業フォルダの作成
```
mkdir -p ~/work/mysqlplugin
cd !$
```

### docker-composeファイルの作成
- docker-compose.yaml
```
services:
    db:
      image: mysql:9.7.2
      environment:
          MYSQL_ROOT_PASSWORD: mypass
          MYSQL_DATABASE: mydb
      ports:
        - "3306:3306"
      volumes:
        - db_data:/var/lib/mysql
        - ./spatial_plugin-mysql-9.7.2-glibc-2.34.so:/usr/lib64/mysql/plugin/spatial_plugin-mysql-9.7.2-glibc-2.34.so

volumes:
   db_data:
```

`.so` はコンテナ側でもファイル名を変えずにマウントしています。ファイル名にビルド対象の
MySQL バージョン (`9.7.2`) と必要 glibc floor (`2.34`) が入っており、あとで
`SHOW STATUS LIKE 'spatial_plugin_built_for'` の値と照合できます。

### pluginを取得
公式 mysql:9.7.2 イメージは Oracle Linux 9 ベース (glibc 2.34) なので glibc-2.34 版を使います。

```
wget https://github.com/sakaik/mysql-spatial-plugin/releases/download/v0.3.0/spatial_plugin-mysql-9.7.2-glibc-2.34.so
```

### dockerでMySQLサーバを起動
```
docker compose up
```


### MySQLサーバにmysqlクライアントを使って接続（サーバは立ち上がりっぱなしなので別の窓を開けて作業）
```
docker compose exec db mysql -u root -pmypass mydb
```

※パスワードの扱いは docker-compose.yamlの記述とあわせ、よしなに変更してください

- 接続時の例
```
	$ docker compose exec db mysql -u root -pmypass
	mysql: [Warning] Using a password on the command line interface can be insecure.
	Welcome to the MySQL monitor.  Commands end with ; or \g.
	Your MySQL connection id is 9
	Server version: 9.7.2 MySQL Community Server - GPL

	Copyright (c) 2000, 2026, Oracle and/or its affiliates.

	Oracle is a registered trademark of Oracle Corporation and/or its
	affiliates. Other names may be trademarks of their respective
	owners.

	Type 'help;' or '\h' for help. Type '\c' to clear the current input statement.
```


### プラグインのインストール
plugin ディレクトリに置いた `.so` のファイル名で load します：
```
INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin-mysql-9.7.2-glibc-2.34.so';

-- .so とサーバのバージョン整合性を確認
SHOW STATUS LIKE 'spatial_plugin_%';
```

`spatial_plugin_built_for` が接続先サーバのバージョンと一致していれば OK です
（一致しない場合は `INSTALL PLUGIN` 自体が "API version for DAEMON plugin is too different" で失敗します）。

### 動作確認の例
```
mysql> SELECT STX_Perimeter(ST_GeomFromText('POLYGON((35 135, 35 136, 36 136, 36 135, 35 135))', 6668)) pm;
+--------------------+
| pm                 |
+--------------------+
| 403352.30562388065 |
+--------------------+
1 row in set (0.049 sec)
```

これで60個以上の追加Spatial関数をMySQLで使うことができます。「バグ報告ドリブン開発(BRDD)」ですので、おかしな挙動があったらぜひ報告をお願いします。