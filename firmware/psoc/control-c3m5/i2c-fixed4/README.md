# Step 3A: C3M5 I²C Controller試験

[E84固定4バイト試験の手順・検証結果](../../edge-e84-ai/i2c-fixed4/README.md)を参照。
既存UART試験を保持した独立アプリ。書き込み後はボード上のUART試験アプリを置き換える。

SCB0、SDA=J3.9/P9.2、SCL=J3.10/P9.0。100 kHzで0x30から4バイトを直接読み、
`A5 01 30 5A`と一致した連続回数を`streak`に表示する。最後のバイトはNACK後STOP。
失敗時はSCBを再初期化する。デバッグUARTはSCB3 / 115200 / 8N1。
設定原本は`bsps/TARGET_APP_KIT_PSC3M5_EVK/config/design.modus`。
SCB0は2 MHz（96 MHz÷48、16-bit divider 0）、データレート100 kHz。

ModusToolbox 3.9 terminalでこのディレクトリから実行する。

```sh
make getlibs
make build CONFIG=Debug
make program CONFIG=Debug MTB_PROBE_SERIAL=<C3M5のKitProg3シリアル番号>
```

今回のビルドは既存UART試験から取得済みライブラリと`libs/`をローカルコピーして使用。
最終版ではConfigurator生成を有効にしてDebugビルド・書き込み成功。
クリーン環境からのgetlibsは未確認。SDK・生成物・ログはGit対象外。

書き込み後に通常起動しない場合はRESETボタンを押す。
今回の実機ではOpenOCDの`ENABLE_ACQUIRE=0`で`reset run`を実行して起動した。
COM9で通信試行を確認したが、SCL/SDA両方Lowでタイムアウトしており通信成功は未達。
外部プルアップ・配線を確認してから30回以上の連続応答を再試験する。
