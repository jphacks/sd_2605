# E84 AI Kit レーダーAI / I²C Target

公式DEEPCRAFT Radarにイベント出力を追加した開発中の実装。
**ModusToolboxビルド・書き込み・シリアル・センサー・実配線I²Cは未確認。**
この環境はLinuxで、ModusToolbox 3.9と対象ボードはない。
既存の動作済みプロジェクトは使わず、ユーザー了承のもと公式固定コミットから構成した。
CoreS3など他のボードのソースには変更しない。

## ソースと構成

[公式レーダー例](https://github.com/Infineon/mtb-example-psoc-edge-ml-deepcraft-deploy-radar/tree/203e88c02e0503a8dee8cfcf49ece3fdb813e544)
のCM33 Secure / CM33 Non-secure / CM55構成、モデル、レーダー設定と取得処理を維持。
`proj_cm55/main.c` に起動処理とIMAI_dequeue成功時の結果受け渡しを追加。
公式ソースのライセンスは `LICENSE` および各ファイルの表記を参照。
取得元は `UPSTREAM.md`、直接依存の固定コミットは `dependency-pins.json` と各 `.mtb` に記録。
推移的依存は `make getlibs` が解決するため、初回実機ビルド時の `assetlocks.json` を保存して
完全な依存固定を行うこと。現時点でビルド済みの依存ロックはない。

| モジュール | 役割 |
|---|---|
| `proj_cm55/edge/protocol_v1` | 明示的little-endian、24 bytes、CRC-8 |
| `radar_events` | ラベル変換、安定化、seq、entropy注入によるboot_id |
| `radar_ai` | 公式推論への接続、TRNG、1 ms SysTick、シリアルログ |
| `i2c_target` | PDL SCB Target割り込み、転送単位のスナップショット |

## ピンと競合

| 用途 | 配線 / リソース |
|---|---|
| SDA | J14.18（指定配線）→ BSP `CYBSP_I2C_SDA_3V3` / P17.1 / SCB5 |
| SCL | J14.17（指定配線）→ BSP `CYBSP_I2C_SCL_3V3` / P17.0 / SCB5 |
| GND | J14.20 → C3M5 GND |
| C3M5 | SDA J3.9 / P9.2、SCL J3.10 / P9.0 |
| レーダーSPI | SCB3 / `CYBSP_SPI_CONTROLLER`、公式設定を維持 |
| その他I²C | SCB0 / 1.8 Vは外部バスに使用しない |

[BSP固定版](https://github.com/Infineon/TARGET_KIT_PSE84_AI/tree/4894dd6a1ac2cee8be0d028476da4c7e9e640431)
の `config/design.modus` にP17.0/P17.1→SCB5の接続がある。
SCB5の生成名は `CYBSP_I2C_CAM_CONTROLLER` / `CYBSP_I2C_DISPLAY_CONTROLLER`。
そのconfigをコピーし、Target専用・0x30に変更してPDLで初期化する。
同じSCBのカメラ／ディスプレイ初期化を追加しないこと。
J14端子とネットの対応、SCB5のCM55アクセス権・割り込み経路、TRNGアクセス権は実機で確認する。
PDL APIは[DSL](https://github.com/Infineon/mtb-dsl-pse8xxgp/tree/c2793cf168c31420266648473122d78897c4faf9)
の `cy_scb_i2c.h`、HAL `mtb_hal_trng.h` / `mtb_hal_trng_impl.h` を参照した。

両ボードは独立USB給電、GND共通。3.3 VへのSDA/SCLプルアップの有無と合成抵抗を確認する。
Controllerは100 kHz。7-bit addressは0x30（左シフトした0x60をAPIへ渡さない）。

## イベント仕様と同時アクセス

Controllerはレジスタアドレス書き込みなしで24 bytesを直接読む。

| Offset | 値 |
|---|---|
| 0–2 | A5 / 01 / source=01 |
| 3–6 | kind / code / confidence / valid bit0 |
| 7 | 0 |
| 8–11 | boot_id uint32 LE |
| 12–15 | seq uint32 LE |
| 16–19 | イベント確定時のuptime_ms uint32 LE |
| 20–22 | 0 |
| 23 | bytes 0–22のCRC-8 poly=07 init=00、反転なし |

起動時はNONE・valid=0・seq=0・confidence=255。初回イベント後は最後の有効イベントを保持。
読み出しでseq/uptimeは変わらない。boot_idは起動時にハードウェアTRNGから32 bit生成する。
TRNGが失敗した場合は起動を停止し、固定値へのフォールバックは行わない。
32 bit乱数のため再起動間の衝突確率はゼロではない。実機再起動テストは別途必要。
seqは新しい確定イベントだけで増加（uint32で周回）。受信側は `(boot_id, seq)` の一致で重複排除。

公式ラベルは `unlabelled`, `Circle`, `Push`。Push→kind=1/code=1、Circle→kind=1/code=2。
選ばれたラベルのモデルスコア0.80以上を連続2回確認し、前回から1000 ms以上で確定する。
次の確定には高スコアのunlabelledによる再アームが必要。
scoreは0–1を0–100へ丸める。confidenceは校正された確率ではなくモデルスコア由来。
実機データで閾値・連続数・モデル出力範囲を評価する必要がある。
未知ラベルや非有限値はイベントを生成しない。**RADAR_MOTIONは実装せず、一切生成しない。**

CM55メインループだけがイベントを書き込む。serialize後、24 bytesのlatestコピーだけ割り込みを禁止。
I²C READイベントISRがlatestを専用transferへコピーし、PDLへ渡す。
転送中にlatestが更新されてもtransferは変わらず、次のREAD開始時に更新する。
ISRはログ・推論・待機を行わない。レーダーGPIO優先度1、SPI優先度2、I²C優先度3。
DMA/他コアから直接publishしない。追加する場合は同じ所有権規約に従ってCM55へ通知する。
キューはなく、ポーリング間の複数イベントは上書きされる。seqの飛びで欠落を検知できる。
公式の推論・UART表示処理はそのままなので、負荷やclock stretchingは実測が必要。
SysTickはこのbare-metal CM55で専有する。RTOS導入時には既存OS時刻APIへ置き換える。

## Windows / ModusToolbox 3.9

1. GitとModusToolbox 3.9（GCC_ARM 14.2.1）を用意。
2. このフォルダでPowerShellから `./scripts/prepare.ps1` を実行。
   BSPを固定コミットで `bsps/TARGET_KIT_PSE84_AI` に取得する。既存BSPは変更せず停止する。
3. modus-shellでこのフォルダを開き、次を実行する。

```sh
make getlibs
make build TARGET=KIT_PSE84_AI TOOLCHAIN=GCC_ARM CONFIG=Debug
make program TARGET=KIT_PSE84_AI TOOLCHAIN=GCC_ARM CONFIG=Debug
```

依存取得先はこのフォルダ内の `mtb_shared/`。他のワークスペースの共有ライブラリを使わない。
Configurator生成が必要な場合はこのコピーの `make device-configurator` で
`bsps/TARGET_KIT_PSE84_AI/config/design.modus` を開いてGenerateし、再ビルドする。
IDEが必要な場合は `make eclipse` で生成した3プロジェクトを新しい専用workspaceへimportする。
GCC_ARM Releaseは公式例で非対応のためDebugを使う。
最初に3コアの結合・署名・書き込みログを保存する。
既存ボードのライフサイクル／ブート設定は公式 `docs/using_the_code_example.md` に従う。
ビルド成果物、取得ライブラリ、BSPコピーはGit対象外。

## ホストテスト

Linux / C11コンパイラで実行する。

```sh
sh tests/run.sh
```

AddressSanitizer / UndefinedBehaviorSanitizer有効で以下はPASS：
CRC `123456789` → F4、全オフセットとLE、初期NONE、entropy成功/失敗契約、
Push/Circle変換、未知/NaN/低スコア除外、連続判定、再アーム、クールダウン、
イベント保持、seq周回、時刻周回、24-byte read callback、転送中publish、1万回の同一イベント読み出し。
PDL/割り込み/NVICはモック。実TRNGのエントロピー、実バス・DMA・AIの同時稼働を証明するテストではない。

## 実機確認チェックと未確認事項

以下はすべて未確認。完了条件のうちModusToolboxビルド成功・公式AI継続動作・0x30応答は未達。

1. **コンパイル**：上記Debug buildで3コア成功、生成されたSCB名、TRNG API/権限、リンクを確認。
2. **書き込み**：KitProg3経由で結合イメージをprogramし、ログに成功があることを確認。
3. **シリアル**：115200 / 8N1。boot_id、Radar initialized、モデル各スコアを確認。
   再起動を繰り返しboot_idが変わることも確認する。
4. **センサー**：公式例の向き・約60 cmでPush/Circleを行い、次のログを確認。
   unlabelledへ戻した後に再ジェスチャーでseqが1つ増えることを確認する。
5. **通信**：C3M5で100 kHz、0x30から24 bytesを読み、magic/version/source/CRCを検証。
   起動直後はNONE、以降はログとkind/code/seq一致、同じ読み出しでseq不変を確認する。
6. **同時動作**：10 ms間隔などで連続ポーリングしながらジェスチャーを繰り返す。
   CRC不一致、取得停止、リセット、認識遅延、clock stretching時間を計測。
   STOP・短い読み出し・反復START後も次の24 bytesが正常なことを確認する。
   C3M5未完成ならUSB-I²C Controller等を使う。ホストモックだけで通信成功扱いにしない。

```text
[RADAR] PUSH confidence=92
[I2C] event seq=14 kind=1 code=1
```

音声再生は別ボードの担当。レーダーモデルを人物通過検知として評価しない。
