# 作業ルール

## プロジェクトの前提

- 製品名は「おかえりんく」。Webで登録した音声を、玄関のデバイスで外出・帰宅に合わせて再生する。
- 本リポジトリ: https://github.com/jphacks/sd_2605
- Webアプリは https://github.com/jphacks/sd_2605-mobile で別途開発し、ハッカソン1日目の夜に統合した。現在は履歴を保持するGit subtreeとして `apps/` に配置している。
- ハードウェアはCoreS3をメインに開発し、デモもCoreS3で行った。
- InfineonのPSoCボードを組み合わせて高度な処理を行う構成は開発中。CoreS3のデモ実績と、PSoC構成の実装・検証状況を分けて記載する。

## 役割分担

| メンバー | 主な担当 |
| --- | --- |
| 伊藤知世 | Webアプリのフロントエンド開発 |
| 工藤康矢 | Webアプリのバックエンド開発 |
| Norjinkhorloo Tergel | ハードウェアのUI開発 |
| 近藤宙 | ハードウェアの組み立て・組み込み制御ロジックの開発 |

## 配置・技術スタック

- Webフロントエンド: `apps/frontend/`。TypeScript、React、Vite、React Router、Web Audio APIを使用する。
- Webバックエンド: `apps/backend/`。Ruby on Rails、PostgreSQL、JWTを使用する。ローカル起動は `apps/` で `docker compose up --build`。デプロイ構成はRender向け。
- Coreは `firmware/m5stack/core/`、CoreS3は `firmware/m5stack/cores3/` に配置する。
- AtomS3R-CAMは `firmware/m5stack/atoms3r-cam/`、PSoCは `firmware/psoc/` に配置する。
- M5StackはArduino / C++とM5Unifiedを使用する。CoreS3の音声保存はLittleFS、設定保存はNVS、Coreの音声保存はmicroSD。
- PSoCはModusToolboxとInfineon HAL / PDLを使用する。各ボードのREADMEにあるツールチェーン・BSP・配線条件を確認する。

## ファームウェアの変更

- Arduinoのスケッチフォルダと `.ino` の名前を一致させる。デバイス操作はM5Unifiedを使用する。
- Coreのビルドは `scripts/build-core.ps1`、書き込みは `scripts/upload-core.ps1 -Port COM5` を使用する。
- CoreS3のビルドは `scripts/build-cores3.ps1`、書き込みは `scripts/upload-cores3.ps1 -Port <実接続ポート>` を使用する。ポートは `arduino-cli board list` で確認する。
- 書き込みスクリプトは現在のソースをビルドしてから書き込む。書き込み前にシリアルモニターを閉じる。
- 音声・SDヘッダーはスケッチの共有状態宣言後にincludeする。構成変更時は依存順序に注意する。
- CoreとCoreS3では保存先・ピン・UI・センサー構成が異なるため、対象ボードの実装とREADMEを確認する。

## Webアプリと連携の変更

- フロントエンドのコマンドは `apps/frontend/package.json`、APIの動作と起動手順は `apps/README.md` を確認する。
- APIやWAV形式を変更する際は、WebとCoreS3の両方への影響を確認し、`docs/m5stack-api.md` などの関連仕様を更新する。
- `apps/` は別リポジトリ由来のsubtreeであることを維持する。開発元からの取り込みは、作業ツリーがクリーンな状態で `git subtree pull --prefix=apps https://github.com/jphacks/sd_2605-mobile.git main` を使用する。
- デプロイ先が参照するリポジトリとRoot Directoryを確認する。本リポジトリへの変更だけで別リポジトリの本番環境へ反映されたと扱わない。

## ドキュメント・検証・Git

- ルートREADMEは既存のJPHACKS雛形の「製品概要」「開発技術」を基本構成とし、製品説明・担当・開発経緯・現状を記載する。
- 構成や使い方を変更したらドキュメントも更新する。主な入口は `README.md`、`apps/README.md`、各ボードのREADME、`docs/`。
- コンパイル・書き込み・シリアル確認と、実際の発音やセンサー・タッチ・表示の実機確認を分けて報告する。
- ホストテストの成功を実機動作の確認として報告しない。未確認・開発中の項目を明示する。
- 変更に応じたビルド・テストを行い、実行コマンドと結果を記録する。ドキュメントのみの変更では内容・パス・差分を確認する。
- 秘密情報とビルド生成物をGitへ追加しない。`secrets.h`、ローカル環境変数、認証情報、`build/`、ダウンロードしたSDK・依存ライブラリなどの追跡状態に注意する。
