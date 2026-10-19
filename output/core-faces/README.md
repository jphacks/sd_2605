# Core face previews

`firmware/m5stack/core/faces.h` の描画形状をもとにしたホスト側の画像プレビューです。実機の描画とピクセル単位で一致することや、実機でのアニメーション動作を保証するものではありません。

- `core-face-{pink,yellow,blue}.png`: 通常時の顔（320×240）。
- `core-face-{pink,yellow,blue}.gif`: まばたき、驚き、歌う顔、笑顔、固有表情、眠い顔を約8秒でループ（320×240）。
- `core-faces.png` / `core-faces.gif`: 3種類を横に並べた一覧（960×240）。

GIFの再生成（Python、Pillowが必要）:

```powershell
python output/core-faces/render_gifs.py
```

検証: 上記コマンドでGIFを生成し、4ファイルすべてのサイズ、フレームの読み込み、総再生時間8430ms、無限ループ設定を確認。代表フレームを画像で目視確認。ファームウェアのビルド・書き込み・実機確認はこの画像出力では実施していません。
