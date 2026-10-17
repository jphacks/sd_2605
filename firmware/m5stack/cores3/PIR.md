# CoreS3 PIR照合

現行スケッチはCORES3_DETECTION_ONLY=1のカメラ専用構成です。Rails/Wi-Fi、LittleFS/NVS、方向別WAV再生はこのモードでは使用していません。今回もこの設定を維持しています。既存の方向フックは方向とLEAVE/ARRIVEの通知で、WAV再生には未接続です。PIR照合後に鳴らすのは既存の確認音です。旧通信版・Core版・API仕様は変更していません。

## 接続・設定

Unit PIR U004をPORT.B（黒）へ接続します。信号GPIO8、5V、共通GND、GPIO9未使用です。
PirFusion.hのPirConfigに以下を定義しています。

| 定数 | 初期値 |
|---|---|
| PIR_ENABLED | true |
| PIR_PIN | 8 |
| PIR_WARMUP_MS | 30000 |
| PIR_BEFORE_WINDOW_MS | 2000 |
| PIR_AFTER_WINDOW_MS | 1000 |
| EVENT_COOLDOWN_MS | 5000 |

起動後30秒間は自動確定しません。ウォームアップ後に一度LOWを確認してから、LOW→HIGHを採用します。ウォームアップをまたぐHIGHは採用しません。
カメラタスクが記録した方向確定時刻の2秒前〜1秒後にPIRがHIGHなら照合できます。LOW後も最後のHIGHから2秒以内なら有効です。保留候補は1件で、1秒の期限を超えると棄却します。
同じLOW→HIGH反応は一度のみ使用し、確定後5秒間も抑止します。長い再トリガーHIGH中は再確定せず、次のLOW→HIGHが必要です。PAUSEで保留を取り消します。UNKNOWNとPIR単独ではイベントを発行しません。
方向アルゴリズムと左右→外出/帰宅のMAPを維持しています。PIRは人体の動きの補助確認で、静止人物やすべての物体を検知できるわけではありません。

PIR有効時は動体Countだけでは自動音を鳴らさず、照合確定時に方向フックと確認音を呼び出します。CHIME OFFは自動音を止めます。TEST SOUNDは手動診断として使用できます。
PIR_ENABLED=falseで再ビルドすると従来の動体Count確認音と方向フックへ戻ります。追加の5秒抑止も適用しません。

Serialは[PIR] HIGH/LOW、[CAMERA] 方向、[FUSION] PENDING/CONFIRMED/REJECTED、音声開始API成功時に[AUDIO] PLAYを出します。PLAYは実際の発音確認とは別です。GPIO状態遷移時刻も内部で保持します。
画面の最下段の統計行はPIR:WARMUP/DETECTED/IDLE/OFF、F:PENDING/CONFIRMED/NONEです。CONFIRMEDは最後の照合結果です。Now/LastとCount/Dirは従来のカメラ単独の統計です。従来READMEのCount時点の確認音の説明はPIR無効時に適用します。
画面は既存200ms周期、ログは遷移時に更新します。PIR処理にdelayは使いません。

## ビルド・検証

```powershell
./scripts/build-cores3.ps1 -Jobs 4
# C++コンパイラー（g++またはzig c++）でホスト検証
zig c++ -std=c++11 tests/cores3/pir_fusion_test.cpp -o build/pir-fusion-test.exe
./build/pir-fusion-test.exe
zig c++ -std=c++11 tests/cores3/motion_direction_test.cpp -o build/motion-direction-test.exe
./build/motion-direction-test.exe
```

ホストテストは前後窓の境界・期限切れ・HIGH再使用防止・クールダウン・ウォームアップ・PIR単独・キャンセル・millis周回と、既存カメラ方向判定の回帰を検証します。GPIOの電気的入力や発音の実機試験ではありません。

実機確認手順:
1. 本体とPIRを固定し起動、30秒待ち、PIRがLOWへ戻ることを確認する。
2. 人が通過したとき[PIR] HIGH/LOWを確認する。
3. 左右各10回、人を横切らせ、[CAMERA]の方向、同方向のCONFIRMED、確認音1回とMAPを確認する。各回LOWかつ5秒以上空ける。
4. PIRを反応させずカメラだけ動かし、PENDING→REJECTED、自動音なしを確認する。
5. カメラ範囲外でPIRだけ反応させ、方向イベントと発音がないことを確認する。
6. HIGH保持・連続通過・5秒以内の再通過で重複再生されないことを確認する。
7. PAUSEとウォームアップ中の抑止、手動TEST SOUND、既存UI操作を確認する。
8. PIR_ENABLED=falseをビルド・書き込みして従来動作を確認し、trueに戻す。
9. Wi-Fi/Railsは現行カメラ専用モードでは稼働しないため、このモードで通信維持を検証済みとは扱わない。

コンパイル、書き込み、Serial確認、実際の発音、人の左右通過、Wi-Fi/Rails通信は別項目として結果を記録してください。

## 今回の検証結果（2026-10-17）

- scripts/build-cores3.ps1 -Jobs 4でPIR有効版のコンパイル・リンク成功。Flash 634,159 / 1,310,720 bytes（48%）、静的RAM 46,016 / 327,680 bytes（14%）。ログ: build/cores3-pir-build.log。
- PIR照合ホストテスト成功。既存motion_direction_testの左右通過・揺れ・逆行・曖昧判定・MAP・再待機などの回帰テストも成功。ログ: build/cores3-pir-fusion-test.log、build/cores3-pir-direction-test.log。
- Core用ファームウェア、Rails API、Wi-Fi・保存・ダウンロード実装に差分なし。新規ライブラリなし。
- COM4は列挙されたが端末種別はUnknown。今回の書き込みとSerial確認は未実施。
- 実GPIOのHIGH/LOW、人による左右通過、誤検知率、重複発音抑止、実際の発音、無効設定での実機動作は未確認。上記の実機手順で検証する。
- 現行のカメラ専用モードではWi-Fi/Rails通信の実動作を検証できない。
- 作業用コピーでPIR_ENABLED=falseのコンパイル・リンクも成功。Flash 633,407 bytes（48%）、静的RAM 45,904 bytes（14%）。ログ: build/cores3-pir-disabled-build.log。配布用build/cores3/とソースの設定はPIR有効のまま。
