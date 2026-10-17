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

ローカルでログイン画面を確認する場合は、デバイスID `entrance-1`、仮パスワード `tadaima-demo` を使用してください。この仮ログインは開発・テスト環境のみで有効です。本番環境では利用できず、実際のデバイス認証や他のAPIエンドポイントも実装していません。
