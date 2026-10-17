# 作業ルール

- Coreは `firmware/m5stack/core/`、CoreS3は `firmware/m5stack/cores3/` に配置する。
- Arduinoのスケッチフォルダと `.ino` の名前を一致させる。デバイス操作はM5Unifiedを使用する。
- Coreのビルドは `scripts/build-core.ps1`、書き込みは `scripts/upload-core.ps1 -Port COM5` を使用する。
- 音声・SDヘッダーはスケッチの共有状態宣言後にincludeする。構成変更時は依存順序に注意する。
- 構成や使い方を変更したらドキュメントも更新する。
- コンパイル・書き込み・シリアル確認と、実際の発音やセンサーの実機確認を分けて報告する。
- 秘密情報とビルド生成物をGitへ追加しない。
