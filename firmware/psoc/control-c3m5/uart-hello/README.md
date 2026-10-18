# Step 1: C3M5 → AtomS3R-CAM UART送信試験

起動初期化後約1秒で `HELLO,1\n`、以降約1秒ごとに十進連番を送信します。
RXも設定しますが受信内容は処理しません。I²C、AI、API、融合、ACK処理は含みません。
周期は1000ms待機＋ループ処理時間です。uint32最大値の送信後は停止します。

## 配線と設定根拠

| 用途 | C3M5 | Atom | ペリフェラル |
| --- | --- | --- | --- |
| TX | J4.2 / P3.3 | GPIO2 RX | SCB4 TX |
| RX（予約） | J4.1 / P3.2 | GPIO1 TX | SCB4 RX |
| GND | GND | GND | 共通 |
| KitProg3 USB-UART | P6.3 TX / P6.2 RX | 接続しない | SCB3、本試験では未使用 |

115200 baud、8N1、フロー制御なし、3.3Vロジック。
P3.2/P3.3はmikroBUS、Shield2Go、Arduinoヘッダーでも共有されるので、他シールドを接続しないでください。
本設定はSCB4だけをUARTとして使用し、SCB3を初期化しません。

- [公式キットガイド](https://documentation.infineon.com/psoccontrolc3/docs/tcc1731244321580_2): ヘッダーと共有ピン
- [公式BSP](https://github.com/Infineon/TARGET_KIT_PSC3M5_EVK): UART／DEBUG_UARTエイリアス
- [公式UART例](https://github.com/Infineon/mtb-example-ce240490-uart-transmit-receive/tree/1ce0a0d9936f9651e4be6c7616c6b66d72ee0b8b): PDL UARTとクロック設定の基準
- [公式通信試験設定](https://github.com/Infineon/mtb-example-ce241084-safety-communication-test/blob/cf9b93b45bcac376d428fbf8cc932855366862da/templates/TARGET_KIT_PSC3M5_EVK/config/design.modus): P3.2/P3.3 → SCB4の接続根拠

公式Empty AppからProject Creator 3.9で作成。BSPはKIT_PSC3M5_EVK release-v3.3.0を
アプリ専用の `APP_KIT_PSC3M5_EVK` としてコピーしています。
公式PDL release-v3.24.0、PSOC Control HAL release-v1.2.0を使用します。
旧CAT1 HALの `cyhal.h` は使用しません。UART送信・待機はPDL APIです。

設定原本は `bsps/TARGET_APP_KIT_PSC3M5_EVK/config/design.modus`。
Device ConfiguratorでUSER_UART=SCB4、TX=P3.3、RX=P3.2、115200/8N1を確認できます。
公式UART例のSCB3ルーティングを、別の公式例で確認したSCB4ルーティングに変更し、
Device Configurator CLIで検証・生成しています。生成コードを直接編集していません。
生成された `config/GeneratedSource/cycfg_routing.h` は
`P3_2_SCB4_UART_RX` と `P3_3_SCB4_UART_TX` を指定します。
UARTクロックは96MHz÷104、オーバーサンプリング8で、実効約115384.6baud（+0.16%）です。

## Windowsでのビルド・書き込み

リポジトリルートのPowerShellから:

```powershell
.\firmware\psoc\control-c3m5\uart.ps1 getlibs
.\firmware\psoc\control-c3m5\uart.ps1 device-configurator
# UART設定を確認し、Save/Generateして閉じる
.\firmware\psoc\control-c3m5\uart.ps1 build
.\firmware\psoc\control-c3m5\uart.ps1 program
```

インストール先が異なる場合は `-ToolsPath` を指定します。
SWD書き込みはKitProg3を使用し、COM9等のUSB-UARTポートは指定しません。
ModusToolbox terminalでは、このディレクトリで `make getlibs`、
`make build CONFIG=Debug`、`make program CONFIG=Debug` でも実行できます。
親ディレクトリの統合アプリは今回の試験には使用しません。

## Atomでの実機テスト

既存AtomアプリはHELLOを不正イベントとして破棄します。
専用の `../atom-uart-monitor/atom-uart-monitor.ino` はM5Unifiedを使用し、
GPIO2で受信したバイトをUSB Serialへ転送します。GPIO1からは送信しません。
このスケッチの書き込みはAtomの現在のアプリを置き換えます。
CoreS3/PIRのソースや書き込みには触れません。

```powershell
# リポジトリルートから。ポートは arduino-cli board list で確認
arduino-cli compile --jobs 4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB" --build-path "$PWD/firmware/psoc/control-c3m5/build/atom-monitor" firmware/psoc/control-c3m5/atom-uart-monitor
arduino-cli upload -p COM4 --fqbn "m5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB" --input-dir "$PWD/firmware/psoc/control-c3m5/build/atom-monitor" firmware/psoc/control-c3m5/atom-uart-monitor
arduino-cli monitor -p COM4 --config baudrate=115200
```

1. 交差配線・共通GNDを確認し、両基板をUSB給電。電源線同士は接続しない。
2. AtomのUSBモニターを開いてC3M5をリセットする。
3. `HELLO,1` から約1秒ごとに増加する行を60行以上確認する。途中接続なら1からでなくてもよい。
4. 欠番・重複・文字化けを確認し、ログを保存する。
5. P3.3自体の電気波形を確認する場合はロジックアナライザーで115200/8N1として読み取る。

ビルド、書き込み、P3.3の波形、Atomでの連続受信はそれぞれ別に判定します。
リセットで連番は1へ戻ります。音声・センサーの動作確認は対象外です。
## 今回の確認記録（2026-10-18）

- ブランチ `main`、worktree `C:/Users/soraj/dev/sd_2605`、開始時HEAD `66bf8ea`。開始時の未コミット変更なし。
- ModusToolbox 3.9、GCC ARM 14.2.1、Debugビルド成功。Flash使用量11,556バイト。
- Device Configurator Backend 3.90、device-db 4.41.0で生成成功。SCB4とP3.2/P3.3ルーティングを生成コードで確認。
- C3M5: KitProg3 SWDで書き込み成功（12,288バイト）。KitProg3 USB-UARTはCOM9で、試験UARTとは別。
- Atom: Arduino CLI、M5Stack ESP32 3.3.10、M5Unified 0.2.26、M5GFX 0.2.33でビルド・COM4への書き込み成功。
  試験スケッチはFlash 560,255バイト、静的RAM 26,272バイト。
- デバッガでC3M5の1000ms待機ループと `line="HELLO,146\n"`、次の `sequence=147` を確認。
  SCB4は有効、TX FIFOが空へ戻っていることを確認。これは外部ピンの波形測定ではない。
- AtomのUSBログで `[MONITOR] bytes=0 RX=2 TX=1 baud=115200` を確認。
  66秒の取得でモニター状態行13行、HELLOは0行。受信モニター自体は動作しているが、HELLOの連続受信は未確認。
- P3.3の電気波形は測定していない。通信成功・全完了とは判定しない。
  J4.2→GPIO2、共通GNDの接触と接続先を確認し、再取得する必要がある。

書き込み後に実行が始まらない場合はC3M5のSW1リセットを押す。
今回の切り分けではOpenOCDの `ENABLE_ACQUIRE=0` で `reset run` を実行し、
ROMのTest Modeではなくアプリ内の待機ループで動作していることを確認した。
Atomモニターは5秒ごとに受信総バイト数をUSBへ表示する。音声・センサーは初期化しない。

ローカルの `../c3-build.log`、`../c3-program.log`、`../atom-build.log`、
`../atom-program.log`、`../uart-capture.log` はGit対象外。
生成ソース、取得ライブラリ、ビルド成果物もGit対象外。BSPのソース・設定・依存ロックは保持する。
このプロジェクトは既定のSecureアプリ構成で検証しており、BSP付属のNon-secure用プリビルドは対象外。

### 接続後の再試験

ユーザーの接続完了連絡後、COM4/COM9を再認識。66秒の再取得でもHELLO 0行、
モニター状態行13行、受信総バイト数0だった。
C3M5を `ENABLE_ACQUIRE=0` のOpenOCDでリセットし、`CYBOOT_NEXT_APP_LAUNCHED` を確認。
リセット後の追加取得でもAtom受信0バイト。連続受信の完了条件は未達のまま。

### 再配線後の再試験

再配線完了の連絡後に66秒間取得。モニター状態行13行、HELLO 0行、受信総バイト数0。
取得中にC3M5をリセットし、VTarget=3.283V、`CYBOOT_NEXT_APP_LAUNCHED` を確認したが、
Atomの受信は変わらなかった。再配線後も実機通信の成功は確認できていない。

### 最終確認：UART通信成功

後続の [Atom専用受信試験](../../../m5stack/atoms3r-cam/tests/uart-rx/README.md) で、
C3M5をPCへ接続した後に正常なHELLO受信を確認した。
最終版の約66秒の取得ではHELLO,149〜HELLO,215の67行が欠番・重複なくUSBへ表示され、
間隔は0.984〜1.000秒、破棄行は0だった。上記の受信未確認という記録は切り分け途中の結果である。
C3M5のビルド・書き込みと、Atomでの連続受信の完了条件を達成した。
P3.3の波形測定は別途未実施で、今回の確認は配線先Atomでの受信による。
