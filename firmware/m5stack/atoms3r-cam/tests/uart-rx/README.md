# AtomS3R-CAM UART受信だけの試験

既存の `../../atoms3r-cam.ino` と音声・API関連ファイルを保持した独立スケッチです。
ACK、UART送信、API、音声再生連携、カメラ処理は実装しません。
書き込むとAtom上の実行アプリはこの試験に置き換わります。フラッシュ全消去は行いません。

## ボードと配線

公式 [AtomS3R-CAM資料](https://docs.m5stack.com/en/core/AtomS3R-CAM) と
[Arduino手順](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/pdf/static/en/arduino/m5atoms3r-cam/program.pdf)、
インストール済みM5Stack ESP32 3.3.10の `boards.txt` / `variants/m5stack_atoms3r/pins_arduino.h` を確認しました。
ボード名は `M5AtomS3R`、FQBNは以下です。USB CDCを明示して外部UARTと分離します。

```text
m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc
```

| Atom | C3M5 | 用途 |
| --- | --- | --- |
| GPIO2 | J4.2 / P3.3 TX | UART1 RX |
| GPIO1 | J4.1 / P3.2 RX | UART1 TX（送信しない） |
| GND | GND | 共通GND |

`HardwareSerial(1)`、115200 baud、8N1。`Serial` はUSB CDC専用。
GroveのG2/G1に接続し、電源線同士は接続せず各基板を給電します。
M5Unifiedの音声・センサー・外部ディスプレイ自動検出を無効にしています。

## ビルド・書き込み

リポジトリルートのPowerShellから実行します。

```powershell
arduino-cli board list
arduino-cli board details --fqbn "m5stack:esp32:m5stack_atoms3r"
arduino-cli compile --jobs 4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc" --build-path "$PWD/firmware/m5stack/atoms3r-cam/build/uart-rx" firmware/m5stack/atoms3r-cam/tests/uart-rx
arduino-cli upload -p COM8 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc" --input-dir "$PWD/firmware/m5stack/atoms3r-cam/build/uart-rx" firmware/m5stack/atoms3r-cam/tests/uart-rx
arduino-cli monitor -p COM8 --config baudrate=115200
```

COM8は今回の検出値です。接続し直した場合は `board list` で確認します。
USBモニターを閉じてから書き込んでください。開始メッセージを見たい場合はモニター接続後にAtomをリセットします。

## 判定方法

```text
[UART] Waiting for C3M5...
[UART RX] HELLO,1
[UART RX] HELLO,2
[UART RX] HELLO,3
```

受信はLF区切り、CRLFにも対応します。64文字までを固定65バイトのバッファで保持し、
65文字以上の行は次のLFまで破棄して通知します。空行は無視します。
破棄行の先頭最大16バイトを16進表示して、文字化けの切り分けに利用できます。
印字可能ASCII以外（CR/LFを除く）を含む行も破棄します。
ハードウェアUART受信バッファは1024バイトです。動的Stringは使用しません。

1. C3M5側がHELLOを約1秒ごとに送信する状態にする。
2. Atom側モニターで60行以上の連番と約1秒間隔を確認する。途中から接続すれば連番は1以外から始まる。
3. 受信がない場合は交差配線、共通GND、COM番号、両端115200/8N1、USB CDCとUART1の分離を確認する。
4. ビルド成功・書き込み成功・HELLOの連続受信を別々に判定する。

既存アプリへの復帰はリポジトリの `scripts/upload-atoms3r-cam.ps1 -Port COM8` で可能です。
本試験では復帰書き込みや音声の動作確認は行いません。

## 確認記録（2026-10-18）

- worktree: `C:/Users/soraj/dev/sd_2605`、ブランチ `main`、HEAD `66bf8ea`。
  既存のC3M5試験変更は保持し、今回の編集は `firmware/m5stack/atoms3r-cam/` 内のみ。
- Arduino CLI / M5Stack ESP32 3.3.10、M5Unified 0.2.26、M5GFX 0.2.33で最終版ビルド成功。
  Flash 560,475バイト（16%）、静的RAM 26,360バイト（8%）。
- COM8へ最終版の書き込み成功。書き込みデータのハッシュ検証も成功。
- 最終版の約66秒の取得で **HELLO,149〜HELLO,215の67行を連続受信**。
  欠番・重複なし、破棄行0、USBへの表示間隔は0.984〜1.000秒。
  USB上で `[UART RX] HELLO,149` などの指定形式を確認。通信の完了条件を達成した。
- 初回取得中は不正行が届いていたが、C3M5をPCへ接続した後にHELLOの連続受信へ切り替わった。
  初回ではHELLO,2〜HELLO,34の33行を欠番なく確認した。
- 既存 `atoms3r-cam.ino` のGitオブジェクトハッシュは作業前後とも
  `1fc203c588c23dba83be1ebf91678ad562ce70d4`。音声・API関連ソースとCoreS3版は変更していない。
- ビルド成果物と `uart-rx-*.log` はGit対象外。音声の発音・センサーの実機試験は今回実施していない。
