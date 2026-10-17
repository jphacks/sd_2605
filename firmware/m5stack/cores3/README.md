# CoreS3 内蔵カメラ動体検知・音声再生

外付けセンサー・microSD・Wi-Fiは使用しません。既存のCoreとは独立しています。
M5Unifiedで内蔵画面、タッチ、AW88298スピーカーを操作し、esp_cameraでGC0308を取得します。
`cores3.ino`は共有状態とUI、`motion_detector.h`は内蔵センサー、`motion_score.h`は画像差分、
`storage.h`はLittleFS/WAV検証、`audio_player.h`は非同期再生を担当します。
以前の`pir_sensor.h`は使用していません。

## 検知と操作

起動時に1000Hz・200msの確認音を再生します。初期化後3秒はカメラの露出安定待ちです。
160×120 RGB565をPSRAMの2フレームバッファで取得し、150msごとに40×30の輝度へ間引きます。
前回との差の平均値を引いて全体の明るさ変化を補正し、変化量20以上の画素率をスコアにします。
平均輝度変化が30を超えるフレームを除外し、スコア8%以上が2回連続したら検知します。
これは人の識別ではありません。影、画面や本体の揺れ、局所的な照明変化でも検知し得ます。

検知時はMOTIONを250ms表示後、LittleFSの`/welcome.wav`または2音チャイムを再生します。
検知直後から再生中の検知を抑制し、再生終了後さらに10秒待ちます。
再生・停止・クールダウン中も背景を更新し、古い画像との比較による再検知を避けます。
フレームは正常時・異常時とも取得後に返却します。カメラ初期化失敗時も手動音テストは可能です。

- `TEST PLAY`: 手動再生。再生中・検知表示中は重複要求を受け付けません。
- `DETECT ON/OFF`: 検知切り替え。切り替え時に連続フレーム数と比較基準をリセットします。
- 画面: カメラ初期化状態、READY / MOTION / PLAYING、スコアと閾値、回数、残り待機秒、近接反射強度。
  READYでもOFF・WARMUP・クールダウン中は検知しません。左側に160×120プレビューを表示します。
- シリアル115200 baud: `[CAMERA]`、`[SCORE]`、`[MOTION]`、`[AUDIO]`、エラーを出力します。

調整用の定数は`cores3.ino`冒頭です。`MOTION_THRESHOLD`は変化画素率（%）、`PIXEL_DELTA`は
画素変化量、`REQUIRED_FRAMES`は連続回数、`LIGHT_SHIFT_LIMIT`は全体輝度変化の上限、
`FRAME_INTERVAL_MS`は取得間隔、`COOLDOWN_MS`は待機時間です。`SHOW_PREVIEW=false`で表示負荷を減らせます。

## 内蔵近接センサー比較モード

`CORES3_USE_PROXIMITY`を1へ変更して再ビルドするとLTR-553単独モードになります（既定0）。
この場合カメラを初期化せず、既存の`M5.In_I2C`で内部アドレス0x23を操作します。
カメラの配線・RGB565設定は公式サンプルを参考にしています。内部GPIO12/11のI2Cは解放せず、
esp_cameraの既存ポート借用（SCCB SDA=-1、`M5.In_I2C.getPort()`）を使います。
初期化はスピーカー再生開始前に完了させ、再生タスクからセンサー操作はしません。

PS値は0～2047の近接反射強度であり、距離ではありません。`PROX_RELEASE=200`未満で再待機し、
`PROX_THRESHOLD=300`以上が2回連続すると検知します。無効値・飽和値を検知に使いません。
再生中・待機中の反応を予約しません。カメラモードではPS読み取りはOFFです。

## コンパイル・書き込み

ビルドスクリプトはキャッシュ先を`build/cores3-cache`に固定します。
Arduino標準クラスの未解決参照などキャッシュ異常時は`./scripts/build-cores3.ps1 -Clean -Jobs 1`で再ビルドしてください。

必要環境: Arduino CLI、M5Stack ESP32 3.3.10、M5Unified 0.2.26、M5GFX 0.2.33。
リポジトリ直下で実行します。スケッチの配置は`firmware/m5stack/cores3/cores3.ino`です。

```powershell
.\scripts\build-cores3.ps1
# 書き込み先がCoreS3であることを確認して実行
.\scripts\upload-cores3.ps1 -Port COM5
arduino-cli monitor -p COM5 --config baudrate=115200
```

直接コンパイルする場合:

```powershell
arduino-cli compile --fqbn 'm5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default' --output-dir .\build\cores3 .\firmware\m5stack\cores3
```

PSRAMとLittleFS用パーティションを明示します。オプションなしのボード既定ではLittleFS領域を
利用できないため、上記設定を使用してください。出力・音源・ログはGit対象外の`build/`へ保存します。

## LittleFSへの音源配置

`LittleFS.begin(false)`でマウントし、自動フォーマットしません。未作成・破損・音源なしでは
チャイムを再生します。通常のファームウェアuploadではWAVは配置されません。
WAVはPCM形式1、16kHz、16bit、モノラルに限定し、RIFF長、fmt、data、チャンク境界を検証します。
2×1024バイトを交互に読み、スピーカーの解放通知後に再利用します。

以下はM5Stack ESP32 3.3.10の`PartitionScheme=default`専用です。
LittleFSが使う`spiffs`領域はoffset `0x290000`、size `0x160000`（1,441,792バイト）です。
設定を変えた場合はアドレス・容量を再確認してください。イメージ書き込みは既存のFS全体を置き換えます。

```powershell
New-Item -ItemType Directory -Force build/cores3-data
ffmpeg -i input.wav -acodec pcm_s16le -ac 1 -ar 16000 build/cores3-data/welcome.wav
$pkg = "$env:LOCALAPPDATA/Arduino15/packages/m5stack"
& "$pkg/tools/mklittlefs/4.0.2-db0513a/mklittlefs.exe" -c build/cores3-data -b 4096 -p 256 -s 1441792 build/cores3-littlefs.bin
if ($LASTEXITCODE -ne 0) { throw 'LittleFS image creation failed' }
# シリアルモニターを閉じ、接続先がCoreS3であることを確認する
& "$pkg/tools/esptool_py/5.3.0/esptool.exe" --chip esp32s3 --port COM5 write-flash 0x290000 build/cores3-littlefs.bin
if ($LASTEXITCODE -ne 0) { throw 'LittleFS upload failed' }
```

ツールのバージョン・配置はインストール済み環境に合わせてください。旧esptoolは`write_flash`を使用します。
再起動後にLittleFS容量ログとTEST PLAYで音源を確認してください。

## 将来の音源取得

Webアプリ・ダウンロードは今回未実装です。将来は取得したPCM WAVを一時ファイルに保存し、
検証完了後、再生中の置き換えを避けて`/welcome.wav`へrenameする構成を想定しています。

## 実機確認手順

1. 起動チャイム、Camera OK、プレビューを確認し、3秒以上静止します。
2. 前を横切り、スコア、MOTION、回数増加、PLAYING、発音を確認します。
3. 再生中と再生終了後10秒以内は連続検知しないことを確認します。
4. 待機終了後に再度横切り、再検知することを確認します。
5. 照明ON/OFF、静止映像で誤検知を調べ、閾値を調整します。
6. TEST PLAY連打とDETECT ON/OFF、正常WAV・音源なし・不正WAVを確認します。
7. 近接モードへ切り替え、PS生値、閾値越え、離した後の再検知を確認します。

コンパイル、書き込み、シリアル確認、実際の発音・カメラ検知・近接・タッチの確認は別々に記録してください。
以前のPIR版の書き込み・発音確認は、このカメラ版の実機確認結果には含めません。

参考: [公式カメラサンプル](https://docs.m5stack.com/en/arduino/m5cores3/camera)、
[公式LTR553サンプル](https://docs.m5stack.com/en/arduino/m5cores3/ltr553)。

## ホスト検証

```sh
g++ -std=c++11 tests/cores3/motion_score_test.cpp -o /tmp/cores3-motion-test && /tmp/cores3-motion-test
g++ -std=c++11 tests/cores3/storage_test.cpp -o /tmp/cores3-storage-test && /tmp/cores3-storage-test
```

同一画像・一様な照明変化・局所変化・小さなノイズと、WAVのチャンク境界や形式不正を検証します。
実際の発音・カメラ映像・タッチ・共有バス動作は実機確認が必要です。

## 今回のコンパイル検証（2026-10-17）

- 最終カメラ版: Arduino CLI、FQBN `m5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default`で成功。
- Flash 653,643 / 1,310,720バイト（49%）、静的RAM 39,288 / 327,680バイト（11%）。PSRAM確保は実行時です。
- 共有キャッシュの`FunctionalInterrupt.cpp.o: file truncated`を避け、`--build-path build/cores3-camera-verify`で再検証。
  検証済み出力は`build/cores3-camera-verified/`です。通常のスクリプト出力は`build/cores3/`です。
- 今回の書き込み・シリアル確認・実際の発音・カメラ動体検知・タッチ・近接センサー確認は未実施です。
  近接モードの個別ビルドとホスト単体テストも今回の検証対象には含めていません。
- `firmware/m5stack/core/`の差分なし。生成物はGit対象外です。

## カメラ版の書き込み・シリアル確認（2026-10-17）

- COM4のESP32-S3へ`build/cores3-camera-verified/`の最終版を書き込み、全領域のハッシュ検証成功。
- PSRAM 8,388,608バイト、カメラ初期化`0x0`、LittleFSと既存`/welcome.wav`の形式検証成功を起動ログで確認。
- CAMERA検知2回、MOTION→PLAYING、WAV再生終了、終了後10秒の検知抑制と再検知をシリアルで確認。
  反応の原因が人の通過であるか、実際の発音・プレビュー・タッチの動作は未確認。
- 起動時に既存コアダンプ領域の`Incorrect size of core dump image`警告あり。その後は起動・検知・再生処理が継続。
- ログ: `build/cores3/serial-camera-upload.log`（Git対象外）。LittleFSの音源は置き換えていません。

## 今回の検証結果（2026-10-17）

- Arduino CLI 1.5.1、M5Stack ESP32 3.3.10、M5Unified 0.2.26、M5GFX 0.2.33で
  最終カメラモードのコンパイル・リンクに成功しました。
- FQBN: `m5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default`。
  `--clean --jobs 1 --build-path ./build/cores3-verify --output-dir ./build/cores3`で検証。
  Flash使用率49%、静的RAM使用率11%。成果物とログは`build/`配下です。
- 最初の一時キャッシュ先エラー、続くArduino標準クラスのリンクエラーは、
  新規ビルド先・キャッシュなし・単一ジョブでのビルドで回避しました。
  ビルドスクリプトの既定も1ジョブとし、`-Clean`を追加しています。
- 差分スコアとWAVパーサーのホストテストが成功しました。
- 今回の書き込み・シリアル確認・実際の発音・カメラ検知・近接・タッチの実機確認は未実施です。
  近接モードは実機での生値確認と閾値調整が必要です。
- `firmware/m5stack/core/`は変更していません。
