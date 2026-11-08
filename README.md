# ECAPA-TDNN モバイル話者照合 PoC

SpeechBrain `spkrec-ecapa-voxceleb` を ONNX + INT8 に変換し、Flutter（ONNX Runtime）で
「登録音声と今の発話が同一人物か」をオンデバイスで判定する。

## 構成

```
export/                 Python: 変換・量子化・評価
  export_onnx.py        生波形→埋め込み を1本の ONNX に（Fbank もグラフに同梱）
  prepare_data.py       評価用 LibriSpeech test-clean 抽出（40話者×12発話）
  quantize_eval.py      INT8 量子化（dynamic / static QDQ）＋ EER・速度比較 → app/assets にコピー
  check_ort115.py       Flutter 同梱の ORT 1.15.1 でロードできるか確認
app/                    Flutter アプリ（Android / iOS）
  lib/speaker_verifier.dart  ORT 推論・コサイン類似度・PCM変換・簡易VAD
  lib/main.dart              登録（3回）→ 照合 UI、閾値スライダー
```

## 再生成手順

```bash
cd export
uv sync                       # torch 等で約1.3GB
uv run python export_onnx.py  # models/ecapa_fp32.onnx
uv run python prepare_data.py
uv run python quantize_eval.py
# 任意: 古い ORT での互換確認（新しい glibc では GLIBC_TUNABLES が必要）
GLIBC_TUNABLES=glibc.rtld.execstack=2 uv run --no-project --python 3.11 \
  --with onnxruntime==1.15.1 --with "numpy<2" --with soundfile python check_ort115.py

cd ../app
flutter run   # 実機推奨（マイクを使うため）
```

## 結果（LibriSpeech test-clean、キャリブ話者を除く 31話者433発話 / 先頭4秒、x86 CPU 1スレッド）

| model | size | EER | EER閾値 | fp32との cos | 3秒音声の推論 |
|---|---|---|---|---|---|
| fp32 | 84.1 MB | 0.80 % | 0.327 | 1.000 | 226 ms |
| int8 dynamic | 22.0 MB | 0.78 % | 0.334 | 0.995 | 320 ms |
| **int8 static (QDQ)** ← 採用 | 25.8 MB | 0.78 % | 0.329 | 0.996 | **46 ms** |

※ 評価セットは小さく、条件の良い英語の読み上げ音声。実運用では日本語・実機マイク・雑音環境での再評価と閾値調整が必須。

## ハマりどころ（対処済み）

- **ASP のマスク長計算まで量子化すると可変長入力で壊れる** → 量子化対象を Conv/MatMul に限定
- **ORT 1.31 の QDQ 融合が最終 fc Conv でクラッシュ** → そのノードだけ FP32 のまま
- **量子化ツールが付ける未使用 opset（ai.onnx.ml v5 等）で ORT 1.15 がロード拒否** → 未使用の opset 宣言を削除
- 前処理（DFT conv / log-mel）は FP32 のまま（量子化すると精度が落ちるため）
