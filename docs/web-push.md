# ブラウザー通知（Web Push）

メール通知は既定で停止しています。`MAIL_NOTIFICATIONS_ENABLED=false`のままSMTP設定を保持し、イベント・録音の保存は継続します。明示的にメールも再開するときだけ、この値を`true`にします。

## iPhoneのChromeから使う

1. 本番のWebアプリをChromeで開き、「共有」から「ホーム画面に追加」します。
2. ホーム画面のアイコンから開き、差し出し手として参加しているアカウントでログインします。
3. 下部の「情報」→「ブラウザー通知」→「通知を有効にする」を押し、端末の通知を許可します。
4. 「通知をテスト」を押して、通知センターへの到着を確認します。
5. 新しい録音の通知をタップすると、対象の家・できごとの履歴が開きます。「録音を開く」→音声プレーヤーの再生ボタンで聴けます。

iPhoneではiOS 16.4以降のホーム画面Webアプリが必要です。Chromeの通常タブ内ではなく、追加したアイコンから開いてください。以前のアイコンがブラウザー内で開く場合は、本番更新後にホーム画面へ追加し直してください。通知許可を拒否している場合は、端末・ブラウザーの設定で変更してください。

通知の登録は端末・ブラウザーごとです。通知を受けるのは差し出し手だけで、居住者・無関係なアカウントには送りません。参加している複数の家がある場合、そのアカウントが差し出し手である家の通知を受け取ります。「通知を停止」や明示的なログアウトで、このブラウザーの購読を解除します。ログイン期限が切れている場合、通知からログインした後に対象の履歴へ戻ります。

## 本番設定

Renderには次を設定します。秘密鍵・購読endpoint・authをログやGitへ出さないでください。

| 環境変数 | 用途 |
| --- | --- |
| `WEB_PUSH_PUBLIC_KEY` | VAPID P-256公開鍵（base64url） |
| `WEB_PUSH_PRIVATE_KEY` | VAPID P-256秘密鍵（base64url） |
| `WEB_PUSH_SUBJECT` | 送信元の連絡先URI（本番サイトのHTTPS URLまたはmailto URI） |
| `MAIL_NOTIFICATIONS_ENABLED` | 現在は`false`。メールの停止を維持 |

VAPID鍵は再デプロイのたびに作り直さず、同じペアを継続して使います。鍵を変更した場合は、各ブラウザーで通知を有効にし直します。秘密鍵はサーバーだけに置き、Webへ返すのは公開鍵だけです。

本番は`jphacks/sd_2605`の`main`、Root Directory `apps/`、Dockerfile `Dockerfile.render`です。起動時の`rails db:prepare`で購読・送信待ちテーブルを作成します。別のワーカーやRedisは不要です。

## 送信・再送

録音受信時（または従来の`voice=skipped`イベント受信時）に、対象の差し出し手の購読ごとの送信待ちをPostgreSQLへ保存し、Active Jobで送信します。通知API待ちや端末の応答待ちを、録音受信処理から切り離しています。

`push_deliveries`はイベントと購読の組を一意にします。成功済みの送信は再実行せず、一時エラーは30/60/120/240秒の間隔で最大5回試します。プロセス再起動後の未処理データはCoreS3のheartbeatでも回収します。期限切れの購読（404/410等）は削除し、参加権限がなくなったアカウントへの送信は取り消します。通知の同じタグにイベントIDを使うことで、通知表示の重複も抑えます。

`push_deliveries.sent_at`は通知サービスの受付成功を表します。iPhone画面に表示されたことまでは保証しません。`home_events.messengers_notified_at`は通知の送信待ちを作成済みであることを示し、到着確認として使いません。通知サービスでの保持期限は1時間です。購読前の過去のイベントを自動で通知し直すことはしません。

VADで発話がなかった録音は従来どおりイベントだけを保存し、音声通知は作成しません。

## APIと録音の保護

次の購読APIはすべてユーザーJWTが必要です。

- `GET /api/push_config`: 利用可否・公開鍵・差し出し手資格
- `POST /api/push_subscriptions/check`: このアカウントがこのブラウザーを登録済みか確認
- `POST /api/push_subscriptions`: ブラウザーが生成した購読の登録
- `DELETE /api/push_subscriptions`: 自分のブラウザー購読だけを解除
- `POST /api/push_subscriptions/test`: 自分の、このブラウザーだけへテスト通知

登録先はApple・Google・Mozilla・Microsoftの既知のHTTPS Pushサービスに限定し、内部IPや任意ホストへの送信を拒否します。P-256の公開鍵・認証鍵も検証します。アカウントを切り替えて同じendpointを登録し直す場合、既存の2つの暗号鍵も一致する必要があります。

履歴の`GET /api/devices/:device_id/events`と個別イベント取得には`voice_available`が付きます。録音本体の`GET /api/devices/:device_id/events/:event_id/voice`は、ログインとその家へのアクセス権を確認します。JWTをURLへ付けず、Authorizationヘッダーで取得したBlobをプレーヤーに渡します。録音の応答は`private, no-store`です。

通知にはWAV・認証トークン・秘密鍵を含めません。Service Workerにfetchハンドラーはなく、API応答や録音をキャッシュしません。クリック先は同一オリジンの`/history`に限定します。`/push-worker.js`はRailsからJavaScriptとして配信し、更新を妨げないようキャッシュを無効にしています。

## 検証

バックエンドはWSLの隔離したローカルPostgreSQLを使用し、本番DBにテストデータを投入しません。購読の認証・所有権・許可ホスト・暗号鍵・差し出し手だけへの配信・重複抑止・再送・権限取消・期限切れ削除・録音取得権限をテストします。

フロントエンドは`npm test`と`npm run build`を使用します。Service Workerの安全な遷移、iPhoneのホーム画面判定、ユーザー操作時だけの許可要求、登録失敗時の取消、ログアウト時の購読解除を確認します。ChromeのブラウザーテストはPushプロバイダーをモックし、設定操作・録音Blobの取得・スマートフォン幅の表示を確認します。実際のiPhone通知の到着は、本番で購読後に別途確認します。

2026-11-14の検証結果:

- バックエンド: `push_subscriptions_test.rb`、`event_voice_playback_test.rb`、`web_push_notice_test.rb`、`home_event_voice_test.rb`、`device_api_test.rb`、`push_worker_controller_test.rb`の42 tests / 257 assertions、失敗・エラーなし。旧ハードウェア向けPOSTの3フィールド応答も維持しています。
- フロントエンド: `npm test`の30 tests成功、`npm run build`の型チェック・Viteビルド成功。
- `node build/test-web-push-browser.mjs`: Chromeで通知の有効化・テスト要求・通知からの履歴遷移・認証付きWAV Blobの読み込みと音声メタデータ・ログアウト解除を確認。Pushプロバイダーはモックです。デスクトップと390px幅の画面は`build/push-ui/`へ保存（Git対象外）。
- バックエンドはWSL Ruby 3.2の隔離コピー・新規ローカルDBで実行しています。互換性のための開発依存`parallel`/`rbs`の解決は隔離コピー内だけで行い、リポジトリの既存依存バージョンは維持しています。追加した本番依存は`web-push`と`openssl`です。

参考: [Apple Web Push](https://webkit.org/blog/13878/web-push-for-web-apps-on-ios-and-ipados/)、[WebPushライブラリー](https://github.com/pushpad/web-push)。
