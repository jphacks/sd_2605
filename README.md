# おかえりんく

<p align="center">
  <img src="docs/images/okaelink_logo_animated.gif" alt="おかえりんく" width="420">
</p>

## 各種リンク

- **アプリURL**: https://sd-2605-mobile.onrender.com/
- スライドURL: https://canva.link/qu8e8hxn11fekr0

公開アプリは Render の無料プランです。15分間アクセスがないとサーバーがスリープし、次に開いたときは起動まで数十秒かかります。

<p align="center">
  <img src="docs/images/qr.png" alt="アプリのQRコード" width="160">
</p>

大切な人の声で「いってらっしゃい」「おかえり」を届ける、玄関向けの音声メッセージデバイスです。

- 本リポジトリ: [jphacks/sd_2605](https://github.com/jphacks/sd_2605)
- Webアプリの開発元: [jphacks/sd_2605-mobile](https://github.com/jphacks/sd_2605-mobile)

## 製品概要

### 背景(製品開発のきっかけ、課題等）

突然ですが、みなさんは一人暮らしをしたことがありますか？
実際、このプロダクトの開発メンバーは全員が一人暮らしの経験者です。そして、私たちが共通して抱えていた感情、それは――「ふとした瞬間に寂しい」ということでした。

特に、誰もいない玄関を出入りするときの静けさは、一人暮らしならではの孤独感を感じさせます。

そこで私たちは、「離れて暮らす大切な人の温もりを、日々の生活の中で感じられる体験を作りたい」と考えました。

家族や恋人、友人など、遠くに住む人たちが吹き込んでくれた「いってらっしゃい」や「おかえり」の声を、出発や帰宅のタイミングに合わせて届ける。テキストのメッセージとは一味違う、声ならではの温かさで孤独感を和らげるために、このプロダクトを開発しました。

### 製品説明（具体的な製品の説明）

Webアプリで録音・登録した短い音声を、玄関に置いたデバイスで再生します。外出用と帰宅用のメッセージを別々に登録でき、通過方向に合わせて挨拶を届けます。デバイスには表情のある顔を表示し、声と見た目で出迎えます。

ハードウェアは **M5Stack CoreS3をメインに開発し、デモもCoreS3で実施しました**。内蔵カメラの動体検知・左右方向判定とPIRセンサーの照合を用い、Webから取得した音声を本体に保存して再生する構成です。

CoreS3は、**内蔵カメラと人感センサー（PIR）**を組み合わせて通過を検知します。カメラでは**画像処理によって物体の移動と左右の移動方向を検出**し、人感センサーの反応と照合します。移動方向に応じて外出・帰宅を判定し、外出時は「いってらっしゃい」、帰宅時は「おかえり」の音声を再生します。左右の方向と外出・帰宅の対応は、設置場所に合わせて切り替えられます。

画像処理では、撮影画像を40×30の明るさ画像に縮小し、2フレーム前との差分から動いた部分を抽出します。小さな画像領域の模様が左右どちらにずれたかを照合して移動方向を求め、十分に照合できない場合は動いた領域の中心位置の変化を使います。照明の変化や小さなノイズを抑え、複数フレームの移動をまとめて判定します。現在の実装には、方向を確定できないときに前回と逆方向を採用する補完処理もあります。

別途、学習済みAIモデルによる人物検出も単独で検証しています。移動方向の判定は上記の画像処理で行い、AI人物検出と方向判定・PIR照合・挨拶再生の統合動作は未確認です。

<p align="center">
  <img src="docs/images/device.png" alt="CoreS3の顔画面とデバッグ画面、PIRセンサー" width="720">
</p>

### 特長

#### 1. 大切な人の声を、外出・帰宅のタイミングに

ブラウザで録音・試聴し、外出用（leave）と帰宅用（arrive）の音声を登録できます。音声はPCM 16bit・モノラル・16kHzのWAVに変換し、5秒以内のメッセージとして扱います。

<p align="center">
  <img src="docs/images/web-voice-mobile.png" alt="メッセージ登録画面（スマートフォン）" width="280">
  <img src="docs/images/web-voice-pc.png" alt="メッセージ登録画面（PC）" width="720">
</p>

#### 2. Webと連携し、取得した音声は本体に保存

CoreS3がWi-Fi経由でAPIに定期アクセスし、音声の版番号を比較して更新します。取得済みの音声はLittleFSに保存するため、通信できない間もローカルで再生できます。

<p align="center">
  <img src="docs/images/web-home-mobile.png" alt="ホーム画面（スマートフォン）" width="280">
  <img src="docs/images/web-home-pc.png" alt="ホーム画面（PC）" width="720">
</p>

#### 3. 顔のあるデバイスと、本体での操作

顔の切り替えやまばたき、音声再生に合わせた表情を実装しています。タッチで顔を変更でき、長押しでデバッグ画面へ切り替えて音量や方向の割り当てを操作できます。

### 解決出来ること

- その場にいない家族・友人からの挨拶を、外出や帰宅のタイミングで受け取れます。
- Webから音声を更新でき、デバイスを操作せずにメッセージを届けられます。
- 外出用・帰宅用の音声を分け、生活の場面に合った声かけができます。

### 今後の展望

今後は検知の安定性を高め、設置環境ごとの調整や複数センサーの活用を進めます。

### 注力したこと（こだわり等）

- カメラの通過方向判定とPIR照合を組み合わせ、外出・帰宅それぞれの音声につなげること。
- 音声の保存・更新と再生を分け、通信待ちが検知や本体操作を妨げない構成にすること。
- 顔の表情やタッチ操作を通じて、玄関に置きたくなる親しみやすさを持たせること。

## 開発技術

### 活用した技術

#### API・データ

- RailsのJSON API: ユーザー認証、デバイス管理、設定、音声登録・取得、heartbeat、イベント履歴。
- Web Audio API / MediaRecorder: ブラウザ録音、試聴、WAV変換。
- PostgreSQL: ユーザー・デバイス・設定・イベント・音声データの保存。
- HTTP/HTTPS、Wi-Fi、NTP: CoreS3とWebの通信・時刻同期。

#### フレームワーク・ライブラリ・モジュール

| 対象 | 技術スタック |
| --- | --- |
| Webフロントエンド | TypeScript、React 18、Vite、React Router、Lucide React |
| Webバックエンド | Ruby 3.3、Ruby on Rails 8.1、Puma、JWT、bcrypt |
| 開発・デプロイ | Docker Compose、Render、Git subtree |
| CoreS3 / Core | C++、Arduino CLI、M5Stack ESP32ボードパッケージ、M5Unified、M5GFX、ArduinoJson |
| CoreS3の保存・並行処理 | LittleFS、Preferences / NVS、FreeRTOS |

#### デバイス

| デバイス | 位置づけ |
| --- | --- |
| M5Stack CoreS3、内蔵カメラ・タッチ画面・スピーカー、PIRセンサー | メイン開発機・デモ使用機 |
| M5Stack Core、PIRセンサー、microSD | 初期の音声再生・検知実装 |

### 独自技術

#### ハッカソンで開発した独自機能・技術

- [CoreS3ファームウェア](firmware/m5stack/cores3/): カメラによる通過方向判定、PIR照合、方向別音声再生、顔のUI、タッチ操作、Webとの音声同期を統合。
- [Webフロントエンド](apps/frontend/): 録音・WAV変換、外出／帰宅メッセージの登録、デバイス管理・設定画面。
- [Webバックエンド](apps/backend/): slot別の音声保存と版管理、端末向け音声配信、ユーザー認証・所有デバイス管理。

## 開発体制・開発経緯

| メンバー | 主な担当 |
| --- | --- |
| 伊藤知世 | Webアプリのフロントエンド開発 |
| 工藤康矢 | Webアプリのバックエンド開発 |
| Norjinkhorloo Tergel | ハードウェアのUI開発 |
| 近藤宙 | ハードウェア設計・組み込みソフトウェア開発 |

Webアプリは別リポジトリ [sd_2605-mobile](https://github.com/jphacks/sd_2605-mobile) で開発し、**ハッカソン1日目の夜に本リポジトリへ統合しました**。現在はGit subtreeで履歴を保持して `apps/` に取り込んでいます。

## リポジトリ構成

| パス | 内容 |
| --- | --- |
| `apps/frontend/` | Webフロントエンド |
| `apps/backend/` | Rails API |
| `firmware/m5stack/cores3/` | メイン開発・デモ用CoreS3ファームウェア |
| `firmware/m5stack/core/` | Core向けファームウェア |
| `firmware/m5stack/atoms3r-cam/` | AtomS3R-CAM向けの開発中ファームウェア・通信試験 |
| `firmware/psoc/` | 開発中のPSoC構成 |
| `scripts/` | M5Stackのビルド・書き込みスクリプト |
| `docs/` | 開発手順・ハードウェア・API仕様 |
| `tests/` | ファームウェアのホストテスト |

## 開発・起動方法

### Webアプリ

Docker Desktopを起動し、リポジトリ直下から実行します。

```powershell
cd apps
docker compose up --build
```

フロントエンドは `http://localhost:5173`、APIのヘルスチェックは `http://localhost:3000/api/health` です。詳細は [WebアプリREADME](apps/README.md) を参照してください。

### CoreS3（メイン開発機）

Arduino CLIと必要なボードパッケージ・ライブラリを用意し、[CoreS3 README](firmware/m5stack/cores3/README.md) に従って `secrets.example.h` からローカルの `secrets.h` を作成します。リポジトリ直下で実行してください。

```powershell
arduino-cli board list
.\scripts\build-cores3.ps1 -Jobs 2
.\scripts\upload-cores3.ps1 -Port COM4
arduino-cli monitor -p COM4 --config baudrate=115200
```

`COM4` は例です。実際の接続ポートに置き換えてください。書き込みスクリプトはビルドしてから書き込みます。Wi-Fi認証情報を含む `secrets.h` やビルド生成物はGitへ追加しません。

### Core

```powershell
.\scripts\build-core.ps1
.\scripts\upload-core.ps1 -Port COM5
```

### Web開発元との同期

作業ツリーがクリーンな状態で、本リポジトリ直下から実行します。

```powershell
git subtree pull --prefix=apps https://github.com/jphacks/sd_2605-mobile.git main
```

開発元への反映やRenderの設定については [WebアプリREADME](apps/README.md) を参照してください。

## 詳細ドキュメント・確認状況

- [CoreS3の機能・操作・検証記録](firmware/m5stack/cores3/README.md)
- [ハードウェアと動作確認手順](docs/hardware.md)
- [開発環境と書き込み](docs/development.md)
- [M5Stack・Rails APIインターフェース仕様](docs/m5stack-api.md)
- [CoreS3のRender連携・確認手順](docs/cores3-render-migration.md)

CoreS3でデモを実施していますが、その後の変更も含めたすべての機能の実機確認を意味するものではありません。各変更の確認状況は、コンパイル・書き込み・シリアルログと、実際の発音・センサー検知・画面操作に分けて各ドキュメントに記録しています。

## 発展構成（開発中）

InfineonのPSoCとAtomS3R-CAMを組み合わせ、AI・センサー処理を拡張する構成も開発中です。CoreS3のデモとは別の構成で、全体の実機連携は未検証です。詳細は [PSoCファームウェア](firmware/psoc/) を参照してください。
