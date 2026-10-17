# sd_2605-mobile

Rails API（`backend/`）と Vite + React（`frontend/`）を Docker Compose で起動します。DB は PostgreSQL です。

Docker Desktop を入れてから、リポジトリ直下で次を実行してください。

```bash
docker compose up --build
```

- フロント: http://localhost:5173
- API: http://localhost:3000/api/health

画面に `status: ok` と出れば、Vite から Rails へ繋がっています。

開発用の DB パスワードは `password` です。

ログインはメールアドレスとパスワードです。デバイスID `entrance-1`、仮パスワード `tadaima-demo` の仮ログインは開発・テスト環境のみで有効です。

## デプロイ（Render）

フロントのビルド成果物を Rails が配信する1つの Web Service と、PostgreSQL を Render の無料枠に置きます。設定は `render.yaml` です。

1. このリポジトリを GitHub に push する
2. [Render](https://render.com) に GitHub でサインアップし、`jphacks` org のリポジトリへのアクセスを許可する
3. Dashboard の **New > Blueprint** でこのリポジトリを選ぶ。Web Service と PostgreSQL が作られる
4. ビルドが終わったら `https://<service>.onrender.com/api/health` が `{"status":"ok"}` になることを確認する

環境変数は Blueprint が設定します。

- `RAILS_ENV=production`
- `SECRET_KEY_BASE`（Render が生成）
- `DATABASE_URL`（PostgreSQL の接続文字列）

push するたびに自動でデプロイされます。15分アクセスがないとスリープし、次のアクセスで数十秒かかります。発表の直前に一度開いて起こしておいてください。無料の PostgreSQL は作成から30日で失効します。


## CoreS3との音声同期

ログイン認証の失敗はHTTP 401、ログイン済みユーザーが所有しないデバイスへのアクセスはHTTP 403（`device_forbidden`）で返します。403ではユーザーセッションを削除せず、デバイス管理で登録・切り替えができます。古いリクエストの401が再ログイン後に届いても、新しいトークンは削除しません。フロント側の回帰テストは `cd frontend && npm test`、ビルドは `npm run build` で実行します。

端末は約20秒ごとに `POST /api/devices/entrance-1/heartbeat`（JSON `{}`）を送り、返された `leave_audio` / `arrive_audio` の版番号を比較して `GET /api/devices/entrance-1/audio/leave` または `/arrive` を取得します。応答は200のWAV本体で、外部ストレージへのリダイレクトは行いません。イベントは従来のevent_id/event_type/occurred_atを `POST /api/devices/:device_id/events` へ送ります。端末用の3 APIは既存契約どおり認証不要です。

最初にWebでユーザー登録・ログインし、デバイスID `entrance-1` を登録してください。未知の端末はJSON 404になり、自動登録しません。Webの状態取得・履歴・設定変更・音声登録はユーザー認証と所有確認が必要です。

メッセージ登録画面のWAVは帰宅用arriveとして保存します。`POST /api/devices/:device_id/message` はmultipartの `file` / `sender_name` を受け付け、必要なら `slot=leave` で外出用を指定できます。PCM16bit・mono・16kHz・5秒以内・200KiB以下を全RIFFチャンクまで検証し、失敗時は以前の音声と版番号を維持します。

音声本体は `device_audios.wav`（PostgreSQL bytea）へ保存し、音声本体とslotの版番号を同じトランザクションで更新します。Render無料Web Serviceの一時ディスクへ音声を保存しないため、再起動・再デプロイ後もDBが保持されていれば音声が残ります。初回の未登録slotには `backend/config/audio/{arrive,leave}.wav` を返します。これは既存CoreS3音源を16kHzへ変換したものです。Webの登録済み件数には既定音源を含めません。

デプロイ時は従来どおり `rails db:prepare` で `20261018000000_create_device_audios` を適用します。モノレポからRenderをビルドする場合はRoot Directoryを `apps`、Dockerfileを `Dockerfile.render` にします。既存Renderが `sd_2605-mobile` リポジトリを参照している場合、このモノレポへの変更だけでは反映されないため `git subtree push --prefix=apps https://github.com/jphacks/sd_2605-mobile.git main` でWeb側を反映します。

```bash
cd backend
RAILS_ENV=test bundle exec rails db:prepare
bundle exec rails test
```

`device_api_test` はWeb登録→heartbeat→WAVのバイナリ取得、slot別更新、不正音声時の保持、端末イベント再送、所有者制限を実際のRailsとPostgreSQLで検証します。サービスのテストはWAV仕様とDBロールバックを検証します。
