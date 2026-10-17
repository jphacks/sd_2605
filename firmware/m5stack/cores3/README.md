## ローカル音声再生（2026-10-17）

カメラ専用モードでもLittleFSを読み取り、確定したARRIVEでは `/arrive0.wav`、LEAVEでは `/leave0.wav` を再生します。Wi-Fi/API/NVSは使用しません。PIR有効時は照合成立後に再生し、CHIME OFFでは方向別音声も消音します。TEST SOUNDは従来の1200Hz確認音です。検知開始時の共通確認音は鳴らしません。

音源はPCM WAV・モノラル・16bit・16kHz・最大5秒です。起動時に検証してPSRAMへ読み込み、シリアルに `[VOICE] loaded` を出します。欠損・不正形式・メモリ不足時はログを出し、代替音は鳴らしません。LittleFSの自動フォーマットは行いません。

今回の直接書き込み音源は `okaeri_03.wav`（ARRIVE）と `itterasshai_01.wav`（LEAVE）を16kHzへ変換したものです。以下の旧説明にある「LittleFS不使用」「方向フック未接続」はこの変更より前の状態です。

# CoreS3 カメラ動体・移動方向検知

PIR追加仕様・設定・実機確認は[PIR.md](PIR.md)を参照してください。現行は照合成立時だけ自動確認音を鳴らします（PIR無効時は従来動作）。

内蔵GC0308カメラで物体の動きと画面上の左右方向を検知します。
現在の構成は`cores3.ino` → `CameraOnly.h` → `CameraMotionDetector.h`です。
人物分類モデル、TensorFlow Lite、LTR-553、Web、Wi-Fi、SD、LittleFS、NVSは使用しません。
画像を保存・送信しません。USB Serialは送信待ち0ms・バッファ4KBとし、モニター未接続や読み取り停止でも撮影を待たせません。送信が詰まった場合、診断ログは一部欠落することがあります。旧Web版の通信・音声処理やCoreのファームウェアは変更していません。

## 重心だけでは方向が出ないケースへの対応（2026-10-17）

実機ログで`[MOTION] Direction: UNKNOWN ... dx=8.9`と`[UI DIRECTION] ... Last:UNKNOWN`が一致し、表示への伝達より前でUNKNOWNになっていることを確認しました。差分重心は53→79→62程度に戻っており、物体の実移動と一致しませんでした。`HorizontalMotion.h`の水平小領域照合を追加し、重心とは独立した変位を方向判定に使用します。動体Count・カメラ初期化・イベント配送・クールダウン・音声フックは維持します。AIモデルや画像バッファの追加はありません。

Serialの追跡ログに`flow`（1フレームあたりの画面換算変位）、`votes`、`reliable`、確定ログに`flowDx`（累積変位）、`flowFrames`を追加しました。画面の赤点と`x / dx`は従来どおり差分重心です。画像照合で確定した場合、重心dxが32未満でも正常です。模様が少ない物体、探索幅を超える速い動き、上下移動、複数物体や本体の揺れではUNKNOWNまたは誤判定があり得ます。

ホストテストで静止背景上の模様付き物体の左右移動、重心が中央に残る場合の左右判定、静止・露出のみの変化・往復・複数領域の除外、1通過1イベントに成功しました。変更は`HorizontalMotion.h`、`MotionDirectionConfig.h`、`MotionDirectionTracker.h`、`CameraMotionDetector.h`、方向テスト、このREADMEです。

ビルド・COM4への書き込み・ハッシュ照合成功。Flash 633,235 bytes（48%）、静的RAM 45,872 bytes（13%）。実機Serialで以下を確認しました。

- seq=5: 重心150.6→17.8、`flowDx=-128.1 flowFrames=20`、`Direction: RIGHT_TO_LEFT`、同じseqの`Last:RIGHT_TO_LEFT`描画ログ。確定後COOLDOWN→IDLEへ復帰。
- seq=6: 重心13.4→145.7、`flowDx=114.9 flowFrames=17`、`Direction: LEFT_TO_RIGHT`、同じseqの`Last:LEFT_TO_RIGHT`描画ログ。
- 動いている間も撮影約19〜20fps、処理約12〜18ms、errors=0。LCD更新は通常約19〜20fpsですが、一時的に約10〜13fpsになる区間もありました。
- 連続した往復を一つの追跡として取得したケースはUNKNOWNでした。逆方向を別イベントとして試す際は、物を枠外へ出してREADYへ戻るまで待ってください。

実物の入力による左右の確定と画面描画呼び出しはSerialで確認済みです。ユーザーからも「できてる！」と実機動作の確認を得ました。長時間の認識精度と実際の発音は未確認です。ログは`build/cores3-horizontal-motion-build.log`、`build/cores3-horizontal-motion-upload.log`、`build/cores3-horizontal-motion-serial.log`です。

## 方向判定の再待機修正（2026-10-17）

以前の実機ログで、動体Countが増えても背景差約90%により方向追跡がCOOLDOWNに固定されていました。背景一致を必須条件から外し、400ms静止で方向を確定、確定から最低2秒・連続1秒静止で再待機するよう変更しました。一時的な領域欠落と単発の曖昧判定を許容し、逆行量は平滑化後の座標で評価します。

- 変更: `MotionDirectionTracker.h`、`MotionDirectionConfig.h`、`CameraStatusText.h`、方向テスト、このREADME。
- 人工画像による左右通過・静止・逆戻り・多物体・重複抑止に加え、背景不一致のままの確定／再待機、一時的な重心欠落のテスト成功。
- ビルド成功: Flash 631,955 bytes（48%）、静的RAM 45,808 bytes（13%）。COM4への書き込み・ハッシュ照合成功。
- シリアル確認: カメラ初期化成功、撮影約19〜20fps、errors=0、静止中IDLE・Count=0を維持。今回の取得中に通過はなく、実物による左右方向の正答と発音は未確認。
- ログ: `build/cores3-direction-rearm-build.log`、`build/cores3-direction-rearm-upload.log`、`build/cores3-direction-rearm-serial.log`。

## 方向表示の見分け方

以前の`Dir:方向名`は最後の確定イベントだけを表示し、追跡途中には変化しませんでした。現在はTRACKING中に`Now:LEFT_TO_RIGHT`／`Now:RIGHT_TO_LEFT`（方向候補）、終了後に`Last:...`（最後の確定結果）を表示します。根拠不足の候補はUNKNOWNです。候補は音声・イベントを発火しません。`Count:... Dir:数値`のDirは確定イベント数で、UNKNOWNも含みます。

Serialの`[UI DIRECTION] seq=... state=... label=... final=...`は実際にLCDへ描画した文字列と確定イベント番号を記録します。`[MOTION] Direction:`／`[DIRECTION EVENT]`が左右を示すのに、同じseqの`[UI DIRECTION]`だけ違う場合は表示側の問題です。本体を固定して左右それぞれ横切らせ、Nowの変化、静止400ms後のLast、確定番号を比較してください。

変更ファイルは`CameraOnly.h`（表示・ログ）、`CameraMotionDetector.h`（候補の共有）、`MotionDirectionTracker.h`（副作用のない候補取得）、方向テスト、このREADMEです。方向判定・確定イベントの条件は変更していません。

候補が確定イベントを生成せず、確定後にLast用の結果が保存されるホストテストと既存の左右テストに成功。ビルド成功（Flash 632,403 bytes、静的RAM 45,840 bytes）、COM4へ書き込み・ハッシュ照合成功。Serialでerrors=0、撮影約18〜20fpsの稼働を確認しました。取得中に通過はなく、実機でのNow／Lastの切り替わりと発音は未確認です。ログは`build/cores3-direction-display-{build,upload,serial}.log`です。

## ビルド・書き込み

M5Unified 0.2.26、M5GFX 0.2.33、M5Stack ESP32 3.3.10を使用します。

```powershell
.\scripts\build-cores3.ps1 -Jobs 4
.\scripts\upload-cores3.ps1 -Port COM4
arduino-cli monitor -p COM4 --config baudrate=115200
```

## 操作と実機確認

1. 本体を固定し、前景を空けて起動します。約1.5秒のWARMUPで背景を取得し、IDLEから待機します。
2. プレビューに写る物を、画面左から右へ約1秒かけて横切らせ、最後は画面外へ出します。
3. `Tracking started`と重心座標、移動終了後の`Direction: LEFT_TO_RIGHT`をSerialで確認します。
4. 逆方向に通過させ、`RIGHT_TO_LEFT`を確認します。基準は実空間の向きではなくプレビュー上の左右です。
5. 方向確定後はDETECTED→COOLDOWNへ進みます。確定から最低2秒、かつ1秒以上連続して動きが収まるとIDLEへ戻ります。背景の完全一致は必要ありません。
6. 各方向を10回程度繰り返し、Dirが原則1通過1回になることを確認します。
7. 無人で60秒、物体を静止、途中停止、微小な揺れ、左右への往復、2物体の交差を試し、誤った方向が出ずUNKNOWNまたは無検知となることを確認します。
8. PAUSE/RUN、CHIME、TEST SOUND、RESET COUNT、SENS +/−の操作も確認します。実際の発音はログとは別に確認してください。

画面表示:
- `IDLE / TRACKING / DETECTED / COOLDOWN`: 方向追跡の状態。
- `Change / T`:  従来のフレーム差分率と感度。初期Threshold=3%、SENS +で1ポイント下げ、SENS -で上げます（1〜40%）。
- `FPS / LCD`: 実測の撮影・解析FPSとプレビュー更新FPS。
- `Count`: 従来の動体検知回数。従来の確認音はこの段階で鳴ります。
- `Dir`: 方向確定イベント回数（UNKNOWNを含む）。通過イベントへの接続にはこちらを使用します。
- `Now: LEFT_TO_RIGHT / RIGHT_TO_LEFT / UNKNOWN`: 追跡中の方向候補。
- `Last: LEFT_TO_RIGHT / RIGHT_TO_LEFT / UNKNOWN`: 追跡終了後に表示する最後の確定結果。
- `x / dx`: 平滑化した重心と移動量。0〜160のプレビュー画素座標で、実物の距離ではありません。
- 黄色い枠が検知範囲、縦線が左・中央・右の領域、赤点が追跡中の重心のX位置です。

画面上端のMAP（タイトル部分）をタップすると、方向から外出／帰宅への割り当てを反転します。
標準はLEFT_TO_RIGHT→LEAVE、RIGHT_TO_LEFT→ARRIVE。反転時は逆になります。
**方向名そのものは反転しません。** UNKNOWNは常にNONEです。
Serialは`+`/`-`/`p`/`m`/`t`/`r`に加え、`i`で割り当て反転ができます。
設定と表示回数は再起動で初期値へ戻ります。RESET COUNTは両方の表示回数をリセットし、追跡状態を変更しません。
背景が変わってCOOLDOWNが終わらない場合は、前景を空けてPAUSE→RUNを操作します。明示的な再開時に古い追跡と背景をリセットし、約1.5秒間背景を取り直してIDLEへ戻ります。この間は本体を固定し、物を横切らせないでください。再開自体では方向イベントを生成しません。

## 下端の案内表示

操作結果や方向結果は2.5秒間表示し、その後は現在の状態を表示します。
IDLEなら`READY: move left/right across view`、停止中ならPAUSED、追跡中ならTrackingを表示します。
COOLDOWNでは`waiting for quiet`を表示します。
以前の`RUN: hold still to rearm`は操作時の固定メッセージで、IDLEへ戻っても消えない表示不具合でした。
RUN操作後は`RUN: learning background (clear view)`を短時間表示して現在の状態へ戻します。
下端の表示内容は`[UI STATUS]`としてSerialにも出力します。

## 判定アルゴリズム

1. 従来のカメラ初期化・I2C共有・タッチ処理を維持します。撮影処理の周期上限を33ms（約30fps目標）、プレビューを50ms（約20fps上限）に分離しました。実速度はセンサーと照明に依存します。
2. 160×120 RGB565の各4×4画素を平均し、ノイズを抑えた40×30の明るさ画像にします。
3. 2フレーム前との差から画面全体の平均輝度差を差し引き、差20以上のサンプルを変化画素にします。従来のChange判定は維持します。
4. 3×3の近傍支持が少ない画素と小領域を除去し、8近傍の連結領域から変化画素の重心Xを計算します。追跡座標は指数移動平均で平滑化します。
5. 2フレーム連続の動きでTRACKINGを開始し、動きが収まった3フレーム以上・400ms以上で`dx=x_end-x_start`を求めます。背景不一致は終了を妨げません。
6. 同じ40×30画像で疎な5×5小領域を左右±6サンプル照合します。模様のない領域、静止領域、逆方向も同程度に一致する領域は除外します。3票以上・75%以上の左右一致があるフレームの変位を、2フレーム比較の重複分を補正して画面画素単位で累積します。4フレーム以上の有効照合、累積移動量の絶対値>32px、全移動の65%以上が純移動となる場合に左右を判定します。赤点の重心が中央付近で揺れても、模様のずれから方向を判定できます。
7. 画像照合が4フレーム未満なら従来の重心判定へフォールバックします。有効重心4フレーム以上、dx>32ならLEFT_TO_RIGHT、dx<-32ならRIGHT_TO_LEFTで、左右3領域の順序との矛盾、逆行、重心の飛びを除外します。複数領域の曖昧さが3フレーム以上かつ有効座標数の1/3以上なら両方式ともUNKNOWNです。画像照合で往復や移動不足が分かった場合は重心で上書きしません。
8. 6秒で終了しない追跡やフレーム欠落・500ms超の間隔・停止・感度変更による中断もUNKNOWNにして、一度だけ確定します。
9. 方向確定後は最低2秒と連続1秒の静止で再待機します。背景照合は診断ログ用に残しますが、露出変化による不一致で永久にCOOLDOWNへ留まることはありません。静止した物体は新しいイベントを生成しません。ただし一度停止してから再び動いた場合は別の動きとして扱われることがあります。物体IDによる追跡は行いません。

一つの動く物体でも差分には前後の2つの縁が現れるため、近い位置にあり上下に重なる領域は同じ通過として扱います。
完全な物体識別は行いません。近接・重なった複数物体、影、照明、カメラの揺れは完全には区別できず、UNKNOWNや誤判定の可能性があります。

## 調整定数・拡張箇所

`MotionDirectionConfig.h`で次を変更し、再ビルドできます。

| 定数 | 初期値 | 用途 |
|---|---:|---|
| captureIntervalMs / previewIntervalMs | 33 / 50ms | 撮影・プレビュー周期の上限 |
| differenceLagFrames | 2 | 比較画像のフレーム間隔 |
| flowSearchRadius / flowMinVotes | 6 / 3 | 水平照合の探索幅（40×30上）と最低一致数 |
| flowVoteAgreement / flowPathAgreement | 0.75 / 0.65 | フレーム内の左右一致率・追跡全体の純移動率 |
| endQuietMs / backgroundClearPercent | 400ms / 5% | 方向確定までの静止時間・背景一致の診断閾値 |
| rearmQuietMs | 1000ms | 再待機に必要な連続静止時間 |
| pixelDelta | 20 | 画素の明るさ差 |
| minNeighbors / minBlobPixels / minRegionPixels | 3 / 4 / 12 | 孤立ノイズと小領域の除去 |
| startFrames / minTrackFrames / endFrames | 2 / 4 / 3 | 開始・方向確定・終了に必要なフレーム数 |
| minDisplacement | 32px | 方向を確定する最低移動量（厳密に超える必要あり） |
| smoothingAlpha | 0.5 | 重心の平滑化係数 |
| maxCentroidStep / maxReverseTravel / jitterPixels | 64 / 8 / 2px | 追跡の飛び・平滑化後の逆戻り・座標ノイズ |
| cooldownMs / detectedHoldMs | 2000 / 300ms | 再判定待ちとDETECTED表示時間 |
| maxTrackMs / maxFrameGapMs | 6000 / 500ms | 追跡上限とフレーム中断 |
| mirrorImage | false | プレビューと解析画像を同時に左右反転 |
| invertArrivalDeparture | false | 起動時の外出／帰宅割り当て反転 |

`sampleWidth / sampleHeight / imageWidth`は現在の固定撮影サイズに対応し、感度調整用ではありません。
画像の左右補正は`mirrorImage`を使い、設置向きによる外出／帰宅の反転は`invertArrivalDeparture`またはMAP操作を使います。

変更・追加ファイル:
- `MotionDirectionConfig.h`: 調整定数。
- `MotionRegion.h`: 差分領域抽出と重心計算。固定ワーク領域約4.8KB。
- `MotionBackground.h`: 明るさ・コントラスト変化を補正した背景照合。参照背景は上書きしません。
- `MotionDirectionTracker.h`: 状態・方向イベント・外出／帰宅の割り当て。履歴画像は持ちません。
- `CameraMotionDetector.h`: 追跡の接続、背景画像1.2KB、比較用画像2.4KB、Serial出力、FPS計測、プレビュー。LCD転送用の独立した38,400-byte PSRAMバッファへ短時間でコピーし、LCD転送中は撮影用mutexを保持しません。
- `CameraMotionGate.h`: 既存判定のフレーム数・クールダウン定数を共通化（初期値は同じ）。
- `CameraOnly.h`: 方向・状態の表示、MAP操作、`handleMotionDirection()`。
- `CameraStatusText.h`: 操作案内の表示期限と現在状態の案内文。
- `tests/cores3/motion_direction_test.cpp`: 人工画像から方向イベントまでの検証。

`handleMotionDirection()`が一度だけ呼ばれる音声接続用の入口です。
現在は確定方向とLEAVE / ARRIVE / NONEをログ表示し、モード別音声は再生しません。
既存の確認音・TEST SOUNDは維持します。Web版やRails APIの契約は変更していません。

## ホスト検証

Zig 0.13.0のC++コンパイラーで方向テストと、動体判定・フレーム差分・画素平均・タッチの回帰テストを実行して成功しました。
方向テストは実際のフレーム差分→領域抽出→重心→状態管理をつなぎ、左右それぞれで従来の動体検知1回・方向イベント1回を確認しています。
静止、孤立ノイズ、微小移動、逆戻り、複数領域、鏡像、割り当て反転、画面内での停止、再通過、中断、時間上限、millisの折り返し、露出変化の補正と静止物体の残留判定も検証しています。
これは人工画像と座標列による検証であり、実物の左右通過の精度確認とは別です。

```powershell
.\build\host-tools\ziglang\zig.exe c++ -std=c++11 tests/cores3/motion_direction_test.cpp -o build/cores3-direction-test.exe
.\build\cores3-direction-test.exe
```

## 撮影・表示の高速化と検知調整（2026-10-17）

- 撮影の100ms待ちとプレビューの200ms待ちを廃止。撮影は33ms周期を目標、プレビューは50ms上限、数値欄は200msのまま分離しました。
- LCD転送を撮影mutexの外へ移し、専用PSRAMコピーを使用します。撮影待ちと処理時間、実測FPS/LCD FPSをログへ追加しました。
- USB送信待ち0ms・TXバッファ4KBにし、読み取り停止時のログ送信待ちが撮影を止めないようにしました。詰まった診断ログは欠落する場合があります。
- 初期Thresholdは3%。2フレーム前との差分により、高FPSでも動体の変化量を確保します。
- 背景の解除閾値を検知感度から独立した5%にしました。方向の終了判定には400ms以上の静止と前景の解除を必要とし、短い途切れで終了しません。
- 33ms・2フレーム比較・Threshold=3%の人工画像で、左右それぞれ動体1回・方向1回のテスト成功。静止・中断・重複・背景補正など既存の方向テストも成功。
- 最終ビルド・COM4への書き込み・ハッシュ検証成功。Flash 631,951 bytes（48%）、静的RAM 45,808 bytes（13%）。
- 起動ログでカメラESP_OK、空きHeap=260,188 bytes、空きPSRAM=8,230,100 bytes。
- 実測は撮影おおむね19〜20fps、プレビューも通常19〜20fps。一時的な表示FPS低下はあり、常時20fpsを保証するものではありません。
- Serialモニターを12秒閉じて再接続し、フレームカウンターが進み、約20fpsで撮影が継続することを確認しました。
- 実カメラ入力でCountが0から12まで増加し、`[DETECT] CAMERA MOTION`と確認音の再生開始ログを取得。取得エラー0。使用感改善の確認あり。
- 方向はUNKNOWNとなる入力もあり、実物での左右の正答率と実際の発音は個別に未測定です。
- 最終ログ: `build/cores3-fast-motion-usb-build.log`、`build/cores3-fast-motion-usb-upload.log`、`build/cores3-fast-motion-usb-serial.log`（Git対象外）。

## RUN案内の固定表示・明示的再開の修正（2026-10-17）

- 操作案内に2.5秒の期限を設け、その後は実状態の案内へ戻します。READY・PAUSED・追跡中・背景一致待ち・時間待ちを区別します。
- PAUSE→RUNで、カメラタスクから追跡と背景を明示的に初期化し、1.5秒の背景取得をやり直します。Serialの`p`も同じ動作です。
- 方向追跡テストに明示的再開でCOOLDOWNを解除するケースを追加し、既存の左右通過・重複抑止を含めて成功。
- `build-cores3.ps1 -Jobs 4`で最終ソースのビルド成功。その成果物をArduino CLIでCOM4へ書き込み、ハッシュ検証成功。
- Flash 628,635 bytes（47%）、静的RAM 43,352 bytes（13%）。
- 本体SerialからPAUSE→RUNを操作し、背景再取得ログ、2.5秒の案内終了、`[UI STATUS] READY: move left/right across view`への復帰を確認。
- 確認中のCount=0、errors=0、IDLEを維持。実際のLCD表示の目視、最終版での左右通過と発音は未確認。
- ログ: `build/cores3-resume-rearm-build.log`、`build/cores3-resume-rearm-upload.log`、`build/cores3-resume-rearm-serial.log`（Git対象外）。

## 方向判定版のビルド・実機ログ（2026-10-17）

- 最新版を`upload-cores3.ps1 -Port COM4`でビルド・書き込みし、ハッシュ検証成功。
- Flash 627,403 / 1,310,720 bytes（47%）、静的RAM 43,352 / 327,680 bytes（13%）。
- カメラ初期化ESP_OK、空きHeap=266,984 bytes、空きPSRAM=8,269,016 bytes。追加領域抽出・背景参照の固定領域は約6KBです。
- 初回の実物入力では追跡開始と重心139.5→55.5を取得したものの、曖昧判定によりUNKNOWNとなりました。また通過後の露出変化で背景差が残り、COOLDOWNから戻れない事象を確認しました。
- 対応として背景に明るさ・コントラスト補正を加え、重心の1フレーム許容移動を64pxへ調整し、曖昧判定理由のflagsログを追加しました。
- 最新版の約55秒のSerial取得では532フレーム付近まで継続。大半のChange=0.0%、IDLE、Count=0、errors=0を維持。単発変化では追跡を始めず、CHIME OFFタッチも受け付けました。
- 最新版の取得中に通過はなかったため、実物の左右方向の正答と通過後のCOOLDOWN→IDLE復帰は未確認です。実際の発音も未確認です。
- 左右方向・重複抑止・静止・背景補正は人工画像／座標列によるホストテストで確認済みです。実機精度の確認とは分けて扱ってください。
- 起動時の既存コアダンプ領域警告は引き続き出ていますが、その後の初期化・連続撮影は動作。保存領域は消去していません。
- 最終ログ: `build/cores3-direction-refined-upload.log`、`build/cores3-direction-refined-serial.log`（Git対象外）。

以下は以前の動体検知・人物分類・LTR-553/Web構成の記録です。

---

## WAIT STILL・静止時ノイズの修正確認（2026-10-17）

- 旧方式の1点サンプルでは固定・無人との報告時にもChangeが約13〜36%になっていました。
- 4×4画素平均に変更し、最初の待機で静止の連続確認を要求しないよう修正。
- 再待機はThresholdの半分ではなくThreshold未満3フレーム＋2秒待機へ変更。
- ホストテスト: 単一画素ノイズの抑制、ブロック全体の変化保持、色変換、起動時待機、Threshold未満の小さな変化での再待機を確認。
- 最終ビルド・COM4への書き込み・ハッシュ検証成功。Flash 616,607 bytes（47%）、静的RAM 37,200 bytes（11%）。
- 書き込み後約13秒のSerialログで112フレーム付近まで確認。Change=0.0%、ready=1、count=0、errors=0を維持。
- 修正後の実際の横切り・実際の発音・画面表示の目視は未確認。
- ログ: `build/cores3-motion-noise-upload.log`、`build/cores3-motion-noise-serial.log`（Git対象外）。

## 初版の動体検知の確認結果（2026-10-17）

- 最終ソースを`upload-cores3.ps1 -Port COM4`でビルド・書き込みし、ハッシュ検証成功。
- Flash 616,515 / 1,310,720 bytes（47%）、静的RAM 37,200 / 327,680 bytes（11%）。
- カメラ初期化ESP_OK、空きHeap 273,412 bytes、空きPSRAM 8,269,016 bytes。
- 最終Serial取得約11秒で92フレーム付近まで連続取得。errors=0。
- 実カメラの変化でCountが0→1→2、各回の`[DETECT] CAMERA MOTION`と`[SOUND] started=1`を確認。
- 画面に写っていた対象物の種類、意図した横切りとの一致、実際の発音、最終版のタッチ操作は未確認。
- 局所変化、一様な照明変化、平均輝度も変わる大きな物の変化、センサーノイズと、連続判定・待機・中断のホストテスト成功。
- 起動時の既存コアダンプ領域警告は継続するが、その後の撮影・検知は動作。保存領域の消去は実施していない。
- ログ: `build/cores3-camera-motion-final-upload.log`、`build/cores3-camera-motion-final-serial.log`（Git対象外）。

以下は以前の人物分類・LTR-553/Web構成の履歴です。現在の動体検知の操作には適用しません。

---

# 以前のカメラ人物分類版（履歴）

`cores3.ino`の`CORES3_DETECTION_ONLY=1`は`CameraOnly.h`を実行します。
内蔵GC0308カメラの画像をTensorFlow Lite Microで「人がいる／いない」に分類します。
LTR-553は初期化・読み取りとも行いません。Web、Wi-Fi、API、SD、LittleFS、NVSも使用しません。
動体検知や顔検出への置き換えではなく、学習済みperson_detectモデルによる画像全体の人物分類です。
人物の位置・人数・個人識別・移動方向は出力しません。画像は保存・外部送信しません。

## セットアップ

必要ライブラリ: M5Unified 0.2.26、M5GFX 0.2.33、Chirale_TensorFLowLite 2.0.0。
カメラドライバーはM5Stack ESP32ボードパッケージ3.3.10付属のesp_cameraを使用します。

```powershell
arduino-cli lib install 'Chirale_TensorFLowLite@2.0.0'
.\scripts\build-cores3.ps1 -Jobs 4
.\scripts\upload-cores3.ps1 -Port COM4
arduino-cli monitor -p COM4 --config baudrate=115200
```

既存のdefaultパーティションを使用し、保存領域を消去しません。
カメラはM5Unifiedの内部I2Cポートを共用し、バスを解放・再初期化しません。
タッチは動作確認済みのFT6336ポーリングを継続します。

## 最初の確認手順

1. 明るい室内で本体を固定し、内蔵カメラを人物へ向けます。
2. 起動後、公式の人物あり・なし画像で`[SELFTEST] ... PASS`を確認します。
3. カメラ映像が表示され、`NO PERSON`または`PERSON`へ進むことを確認します。
4. 黄色い枠がモデルの入力範囲です。その枠内に人物が写るよう距離・向きを調整します。
5. 人物スコアがThreshold以上で2フレーム続くとCountが増え、CHIME ONなら確認音を鳴らします。
6. 同じ人が写り続けている間は繰り返し鳴らしません。枠から離れ、低スコア3フレームと検知後10秒を満たしてから再度入ります。
7. 無人と人物ありで繰り返し、服装・明るさ・距離を変えて誤検知・見逃しを確認します。

初期Thresholdは70%。SENS +で5ポイント下げ、SENS -で5ポイント上げます（40〜95%）。
感度変更後・PAUSEからの再開後は、いったん人物が写らない状態にして再待機してください。
PAUSE/RUNは自動検知の切り替え、CHIMEは自動検知音の切り替え、TEST SOUNDは単独発音テスト、RESET COUNTは表示回数のリセットです。
Serialは`+`/`-`/`p`/`m`/`t`/`r`が対応します。設定・回数は再起動で初期値へ戻ります。
校正操作は不要です。スコアはモデルの出力であり、検知精度を保証する確率ではありません。
小さく写る人物、逆光、暗所、顔や身体が隠れる構図では見逃す可能性があります。

## カメラ版の確認結果（2026-10-17）

- `upload-cores3.ps1 -Port COM4`で最新ソースのビルド・書き込み・ハッシュ検証に成功。
- Flash 1,022,759 / 1,310,720 bytes（78%）、静的RAM 38,640 / 327,680 bytes（11%）。
- 本体自己テスト: 人物画像 person=113 / absent=-113、無人画像 person=-57 / absent=57で両方PASS（各366ms）。
- カメラ初期化ESP_OK。起動後空きHeap=94,948 bytes、空きPSRAM=8,269,016 bytes、Tensor arena実使用82,264 bytes。
- 約16秒のSerial取得でカメラ画像29フレームの推論を確認。約389〜392ms/回、errors=0、検知回数0。
- ホストの連続判定・静止保持・離脱・クールダウン・停止再開テスト成功。
- 起動時の既存コアダンプ領域に関する`Incorrect size of core dump image`ログは継続。その後は自己テスト・撮影・連続推論が正常に動作。保存領域の消去は実施していない。
- 実際の人物の出入りに対する精度、画面映像の向き・色、カメラ動作中のタッチ、実際の確認音は未確認。
- 最終ログ: `build/cores3-camera-person-fixed-upload.log`、`build/cores3-camera-person-fixed-serial.log`（Git対象外）。

## 実装とモデル出典

- `CameraOnly.h`: 画面・タッチ・確認音。推論中もUIを更新します。
- `CameraPersonDetector.h`: 別タスクで撮影・推論。160×120 RGB565から中央120×120を切り出し、96×96グレースケールint8へ変換します。
- `CameraPresenceGate.h`: 連続判定、離脱、クールダウン。ホストテストは`tests/cores3/camera_presence_test.cpp`。
- Tensor arenaは160KiB、カメラフレームとプレビューはPSRAM。LCD用の全面バッファは確保しません。
- `PersonModel.h`: [TensorFlow Lite Micro公式モデル](https://github.com/tensorflow/tflite-micro/blob/5736ea15e5f68c2c38acd797c90132ca891ff96a/tensorflow/lite/micro/models/person_detect.tflite)（300,568 bytes）。
- モデルSHA256: `808cfdfc0cf3a6fa6f6fa26bfa379ea97c16d5db7334637766e39c3408502e9d`。
- `PersonTestImages.h`: 同じリビジョンの公式person_detection/testdataのBMPの画素バイトをそのまま保持した起動時テスト入力（すでにint8入力として符号化されているため128を引きません）。
- モデル・テスト画像はApache-2.0、`LICENSE-PERSON-MODEL.txt`を同梱。
- [公式person_detectionの説明](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/person_detection)。

以下は以前のLTR-553/Web連携構成の記録です。現在のカメラモードの操作には適用しません。
`CORES3_DETECTION_ONLY=0`は以前のWeb連携版へ戻すための設定です。

---

# CoreS3 玄関音声デバイス

## 現在の既定: 検知専用（Webなし）

### 軽量化とタッチ取得の変更（2026-10-17）

画面用M5Canvas（約53KB）を廃止し、変更のある文字行だけLCDへ直接描画します。
検知専用モードはPSRAMの画面バッファを確保しません。起動時の`[MEM]`に空きヒープを表示します。
タッチはM5Unifiedの内部I2CでFT6336のデータを20ms周期で読み、M5GFXで画面座標へ変換します。
共有割り込み線の状態に依存するタッチ取得を避け、押した座標・actionを画面と`[TOUCH]`ログへ出します。
長押しによる連続実行は従来どおり抑止します。画面の最下行にはタッチ座標、感度変更時にはOffset変更を表示します。
容量超過が原因と確定したものではありません。
修正版はCOM4へ書き込み・ハッシュ検証成功。Flash 549,875 bytes（41%）、静的RAM 26,120 bytes（7%）。
起動時の空きHeap=330,552 bytes、空きPSRAM=8,384,940 bytes、タッチIC応答vendor=0x11を確認しました。
Serialで実入力`[TOUCH] Touch 260,182 action=9`を取得し、CALIBRATEの再実行とREADYへの復帰を確認しました。
全ボタン・実際の発音・人物の接近は未確認です。
ログは`build/cores3-detection-lite-upload.log`と`build/cores3-detection-lite-serial.log`です。


`cores3.ino`の`CORES3_DETECTION_ONLY=1`で、`DetectionOnly.h`だけを実行します。
Wi-Fi/API、LittleFS、NVS、保存音声は使用しません。既存保存データは消去しません。
内蔵LTR-553による近接検知で、人物専用の識別ではなく物体にも反応します。カメラは使用しません。
起動時は前面を空け、Cal:32/32とREADYを待って近づいてください。
検知時はCountが増え、1200Hz・300msの確認音を出します。離れて10秒後に再検知できます。
SENS + / SENS -、CALIBRATE、CHIME、TEST SOUND、RESETをタッチで操作できます。
Serial（115200 baud）では`+`/`-`/`c`/`t`/`r`が対応します。感度は再起動で150に戻ります。
30cmの検知可否と実際の発音は実機で確認してください。

```powershell
.\scripts\build-cores3.ps1 -Jobs 4
.\scripts\upload-cores3.ps1 -Port COM4
arduino-cli monitor -p COM4 --config baudrate=115200
```

Web連携版へ戻す場合は`CORES3_DETECTION_ONLY=0`に変更して再ビルドします。
検知専用版の実行記録（2026-10-17）:
- コンパイル成功: Flash 550,415 bytes（41%）、静的RAM 26,304 bytes（8%）。
- `upload-cores3.ps1 -Port COM4`で書き込み・ハッシュ検証成功。
- Serial: sensor=1、speaker=1、canvas=1、校正32/32、背景値2、Near=152、READYを確認。
- 確認中の生値は0〜4、検知回数0。Serialの`t`で`[SOUND] started=1`、再生後READYへの復帰を確認。
- 起動時に`esp_core_dump_flash: Incorrect size of core dump image`を一度出力。その後の起動・校正・連続測定は継続した。保存領域の消去は実施していない。
- 人の接近・30cmの検知・実際の発音・タッチ操作は未確認。
- ログ: `build/cores3-detection-only-{build,upload,serial}.log`（Git対象外）。

以下はWeb連携版の仕様・過去の検証記録です。


CoreS3専用です。Core/M5GO、Rails APIの契約は変更しません。microSD・カメラ・外付けセンサーは使用しません。
Web → Rails API ← CoreS3のポーリング構成で、Web終了後やWi-Fi切断中もLittleFSの音声とNVSの設定を使用します。

## セットアップ

必要環境: Arduino CLI、M5Stack ESP32 3.3.10、M5Unified 0.2.26、M5GFX 0.2.33、ArduinoJson 7.4.3。
LTR5XXはM5CoreS3公式ドライバーを同梱しています。M5CoreS3全体のインストールは不要です。

```powershell
arduino-cli lib install ArduinoJson
Copy-Item firmware/m5stack/cores3/secrets.example.h firmware/m5stack/cores3/secrets.h
# secrets.hのSSID、パスワード、RailsのLANアドレスを編集する
.\scripts\build-cores3.ps1
.\scripts\upload-cores3.ps1 -Port COM4 # 接続先を確認して指定
arduino-cli monitor -p COM4 --config baudrate=115200
```

`secrets.h`はGit対象外です。未作成でもビルドできますがWi-Fi接続には設定が必要です。
BASE_URLは末尾スラッシュなし、DEVICE_IDはURLで使える英数字とハイフンを使用してください。
localhostはCoreS3自身を指すので使用できません。RailsをLANから到達可能にし、Windowsのファイアウォールも確認してください。

FQBNは`m5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default`。
インストール済みdefault.csvを確認したLittleFS用spiffs領域はoffset `0x290000`、size `0x160000`（1,441,792 bytes）です。
二世代×2音声と一時ファイル（最大各200KiB）を保存できます。起動時に実容量と空きをSerial出力し、更新時にも空きを確認します。
パーティションを変更する場合、既存FSのアドレス・容量との互換性を確認してください。

LittleFSは`begin(false)`でマウントし、自動消去しません。未初期化の場合は画面FS:ERRとなります。
初回のみ、既存データが不要なことを確認して`Config::formatIfMountFails=true`で起動し、falseに戻して再ビルドしてください。
この操作はマウント失敗時にFSをフォーマットします。通常のファームウェア書き込みだけではFSは作成されません。
以前の`/welcome.wav`は自動移行しません。初回heartbeatで二つの音声を取得してください。

## 近接検知と操作

起動後約2.4秒（32連続有効サンプル）は前面を空け、環境値を取得します。75ms周期で測定し画面とSerialに表示します。
初期調整値は`Config.h`の`nearDelta=150`、`releaseDelta=70`（距離ではなく反射強度）です。
実測した背景値・手を近づけた値を見て調整してください。調整画面のSENS + / SENS -でオフセットを変更でき、NVSに保存します。閾値は背景中央値＋オフセットです。
起動中の変動が大きい、背景が飽和に近い場合はキャリブレーションエラーとし、前面を空けてCALIBRATEでやり直します。
起動時に静止物体を置いた場合はそれを背景として学習するため、設置環境での調整が必要です。

直近3値の中央値で単発ノイズを除き、生値とフィルター値の両方が閾値以上3回連続で1回検知します（通常の立ち上がりでは約225〜300ms）。再生中は検知せず、離れた値5回連続と10秒のクールダウンを満たして再待機します。
I2C失敗・無効データ、225ms超のサンプル間隔では連続判定とフィルターをリセットします。センサー故障時も手動再生と通信は動作します。

- MODE: ARRIVE / LEAVE切り替え。初回ARRIVE、変更はNVSに保存。
- TEST ARRIVE / TEST LEAVE: 保存済み音声を再生。APIの許可・音量を適用。イベントは生成しません。
- HEARTBEAT: 手動取得要求。通信タスクが順次処理します。
- SEND EVENT: 現在のモードでイベントを永続キューに追加。時計未同期なら拒否して画面に表示。
- CALIBRATE: 近接基準を再測定。

LTR-553は人専用検出器ではなく手や物体にも反応し、距離が限られます。玄関の通行検知を保証しません。
一つのセンサーで外出・帰宅の方向は識別できません。固定モードも方向認識の結果ではありません。
自動検知イベントは既定無効です。デモ時のみ`secrets.h`の`ENABLE_AUTO_EVENTS=true`で明示的に有効化してください。
その場合、音声無効・音源なしでも検知自体に対して現在のモードのイベントが作成されます。

## 通信・保存・再生

通信は別FreeRTOSタスクで実施します。Wi-Fiは15秒ごとに再接続、heartbeatは約20秒ごと、イベント再送は10秒ごとです。
HTTP接続2秒・読み取り3秒のタイムアウト、WAV転送全体15秒の制限があります。転送のためheartbeat開始が遅れることがあります。
設定のdevice_id、型、音量、音声version・サイズ・形式・相対pathを検証し、不正な応答は以前の設定を維持します。
使用するAPIはheartbeat、二つのaudio GET、eventsのみです。API相対pathは契約の同じdevice/slotと一致するものだけBASE_URLに結合します。
404 not_foundと422 validation_errorをSerialで区別します。イベントは200/201を成功とし、他の応答は同じID・UTC時刻のまま再送します。

音声は一時ファイルへストリーム保存し、サイズ・RIFF全長・全チャンク境界・PCM・mono・16kHz・16bit・5秒以内を検証します。
正常な場合だけ非アクティブ側ファイルへrenameし、NVSのbank/versionを一つのblobとして更新します。
同じversionの正常ファイルがあれば取得しません。ファイル欠落・破損時は同じversionでも再取得します。
転送失敗・検証失敗・NVS更新失敗や途中再起動でも、以前のアクティブ音声とversionを維持します。
削除・rename・NVS切り替え中の電源断、LittleFS自体の破損については実機耐障害試験が必要です。
ダウンロード中も再生可能です。FSアクセスをmutexで保護し、WAV全体をPSRAMへ読み込んでから再生します。
バッファは常時確保し、M5Unifiedの解放コールバックと再生完了の両方を確認してから次の再生に再利用します。
保存済み音源がない、不正、PSRAM不足の場合はエラー表示し再生しません。

設定・モード・音声参照先・イベントはNVS（Preferences）に保存します。イベントキューは最大24件です。
満杯・NVS不足は画面/Serialに表示し、新規追加を拒否します。既存イベントを上書きしません。
永続化後にだけ送信し、成功応答後に削除します。応答直後の再起動による再送はRailsのevent_id重複処理を利用します。
UUID相当の128bit乱数でIDを生成します。時計未同期のイベントは日時を捏造せず生成を拒否します（後からの時刻補完はしません）。

NTPをUTCで同期します。CoreS3内蔵RTCはNTP同期後にのみUTCを書き込み、信頼済みフラグと低電圧状態を確認して次回起動に利用します。
RTC保持電源切れ・初回オフライン時はNTPまでイベント生成できません。ローカル音声再生には時計は不要です。
NTPサーバーへ到達できるネットワークが必要です。

## ソース構成

`cores3.ino`は初期化/loop、`Config.h`は設定、`Storage.h`はFS/NVS、`ApiClient.h`はHTTPタスク、
`AudioManager.h`は再生、`WavValidation.h`はWAV検証、`ProximityDetector.h`は検知、
`EventManager.h`はイベントキュー、`DisplayManager.h`は画面とタッチです。
`LTR5XX.h/.cpp`と`LICENSE-LTR5XX.txt`は公式ドライバーです。
旧`audio_player.h`、`motion_detector.h`、`motion_score.h`、`pir_sensor.h`は旧実装の参考用で現スケッチから使用しません。

参考: [公式LTR-553サンプル](https://docs.m5stack.com/ja/arduino/m5cores3/ltr553)、
[公式Speaker API](https://docs.m5stack.com/ja/arduino/m5unified/speaker_class)、
[同梱ドライバーの取得元](https://github.com/m5stack/M5CoreS3/tree/main/src/utility)。

## 実機での確認

1. 起動ログ、FS容量、PSRAM、近接値、キャリブレーション、離した後のREADYを確認。
2. Wi-Fi/API接続、heartbeat設定、両音声の取得とversionを確認。
3. 手を近づけて一度だけ再生し、離さず待っても繰り返さないことを確認。
4. 離して10秒経過後の再検知、タッチ全操作、無効設定、音量0/100を確認。
5. Webで片方の音声だけ更新し、次回heartbeatで該当versionのみ取得することを確認。
6. 再起動とWi-Fi切断で音声・設定・モードが維持され、自動再生が続くことを確認。
7. 不正WAV、サイズ不一致、404/422、通信中断、容量不足でも以前の音声が残ることを確認。
8. 時計同期後にSEND EVENT、Rails停止中のキュー追加、再起動、復旧後再送と重複処理を確認。
9. 音声更新・イベント保存中の電源断とRTC電源喪失を確認。

コンパイル成功、書き込み成功、Serialログ、実際の発音・近接検知・タッチは別々に記録してください。
今回の実機・Rails接続試験は未実施です。

## ホスト検証（2026-10-17）

`tests/cores3/storage_test.cpp`と`tests/cores3/proximity_test.cpp`をZig 0.13.0のC++コンパイラーで実行して成功しました。
WAVのPCM形式・サンプルレート・深度・チャンク境界・5秒上限・ファイルサイズと、
近接のキャリブレーション・連続検知・離脱・クールダウン・再生中抑制・I2C失敗・再校正を検証しています。
これは疑似I2C値によるロジック検証で、実センサーや発音の検証ではありません。

```powershell
# g++でも実行できます。Zigの場合はg++をzig c++に置き換えます。
g++ -std=c++11 tests/cores3/storage_test.cpp -o build/cores3-wav-test.exe
.\build\cores3-wav-test.exe
g++ -std=c++11 tests/cores3/proximity_test.cpp -o build/cores3-proximity-test.exe
.\build\cores3-proximity-test.exe
```

イベントキューも`tests/cores3/event_test.cpp`で実際のEventManagerとArduinoJsonを使用し、
時計未同期拒否、UTC時刻、同一ID維持、疑似再起動、成功時の削除、NVS保存失敗、24件上限、不正キュー維持を検証して成功しました。
PreferencesとRTCはホスト側の模擬実装であり、実際の電源断に対するNVS耐久試験ではありません。

```powershell
$arduinoUser = (arduino-cli config get directories.user).Trim()
g++ -std=c++11 -I "$arduinoUser/libraries/ArduinoJson/src" -I tests/cores3/stubs tests/cores3/event_test.cpp -o build/cores3-event-test.exe
.\build\cores3-event-test.exe
```

## 最終ビルド結果（2026-10-17）

`./scripts/build-cores3.ps1 -Jobs 2`で最終ソースのコンパイル・リンクに成功しました。
Arduino CLI 1.5.1、M5Stack ESP32 3.3.10、M5Unified 0.2.26、M5GFX 0.2.33、ArduinoJson 7.4.3を使用しました。

- Flash: 1,219,227 / 1,310,720 bytes（93%）。今後機能を増やす際はアプリ領域残量に注意してください。
- 静的RAM: 49,536 / 327,680 bytes（15%）。WAVバッファと通信タスク等は実行時に追加確保します。
- FQBN: `m5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default`。
- ログ: `build/cores3-entrance-build.log`、成果物: `build/cores3/`（いずれもGit対象外）。
- ホストテスト: WAV、近接検知、イベントキューの3種類が成功。
- Core側のファームウェアは差分なし。Rails側も変更していません。
- COM4は列挙されましたが端末種別を確定していないため書き込み・Serial取得は実施していません。
- 実際の発音・LTR-553値・タッチ・Wi-Fi/Rails接続・再起動/切断・電源断テストは未実施です。
  実機試験完了とは扱わないでください。


## 30cmを目標にしたセンサー調整（Webなしで実施可能）

現在の初期画面はSENSOR TUNINGです。WAV・Wi-Fi・Rails・LittleFS音源がなくても、検知時に短い1200Hzの確認音を出します。
この音はAPIの音量・再生許可とは独立した診断音（音量128/255、300ms）です。CHIME ON/OFFで止められます。
調整画面では検知によるWAV再生・イベント生成を行いません。通信処理は継続します。
画面上端またはAPI UIで通常画面へ切り替えると、既存のモード別WAV再生に戻ります。
画面切り替え時は校正をやり直すため、前面を空けてください。調整画面と確認音ONは再起動時の初期状態です。

表示: Raw（生値）、Filter（3値中央値）、Peak（最大生値）、Base（背景中央値）、Noise（校正時の中央28値の幅）、
Offset（背景からの検知差分）、Near/Clear（検知/解除閾値）、Count（検知数）、Invalid（無効/取得失敗数）。
Serialは250msごとに値と状態、検知時に[DETECT]を出力します。距離をcmに変換する機能ではありません。

操作: SENS +は必要差分を下げて高感度化、SENS -は上げて誤検知を減らします（通常25刻み、Offsetが50以下では5刻み）。
調整範囲5〜1000。解除差分は検知差分に比例します。変更後は離脱確認をやり直し、NVSに保存します。
CALIBRATEは背景の再測定、TEST SOUNDは検知状態に関係なく診断音を鳴らします。回数・無効数・最大値はSerialのrでリセットできます。
校正は中央値と両端2値ずつを除いた変動幅を使用し、単発ノイズで失敗しにくくしています。
背景自動追従は行いません。長く立ち止まった人を背景として学習するのを避けるため、環境変化時は手動で再校正してください。

1. 設置予定の位置と角度に固定し、前方に何も置かずCALIBRATEを押す。
2. READY後にSerialでrを送信。誰も通らず60秒待ち、Countが増えないか、Raw/Filterの変動幅を確認する。
3. 定規などで本体前面から30cmの位置を決め、実際に着る服でそこへ近づく。
4. Countが1増えて確認音が鳴るか確認。反応しなければRaw/FilterとNearを比較する。
5. 背景との差が出ている場合はSENS +を1回ずつ押して調整。背景でも反応するならSENS -へ戻す。
6. その場に15秒立ち止まり、繰り返し鳴らないことを確認する。離れてREADYを待ち、再度近づく。
7. 同じ距離で10回試し、検知回数、無人時の誤検知、服装・照明・角度を記録する。
8. 昼/夜、明るい服/暗い服、20/30/40cmでも比較する。

30cmは目標距離であり、今回のコード変更で到達を保証するものではありません。
30cm地点の値が背景の変動に埋もれる場合は、閾値だけで安定検知することはできません。まず設置位置・向きを調整してください。
実機の測定値と発音は未確認です。新しいホストテストでは単発ノイズ、校正外れ値、測定間隔の中断、I2C失敗、
感度変更と統計リセットを含む検知ロジックが成功しています。

調整版の最終コンパイル・リンク成功: Flash 1,221,499 / 1,310,720 bytes（93%）、静的RAM 49,640 / 327,680 bytes（15%）。
実行コマンドは`./scripts/build-cores3.ps1 -Jobs 2`、ログは`build/cores3-proximity-tuning-final-build.log`です。
短い確認音でも再待機に離脱が必要になるよう、測定周期の間にも再生状態を確認します。
今回の書き込み・Serial取得・実際の確認音・30cmでの検知試験は未実施です。


## タッチ・校正・無音の修正

旧実装はALS_PS_STATUS（0x8C）のbit 1（PS割り込み状態）を新規測定値の判定に使っていました。
正しくはbit 0（PS data status）です。割り込みがない通常の測定値を捨てて校正が完了しない不具合を修正しました。
参照: [LTR-553ALS-WAデータシート23ページ](https://datasheet.lcsc.com/lcsc/1912111437_Lite-On-LTR-553ALS-WA_C408178.pdf)。
ホストテストも修正し、0x01だけが立っていても校正でき、0x02だけでは新規測定として受け付けないことを確認しました。

タッチは押し続けで同じ操作が繰り返されない方式に変更しました。
SENS操作後はOffsetの変更前後、保存結果、上下限または拒否理由を画面とSerialに表示します。
TEST SOUNDはセンサー・校正・WAV・APIとは独立してスピーカーを確認します。CHIME ON/OFFは近接検知時の音だけを切り替えます。
Speaker:ERRORならスピーカー初期化失敗、TEST SOUND failed/busyならSerialの[AUDIO]を確認してください。
音声APIの成功は実際の発音確認とは区別してください。

Serial Monitor（115200 baud）から、+で高感度、-で低感度、tでTEST SOUND、cで校正、rで統計リセットできます。
改行は無視します。タッチ時は[TOUCH]に座標とactionが出るため、タッチ受付と操作結果を切り分けられます。

修正版での確認順:
1. TEST SOUNDを押し、実際に300msの音が聞こえるか確認する。鳴らなければSerialからtでも試す。
2. SENS -でOffsetが増え、SENS +で減ることと、変更結果メッセージを確認する。
3. 前面を空けてCALIBRATE。約2.4秒の連続有効値でREADYへ進むことを確認する。
4. 近づいてCountの増加と診断音を確認する。NO SAMPLEが続く場合はRaw/InvalidとSerialを記録する。

今回のテストはホストでの操作マッピング・連続押下抑止・感度下限・レジスターbitの回帰テストです。
実機タッチ・実際の発音・30cmでの検知は未確認です。

修正後の最終ビルド成功: Flash 1,223,159 / 1,310,720 bytes（93%）、静的RAM 49,656 / 327,680 bytes（15%）。
コマンド: `./scripts/build-cores3.ps1 -Jobs 4`、ログ: `build/cores3-ps-status-fixed-build.log`。
タッチのホストテストと、正しいPS data statusによる校正・旧bitのみの拒否を含む近接ホストテストが成功しました。
画面にはCal: n/32で校正進捗を表示します。今回の書き込み・Serial取得・実際の発音確認は未実施です。

## 画面のちらつき対策

数値表示を更新するたびにLCD全体を黒く消去していた処理を廃止しました。
上部320×166ピクセルを8bitのM5Canvas（約53KB、PSRAM優先）で描き終えてからLCDへ転送します。
下部のボタンは初回と画面モード切り替え時だけ描画します。更新間隔は250msのままで、消去中の黒画面を表示しません。
設定の読み取り時だけStorageのmutexを取得し、描画・LCD転送中は保持しません。
PSRAM確保に失敗した場合は内部RAMを試し、両方失敗した場合はエラー表示とSerialログを出します。

書き込み後、数値の更新中に全画面が明滅しないこと、SENS操作の結果が表示されること、
API UIとの切り替えでボタンのラベルが正しく更新されることを実機で確認してください。
今回の変更は描画処理のみで、センサー閾値・検知ロジック・音声再生条件は変更していません。

ちらつき対策版のArduino CLIビルド成功: Flash 1,228,639 / 1,310,720 bytes（93%）、静的RAM 50,008 / 327,680 bytes（15%）。
Canvas約53KBは実行時に別途確保します。ログは`build/cores3-display-buffer-build.log`です。
書き込み・実機でのちらつき改善・タッチ操作の確認は今回未実施です。
