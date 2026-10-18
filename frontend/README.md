# ただいま。 Frontend

家族・友人が録音した短い音声メッセージを登録し、帰宅を検知したM5Stackデバイスから再生するWebフロントエンドです。

## 機能
- メールアドレスとパスワードによるアカウント登録・ログイン
- ログイン中のアカウントへのデバイスID・デバイス用パスワード登録
- 複数デバイスの表示名変更、切り替え、ログアウト
- デバイスのオンライン状態・最終通信・メッセージ登録状況の表示
- イベント履歴の表示
- ブラウザ録音、録音の試聴、WAV変換、メッセージ登録
- デバイスの再生音量設定

## 技術構成
- React 18 / TypeScript / Vite
- React Router
- 音声変換: Web Audio API / OfflineAudioContext
- API: Rails想定のJSON API（`/api`）

## 必要環境
- Node.js 22系を推奨
- npm
- Rails APIサーバー（またはAPI仕様に準拠したモック）
- マイク利用時は `localhost` または HTTPS が必要

## セットアップ
```bash
npm install
cp .env.example .env
npm run dev
```
ブラウザで `http://localhost:5173` を開きます。`VITE_API_PROXY_TARGET` は開発サーバーの `/api` プロキシ先を指定します（既定値 `http://localhost:3000`）。

## 環境変数
| 変数 | 既定値 | 用途 |
|---|---|---|
| `VITE_API_BASE_URL` | `/api` | ブラウザから呼び出すAPIのベースURL |
| `VITE_API_PROXY_TARGET` | `http://localhost:3000` | Vite開発サーバーのAPIプロキシ先。ブラウザから直接参照するURLではない |

`.env.example` は `VITE_API_BASE_URL` の例を含みます。`VITE_API_PROXY_TARGET` を変更する場合は `.env.local` などに追加してください。デバイスは初期状態では空で、デバイス管理で登録したものだけが表示されます。

## コマンド
```bash
npm run dev        # 開発サーバー
npm run typecheck  # TypeScript型チェック
npm run build      # 型チェック後に本番ビルド
npm run preview    # ビルド成果物のプレビュー
```

## APIと認証
- 新規登録は `POST /api/users`、ログインは `POST /api/user_sessions` を使用します。どちらも Rails が発行した JWT を受け取り、ブラウザセッション中のみ `sessionStorage` に保存します。
- デバイス管理画面で登録したデバイスID・パスワードは `POST /api/devices` に送信し、ログイン中ユーザーの所有デバイスとして保存します。既存デバイスへの再ログインは `POST /api/sessions` を使用します。
- フロントエンドが利用するHTTP APIの契約は [API_CONTRACT.md](./API_CONTRACT.md) を参照してください。
- バックエンド担当への実装・確認事項は [BACKEND_HANDOFF.md](./BACKEND_HANDOFF.md) を参照してください。
- M5Stack側のプロトコル・動作要件は [HARDWARE_HANDOFF.md](./HARDWARE_HANDOFF.md) を参照してください。
- 全体要件と受け入れ条件は [SPECIFICATION.md](./SPECIFICATION.md) を参照してください。
- WebログインのBearer tokenとデバイスのポーリング認証は別系統です。フロントエンドはWeb tokenを `sessionStorage` に保存し、ブラウザセッション中のみ利用します。HTTPSを必須とし、XSS対策とCSPも本番構成で検討してください。

## 現状の制約・未検証事項
- Rails APIと実機M5Stackがない状態では、エンドツーエンド動作は確認できません。
- フロントエンドの自動テストはまだありません。バックエンドのユーザー・デバイス認証 API には Rails テストがあります。
- `npm run typecheck` は型チェックです。`npm run build` は依存関係のインストール状態に左右されます。
- デバイス管理用の `/api/sessions` は現状開発・テスト環境向けです。実運用のデバイス認証方式はバックエンド側で別途確定してください。
- ブラウザ録音形式はブラウザ依存です。WAV変換対応・マイク権限・音質をChrome等の対象ブラウザで実機確認してください。

## ディレクトリ
```text
src/
  api.ts                 API型、認証、HTTP共通処理
  audio.ts               ブラウザ音声BlobからWAV PCM16 mono 16 kHzへの変換
  devices.ts             デバイス一覧とセッション状態
  components/            共通レイアウト・ロボット表示・デバイス切替
  pages/                 ログイン、ホーム、履歴、音声登録、設定、デバイス管理
  types.ts               UI用共通型
API_CONTRACT.md          API契約
MULTI_DEVICE.md          複数デバイス設計メモ
BACKEND_HANDOFF.md       バックエンド申し送り
HARDWARE_HANDOFF.md      ハードウェア申し送り
SPECIFICATION.md         システム仕様・受け入れ条件
```

## 開発時の注意
- `VITE_*` の値はビルド成果物に含まれ得るため、秘密情報を置かないでください。
- フロントエンドは音声をサーバーへ送信し、デバイス側の再生を待つ設計です。ブラウザ上でサーバー音声を再生する機能はありません。
- `MULTI_DEVICE.md` は実装意図のメモです。最終的なセキュリティ保証はバックエンドの認可・リソーススコープで実施してください。
