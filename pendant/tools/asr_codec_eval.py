#!/usr/bin/env python3
"""Codec-only ASR proxy (docs/spec/audio.md §7.4, issue #19): Japanese TTS -> 8 kHz -> noise -> codec -> Whisper -> CER.

Needs macOS `say`, ffmpeg, a local Whisper checkpoint and tools/ima_roundtrip built against fw/lib/ima_adpcm:
  cc -O2 -I../fw/lib/ima_adpcm -o ima_roundtrip ima_roundtrip.c ../fw/lib/ima_adpcm/ima_adpcm.c
  asr_codec_eval.py --ima ./ima_roundtrip --model ~/.cache/whisper/large-v3-turbo.pt --out results.json
Synthetic speech and synthetic noise: this measures what the codec does, not what a worn microphone hears.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import unicodedata

import numpy as np
from scipy.signal import resample_poly

SENTENCES = [
    "今日は朝から雨が降っていたので、傘を持って駅まで歩きました。",
    "来週の火曜日までに、見積書を送ってもらえますか。",
    "コンビニでお茶とおにぎりを買ってから会社に向かいます。",
    "昨日の会議で決まったことを、あとでメールにまとめておきます。",
    "電車が遅れているようなので、十分ほど早めに家を出ます。",
    "新しいカメラの使い方がよく分からなくて、説明書を読んでいます。",
    "週末は友達と一緒に、近くの公園でバーベキューをする予定です。",
    "この資料は三枚目の表だけを修正すれば大丈夫だと思います。",
    "夕飯はカレーにしようかと思っているけれど、何か食べたいものはある。",
    "駅前の本屋で、前から探していた本をやっと見つけました。",
    "明日の天気予報では、午後から晴れると言っていました。",
    "お願いしていた荷物は、今日の夕方までに届く予定です。",
]
VOICES = ["Kyoko", "Reed (日本語（日本）)", "Eddy (日本語（日本）)"]
SNRS = [None, 20, 10, 5]
CODECS = ["pcm", "ima4", "g726-24", "g726-16"]
RATE = 8000


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, **kw)


def tts(voice, text, tmp):
    aiff = os.path.join(tmp, "t.aiff")
    run(["say", "-v", voice, "-o", aiff, text])
    raw = run(["ffmpeg", "-loglevel", "error", "-y", "-i", aiff, "-ar", str(RATE), "-ac", "1", "-f", "s16le", "-"]).stdout
    return np.frombuffer(raw, dtype="<i2").astype(np.float64)


def make_speech(voice, tmp):
    gap = np.zeros(int(0.8 * RATE))
    parts = []
    for s in SENTENCES:
        parts += [tts(voice, s, tmp), gap]
    return np.concatenate(parts)


def pink(n, rng):
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.arange(len(spec)); f[0] = 1
    x = np.fft.irfft(spec / np.sqrt(f), n)
    return x / np.sqrt(np.mean(x ** 2))


def active_rms(x, frame=200):
    e = np.array([np.mean(x[i:i + frame] ** 2) for i in range(0, len(x) - frame, frame)])
    return np.sqrt(np.mean(e[e > e.max() * 1e-4]))


def add_noise(x, snr, seed):
    if snr is None:
        return x
    n = pink(len(x), np.random.default_rng(seed))
    return np.clip(x + n * active_rms(x) / 10 ** (snr / 20), -32768, 32767)


def codec(x, name, ima, tmp):
    pcm = np.round(x).astype("<i2").tobytes()
    if name == "pcm":
        return x
    if name == "ima4":
        return np.frombuffer(run([ima], input=pcm).stdout, dtype="<i2").astype(np.float64)[: len(x)]
    bits = {"g726-24": 3, "g726-16": 2}[name]
    enc = run(["ffmpeg", "-loglevel", "error", "-y", "-f", "s16le", "-ar", str(RATE), "-ac", "1", "-i", "-",
               "-c:a", "g726", "-b:a", f"{bits * 8}k", "-f", "g726", "-"], input=pcm).stdout
    dec = run(["ffmpeg", "-loglevel", "error", "-y", "-f", "g726", "-code_size", str(bits), "-ar", str(RATE),
               "-ac", "1", "-i", "-", "-f", "s16le", "-"], input=enc).stdout
    return np.frombuffer(dec, dtype="<i2").astype(np.float64)[: len(x)]


def norm(s):
    s = unicodedata.normalize("NFKC", s)
    return "".join(c for c in s if unicodedata.category(c)[0] in "LN")


def cer(ref, hyp):
    r, h = norm(ref), norm(hyp)
    d = list(range(len(h) + 1))
    for i, rc in enumerate(r, 1):
        prev, d[0] = d[0], i
        for j, hc in enumerate(h, 1):
            prev, d[j] = d[j], min(d[j] + 1, d[j - 1] + 1, prev + (rc != hc))
    return d[len(h)] / max(len(r), 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ima", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--voices", type=int, default=len(VOICES))
    ap.add_argument("--device", default="cpu")
    a = ap.parse_args()
    import torch
    import whisper
    model = whisper.load_model(a.model, device=a.device)
    ref = "".join(SENTENCES)
    results = []
    with tempfile.TemporaryDirectory() as tmp:
        for vi, voice in enumerate(VOICES[: a.voices]):
            speech = make_speech(voice, tmp)
            for snr in SNRS:
                noisy = add_noise(speech, snr, 1000 + vi)
                base = None
                for c in CODECS:
                    y = codec(noisy, c, a.ima, tmp)
                    audio = resample_poly(y / 32768.0, 2, 1).astype(np.float32)  # Whisper wants 16 kHz
                    with torch.no_grad():
                        text = whisper.transcribe(model, audio, language="ja", temperature=0.0, fp16=False,
                                                  condition_on_previous_text=False)["text"]
                    base = text if c == "pcm" else base
                    r = {"voice": voice, "snr": snr, "codec": c, "cer_ref": cer(ref, text),
                         "cer_vs_pcm": cer(base, text), "text": text}
                    results.append(r)
                    print(f"{voice[:6]:6s} snr={snr!s:4s} {c:8s} ref={r['cer_ref']:.3f} vs_pcm={r['cer_vs_pcm']:.3f}",
                          flush=True)
                    json.dump(results, open(a.out, "w"), ensure_ascii=False, indent=1)


if __name__ == "__main__":
    sys.exit(main())
