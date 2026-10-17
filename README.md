# サンプル（プロダクト名）

[![IMAGE ALT TEXT HERE](https://jphacks.com/wp-content/uploads/2026/07/IMG_5316.png)](https://www.youtube.com/watch?v=piaNsc6ilBI)

## 製品概要
### 背景(製品開発のきっかけ、課題等）
### 製品説明（具体的な製品の説明）
### 特長
#### 1. 特長1
#### 2. 特長2
#### 3. 特長3

### 解決出来ること
### 今後の展望
### 注力したこと（こだわり等）
* 
* 

## 開発技術
### 活用した技術
#### API・データ
* 
* 

#### フレームワーク・ライブラリ・モジュール
* 
* 

#### デバイス
* 
* 

### 独自技術
#### ハッカソンで開発した独自機能・技術
* 独自で開発したものの内容をこちらに記載してください
* 特に力を入れた部分をファイルリンク、またはcommit_idを記載してください。

## Entrance Voice 開発

- `apps/`: [sd_2605-mobile](https://github.com/jphacks/sd_2605-mobile) の `main` をgit subtree（履歴を保持）で取り込んだWebアプリ。更新は `git subtree pull --prefix=apps https://github.com/jphacks/sd_2605-mobile.git main` を使用します（作業ツリーがクリーンな状態で実行）。
- `firmware/m5stack/core/`: M5Stack CoreのPIR連動音声再生。`core.ino`が画面と状態制御、`audio_player.h`が再生、`storage.h`がSD/WAV読み込み、`pir_sensor.h`がPIR入力です。
- `firmware/m5stack/cores3/`: 内蔵カメラによる動体検知・左右方向判定・PIR照合・LittleFS音声再生・タッチ操作を維持し、Render向けHTTPS音声同期を接続しています（LTR-553は不使用）。Render本番でheartbeat・leave/arriveのWAV取得を確認済みです。[使い方](firmware/m5stack/cores3/README.md)。
- `api_client.h`と`secrets.example.h`: 将来のWeb連携用。CoreはSD、CoreS3はLittleFSを使用。CoreS3のWi-Fi・Rails APIは専用スケッチ内に実装しています。
- [ハードウェアと動作確認手順](docs/hardware.md)
- [開発環境と書き込み](docs/development.md)
- [M5Stack・Rails API インターフェース仕様](docs/m5stack-api.md)

```powershell
.\scripts\build-core.ps1
.\scripts\upload-core.ps1 -Port COM5
```
