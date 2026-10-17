# sd_2605-mobile

Rails API（`backend/`）と Vite + React（`frontend/`）を Docker Compose で起動します。DB は PostgreSQL です。

## 事前準備

Docker Desktop の Settings → Resources → WSL Integration で、このディストロを有効にしてからターミナルを開き直してください。

## 起動

```bash
docker compose up --build
```

- フロント: http://localhost:5173
- API: http://localhost:3000/api/health

画面に `status: ok` と出れば、Vite から Rails へ繋がっています。

開発用の DB パスワードは `password` です。本番の秘密情報は含めていません。
