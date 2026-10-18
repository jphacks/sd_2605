# Step 3A: 固定4バイト I²C Target（実機通信は未達）

KIT_PSE84_AIを7-bitアドレス`0x30`のTargetとして動作させ、
Controllerの直接Readに`A5 01 30 5A`を返す独立テスト。
レーダー推論・24バイトプロトコルは使用しない。既存レーダーAIのソースは保持する。

| 信号 | E84 | C3M5 |
| --- | --- | --- |
| SDA | J14.18 / P17.1 | J3.9 / P9.2 |
| SCL | J14.19 / P17.0 | J3.10 / P9.0 |
| GND | J14.20 | GND |

J14.17 / P17.2は使用しない。両基板をUSB給電しGNDを共通にする。
SDA/SCLの3.3 VへのプルアップとアイドルHighを確認する。

Device Configurator設定は`bsps/TARGET_APP_KIT_PSE84_AI/config/design.modus`。
P17.0/P17.1をSCB5へ接続し、Target専用・アドレス48（0x30）に設定。
CAM/DISPLAYというBSPエイリアスは同じSCB5を指すため、対応Controllerを初期化しない。
CM55がPDL割り込みを処理し、送受信バッファをRead/Write開始時と完了時に再設定する。
STOP・反復START・エラー後のPDL状態処理を利用する。ISRでは待機・ログ出力をしない。

[公式キットガイド](https://documentation.infineon.com/psocedge/docs/cci1762693051052)では、
KitProg3 USB-I²CとオンボードI²CセンサーはP8.0/P8.1の1.8 Vバス、
外部I²CはP17.0/P17.1の3.3 Vバス。今回はセンサーControllerを起動しない。
同じ外部バスにつながるカメラ・ディスプレイ・追加モジュールは外して確認する。
電気的な競合・プルアップの実測確認は未完了。

## ビルド・書き込み

ModusToolbox 3.9 / GCC_ARM 14.2.1 / Debug。
ModusToolbox terminalでこのディレクトリから実行する。

```sh
make getlibs
make device-configurator
# SCB5、P17.0/P17.1、Target、0x30を確認してGenerate
make build CONFIG=Debug
make program CONFIG=Debug MTB_PROBE_SERIAL=<E84のKitProg3シリアル番号>
```

BSP名はキットのアプリ専用コピー`APP_KIT_PSE84_AI`。
依存ライブラリ・生成コード・ビルド成果物はGit対象外。
初回のgetlibsはネットワークが必要。今回の依存取得は途中停止し、既存公式レーダー例の
`libs/`をコピーしてML・レーダー依存の参照を除き、既存ライブラリでビルドした。
その際は`CY_GETLIBS_SHARED_PATH=C:/Users/soraj/dev/mtw`をbuild/programに指定した。
クリーン環境からのgetlibsと再ビルドは未確認。
同梱`FlashLoaders/PSE84_SMIF.FLM`はBSP付属の書き込み用ファイル。

書き込みはボード上のアプリを置き換える。KitProg3が複数接続されている場合は
必ず`MTB_PROBE_SERIAL`で対象を選ぶ。COM番号は書き込み先の指定に使用しない。

## 実機試験

1. [C3M5専用Controller](../../control-c3m5/i2c-fixed4/README.md)をビルド・書き込みする。
2. 両側の115200 / 8N1ログを開く。今回のポートはE84=COM6、C3M5=COM9。
3. C3M5の`clock=2000000 rate=100000`を確認する。
4. C3M5で`OK data=A5 01 30 5A`と`status=0 stop=0`を確認する。
5. 連続成功を示す`streak`が30以上になるまで記録し、E84の`reads4`増加と照合する。
6. 失敗時はSDA/SCLの電圧・プルアップ・導通・共通GNDを確認する。

## 停止時点の結果（2026-10-18）

- E84: 3コアDebugビルド、結合イメージ生成、書き込み・verify成功。
  書き込み81,920 bytes、verify 72,216 bytes。COM6でTargetアプリ稼働を確認。
- C3M5: Debugビルド・書き込み・COM9のControllerログ確認済み。
  最終設定はDevice Configuratorで生成済み。
- C3M5のRead開始前のGPIO読値は`scl=0 sda=0`。
  `status=11149315`（MANUAL_TIMEOUT）、`stop=11149314`（NOT_READY）。
  E84は`reads4=0 writes=0 errors=0 other_reads=0`。
- **0x30応答、4バイト取得、30回連続応答は未達。通信成功とは判定しない。**
- ユーザー指示により開発を停止。プルアップと配線の確認から再開する。
  原因は未確定。レーダーAI・センサーとの同時動作は試験対象外。
