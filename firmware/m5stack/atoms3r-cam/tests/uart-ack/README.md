# Atom ACK応答専用試験

GPIO2 RXで受信した `HELLO,<uint32>\n` にGPIO1 TXから `ACK,<同じ連番>\n` を返します。
M5Unified使用、115200/8N1。USB CDCは受信・送信の表示用です。
既存の受信専用スケッチ・製品アプリを変更しません。音声・センサー・APIは初期化しません。
ビルド・書き込み・配線は [C3M5の手順](../../../../psoc/control-c3m5/uart-hello/STEP2.md) を参照してください。

スケッチ先頭の `ACK_TEST_MODE` を変更し再ビルドします。

| 値 | 動作 | C3M5期待ログ |
| --- | --- | --- |
| 0 | 正常応答 | OK |
| 1 | 応答なし | TIMEOUT |
| 2 | 連番+1 | MISMATCH、TIMEOUT |
| 3 | 600ms遅延 | TIMEOUT、LATE/UNEXPECTED |

終了後は0へ戻して書き込みます。64文字を超える行・非表示文字は破棄します。
USB接続待ちは最大3秒。書き込みは現在のAtomアプリを置き換えます。

## Arduino CLIでのビルドと実機書き込み

リポジトリルートのPowerShellから実行します。既定の `ACK_TEST_MODE=0` を使用してください。

```powershell
arduino-cli board list
arduino-cli compile --jobs 4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB" --build-path "$PWD/firmware/psoc/control-c3m5/build/atom-ack" firmware/m5stack/atoms3r-cam/tests/uart-ack
arduino-cli upload -p COM4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB" --input-dir "$PWD/firmware/psoc/control-c3m5/build/atom-ack" firmware/m5stack/atoms3r-cam/tests/uart-ack
arduino-cli monitor -p COM4 --config baudrate=115200
```

COM4は `board list` で確認したAtomのポートに置き換えます。
書き込み前にそのポートのモニターを閉じます。

| Atom | C3M5 |
| --- | --- |
| GPIO2 RX | P3.3 / J4.2 TX |
| GPIO1 TX | P3.2 / J4.1 RX |
| GND | GND |

3.3Vロジック、共通GND、両基板は各USBから給電します。電源線同士は接続しません。
外部通信はUART1、USBモニターはUSB CDCで別経路です。

## 動作確認

C3M5を起動すると、AtomのUSBモニターに次のように表示されます。

```text
[UART RX] HELLO,1
[UART TX] ACK,1
[UART RX] HELLO,2
[UART TX] ACK,2
```

60秒以上、同じ連番の送受信が毎秒継続することを確認してください。
C3M5側のKitProg3ログにも `[OK] ACK,N` が出れば往復を確認できます。
AtomのTXログだけではC3M5でのACK受信成功とは判定しません。

外部UARTへ `HELLO,`、`HELLO,-1`、`HELLO,abc`、`HELLO,4294967296`、
`OTHER,1`、65文字以上の行、途中にCRやNULを含む行を送ってもACKは返しません。
数値は0〜4294967295の十進数字のみです。正常なLF終端とCRLF終端に対応します。
行途中のCRは不正です。改行までは返信せず、過長・不正行は次の改行まで破棄します。
その後 `HELLO,42\n` を送り、`ACK,42\n` で復帰することを確認します。
不正な表示可能文字列もRXログに表示しますが、TXログは出ません。
USBモニターへの入力は外部UART受信試験にはなりません。

受信リングバッファは1024バイト、行は最大64文字＋終端NULです。
今回はUART通信だけの試験で、音声再生の実機確認は行いません。
製品の音声再生ソースは保持していますが、この専用スケッチでは再生処理を起動しません。

## 今回の確認結果（2026-10-18）

Arduino CLI、M5Stack ESP32 3.3.10でビルド成功。
Flash 560,799バイト、静的RAM 26,360バイト。
今回の実機書き込みとUART往復ログ取得は未実施です。上記手順で確認してください。
音声再生・センサーの実機試験は実施していません。


### 両基板接続後の双方向通信確認（2026-10-18）

接続中のAtomはCOM8（USB識別子98:88:E0:0E:C3:64）、KitProg3はCOM9。
先のCOM4は別のESP32-S3だったため、接続中のCOM8へACK応答スケッチを書き込み直した。
書き込み・ハッシュ検証成功後、両ポートを65秒間同時取得した。
AtomのHELLO,77〜141とACK,77〜141、C3M5のOK ACK,77〜141の65件が一致し、欠番・重複なし。
この共通連番範囲のACK応答時間は1〜2ms。タイムアウト・不一致なし。
取得冒頭のKitProg3ログには旧連番73〜75に対する不一致とACK,76のOK（117ms）が残っていた。
上記の安定した共通範囲とは分けて評価した。全取得ログが不一致0件だったという意味ではない。
実機の正常なUART往復を確認済み。無応答・故意の不一致・遅延の異常系試験は今回未実施。
音声・センサー・電気波形の確認は未実施。生ログはGit対象外の `ack-capture.log`。
