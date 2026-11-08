"""ecapa_fp32.onnx を INT8 量子化し、話者照合精度(EER)・サイズ・速度を比較する。

  * dynamic : 重みのみ事前量子化、活性は実行時にスケール計算。キャリブ不要。
  * static  : QDQ 形式・per-channel。少量の音声でキャリブレーション。
              ARM の XNNPACK / NNAPI / CoreML で INT8 カーネルが効きやすいのはこちら。
  どちらも前処理(Fbank)部分は精度劣化を避けるため FP32 のまま残す。

usage: uv run python quantize_eval.py
"""

import itertools
import random
import shutil
import time
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
import soundfile as sf
from onnxruntime.quantization import (
    CalibrationDataReader,
    CalibrationMethod,
    QuantFormat,
    QuantType,
    quantize_dynamic,
    quantize_static,
)
from onnxruntime.quantization.shape_inference import quant_pre_process

M = Path("models")
FP32, PRE = M / "ecapa_fp32.onnx", M / "ecapa_pre.onnx"
DYN, STA = M / "ecapa_int8_dynamic.onnx", M / "ecapa_int8_static.onnx"
WAVS = sorted(Path("data/wav").glob("*.wav"))
SEG = 16000 * 4  # 評価は先頭4秒（短い発話を想定）


def load(p, n=SEG):
    x, _ = sf.read(p, dtype="float32")
    return x[:n][None]


def frontend_nodes(path):
    g = onnx.load(str(path)).graph
    return [n.name for n in g.node if not n.name.startswith("/ecapa/")]


class Reader(CalibrationDataReader):
    def __init__(self, files):
        self.it = iter([{"wav": load(f, 16000 * 3)} for f in files])  # Percentile 集計は同一長が必要

    def get_next(self):
        return next(self.it, None)


def quantize():
    quant_pre_process(str(FP32), str(PRE), skip_symbolic_shape=True)
    excl = frontend_nodes(PRE)
    print(f"[quant] FP32 のまま残す前処理ノード数: {len(excl)}")

    quantize_dynamic(str(PRE), str(DYN), weight_type=QuantType.QInt8, per_channel=True,
                     op_types_to_quantize=["Conv", "MatMul"], nodes_to_exclude=excl)

    # キャリブ: 評価に使う話者とは別の10話者×2発話
    by_spk = {}
    for f in WAVS:
        by_spk.setdefault(f.name.split("_")[0], []).append(f)
    spks = sorted(by_spk)
    calib = [f for s in spks[-10:] for f in by_spk[s][:2]]
    quantize_static(str(PRE), str(STA), Reader(calib), quant_format=QuantFormat.QDQ,
                    per_channel=True, activation_type=QuantType.QUInt8, weight_type=QuantType.QInt8,
                    calibrate_method=CalibrationMethod.Percentile,
                    extra_options={"CalibPercentile": 99.99},
                    # ORT 1.31 の QDQ 融合が最終 fc Conv(Pad 経由入力)でクラッシュするため FP32 のまま残す
                    nodes_to_exclude=excl + ["/ecapa/fc/conv/Conv"],
                    # ASP のマスク長計算(Mul 等)まで量子化すると可変長入力で壊れるので Conv/MatMul に限定
                    op_types_to_quantize=["Conv", "MatMul"])
    for p in (DYN, STA):
        strip_unused_opsets(p)
    return set(spks[-10:])


def strip_unused_opsets(path):
    """量子化ツールが付ける未使用 opset 宣言(ai.onnx.ml v5 等)を削除する。
    残っていると Flutter パッケージ同梱の古い ORT(1.15) がロードを拒否する。"""
    m = onnx.load(str(path))
    used = {n.domain for n in m.graph.node} | {""}
    keep = [o for o in m.opset_import if o.domain in used]
    del m.opset_import[:]
    m.opset_import.extend(keep)
    onnx.save(m, str(path))


def eer(scores, labels):
    order = np.argsort(-scores)
    s, l = scores[order], labels[order]
    tp = np.cumsum(l); fp = np.cumsum(1 - l)
    fnr = 1 - tp / l.sum(); fpr = fp / (1 - l).sum()
    i = np.argmin(np.abs(fnr - fpr))
    return (fnr[i] + fpr[i]) / 2 * 100, s[i]


def embed_all(path, files):
    so = ort.SessionOptions(); so.intra_op_num_threads = 4
    sess = ort.InferenceSession(str(path), so, providers=["CPUExecutionProvider"])
    return np.concatenate([sess.run(None, {"wav": load(f)})[0] for f in files])


def latency(path, sec=3.0, threads=1, n=10):
    so = ort.SessionOptions(); so.intra_op_num_threads = threads
    sess = ort.InferenceSession(str(path), so, providers=["CPUExecutionProvider"])
    w = (np.random.randn(1, int(16000 * sec)) * 0.1).astype(np.float32)
    sess.run(None, {"wav": w})
    t = time.perf_counter()
    for _ in range(n):
        sess.run(None, {"wav": w})
    return (time.perf_counter() - t) / n * 1000


def main():
    calib_spk = quantize()
    files = [f for f in WAVS if f.name.split("_")[0] not in calib_spk]  # キャリブ話者を除外
    spk = np.array([f.name.split("_")[0] for f in files])
    pairs = list(itertools.combinations(range(len(files)), 2))
    i, j = map(np.array, zip(*pairs))
    labels = (spk[i] == spk[j]).astype(np.float64)
    print(f"[eval] {len(set(spk))} 話者 / {len(files)} 発話 / 試行 {len(pairs)} "
          f"(target {int(labels.sum())}, non-target {int((1 - labels).sum())})")

    ref = None
    print(f"\n{'model':<22}{'size MB':>9}{'EER %':>8}{'thr':>7}{'cos vs fp32':>13}{'ms/3s(1th)':>12}")
    for name, p in [("fp32", FP32), ("int8 dynamic", DYN), ("int8 static (QDQ)", STA)]:
        e = embed_all(p, files)
        ref = e if ref is None else ref
        sc = (e[i] * e[j]).sum(1)
        er, thr = eer(sc, labels)
        agree = (e * ref).sum(1).mean()
        print(f"{name:<22}{p.stat().st_size / 1e6:>9.1f}{er:>8.2f}{thr:>7.3f}{agree:>13.4f}{latency(p):>12.1f}")

    dst = Path("../app/assets/models/ecapa_int8.onnx")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(STA, dst)
    print(f"\n[copy] {STA} -> {dst}")


if __name__ == "__main__":
    random.seed(0)
    main()
