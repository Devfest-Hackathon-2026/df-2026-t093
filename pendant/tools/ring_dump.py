#!/usr/bin/env python3
"""Reads a raw ring image (pyocd savemem of the slot1 partition) and writes its records as
.rec files for pendant_rx.py decode. Independent of the C code: same layout, docs/spec/storage.md §5.

  pyocd cmd -t nrf54l -c "savemem 0xc2000 729088 ring.bin"
  ring_dump.py ring.bin out_dir [--block 4096 --page 256]
"""
import argparse, os, struct, zlib
import pendant_rx as rx

def meta(img, slot, block):
    m = img[slot * block: slot * block + 36]
    if m[:4] != b"LOSM" or struct.unpack_from("<I4s", m, 28) != (zlib.crc32(m[:28]), b"CMT!"):
        return None
    gen, tp_lo, tp_hi, tail, acked, boot = struct.unpack_from("<IIIIII", m, 4)
    return dict(gen=gen, tp=tp_lo | tp_hi << 32, tail=tail, acked=acked, boot=boot)

def walk(img, block, page, maxrec=70000):
    ms = [m for m in (meta(img, 0, block), meta(img, 1, block)) if m]
    if not ms:
        raise SystemExit("no valid metadata")
    m = max(ms, key=lambda x: x["gen"])
    data = img[2 * block:]
    R = len(data) - len(data) % block
    probe = maxrec // block + 3

    def rec_at(x, seq):
        o = x % R
        h = data[o:o + 32]
        if len(h) < 32 or h[:4] != rx.MAGIC or h[4] != 1:
            return None
        s, ln = struct.unpack_from("<I", h, 8)[0], struct.unpack_from("<I", h, 28)[0]
        size = 32 + ln + 8
        if s != seq or ln % 4 or o + (size + page - 1) // page * page > R:
            return None
        r = data[o:o + size]
        hh, _ = rx.parse_record(r)
        return (r, (size + page - 1) // page * page) if hh else None

    out, e, seq = [], m["tp"], m["tail"]
    while True:
        found = None
        c = e
        for j in range(probe):
            found = rec_at(c, seq)
            if found:
                break
            c = (e // block + 1) * block if j == 0 else c + block
        if not found:
            return m, out
        out.append(found[0])
        e, seq = c + found[1], seq + 1

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("image"); ap.add_argument("out")
    ap.add_argument("--block", type=int, default=4096); ap.add_argument("--page", type=int, default=256)
    a = ap.parse_args()
    m, recs = walk(open(a.image, "rb").read(), a.block, a.page)
    os.makedirs(a.out, exist_ok=True)
    for r in recs:
        h, _ = rx.parse_record(r)
        open(os.path.join(a.out, f"{h['seq']:08d}.rec"), "wb").write(r)
    print(f"meta gen {m['gen']} boot {m['boot']} tail {m['tail']} acked {m['acked']}; {len(recs)} records"
          + (f", seq {rx.parse_record(recs[0])[0]['seq']}..{rx.parse_record(recs[-1])[0]['seq']}" if recs else ""))
