# Step 2: C3M5 ↔ Atom UART ACK試験

起動約1秒後から1秒周期で `HELLO,<連番>\n` を送信します。
Atomの `ACK,<同じ連番>\n` を500ms未満で受信すると正常です。
不一致は期限を延長せず、正しいACKが来なければ500msでタイムアウトします。
成功・タイムアウト後も次の送信を継続します。uint32連番は1から開始し最大値の次は0です。
I²C、Edge AI、Rails API、既存統合アプリは変更していません。

| 用途 | C3M5 | Atom | ペリフェラル |
| --- | --- | --- | --- |
| 外部TX | J4.2 / P3.3 | GPIO2 RX | SCB4 / USER_UART |
| 外部RX | J4.1 / P3.2 | GPIO1 TX | SCB4 / USER_UART |
| GND | GND | GND | 共通 |
| デバッグ | P6.3 TX / P6.2 RX | 接続しない | SCB3 / DEBUG_UART → KitProg3 USB |

両UARTは115200 baud、8N1、フロー制御なし。外部信号は3.3Vです。
両基板はUSB給電、GNDを共通にし電源線同士は接続しません。共有ピンを使うシールドは外します。
KitProg3 USB-UARTとAtom USB CDCは別のCOMポートです。SWD書き込みはCOM指定不要です。
設定原本は `bsps/TARGET_APP_KIT_PSC3M5_EVK/config/design.modus`。
SCB4とSCB3は各々96MHz÷104、oversample=8（実効約115384.6baud）です。
生成コードは直接編集しません。

## ビルド・書き込み

リポジトリルートのPowerShellで実行します。

```powershell
.\firmware\psoc\control-c3m5\uart.ps1 getlibs
.\firmware\psoc\control-c3m5\uart.ps1 build
.\firmware\psoc\control-c3m5\uart.ps1 program
```

ModusToolbox terminalでは `uart-hello` ディレクトリで `make build CONFIG=Debug`、
`make program CONFIG=Debug` を実行します。
設定変更時は `uart.ps1 device-configurator` でSave/Generateします。
Atomは専用の [uart-ack](../../../m5stack/atoms3r-cam/tests/uart-ack/README.md) を使用します。
既存の受信専用 `uart-rx` はACKを返しません。

```powershell
arduino-cli board list
arduino-cli compile --jobs 4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB" --build-path "$PWD/firmware/psoc/control-c3m5/build/atom-ack" firmware/m5stack/atoms3r-cam/tests/uart-ack
arduino-cli upload -p COM4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB" --input-dir "$PWD/firmware/psoc/control-c3m5/build/atom-ack" firmware/m5stack/atoms3r-cam/tests/uart-ack
```

COM4は実際のAtomポートに置換します。書き込みは現在のAtomアプリを置き換えます。

## 実機確認

1. 交差配線・共通GND・USB接続を確認します。
2. KitProg3 USB-UARTのCOMポートをデバイスマネージャーで確認し、115200/8N1で開きます。
   例: `arduino-cli monitor -p COM9 --config baudrate=115200`。COM9は実際のKitProg3ポートに置換します。
3. C3M5をリセットし `[START] external=SCB4 ... debug=SCB3 ...` を確認します。
4. 60秒以上取得し、KitProg3で `[OK] ACK,1 elapsed=...ms` から欠番なく連続することを確認します。
   Atom USB側では `[UART RX] HELLO,1` と `[UART TX] ACK,1` を確認します。
5. Atom TX→P3.2の線だけを外し、毎秒 `[TIMEOUT] expected=N after=500ms` が出ることを確認します。
   HELLO送信が継続し、再接続後はOKに復帰することも確認します。
6. Atomスケッチ先頭を `ACK_TEST_MODE=2` に変更して再ビルド・書き込みします。
   `[MISMATCH] expected=N received=N+1` と、その後同じNのTIMEOUTが出ることを確認します。
7. `ACK_TEST_MODE=3` では600ms遅延応答がTIMEOUT後に `[LATE/UNEXPECTED]` となることを確認します。
   終了後は必ず0へ戻してビルド・書き込みします。

不正形式・長すぎる行は `[INVALID]`。次の改行で同期を回復します。
LF終端のACKを使用します。改行なし・連続入力でも待機期限は延長されません。
残留入力は次の送信直前に破棄します。周期はSysTickの1ms時刻を基準とします。
ビルド、SWD書き込み、シリアル取得、実機双方向通信を分けて判定します。
電気波形・音声・センサーの実機確認は対象外です。
ログ・取得ライブラリ・生成物はGitに追加しません。

## 今回の確認結果（2026-10-18 / Step 2）

- ModusToolbox 3.9 / GCC ARM 14.2.1 / Debugビルド成功。Flash使用量12,572バイト。ビルドログにwarning/errorなし。
- Device Configurator CLIで設定検証・コード生成成功。生成されたSCB4 RX=P3.2、SCB3 TX=P6.3のルーティングを確認。
- Atom ACK試験: Arduino CLIでビルド成功。Flash 560,783バイト、静的RAM 26,360バイト。
- C3M5書き込みコマンドは実行したが `unable to find a matching CMSIS-DAP device` で失敗。
  PCの認識ポートはCOM4のみ。KitProg3 USB-UART、実機書き込み、ACKログの取得は未確認。
- Atomへの書き込みは今回未実施。正常・タイムアウト・不一致・遅延応答の実機確認は上記手順で実施する。
- P3.2/P3.3の電気波形測定、音声・センサーの実機確認は未実施。

ローカルの `../c3-build.log`、`../c3-config.log`、`../c3-program.log`、
`../atom-ack-build.log` はGit対象外。

### C3M5接続後の書き込み結果（2026-10-18）

KitProg3 SWDでC3M5への書き込み成功（13,312バイト）。
書き込み後、COM9でTIMEOUT expected=15〜22を取得し、アプリの実行を確認。
同時にCOM8でHELLO,16〜23を受信したが、ACK送信表示とC3M5のOKログは確認できなかった。
C3M5の書き込み・HELLO送信・タイムアウト処理は確認済み。双方向ACK成功は未確認。
COM8側は受信表示のみのため、ACK応答スケッチの書き込み先と実行状態の確認が必要。


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
