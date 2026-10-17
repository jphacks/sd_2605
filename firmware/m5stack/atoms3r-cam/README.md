# AtomS3R-CAM AI Chatbot Kit: Rails API / LittleFS 音声

スケッチは `atoms3r-cam.ino`。M5Unifiedの `external_speaker.atomic_echo = true` を使い、動作確認済みのEcho Baseスピーカー構成でWAVを再生します。カメラ・IMU・マイク・RTC・PSoCは使用しません。元のスピーカー／カメラ確認スケッチは `examples/camera-smoke/camera-smoke.ino` に保存しています。

## 再利用とAPI互換性

CoreS3の切り替え可能なRails連携版から `ApiClient.h`、`Storage.h`、`WavValidation.h`、`AudioManager.h`、`EventManager.h` をコピーして再利用しました。CoreS3の現在の既定はカメラ単体版です。CoreS3ディレクトリ、Rails、API契約には変更を加えていません。Atom側だけに、独立したNVS名前空間、device_id/API接続先とのキャッシュ紐付け、診断ログを追加しています。

既存APIをそのまま使用します（認証ヘッダー不要、HTTPリダイレクト追従なし）。LAN内の `http://` と証明書を検証する `https://` に対応しています。HTTPSは mainと共通の `TlsRoots.h` の公開ルートCA群を使用します。接続先のCAが変わる場合は、検証済みのルートCAでバンドルを更新し再ビルドしてください。`API_TLS_INSECURE` の既定値は `false` です。証明書検証のためNTP時刻同期が必要で、起動直後の同期前には接続失敗後に再試行する場合があります。

| 処理 | エンドポイント | 間隔・保存 |
| --- | --- | --- |
| 設定と音声メタデータ | `POST /api/devices/:device_id/heartbeat`、本文 `{}` | 起動後接続時・約20秒ごと・`POLL` |
| 音声取得 | `GET /api/devices/:device_id/audio/arrive` と `/audio/leave` | より新しい版番号、または保存WAV不正時 |
| イベント送信 | `POST /api/devices/:device_id/events` | 永続キュー先頭を約10秒ごとに再送、200/201で削除 |

WAVはPCM16・モノラル・16kHz、最大5秒／200KiB。JSON、ファイルサイズ、RIFF構造を検証して保存します。LittleFSの一時ファイルから非アクティブバンクへ移動し、検証成功後にNVSの版番号・バンクを更新します。ダウンロード失敗時は前回の有効音声を保持します。設定とイベントキューもNVSに保存します。音声再生はPSRAMへ読み込んでから行い、ネットワーク処理は別タスクです。

## device_idは複数対応

Railsの `DeviceContentController#load_device` はURLのIDからDeviceを検索し、設定・音声・履歴は各Deviceの関連で管理しています。`DeviceAudio` はdevice_id/slot単位です。ハードウェアAPIは未登録IDを自動作成せず404を返します。Atomの既定IDはCoreS3の `entrance-1` と異なる `atoms3r-cam-1` です。

Webアプリのデバイス管理で、Atom専用のIDと8文字以上のデバイスパスワードを登録してください（既存の認証付き `POST /api/devices` を利用）。その端末を選択して音声と設定を管理します。登録用パスワードをファームウェアへ入れる必要はありません。同じIDを両端末に設定すると、同じ設定・音声・履歴を共有するため、別IDを使用してください。

調査根拠: `apps/backend/app/controllers/api/device_content_controller.rb`、`api/devices_controller.rb`、`app/models/device.rb`、`app/services/device_audio_store.rb`、`test/controllers/api/device_api_test.rb`。この確認はリポジトリ内の実装に基づきます。稼働中サーバーのデプロイ内容・新IDの登録状態は別途確認してください。

## 設定と初回書き込み（Windows PowerShell）

リポジトリルートで実行します。Arduino CLIにM5Stack ESP32コア、M5Unified、M5GFX、ArduinoJson 7が必要です。

```powershell
Copy-Item .\firmware\m5stack\atoms3r-cam\secrets.example.h .\firmware\m5stack\atoms3r-cam\secrets.h
notepad .\firmware\m5stack\atoms3r-cam\secrets.h
```

`WIFI_SSID`、`WIFI_PASSWORD`、`RAILS_BASE_URL`（末尾スラッシュなし、localhost不可）、`ENTRANCE_DEVICE_ID` を設定します。`secrets.h` と `build/` はGit除外対象です。

初回の未フォーマットLittleFSでは `[FS] unavailable` となります。シリアルから `INIT_FS` を明示的に送ると初期化できます（既存FSデータは消去）。設定で初期化する場合は以下の手順を使用します。初回プロビジョニング時だけ `secrets.h` の `ATOM_FORMAT_FS_ON_FAILURE` を `true` にしてビルド・書き込みしてください。これはマウント失敗時にLittleFSを消去する明示設定です。キャッシュがある端末で安易に有効にしないでください。成功後は `false` に戻して再ビルド・書き込みします。通常の書き込みではNVS/LittleFSを保持します。パーティションは変更しないでください。

```powershell
.\scripts\build-atoms3r-cam.ps1 -Jobs 4
arduino-cli board list
# 実際のAtomのCOM番号に置き換える。CoreS3と取り違えないこと。
.\scripts\upload-atoms3r-cam.ps1 -Port COM8
arduino-cli monitor -p COM8 --config baudrate=115200
```

ビルド・書き込みのFQBNは `m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB`。ボード既定のOPI PSRAMを使用し、3.1875MiBのアプリ領域と1.5MiBのLittleFS領域を確保します（パーティション表の名前は `spiffs`、使用する実装はLittleFS）。書き込み前にシリアルモニターを閉じてください。

device_idまたはAPI接続先を後から変更した場合、他端末のキャッシュや未送信イベントの流用を防ぐため起動を停止します。元の設定へ戻すか、Atomの保存データを破棄してよい場合のみ、対象COMを確認してフラッシュを全消去し、新端末として設定・初回プロビジョニングをやり直してください。

## シリアル実機テスト

コマンドは改行付きで送信します。CR/LF/CRLFに対応し、大文字小文字は区別しません。64文字超の行は破棄します。

| コマンド | 動作 |
| --- | --- |
| `LEFT_TO_RIGHT` | 外出WAV（leave）再生と `left_home` イベント追加 |
| `RIGHT_TO_LEFT` | 帰宅WAV（arrive）再生と `arrived_home` イベント追加 |
| `STATUS` | device_id、Wi-Fi/IP、HTTP結果、FS、PSRAM、設定、各キャッシュの版番号・妥当性 |
| `POLL` | 次の通信ループでheartbeatを要求 |
| `CHIME` | 診断用1200Hz・300ms（Railsの音量・挨拶無効設定とは独立） |
| `INIT_FS` | FS未マウント時のみ明示的にフォーマット・マウント。既存FSデータを消去するため初回構築時に限って実行。マウント済みなら拒否 |
| `HELP` | コマンド一覧 |

1. 起動時の `[SPEAKER] init=1` とPSRAM容量、FS容量を確認します。`CHIME` で実際に鳴ることを確認します。
2. Railsを起動し、Atom専用IDを登録します。`STATUS` のWi-Fi接続とIP、`[API] HTTPS heartbeat HTTP 200`（HTTP設定の場合は `HTTP heartbeat HTTP 200`）、両slotの `update=OK` を確認します。404ならID登録とURLを確認します。`STATUS` で両WAVが `valid=1` になったことを確認します。
3. `LEFT_TO_RIGHT` と `RIGHT_TO_LEFT` を再生終了を待って順に送ります。外出／帰宅の実際の発音、`[AUDIO] PLAY LEAVE/ARRIVE` → `DONE`、`[EVENT] HTTP 201 ack=1` を確認します。WebでAtom側の履歴にのみ増え、CoreS3側の設定・音声・履歴が変わらないことを確認します。疑似イベントも通常のRails履歴へ保存されます。
4. WebでAtomの音声・音量を変更し、`POLL` 後の版更新と再生を確認します。挨拶を無効にしたslotは `DISABLED` になり、音声を鳴らさずイベントだけ送ります。音量0は正常な無音です。再生中の入力は音声が `BUSY` となりますが、イベントは各入力分を追加します。
5. キャッシュ取得後にWi-Fiアクセスポイントを停止し、両方向を入力します。保存済み音声が再生されることを耳で確認します。電源を入れ直した状態でも両方向を再確認します。Wi-Fi復帰後、保存済み版と同じなら再ダウンロードせず再生できます。
6. NTP同期後に通信を切って追加したイベントはキューに残り、再接続後に送信されます（上限24件、同じevent_idで再送）。一方、RTCを使わないため、オフラインで電源を入れ直した直後は時刻未同期となり、音声は再生しますがイベントは `clock unsynced` として追加しません。満杯・保存失敗もログで確認できます。未同期イベントを後で捏造した時刻で送ることはしません。
7. Rails停止中にも保存音声を再生できること、再起動・再接続後に更新できることを確認します。音声GETの通信切断時は `FAILED (previous retained)` などを確認し、前回音声が引き続き使えることを確認します。

## 検証の区分

2026-10-18にM5Stack ESP32コア3.3.10、M5Unified 0.2.26、M5GFX 0.2.33、ArduinoJson 7.4.3のローカル環境で `scripts/build-atoms3r-cam.ps1 -Jobs 4` によるArduino CLIビルド成功を確認しました。初回はWi-Fi等がテンプレートの既定設定でのコンパイル確認です。その後、実機用設定とHTTPS対応を含めて再ビルドし、プログラム領域1,340,927バイト（40%）、静的RAM50,400バイト（15%）で成功しました。Rails同梱の両WAVについても16kHz・PCM16・モノラル・サイズ上限内を確認しました。

Arduino CLIでのコンパイル確認と、書き込み／シリアルログ確認／実際の発音確認は別の検証です。ビルド・書き込み・稼働中Railsとの通信とWAV保存は以下の通り確認済みです。音声の実発音・オフライン実機動作は上記手順による確認対象です。カメラやセンサーの実機確認は今回の対象外です。


### 2026-10-18 書き込み・通信の実機確認

- COM8のAtomS3R-CAMへArduino CLIで書き込み、書き込みデータのハッシュ検証成功を確認。
- ユーザーが登録した専用ID `atoms3r-cam-1` で動作。既存CoreS3はCOM4で動作し、変更・書き込みなし。
- 既存のローカル接続設定をAtom用のGit除外 `secrets.h` に引き継ぎ、証明書検証付きHTTPSで接続。
- シリアル `STATUS`: `wifi=1 HTTP=200 FS=1 PSRAM=8388608`。
- 初回LittleFSは未マウントだったため、`INIT_FS` を明示実行して `Explicit initialization: OK` を確認。
- `[API] HTTPS heartbeat HTTP 200`（HTTP設定の場合は `HTTP heartbeat HTTP 200`） を確認。
- 帰宅用: `slot=0 version=1 update=OK bytes=26144`、`arrive version=1 valid=1`。
- 外出用: `slot=1 version=1 update=OK bytes=34224`、`leave version=1 valid=1`。
- シリアル記録はGit除外の `build/atoms3r-cam-hardware.log` に保存。
- 今回は疎通確認のため、方向疑似イベントの送信・WAV再生・実発音・オフライン再起動は実施していません。


### main取り込み後のAtom側追従

リモート `origin/main` をfetchして差分がないことを確認後、ローカルmainの未push 4コミットを `38379b7` までfast-forwardで取り込みました。CoreS3側への独自の修正は加えていません。Railsのデバイス所有者修復は既存 `entrance-1` 向けで、Atomの専用ID・API形式の変更は不要です。

- `ApiClient.h`、`AudioSyncPolicy.h`、`TlsRoots.h` は取り込み後のCoreS3版をそのまま再利用しています。
- HTTPSはNTP同期まで待機し、待機中も保存音声の再生が可能です。Wi-Fi状態、TLS/HTTPエラー、各音声のGET結果と版番号をログに表示します。
- 正常な保存音声より古いAPI版での上書きを防ぎ、同じ版番号でも不正・欠損キャッシュなら再取得します。
- 接続先の既定値と設定例はmainと同じRender HTTPS URLです。ローカルの `secrets.h` 設定、専用ID `atoms3r-cam-1`、NVS名前空間、LittleFS、Echo Baseオーディオ、シリアル方向イベントは維持します。
- `.worktreeinclude` にAtom用 `secrets.h` のパスも追加しました。秘密情報の本体は引き続きGit除外です。
- ホスト回帰テストは `tests/atoms3r-cam/api_client_test.cpp`（Zig 0.13.0 / ArduinoJson 7.4.3で成功確認済み）。CoreS3と共通のホストモックを使い、実際のAtom APIヘッダーについて初回取得、同版スキップ、片側更新、HTTP失敗、途中切断、不正WAV、保存失敗、再起動、イベント再送、古い版の拒否、同版キャッシュ修復、NTP同期前のHTTPS待機を検証します。
- テスト実行例（Zigを使う場合は `g++` を `zig c++` に置き換え）:

```powershell
$arduinoUser = (arduino-cli config get directories.user).Trim()
g++ -std=c++14 -I "$arduinoUser/libraries/ArduinoJson/src" -I tests/cores3/api_stubs tests/atoms3r-cam/api_client_test.cpp -o build/atoms3r-cam-api-test.exe
.\build\atoms3r-cam-api-test.exe
.\scripts\build-atoms3r-cam.ps1 -Jobs 4
```

追従版はArduino CLIビルド成功（プログラム領域1,348,383バイト／40%、静的RAM50,400バイト／15%）。PCから現在のRails接続先を新CAバンドルで検証できることも確認しました。この追従版の実機への再書き込み・シリアル通信・実発音確認は未実施で、前項の書き込み済み版の検証結果とは別です。
