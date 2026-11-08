"""評価・キャリブレーション用に LibriSpeech test-clean (約350MB) から 40話者×12発話(3秒以上) を抽出する。
usage: uv run python prepare_data.py
"""
import collections
import io
import os

import pyarrow.parquet as pq
import soundfile as sf
from huggingface_hub import hf_hub_download

p = hf_hub_download("openslr/librispeech_asr", "clean/test/0000.parquet", repo_type="dataset")
os.makedirs("data/wav", exist_ok=True)
count = collections.Counter()
for batch in pq.ParquetFile(p).iter_batches(batch_size=64, columns=["audio", "speaker_id", "id"]):
    for r in batch.to_pylist():
        spk = r["speaker_id"]
        if count[spk] >= 12:
            continue
        x, sr = sf.read(io.BytesIO(r["audio"]["bytes"]), dtype="float32")
        if len(x) < 16000 * 3:
            continue
        sf.write(f"data/wav/{spk}_{r['id']}.wav", x, sr)
        count[spk] += 1
print(f"{len(count)} speakers, {sum(count.values())} utterances")
