"""SpeechBrain ECAPA-TDNN を「生波形 → 埋め込み」1本の ONNX にエクスポートする。

ポイント:
  * Fbank(STFT + Mel + log) を Conv1d / MatMul だけで再実装してグラフに同梱する。
    -> アプリ側(Dart)で特徴量抽出を書かなくてよく、前処理の不一致による精度劣化を防げる。
    -> STFT op を使わないので ORT Mobile / XNNPACK / NNAPI / CoreML どれでも動きやすい。
  * 入力:  wav  float32 [1, T]  (16kHz, -1..1)
    出力:  emb  float32 [1, 192] (L2 正規化済み。内積 = コサイン類似度)

usage: uv run python export_onnx.py
"""

import math
from pathlib import Path

import numpy as np
import onnxruntime as ort
import torch
import torch.nn as nn
import torch.nn.functional as F
from speechbrain.inference.speaker import EncoderClassifier

OUT = Path("models")
OUT.mkdir(exist_ok=True)


class OnnxFriendlyFbank(nn.Module):
    """speechbrain.lobes.features.Fbank(n_mels=80) と数値的に等価な実装。"""

    def __init__(self, fbank_matrix: torch.Tensor, n_fft=400, hop=160, top_db=80.0):
        super().__init__()
        self.n_fft, self.hop, self.top_db = n_fft, hop, top_db
        n_bins = n_fft // 2 + 1
        win = torch.hamming_window(n_fft)  # SpeechBrain の既定 (periodic)
        n = torch.arange(n_fft, dtype=torch.float64)
        k = torch.arange(n_bins, dtype=torch.float64)[:, None]
        ang = 2 * math.pi * k * n / n_fft
        real = (torch.cos(ang) * win).float()
        imag = (-torch.sin(ang) * win).float()
        # [2*n_bins, 1, n_fft] の DFT カーネル
        self.register_buffer("dft", torch.cat([real, imag], 0)[:, None, :])
        self.register_buffer("fb", fbank_matrix.float())  # [n_bins, n_mels]

    def forward(self, wav):  # wav: [B, T]
        x = F.pad(wav[:, None, :], (self.n_fft // 2, self.n_fft // 2))  # center=True, constant
        spec = F.conv1d(x, self.dft, stride=self.hop)  # [B, 2*bins, frames]
        re, im = spec.chunk(2, dim=1)
        power = (re * re + im * im).transpose(1, 2)  # [B, frames, bins]
        mel = power @ self.fb  # [B, frames, 80]
        db = 10.0 * torch.log10(torch.clamp(mel, min=1e-10))
        floor = db.amax(dim=(1, 2), keepdim=True) - self.top_db
        db = torch.maximum(db, floor)
        return db


class WavToEmbedding(nn.Module):
    def __init__(self, sb: EncoderClassifier):
        super().__init__()
        fbm = sb.mods.compute_features.compute_fbanks
        fbm.log_mel = False
        eye = torch.eye(201)[None]  # 単位行列を通して Mel 行列そのものを取り出す
        with torch.no_grad():
            fbank_matrix = fbm(eye)[0]
        fbm.log_mel = True
        self.fbank = OnnxFriendlyFbank(fbank_matrix)
        self.ecapa = sb.mods.embedding_model

    def forward(self, wav):
        feats = self.fbank(wav)
        feats = feats - feats.mean(dim=1, keepdim=True)  # InputNormalization(sentence, std_norm=False)
        emb = self.ecapa(feats).squeeze(1)  # [B, 192]
        return F.normalize(emb, dim=-1)


def main():
    sb = EncoderClassifier.from_hparams(
        source="speechbrain/spkrec-ecapa-voxceleb", savedir="pretrained/ecapa", run_opts={"device": "cpu"}
    )
    sb.eval()
    model = WavToEmbedding(sb).eval()

    # --- 元の SpeechBrain 推論と一致するか確認 ---
    torch.manual_seed(0)
    wav = torch.randn(1, 16000 * 3) * 0.1
    with torch.no_grad():
        ref = F.normalize(sb.encode_batch(wav).squeeze(1), dim=-1)
        ours = model(wav)
    print(f"[check] torch re-impl vs speechbrain cos = {(ref * ours).sum().item():.6f}")

    # --- ONNX エクスポート ---
    path = OUT / "ecapa_fp32.onnx"
    torch.onnx.export(
        model,
        (wav,),
        str(path),
        input_names=["wav"],
        output_names=["emb"],
        dynamic_axes={"wav": {1: "samples"}, "emb": {0: "batch"}},
        opset_version=17,
        dynamo=False,
    )
    print(f"[export] {path}  {path.stat().st_size / 1e6:.1f} MB")

    # --- ORT で検証（長さを変えて動的軸もテスト） ---
    sess = ort.InferenceSession(str(path), providers=["CPUExecutionProvider"])
    for sec in (1.5, 3, 7):
        w = torch.randn(1, int(16000 * sec)) * 0.1
        with torch.no_grad():
            t = model(w).numpy()
        o = sess.run(None, {"wav": w.numpy()})[0]
        print(f"[ort] {sec:>4}s  cos(torch, onnx) = {float((t * o).sum()):.6f}  max|diff|={np.abs(t - o).max():.2e}")


if __name__ == "__main__":
    main()
