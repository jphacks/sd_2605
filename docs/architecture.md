# システム構成

## 現在の実装

M5Stack CoreとCoreS3がそれぞれ単体で動作します。以下はCoreの構成です。PORT BのPIR（GPIO36）のLOW→HIGHを検知し、microSDの `/welcome.wav` を内蔵スピーカーで再生します。SDまたはWAVが利用できない場合は代替チャイムを鳴らします。再生終了後10秒間は新しい検知を受け付けません。

- `core.ino`: 起動、メインループ、検知・クールダウンの状態制御、日本語UI、A/B/Cボタン。
- `pir_sensor.h`: PIRの初期化と入力読み取り。
- `storage.h`: SDマウント、WAVヘッダー・形式検証、ファイルオープン。
- `audio_player.h`: 二重バッファの音声ストリーミング、チャイム、起動音。

共有状態は `core.ino` で宣言し、その後storage、audio、pirの順でヘッダーを読み込みます。
音声タスクのバッファ解放通知にはatomicフラグを使用します。メインループで再生・検知・ボタンを更新します。

## 将来の連携

`apps/`、`contracts/`、`api_client.h` はWebから音源を取得する機能のための配置先です。API通信・Wi-Fiはまだ実装していません。Coreの `storage.h` はSD専用です。
CoreS3は内蔵GC0308カメラのフレーム差分・LittleFS・WAV分割再生・タッチUIを備えます。外付けセンサーは使わず、LTR-553単独モードも定数で選択できます。
詳細と将来の音源更新方針は[CoreS3 README](../firmware/m5stack/cores3/README.md)を参照してください。
