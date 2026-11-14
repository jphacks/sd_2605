# M5Stack・Rails API インターフェース仕様

Webアプリ担当者から共有された仕様（2026-10-17）。本書は連携時の契約をまとめたもので、ファームウェアの実装済み機能を示すものではありません。現在の実装は[システム構成](architecture.md)を参照してください。

## 通信と保存の方針

- M5StackとWebは直接通信しません。M5StackがRails APIへHTTPリクエストを送り、Railsから端末への接続は張りません。
- 挨拶には本体のSDまたはフラッシュに保存したWAVを使います。Webを閉じていても、前回取得した設定と音声があれば挨拶できます。
- 音量、挨拶のオン・オフ、音声の差し替えはWebが保存し、次のheartbeat以降の応答に反映されます。
- 端末が使用するAPIはheartbeat、音声取得、イベント送信、居住者の声のアップロードです。Web向けの状態取得、履歴、設定更新、挨拶音声のアップロードAPIは使用しません。
- heartbeatの応答には、保存済みの顔パーツを後から取るための`character.version`と`character.path`も含めます。現在のファームウェアはこのフィールドを無視します。後から足す処理は、版番号が本体の保存値と違うときだけ`path`をGETし、200のJSONを保存するだけです。画面への顔の反映は別作業です。`version`が0、またはGETが404のときは、保存済みの顔はありません。

## 接続・共通形式

| 項目 | 値・規則 |
| --- | --- |
| ベースURL | `http://<Rails マシンの LAN アドレス>:3000` |
| device_id | MVPでは `entrance-1` |
| 認証ヘッダー | 不要 |
| JSON | UTF-8、`Content-Type: application/json` |
| 時刻 | ISO 8601のUTC（例: `2026-10-17T05:30:00Z`） |
| 成否判定 | 2xx以外は失敗。イベント送信の完了条件は200または201 |
| 未知のdevice_id・slot | 404。端末の自動登録はしない |

`localhost`は端末からRailsマシンに届くアドレスではありません。RailsマシンのLANアドレスを指定します。応答内の`path`は絶対URLではなくパスです。端末がベースURLを付けてGETします。

### 共通エラー

```json
{
  "error": {
    "code": "validation_error",
    "message": "event_type is invalid"
  }
}
```

| フィールド | 型 | 意味 |
| --- | --- | --- |
| error | object | 失敗の内容。成功時の応答には含まれない |
| error.code | string | `not_found`: 指定した端末やslotがない（404）。`validation_error`: リクエストの値が不正（422） |
| error.message | string | 人向けの説明。端末の成否判定には使わず、codeとHTTPステータスを見る |

## 端末の動作

1. 約20秒ごとに`POST /api/devices/entrance-1/heartbeat`を送信します。ボディは`{}`で構いません。
2. 応答の設定を保存し、`leave_audio.version`と`arrive_audio.version`を、それぞれ本体に保存した版番号と比較します。
3. 版番号が異なるslotだけ、その`path`をGETします。受信したバイトをSDまたはフラッシュに保存してから、そのslotの保存済みversionを更新します。
4. 外出を検知したら、`leave_greeting_enabled`が`true`のときだけ保存済みの外出WAVを`volume`で再生し、その後`left_home`イベントをPOSTします。帰宅も同様に`arrive_greeting_enabled`と`arrived_home`を使用します。挨拶が無効でもイベントは送信します。
5. イベントPOSTが失敗したら、同じ`event_id`のまま再送します。200または201が返るまでIDを変更しません。

heartbeatや音声GETが失敗しても、前回保存した設定とWAVで挨拶します。音声GETに失敗したslotの保存済みversionは更新しません。

## POST /api/devices/:device_id/heartbeat

`last_seen_at`を更新し、設定と音声メタデータを返します。音声バイトは含みません。

リクエストボディ:

```json
{}
```

応答 **200**:

```json
{
  "device_id": "entrance-1",
  "leave_greeting_enabled": true,
  "arrive_greeting_enabled": true,
  "volume": 70,
  "leave_audio": {
    "version": 1,
    "byte_size": 16044,
    "content_type": "audio/wav",
    "path": "/api/devices/entrance-1/audio/leave"
  },
  "arrive_audio": {
    "version": 1,
    "byte_size": 16044,
    "content_type": "audio/wav",
    "path": "/api/devices/entrance-1/audio/arrive"
  },
  "character": {
    "version": 0,
    "path": "/api/devices/entrance-1/character"
  }
}
```

| フィールド | 型 | 意味 |
| --- | --- | --- |
| device_id | string | 設定の対象ロボット。MVPでは`entrance-1` |
| leave_greeting_enabled | boolean | `true`なら外出時に「いってらっしゃい」を再生 |
| arrive_greeting_enabled | boolean | `true`なら帰宅時に「おかえり」を再生 |
| volume | integer | 0〜100。0は無音、100は最大 |
| leave_audio | object | 外出用WAVのメタデータ。ファイル本体はpathから取得 |
| arrive_audio | object | 帰宅用WAVのメタデータ。構造はleave_audioと同じ |
| character | object | 顔パーツの取得情報。JSON本体は含まない。現在のファームウェアは無視する |

音声メタデータのフィールド:

| フィールド | 型 | 意味 |
| --- | --- | --- |
| version | integer | slotごとの版番号。初期値1。Webで差し替えるたびに1増える。本体の保存値と同じならGETしない |
| byte_size | integer | WAVのバイト数。保存前の空き容量確認に使用 |
| content_type | string | 常に`audio/wav` |
| path | string | ベースURLに付ける音声GETパス。leave_audioは外出用、arrive_audioは帰宅用 |

`character`のフィールド:

| フィールド | 型 | 意味 |
| --- | --- | --- |
| version | integer | 顔パーツの版番号。未保存は0。Webで保存するたびに1増える。本体の保存値と同じならGETしない |
| path | string | ベースURLに付ける顔パーツのGETパス。未保存でもパスは返す |

## GET /api/devices/:device_id/character

保存済みの顔パーツJSONを返します。認証ヘッダーは不要です。ヘッダーを付けたときは、そのログインが有効で、その家のメンバーである必要があります。壊れたトークンは401、参加していない家は403です。

未保存は **404** で、`error.code`は`not_found`です。

応答 **200** はパーツの辞書そのものです。包み直しません。

```json
{
  "bg": "pink",
  "hairColor": "black",
  "hair": "straight-bangs",
  "eyeColor": "brown",
  "eyes": "female",
  "glasses": "round",
  "beard": "none",
  "mouth": "closed-smile",
  "extras": ["freckles"],
  "uncertain": []
}
```

| フィールド | 型 | 意味 |
| --- | --- | --- |
| bg, hairColor, hair, eyeColor, eyes, glasses, beard, mouth | string | カタログにあるIDのどれか |
| extras | array | カタログにある飾りのID。無いときは空配列 |
| uncertain | array | 写真から選んだとき、自信がなかった項目名。手で保存したときは空でもよい |

M5が後から行うのは、このJSONを保存することまでです。この辞書を画面の顔に描く処理は、このAPIの範囲外です。写真のアップロードとパーツの保存はWeb向けで、端末は呼びません。

## GET /api/devices/:device_id/audio/:slot

`slot`は`leave`（外出）または`arrive`（帰宅）です。

応答 **200** はWAVの生バイトで、`Content-Type: audio/wav`と`Content-Length`が付きます。リダイレクトはせず、JSONやBase64にも変換しません。

WAVの条件:

- モノラル
- 16bit PCM
- 16kHz
- 5秒以内
- 200KB以下

取得例:

```text
GET http://192.168.1.20:3000/api/devices/entrance-1/audio/leave
```

## POST /api/devices/:device_id/events

リクエスト:

```json
{
  "event_id": "entrance-1-20261017T053000Z-left",
  "event_type": "left_home",
  "occurred_at": "2026-10-17T05:30:00Z"
}
```

| フィールド | 型 | 意味・制約 |
| --- | --- | --- |
| event_id | string | 検知1回に端末が付けるID。空は不可。同じ外出・帰宅の重複登録を防ぐため、再送でも変更しない |
| event_type | string | 外出は`left_home`、帰宅は`arrived_home`。これ以外は422 |
| occurred_at | string | 端末が検知した時刻。ISO 8601のUTC。Webが後から履歴を取得する時刻とは別。欠落時は422 |
| voice | string | 省略可。居住者が録音しなかったときだけ `skipped` を付ける。そのとき、通知を許可した差し出し手のブラウザーへ文面のお知らせを送る。省略したイベント作成だけでは通知しない。再送では重複通知しない |

新規登録は **201**、同じ`event_id`が既に存在する場合は **200** を返します。どちらの場合もイベントは1件だけとなり、成功時は`last_seen_at`も更新します。

応答 **200または201**:

```json
{
  "event_id": "entrance-1-20261017T053000Z-left",
  "event_type": "left_home",
  "occurred_at": "2026-10-17T05:30:00Z"
}
```

フィールドの意味はリクエストと同じで、Railsが保存した値を返します。同じ`event_id`の再送では、リクエストの内容で上書きせず、最初に保存した値を返します。応答に `voice` は含まれません。

## POST /api/devices/:device_id/events/:event_id/voice

外出または帰宅のイベントを保存したあと、居住者がその場で録った声を送ります。挨拶用の `leave` / `arrive` には書きません。認証ヘッダーは不要です。

返信録音のWAVはPCM16bit・16kHz・monoで、10秒以内・320KiB以下です（挨拶音声は従来どおり5秒以内・200KiB以下）。次のどちらかで送ります。

- `Content-Type: audio/wav` のリクエストボディにWAVの生バイトを入れる
- `multipart/form-data` の `file` にWAVを入れる

新規保存は **201**、同じイベントへの再送は保存済みのWAVを上書きして **200** を返します。通知は最初の1回で、再送では重複通知しません。先に `voice=skipped` の通知を送ったあとに声が届いても、通知は追加しません。

メール通知は既定で停止しています。通知を許可した差し出し手のブラウザーにWeb Pushを送り、通知から認証付きの履歴・録音再生へ遷移します。通知サービスへの送信はDBの送信待ちとActive Jobで処理し、録音APIの応答を待たせません。詳細は[Web Push通知](web-push.md)を参照してください。ハードウェア向けPOSTの応答JSONは従来の3フィールドを維持します。

応答 **200または201**:

```json
{
  "event_id": "entrance-1-20261017T053000Z-left",
  "event_type": "left_home",
  "occurred_at": "2026-10-17T05:30:00Z"
}
```

未知の `event_id` は404です。WAVの形式が違うときは422で、保存済みの声は変えません。

CoreS3の返信録音（2026-11-14、10秒・VAD版）は、挨拶再生終了後の10秒間をPCM16 mono 16kHzで録音します。ESP-SRのWebRTC VADで20msごとに判定し、400ms以上続く発話区間の前200ms・後300msを残して連結します。`events`作成成功後に、そのIDの`voice`へ最大320,044 bytesのWAVを生バイトでPOSTします。発話なし・VAD初期化失敗時はイベントのみを送り、WAV保存・音声送信・メール送信は行いません（`voice=skipped`は付けません）。再試行時もイベントIDを保持します。挨拶用の音声取得・登録APIやWAV仕様は変更していません。録音は1件ずつ保持し、送信待ち中の追加録音は省略します。旧ファームウェアの未送信3秒録音も復元・送信できます。メールの同期送信を待つ録音POSTに限り応答待ち時間を45秒にしています。Webの録音受信成功と、Web画面での再生確認は別です。
