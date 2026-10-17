# 開発環境

Arduino CLI、M5Stack ESP32ボードパッケージ3.3.10、M5Unified 0.2.26、M5GFX 0.2.33を使用します。
既存のArduino CLI設定・ライブラリを利用します。

リポジトリ直下で実行:

```powershell
arduino-cli board list
.\scripts\build-core.ps1
.\scripts\upload-core.ps1 -Port COM5
arduino-cli monitor -p COM5 --config baudrate=115200
```

書き込みスクリプトは必ず現在のソースをコンパイルしてから書き込みます。
モニターを閉じてから書き込んでください。出力先は `build/core/` です。
FQBNの既定値は `m5stack:esp32:m5stack_core`。スクリプトの `-Fqbn` で変更できます。

配線・WAV形式・ボタン操作・実機確認項目は [hardware.md](hardware.md) を参照してください。
SDを使用する現在の実装では、SDカードに `/welcome.wav` を配置します。SDがなくても代替チャイムで動作します。

## 移植後の検証（2026-10-17）

- 移植元の処理と、ヘッダー分割・PIRヘルパーを展開した移植後コードの一致を確認。
- `scripts/upload-core.ps1 -Port COM5` のコンパイル成功。フラッシュ736,167バイト（56%）、静的RAM29,344バイト（8%）。
- 開始時にCOM5（USB VID 1A86 / PID 55D4、シリアル5B1E036225）を検出。
- 初回はCOM5消失により書き込み失敗。USB再接続後、同じデバイスのCOM5へ書き込み成功。全書き込み領域のハッシュ検証成功、RTSリセット完了。
- 書き込み後に115200 baudで約10秒の起動ログを取得。`boot complete; PIR=HIGH`、起動音処理の開始・終了、`PIR -> LOW`を確認。ログは `build/core/serial-boot.log`（Git対象外）。
- SDは `card mount failed` で未認識。起動時に保存済みコアダンプ領域の `Incorrect size of core dump image` 警告があるが、その後起動処理は完了。
- 実際の発音・画面の目視・PIRのLOW→HIGHによる再生・ボタン操作・SD/WAV再生は未確認。

