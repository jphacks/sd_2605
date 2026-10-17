# CoreS3 Render移行の検証記録（2026-10-18 JST）

## 端末内WAVと本番配信WAVのバイト照合

COM4のLittleFS領域（0x290000、0x160000 bytes）をesptoolのread-flashで読み取り、PC上のコピーをLittleFSとして開いて照合しました。端末のFlash消去・ファイル更新は行っていません。

| 端末のアクティブ音声 | 本番音声 | サイズ | SHA-256 / バイト列 |
| --- | --- | --- | --- |
| /arrive1.wav、version 1 | GET audio/arrive | 26,144 bytes | 完全一致 |
| /leave1.wav、version 1 | GET audio/leave | 34,224 bytes | 完全一致 |

読み取り後の再起動でも両WAVの正常読込、Wi-Fi接続、HTTPS heartbeat 200を3回、画面のWiFi:ON API:200を確認しました。サーバーの音声は現在どちらも既定音声で、updated_at/registered_byはnullです。同版なので新たなGETは省略されています。Web録音を更新した際の実機ダウンロード・実際の発音は、この照合とは別の確認です。

証跡はGit対象外の `build/cores3-audio-byte-comparison.json`、`build/cores3-audio-readback.log`、`build/cores3-audio-post-readback.log`。

## 最新の実機確認：Wi-Fi状態表示版

COM4のESP32-S3へ `scripts/upload-cores3.ps1 -Port COM4` で書き込み、ハッシュ照合に成功しました。Flash 1,390,619 / 2,621,440 bytes（53%）、静的RAM 70,248 / 327,680 bytes（21%）。Wi-Fi認証情報はGit対象外のsecrets.hに設定し、画面・ログには出していません。

45秒の実機シリアル取得で以下を確認しました。

- `[WiFi] connected`、`[TIME] clock ready for API`。
- HTTPS heartbeat HTTP 200を3回。両slotのversionは1。
- `[UI WiFi] WiFi:CONNECTING` → `WiFi:ON API WAIT` → `WiFi:ON API:200`。画面右側のPIR行の下へ状態を描画する処理の実行を確認。
- 再起動後に `/arrive1.wav`（26,144 bytes、version 1）と `/leave1.wav`（34,224 bytes、version 1）を正常に読み込み。同版のため今回の起動ではGETを省略。
- カメラ約20fps、errors=0、IDLE。人の通過による方向判定と実際の発音は今回未確認。

ログはGit対象外の `build/cores3-wifi-display-upload.log` と `build/cores3-wifi-display-serial.log`。以下の「実機未実施」はこの確認より前の記録です。

## 現在の本番確認結果（2026-10-18 01:18 JST）

Web APIをデプロイ済みです。ファームウェアと同じCA集合でPCからTLS・ホスト名を検証し、次を確認しました。

| API | 結果 |
| --- | --- |
| POST heartbeat `{}` | HTTP 200、既存契約のJSON。device_id=entrance-1、音量70、両挨拶有効 |
| GET audio/leave | HTTP 200、version 1、34,224 bytes、audio/wav、Content-Length一致 |
| GET audio/arrive | HTTP 200、version 1、26,144 bytes、audio/wav、Content-Length一致 |
| POST events `{}`（不正入力） | HTTP 422、JSON validation_error。架空の通行履歴は登録せず |

認証なしで端末APIを利用でき、リダイレクトはありませんでした。両WAVはPython waveと実際のRuby WavAudioパーサーでPCM16bit/16kHz/mono・5秒以内・全RIFFチャンク境界を検証して成功。取得結果はGit対象外の `build/render-{leave,arrive}.wav` と `render-heartbeat-current.*` / `render-events-current.*` にあります。Web録音のアップロード成功・版番号更新・イベント201/200は実Rails/DBテストで確認済みで、本番ユーザーの音声や履歴は変更していません。

CoreS3の書き込み・シリアル確認・実際の発音と人検知は未実施です。以下の404表は実装前の調査履歴です。

## 初回調査時の本番APIの状態（Web実装前）

`https://sd-2605-mobile.onrender.com` に対し、PCから証明書検証を有効にしたcurlでリクエストしました。リダイレクトは追従していません。

| リクエスト | HTTP | Content-Type | 結果 |
| --- | --- | --- | --- |
| POST /api/devices/entrance-1/heartbeat、JSON `{}` | 404 | text/html; charset=UTF-8 | 音声メタデータのJSONなし |
| GET /api/devices/entrance-1/audio/leave | 404 | text/html; charset=UTF-8 | WAVなし |
| GET /api/devices/entrance-1/audio/arrive | 404 | text/html; charset=UTF-8 | WAVなし |
| POST /api/devices/entrance-1/events、JSON `{}` | 404 | text/html; charset=UTF-8 | 契約の422検証エラーも確認できず |

いずれもLocation・WWW-Authenticateヘッダーはなく、認証要求・リダイレクトは観測されませんでした。ただし404だけでは正しいAPIに認証が不要であると断定できません。eventsは空の不正リクエストのみで、架空の通行履歴を登録していません。成功時の201/200・重複処理は本番では未確認です。

トップページはReactのHTMLを返します。配信JSのAPIベースは同一ホストの`/api`で、別ホストの指定は見つかりませんでした。Rails由来と思われるx-runtime/x-request-idも観測しましたが、404の原因が未デプロイ・ルーティング・端末未登録のどれかは判定できません。初回調査時には`apps/`へRails実装が未取り込みでした。後でsd_2605-mobileをsubtreeとして取り込み、実際のroutes.rbにも端末APIが未定義であることを確認しました。既存契約は`contracts/openapi.yaml`と`docs/m5stack-api.md`にあります。APIのパス・認証方式・JSON契約は推測で変更していません。

その後、`apps/backend`へ上記APIを追加しました。Webの認証・所有者確認付きメッセージ登録からWAV保存、端末向けheartbeat・音声GET・イベントPOSTまで実装しています。音声はPostgreSQL byteaで永続化し、既存の音声versionと同時にトランザクションで更新します。未登録slotには既存音源を16kHzへ変換した既定WAVを返します。端末の未知IDはJSON 404です。

生レスポンスはGit対象外の`build/render-{heartbeat,leave,arrive,events}.{headers,body}`に保存しました。ホストの証明書はGoogle Trust ServicesのWE1発行で、ファームウェアに同梱したGTS Root R1〜R4/ISRG Root X1を使ったPython SSLでも、ホスト名・証明書チェーン検証に成功しました。これはPCでの確認でありESP32上のTLS検証ではありません。

## 実装

- 現行カメラ版の方向判定・PIR照合・MAP・CHIME・TEST SOUNDは維持し、既存Storage/EventManager/ApiClientを接続。
- ベースURLはRender、端末IDはentrance-1。Git対象外のsecrets.hでLANのHTTP URLへ切り替え可能。既存ローカルsecrets.hのURLも更新済み。認証情報は変更・記録していません。
- HTTPSはCAとホスト名を検証。開発用API_TLS_INSECUREの既定はfalse。NTP待ちは通信タスクだけに限定。
- 同期は約20秒周期。新しい版または保存ファイル欠落・破損時だけ取得。有効な同版・古い版はスキップ。
- 既存の有界ストリーミング、一時ファイル、WAV検証、非アクティブbank、NVSコミットを再利用。
- 再生用PSRAMバッファはSpeaker解放後・再生停止中だけ更新。通信失敗時は保存音声を維持し、再生時のAPI問い合わせは不要。
- イベントのフィールドと再送はEventManagerを変更せず使用。自動送信は従来どおり明示的にENABLE_AUTO_EVENTS=trueの場合だけ。

## ビルド・Rails検証結果

- Windows Arduino CLI、既存M5Stack ESP32 3.3.10 / M5Unified 0.2.26 / ArduinoJson 7.4.3、既存の`build-cores3.ps1 -Jobs 4`でコンパイル・リンク成功。
- Flash 1,389,727 / 2,621,440 bytes（53%）、静的RAM 70,224 / 327,680 bytes（21%）。最終再ビルドのログは`build/cores3-render-build.log`。
- HTTPS + カメラで元の1,310,720 bytesを超えるため、スケッチの`partitions.csv`で未使用OTA領域をfactory領域へ統合。NVSの0x9000/0x5000、LittleFSの0x290000/0x160000は維持。コンパイルされたパーティションバイナリを解析して一致を検証済み。USB更新専用、全Flash消去は不要。
- WSL Ubuntu 24.04のRuby 3.2.3 / Rails 8.1.4 / PostgreSQL 16で、33 tests・154 assertions・0 failures/errors/skips。Windowsマウントの低速I/Oを避け、同じソースをWSLの一時ディレクトリへコピーして実行。
- WSL Ruby 3.2向け開発ツール依存は一時Gemfileで解決し、リポジトリのGemfile/Gemfile.lockは変更していません。本番Dockerは既存のRuby 3.3.11です。
- 実RailsでWeb登録→WAV永続化→heartbeat→バイナリGET、slotごとのversion、不正WAVでの保持、DBロールバック、認証と所有確認、イベントの201/200再送を確認済み。
- Web変更は`6c344bb`、subtree経由のデプロイ元`sd_2605-mobile/main`は`c12af26`へpush済み。CoreS3変更はユーザーの前回指定どおり未コミットでローカルに保持。
- 実機への書き込み・シリアル取得・実際の発音と人検知は未実施。

## テストの範囲

| 要求 | 結果 |
| --- | --- |
| 1. 本番API疎通 | 本番heartbeat・両音声GETが200、不正eventsが422。TLS/形式/サイズ/リダイレクトなしを確認 |
| 2. heartbeat解析 | 実ApiClient + ArduinoJsonによるホスト模擬テスト成功。不正JSON・不正形式も拒否。本番の成功JSONもPCで型・device_id・メタデータを検証済み |
| 3. バージョン変更 | 初回・新しい版・slot別更新・古い版の抑止をホストで確認 |
| 4. WAV取得・LittleFS保存 | 実ApiClientの模擬HTTP/FSテスト成功。実ESP32のLittleFS書き込みは未検証 |
| 5. 同版で取得しない | 模擬GET呼び出し回数を確認、成功 |
| 6. HTTPエラー時の旧WAV維持 | 503・404・転送切断・不正WAV・サイズ不一致・コミット失敗を模擬して成功 |
| 7. 再起動後の再生 | 模擬保存レコードの復元とWAV検証成功。実機再起動・発音は未検証 |
| 8. 既存検知・再生 | 既存の方向判定・PIR照合・WAVパーサーのホスト回帰テスト成功。実センサーと音質は未検証 |
| 9. events契約維持 | EventManager無変更。既存テストでID・UTC・種別・再送・キューを確認。模擬HTTPでも503後の再送と201での削除を確認。本番成功応答は未確認 |

HTTPClient/WiFi/ファイルシステム/NVSはホストテストで模擬しています。模擬テストは実際のTLS・FreeRTOS並行動作・LittleFS/NVS耐電断・Speakerを保証しません。

ホストテスト（Zig 0.13.0、ArduinoJson 7.4.3。g++でも実行可能）:

```powershell
$arduinoUser = (arduino-cli config get directories.user).Trim()
g++ -std=c++14 -I "$arduinoUser/libraries/ArduinoJson/src" -I tests/cores3/api_stubs tests/cores3/api_client_test.cpp -o build/cores3-api-client-test.exe
.\build\cores3-api-client-test.exe
g++ -std=c++14 tests/cores3/audio_sync_test.cpp -o build/cores3-audio-sync-test.exe
.\build\cores3-audio-sync-test.exe
```

既存storage_test.cpp、motion_direction_test.cpp、pir_fusion_test.cpp、event_test.cppも成功しています。

## 書き込み・シリアル・実機確認

今回は書き込みとシリアル取得を実施していません。COM4/COM7は列挙されましたがボード種別はUnknownでした。次のCOM4は過去のCoreS3ポート例で、実際の接続ポートに合わせます。

```powershell
.\scripts\build-cores3.ps1 -Jobs 4
.\scripts\upload-cores3.ps1 -Port COM4
arduino-cli monitor -p COM4 --config baudrate=115200
```

1. secrets.hのURL・Wi-Fi・端末IDとAPI_TLS_INSECURE=falseを確認して書き込む。LittleFS全体の再書き込み・初期化は不要。
2. 起動時の[FS]容量、[VOICE] loaded、[API] base/TLS設定、[WiFi] connected、[TIME]を確認。機密値はログに出さない。
3. API修復後、[API] HTTPS heartbeat HTTP 200、versions、[AUDIO] GET HTTP 200/update=OK、[VOICE] loadedの順で確認。未知のdevice_idはJSON 404になるため、Webでentrance-1を登録しておく。
4. 次回heartbeatで同じ版ならGETが増えないことを確認。Webでleaveだけ更新し、leaveだけ取得されることを確認。
5. 実際に左右から通過し、PIR有効なら照合成立後に方向別音声が一度再生されることを耳で確認。MAP反転・CHIME・TEST SOUNDも確認。
6. 再生中の音声更新、Wi-Fi切断、API停止、再起動を試し、旧音声・保存設定で再生できること、撮影fpsと検知遅延・音質を比較する。
7. 時計同期後にENABLE_AUTO_EVENTS=trueの設定で、既存event_id/event_type/occurred_atとキュー再送・重複抑止をサーバー側でも確認する。
8. 切断・破損WAV・容量不足・NVS保存失敗・更新中電源断で旧音声が残ることを実機で確認する。

コンパイル、書き込み、シリアルログ、実際の発音、人検知はそれぞれ別に記録してください。


## 変更ファイル

CoreS3: `ApiClient.h`、`CameraOnly.h`、`CameraVoice.h`、`Config.h`、`secrets.example.h`、`AudioSyncPolicy.h`、`TlsRoots.h`、`partitions.csv`、専用README。ビルド: `scripts/build-cores3.ps1`。ローカル `secrets.h` はURLのみ更新しGit対象外を維持。Core/M5GOのコードに差分はありません。

Web: `apps/backend/app/controllers/api/device_content_controller.rb`、`app/models/device.rb`・`device_audio.rb`、`app/services/device_audio_store.rb`・`wav_audio.rb`、`config/routes.rb`、`config/audio/{leave,arrive}.wav`、`db/migrate/20261018000000_create_device_audios.rb`、`db/schema.rb`、`db/seeds.rb`、Web API/サービスのテスト、`apps/.gitattributes`、`apps/README.md`。

ホスト検証: `tests/cores3/api_client_test.cpp`、`api_stubs/{HostApi,HTTPClient,WiFi,WiFiClientSecure}.h`、`audio_sync_test.cpp`。ドキュメント: ルートREADMEと本書。
