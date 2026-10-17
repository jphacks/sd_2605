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
