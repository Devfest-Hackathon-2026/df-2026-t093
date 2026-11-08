"""Flutter パッケージ(onnxruntime 1.4.1)同梱の ORT 1.15.1 でモデルが読めて同じ結果になるか確認する。
usage: uv run --no-project --python 3.11 --with onnxruntime==1.15.1 --with "numpy<2" --with soundfile python check_ort115.py
"""
import sys, numpy as np, onnxruntime as ort, soundfile as sf, glob
print("ORT", ort.__version__)
wavs = sorted(glob.glob("data/wav/*.wav"))[:3]
for m in sys.argv[1:] or ["models/ecapa_int8_static.onnx"]:
    s = ort.InferenceSession(m, providers=["CPUExecutionProvider"])
    for w in wavs:
        x, _ = sf.read(w, dtype="float32")
        e = s.run(None, {"wav": x[None, :64000]})[0]
        print(m, w.split("/")[-1], e[0, :4].round(4), "norm", float(np.linalg.norm(e)))
