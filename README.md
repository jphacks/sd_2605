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

- `apps/`: Webアプリ用（未実装）。
- `firmware/m5stack/core/`: M5Stack CoreのPIR連動音声再生。`core.ino`が画面と状態制御、`audio_player.h`が再生、`storage.h`がSD/WAV読み込み、`pir_sensor.h`がPIR入力です。
- `firmware/m5stack/cores3/`: CoreS3専用の内蔵カメラ動体検知・LittleFS WAV・タッチ操作（外付けセンサー不要）。[使い方と音源配置](firmware/m5stack/cores3/README.md)。
- `api_client.h`と`secrets.example.h`: 将来のWeb連携用。CoreはSD、CoreS3はLittleFSを使用。Wi-Fi・Web APIは未実装です。
- [ハードウェアと動作確認手順](docs/hardware.md)
- [開発環境と書き込み](docs/development.md)

```powershell
.\scripts\build-core.ps1
.\scripts\upload-core.ps1 -Port COM5
```
