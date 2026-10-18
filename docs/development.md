# 開発環境

Arduino CLI、M5Stack ESP32ボードパッケージ3.3.10、M5Unified 0.2.26、M5GFX 0.2.33を使用します。
既存のArduino CLI設定・ライブラリを利用します。

GitHub Actionsによる自動ビルドは未設定です。push時に設定エラーを起こしていた空の
`firmware.yml`・`web.yml`は削除しました。ビルドは以下のローカルスクリプトで実行します。

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


## CoreS3

CoreS3は`scripts/build-cores3.ps1`と`scripts/upload-cores3.ps1 -Port COM5`を使用します。
ポートは実接続先に合わせてください。CoreS3のFQBN設定・LittleFS音源配置は
[CoreS3 README](../firmware/m5stack/cores3/README.md)を参照してください。

## Coreの中央ボタン再生（2026-10-18）

B短押しでSDのwelcome.wav（SD異常時は代替チャイム）を再生します。B長押しは消音切り替えです。起動後3秒は入力を無視します。
未使用の描画部品faces.inoはfaces.hへ変更しました。Arduinoが同じフォルダの.inoを自動結合した際の、core.inoとの描画関数の重複定義を防ぎます。顔だけの画面への更新で、現在はこのヘッダーをincludeしています。

検証: コンパイル成功（フラッシュ742,467バイト、静的RAM29,360バイト）。COM5への書き込みとハッシュ検証成功。115200 baudでboot complete、PIR=LOW、起動音処理の開始・終了を確認しました。SDは未認識のため現在は代替チャイムになります。実際の発音・Bボタン操作・PIR検知・正常SD/WAV再生は未確認です。起動ログはbuild/core/serial-button-update.log（Git対象外）へ保存しました。

## Coreの顔だけ表示（2026-10-18）

元のfaces.inoをfaces.hとして直接使用し、画面全体をキャラクターの顔にしました。画面の文字・状態欄・操作ガイド・待機バーを削除しました。A短押しはピンクの女の子、C短押しは青の眼鏡の男の子、以前保存された黄色は友達の顔です。B短押しの音声再生とB長押しの消音は維持し、C長押しのガイド切り替えは廃止しました。SDエラー等はシリアルで確認します。

顔だけ表示版の検証: コンパイル成功（フラッシュ579,963バイト、静的RAM29,360バイト）。COM5への書き込み・ハッシュ検証成功。115200 baudでboot complete（PIR=LOW、pink）と起動音処理の開始・終了を確認しました。SDは未認識です。ログはbuild/core/serial-face-only.log（Git対象外）。画面の目視・実際の発音・ボタン・センサーの実機操作は未確認です。

## Coreの右ボタンによる顔切り替え（2026-10-18）

C短押しでピンクの女の子→黄色の友達→青の眼鏡の男の子→ピンクの女の子と循環します。選択はNVSへ保存します。A短押しでピンクへ戻り、B短押しの音声再生は維持します。

顔循環版の検証: コンパイル成功（フラッシュ579,983バイト、静的RAM29,360バイト）、COM5への書き込み・ハッシュ検証成功。実際のボタン操作・画面の目視・発音・センサーの実機確認は未実施です。


## Coreの左ボタン録音・中央ボタン再生（2026-10-18）

Aを押すとM5Unifiedのマイクで3秒間（16kHz・16bit・モノラル）録音し、Bで直近の録音を再生します。録音はRAMに保存し、次の録音または再起動で失われます。SD不要です。A/B長押しの旧操作は廃止し、Cの顔切り替えとPIRのSD音声／代替チャイムは維持します。マイクとスピーカーは切り替えて使用し、録音中はPIR再生を開始しません。

検証コマンド: `arduino-cli board list`でCOM5を確認し、`./scripts/upload-core.ps1 -Port COM5`で現ソースのビルド・書き込みに成功しました。フラッシュ588,939バイト、静的RAM29,376バイト。書き込みのハッシュ検証成功。pyserialで115200 baudの起動ログを取得し、`recording buffer: ready; mic pin=34`、`boot complete`、起動音処理終了を確認しました。ログはGit対象外の`build/core/serial-recording.log`。SDは未認識です。`git diff --check`成功。実際のA/B操作、マイクでの集音・発音、顔表示、PIR動作は未確認です。
