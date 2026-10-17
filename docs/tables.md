# テーブル構成

マイグレーション `CreateInitialTables` で作る4テーブル。ロボットは1台で、`devices.id` は `entrance-1` を予定している。ログイン API と音声ファイル本体はまだない。WAV は後でディスクに置き、このテーブルには版番号だけを持つ。

`device_settings.device_id` と `home_events.device_id` は `devices.id` を参照する。`users` はほかのテーブルとつながない。

## users

Web で人を識別するためのテーブル。M5Stack は使わない。

- `id` (integer, 主キー): Rails が振るユーザー番号。行を追加するたびにデータベースが 1、2、3 と増やす
- `email` (string, 必須, 一意): ログインに使うメールアドレス。同じアドレスは2件登録できない
- `password_digest` (string, 必須): パスワードをダイジェスト化した値。原文は保存しない
- `created_at` (datetime, 必須): この行を作った時刻
- `updated_at` (datetime, 必須): この行を最後に更新した時刻

## devices

玄関ロボット1台が1行。

- `id` (string, 主キー): ロボットの識別子。整数の連番ではなく、`entrance-1` のような文字列をこちらで入れる。データベースは自動では埋めない
- `last_seen_at` (datetime, 空でもよい): M5Stack から heartbeat またはイベントが最後に届いた時刻。60秒以内ならオンライン、それより前か空ならオフラインとして扱う
- `created_at` (datetime, 必須): この行を作った時刻
- `updated_at` (datetime, 必須): この行を最後に更新した時刻。`last_seen_at` を更新したときも進む

## device_settings

ロボットの挨拶設定。`devices` 1台につき1行。

- `id` (integer, 主キー): Rails が振る設定行の番号
- `device_id` (string, 必須, 一意): どのロボットの設定か。`devices.id` を指す。1台に設定は1行だけ
- `leave_greeting_enabled` (boolean, 必須, 初期値 true): 外出時に「いってらっしゃい」を再生するか。`true` なら再生する
- `arrive_greeting_enabled` (boolean, 必須, 初期値 true): 帰宅時に「おかえり」を再生するか。`true` なら再生する
- `volume` (integer, 必須, 初期値 70): 再生音量。0 は無音、100 は最大
- `leave_audio_version` (integer, 必須, 初期値 1): 外出用 WAV の版番号。Web でファイルを差し替えるたびに 1 増やす。M5Stack は自分の保存値と違うときだけファイルを取り直す
- `arrive_audio_version` (integer, 必須, 初期値 1): 帰宅用 WAV の版番号。意味は `leave_audio_version` と同じ
- `created_at` (datetime, 必須): この行を作った時刻
- `updated_at` (datetime, 必須): 設定を最後に保存した時刻

## home_events

外出と帰宅の履歴。検知1回が1行。

- `id` (string, 主キー): M5Stack が検知1回に付ける `event_id`。再送で同じ ID が来ても2行目は作らない。データベースは自動では埋めない
- `device_id` (string, 必須): どのロボットの検知か。`devices.id` を指す
- `event_type` (string, 必須): 検知の種類。`left_home` は外出、`arrived_home` は帰宅
- `occurred_at` (datetime, 必須): M5Stack が外出または帰宅を検知した時刻。Web がその履歴を後から取得した時刻とは別
- `created_at` (datetime, 必須): Rails がこの行を受け取って保存した時刻
- `updated_at` (datetime, 必須): この行を最後に更新した時刻。同じ `id` の再送では、最初に保存した `occurred_at` を上書きしない
