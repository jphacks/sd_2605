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
