#!/usr/bin/env python3
"""Mean CER over voices per (SNR, codec) from asr_codec_eval.py's JSON: asr_codec_report.py results.json"""
import collections
import json
import sys

rows = json.load(open(sys.argv[1]))
acc = collections.defaultdict(list)
for r in rows:
    acc[(r["snr"], r["codec"])].append((r["cer_ref"], r["cer_vs_pcm"]))
print("| SNR | codec | CER vs reference [%] | degradation vs PCM [pt] | voices |")
print("|---|---|---:|---:|---:|")
for (snr, codec), v in acc.items():
    n = len(v)
    print(f"| {'clean' if snr is None else f'{snr} dB'} | {codec} | {100 * sum(a for a, _ in v) / n:.1f} | "
          f"{100 * sum(b for _, b in v) / n:.1f} | {n} |")
