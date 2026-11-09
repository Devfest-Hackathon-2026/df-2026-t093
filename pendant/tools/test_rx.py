#!/usr/bin/env python3
"""Fixture tests for pendant_rx.py: python3 test_rx.py  (records and PCM come from the C library)."""
import os, struct, tempfile, wave
import pendant_rx as rx

here = os.path.dirname(os.path.abspath(__file__))
raw = open(os.path.join(here, "fixtures/sample.rec"), "rb").read()
want = struct.unpack("<%dh" % (os.path.getsize(os.path.join(here, "fixtures/sample.pcm")) // 2),
                     open(os.path.join(here, "fixtures/sample.pcm"), "rb").read())

recs = rx.load_dir([os.path.join(here, "fixtures/sample.rec")])
assert [h["seq"] for h, _ in recs] == [1, 2, 3, 4, 6], [h["seq"] for h, _ in recs]
assert [h["type"] for h, _ in recs] == [1, 1, 2, 3, 1]

# 1. Python decode equals the C decoder, sample for sample (audio records only).
pcm = []
for h, p in recs:
    if h["type"] == rx.AUDIO:
        pcm += rx.adpcm_decode(p)
assert tuple(pcm) == want, "ADPCM decode differs from C"

# 2. ... and equals audioop where it still exists (removed in Python 3.13).
try:
    import audioop
    for h, p in recs:
        if h["type"] == rx.AUDIO:
            sw = bytes(((b >> 4) | (b << 4)) & 255 for b in p)  # audioop puts the first sample in the high nibble
            ref = struct.unpack("<%dh" % (len(p) * 2), audioop.adpcm2lin(sw, 2, None)[0])
            assert tuple(rx.adpcm_decode(p)) == ref
except ImportError:
    pass

# 3. Corrupt, uncommitted and truncated records are refused.
one = raw[:rx.record_size(raw[:32])]
assert rx.parse_record(one)[0]["seq"] == 1
bad = bytearray(one); bad[40] ^= 1
assert rx.parse_record(bytes(bad)) == (None, "crc")
assert rx.parse_record(one[:-4] + b"\xff" * 4) == (None, "uncommitted")
assert rx.parse_record(one[:-1]) == (None, "truncated")

# 4. Receiver: in-order cumulative ACK, duplicates, holes, gap skipping.
r = rx.Rx(1)
by = {h["seq"]: (h, p) for h, p in recs}
assert r.feed(*by[2]) == "ahead" and r.acked == 0
assert r.feed(*by[1]) == "accept" and r.acked == 1
assert r.feed(*by[1]) == "dup" and r.acked == 1
assert r.feed(*by[2]) == "accept" and r.feed(*by[3]) == "accept" and r.acked == 3
assert r.feed(*by[6]) == "ahead" and r.acked == 3
assert r.feed(*by[4]) == "accept" and r.acked == 5          # the gap record covers 4..5
assert r.feed(*by[6]) == "accept" and r.acked == 6

# 5. Fragments in any chunking reassemble; a hole asks for a resend from the right offset.
a = rx.Reassembler()
out = None
for off in range(0, len(one), 50):
    out, resume = a.add(1, off, one[off:off + 50])
    assert resume is None
assert out == one
a = rx.Reassembler()
a.add(1, 0, one[:50])
assert a.add(1, 100, one[100:150]) == (None, 50)           # fragment at 50 was lost

# 6. Timeline: times before the clock was set come from the timefix, the gap is reported,
#    silence is inserted only for real time gaps.
pcm, events, t0 = rx.build(recs)
assert t0 == 1760000000000 - (1300 - 1000), t0           # record 1 started 300 ms before the timefix
assert any("4..5" in e for e in events), events
d = tempfile.mkdtemp()
rx.write_wav(os.path.join(d, "t.wav"), pcm)
w = wave.open(os.path.join(d, "t.wav"))
assert w.getframerate() == 8000 and w.getnchannels() == 1 and w.getnframes() == len(pcm)

# 7. The RTC was wound back mid-boot: records are still ordered by sequence, audio is contiguous
#    (silence is computed from the monotonic counter, not from rtc_ms), and nothing is reported as lost.
back = rx.load_dir([os.path.join(here, "fixtures/backwards.rec")])
assert [h["seq"] for h, _ in back] == [1, 2, 3, 4]
assert back[1][0]["rtc_ms"] < back[0][0]["rtc_ms"]  # the clock really did go backwards
pcm, events, t0 = rx.build(back)
assert len(pcm) == 3 * 800 and not events, (len(pcm), events)

print("test_rx: all tests passed")
