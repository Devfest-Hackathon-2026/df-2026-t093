#!/usr/bin/env python3
"""Desktop receiver for the lifeOS Pendant (issue #6). Dev tool, not the phone app.

  pendant_rx.py sync  [--name LifeOS-Pendant] [--out rx] [--set-time]   connect, sync, ACK, then decode
  pendant_rx.py follow [--every 60] [--out rx]                          sync forever (wear-it mock); decode later
  pendant_rx.py decode DIR_OR_FILE... [--wav out.wav]                   decode saved records only

Wire format: docs/spec/formats.md §6, §9-§11 and fw/src/ble.h. Standard library only, plus bleak for `sync`.
"""
import argparse, json, os, struct, sys, time, wave, zlib

HDR, TRL = 32, 8
MAGIC, COMMIT = b"LOSC", b"CMT!"
AUDIO, TIMEFIX, GAP, EVENT = 1, 2, 3, 4
EVENT_NAMES = {1: "button press", 2: "AAD wake", 3: "AAD sleep", 4: "reset", 5: "privacy ON", 6: "audio drop"}
RESET_CAUSES = {1: "power-on", 2: "pin", 3: "WATCHDOG", 4: "BROWNOUT", 5: "software", 6: "other"}
F_RTC_INVALID = 1
RATE = 8000
SVC = "a3c1f000-2b6b-4d0c-9f6e-6c6966654f53"
DATA = "a3c1f001-2b6b-4d0c-9f6e-6c6966654f53"
CTRL = "a3c1f002-2b6b-4d0c-9f6e-6c6966654f53"
INFO = "a3c1f003-2b6b-4d0c-9f6e-6c6966654f53"

# ---------------- records (docs/spec/formats.md §2) ----------------

def parse_record(b):
    """Returns (hdr dict, payload) for a complete committed record, else (None, reason)."""
    if len(b) < HDR:
        return None, "short"
    if b[:4] != MAGIC or b[4] != 1:
        return None, "magic/version"
    ln = struct.unpack_from("<I", b, 28)[0]
    if len(b) < HDR + ln + TRL:
        return None, "truncated"
    crc, commit = struct.unpack_from("<I4s", b, HDR + ln)
    if commit == b"\xff\xff\xff\xff":
        return None, "uncommitted"
    if commit != COMMIT or crc != zlib.crc32(b[:HDR + ln]):
        return None, "crc"
    seq, boot, mono, rtc, ln = struct.unpack_from("<IIIQI", b, 8)
    return dict(type=b[5], codec=b[6], flags=b[7], seq=seq, boot=boot, mono_ms=mono, rtc_ms=rtc, len=ln,
                size=HDR + ln + TRL), b[HDR:HDR + ln]

def record_size(head):
    """Total record size from its first 32 bytes, or None if the header is not ours."""
    if len(head) < HDR or head[:4] != MAGIC or head[4] != 1:
        return None
    return HDR + struct.unpack_from("<I", head, 28)[0] + TRL

# ---------------- IMA-ADPCM 4 bit (fw/lib/ima_adpcm) ----------------

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
        97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
        724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
        4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
        18500, 20350, 22385, 24623, 27086, 29794, 32767]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8] * 2

def adpcm_decode(data):
    """Low nibble first. Every record starts from predictor 0, index 0."""
    pred, idx, out = 0, 0, []
    for byte in data:
        for code in (byte & 15, byte >> 4):
            step = STEP[idx]
            diff = step >> 3
            if code & 4: diff += step
            if code & 2: diff += step >> 1
            if code & 1: diff += step >> 2
            pred = max(-32768, min(32767, pred - diff if code & 8 else pred + diff))
            idx = max(0, min(88, idx + INDEX[code]))
            out.append(pred)
    return out

# ---------------- receiver state: in-order cumulative ACK (docs/spec/formats.md §6) ----------------

class Rx:
    def __init__(self, nxt=1):
        self.next = nxt
    @property
    def acked(self):
        return self.next - 1
    def feed(self, h, payload, dry=False):
        first = last = h["seq"]
        if h["type"] == GAP:
            first, last = struct.unpack_from("<II", payload)
        if last < self.next:
            return "dup"
        if first > self.next:
            return "ahead"
        if not dry:
            self.next = last + 1
        return "accept"

class Reassembler:
    """Fragments (seq, off, data) -> complete records. Reports where to resume on a hole."""
    def __init__(self):
        self.parts = {}
    def add(self, seq, off, data):
        buf = self.parts.setdefault(seq, bytearray())
        if off != len(buf):
            return None, len(buf)  # hole or repeat: ask for a resend from len(buf)
        buf += data
        size = record_size(bytes(buf[:HDR]))
        if size is not None and len(buf) >= size:
            return bytes(self.parts.pop(seq)[:size]), None
        return None, None

# ---------------- timeline: records -> wav with gaps and UTC ----------------

def utc_ms(h, timefixes):
    """UTC of a record start: its own RTC if valid, else from the next timefix of the same boot."""
    if not h["flags"] & F_RTC_INVALID and h["rtc_ms"]:
        return h["rtc_ms"]
    for tf in timefixes:
        if tf["boot"] == h["boot"] and tf["seq"] > h["seq"]:
            return tf["after_ms"] - (tf["mono_ms"] - h["mono_ms"])
    return None

def build(records, tol_ms=250):
    """records: list of (hdr, payload) sorted by seq. Returns (pcm list, events list, utc of sample 0)."""
    timefixes = [dict(h, before_ms=struct.unpack_from("<QQ", p)[0], after_ms=struct.unpack_from("<QQ", p)[1])
                 for h, p in records if h["type"] == TIMEFIX]
    pcm, events, t0, expect, prev = [], [], None, None, None
    for h, p in records:
        if h["type"] == GAP:
            first, last, why = struct.unpack_from("<IIB", p)
            events.append(f"gap: sequence {first}..{last} lost (overwritten before sync)")
            prev = last
            continue
        if prev is not None and h["seq"] != prev + 1:
            events.append(f"missing records between {prev} and {h['seq']}")
        prev = h["seq"]
        if h["type"] == EVENT:
            kind = p[0]
            at = struct.unpack_from("<I", p, 4)[0]
            what = EVENT_NAMES.get(kind, f"event {kind}")
            if kind == 4:
                what += f" ({RESET_CAUSES.get(p[1], p[1])})"
            elif kind == 6:
                what += f" ({p[1]} blocks / {p[1] * 100} ms)"
            events.append(f"{what} at boot {h['boot']} +{at} ms (record {h['seq']})")
            continue
        if h["type"] != AUDIO:
            continue
        u = utc_ms(h, timefixes)
        start = u if u is not None else None
        if t0 is None:
            t0 = start
        # silence for time that passed between the end of the last record and the start of this one
        if expect is not None:
            miss = (h["mono_ms"] - expect["mono"]) if h["boot"] == expect["boot"] else None
            if miss is not None and miss > tol_ms:
                pcm.extend([0] * (miss * RATE // 1000))
                events.append(f"silence {miss} ms before record {h['seq']}")
            elif miss is None:
                events.append(f"reboot before record {h['seq']} (boot {expect['boot']} -> {h['boot']})")
        s = adpcm_decode(p)
        pcm.extend(s)
        expect = {"mono": h["mono_ms"] + len(s) * 1000 // RATE, "boot": h["boot"]}
    return pcm, events, t0

def write_wav(path, pcm):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(struct.pack(f"<{len(pcm)}h", *pcm))

def load_dir(paths):
    recs = []
    for p in paths:
        files = sorted(os.path.join(p, f) for f in os.listdir(p) if f.endswith(".rec")) if os.path.isdir(p) else [p]
        for f in files:
            data = open(f, "rb").read()
            while data:  # a file may hold several back-to-back records
                size = record_size(data[:HDR])
                if size is None:
                    break
                h, pl = parse_record(data[:size])
                if h:
                    recs.append((h, pl))
                data = data[size:]
    by = {h["seq"]: (h, p) for h, p in recs}
    return [by[k] for k in sorted(by)]

def report(recs, wav=None):
    pcm, events, t0 = build(recs)
    seqs = [h["seq"] for h, _ in recs]
    print(f"{len(recs)} records, seq {seqs[0] if seqs else '-'}..{seqs[-1] if seqs else '-'}, "
          f"{len(pcm) / RATE:.1f} s audio, start UTC "
          f"{time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(t0 / 1000)) if t0 else 'unknown'}")
    for e in events:
        print("  " + e)
    if wav:
        write_wav(wav, pcm)
        print("wrote", wav)
    return pcm, events

# ---------------- BLE sync (bleak) ----------------

async def sync(args):
    import asyncio
    from bleak import BleakClient, BleakScanner
    os.makedirs(args.out, exist_ok=True)
    state_path = os.path.join(args.out, "state.json")
    st = json.load(open(state_path)) if os.path.exists(state_path) else {}
    rx = Rx(st.get("next", 1))
    dev = await BleakScanner.find_device_by_filter(lambda d, ad: d.name == args.name or SVC in (ad.service_uuids or []),
                                                   timeout=args.scan)
    if not dev:
        sys.exit(f"no device named {args.name} found")
    q = asyncio.Queue()
    t_conn = time.time()
    async with BleakClient(dev) as c:
        mtu = c.mtu_size
        info = await c.read_gatt_char(INFO)
        v, rec, boot, tail, nxt, acked, up, rtc = struct.unpack("<BBxxIIIIIQ", info[:32])
        print(f"connected, mtu {mtu}; pendant boot {boot} ring [{tail}, {nxt}) acked {acked} uptime {up}s "
              f"recording {rec}; phone has up to {rx.acked}")
        await c.start_notify(DATA, lambda _, d: q.put_nowait((time.time(), bytes(d))))
        ctrl = lambda b: c.write_gatt_char(CTRL, b, response=False)
        if args.set_time or not rtc:  # also after a pendant reboot, which forgets the time
            await ctrl(b"\x03" + struct.pack("<Q", int(time.time() * 1000)))
        if args.wait:  # let the link settle (macOS applies the pendant's connection parameters a few seconds in)
            await asyncio.sleep(args.wait)
        await ctrl(b"\x02" + struct.pack("<I", rx.acked))
        ra, got, nbytes, t_first, t_last, nfrag = Reassembler(), 0, 0, None, None, 0
        waiting, resends, ahead = {}, 0, {}
        last_ack, end_next, retries = rx.acked, None, 0
        while not (end_next is not None and rx.next >= end_next):
            try:
                t, d = await asyncio.wait_for(q.get(), args.timeout if end_next is None else 3)
            except asyncio.TimeoutError:
                if end_next is None or retries >= 6:
                    print(f"timeout waiting for data (next {rx.next}, end {end_next}, waiting {dict(waiting)}, "
                          f"ahead {sorted(ahead)}, partial {[(k, len(v)) for k, v in ra.parts.items()]})")
                    break
                retries += 1  # everything was sent but something is still missing: ask for it again
                have = len(ra.parts.get(rx.next, b""))
                print(f"record {rx.next} still missing after the end marker; asking again from {have}")
                waiting[rx.next] = have
                resends += 1
                await ctrl(b"\x04" + struct.pack("<II", rx.next, have))
                continue
            t_first = t_first or t; t_last = t
            seq, off = struct.unpack_from("<II", d)
            nfrag += 1
            if args.drop and seq and nfrag % args.drop == 0:  # test aid: pretend this fragment was lost
                continue
            if seq == 0:
                end_next = off
                print(f"end marker: pendant next_seq {off}")
                continue
            nbytes += len(d) - 8
            if seq in waiting:
                if off == waiting[seq]:
                    pass            # the resend we asked for
                elif off == 0:
                    ra.parts.pop(seq, None)  # a fresh start, e.g. a gap record replacing a record that was overwritten
                else:
                    continue        # still streaming the part we already know we lost; the resend is on its way
            waiting.pop(seq, None)
            raw, resume = ra.add(seq, off, d[8:])
            if resume is not None:
                print(f"hole in record {seq}: have {resume}, got offset {off}; asking for resend")
                waiting[seq] = resume
                resends += 1
                await ctrl(b"\x04" + struct.pack("<II", seq, resume))
                continue
            if raw is None:
                continue
            h, pl = parse_record(raw)
            if h is None:
                print(f"record {seq} rejected ({pl}); asking for resend")
                waiting[seq] = 0
                ra.parts.pop(seq, None)
                resends += 1
                await ctrl(b"\x04" + struct.pack("<II", seq, 0))
                continue
            ahead[h["seq"]] = (h, pl, raw)  # a record can complete before an earlier one that needed a resend
            while True:  # take every record that is now next in order (a gap record skips ahead)
                nxt = next((k for k, (hh, pp, _) in sorted(ahead.items()) if rx.feed(hh, pp, dry=True) != "ahead"), None)
                if nxt is None:
                    break
                hh, pp, rr = ahead.pop(nxt)
                res = rx.feed(hh, pp)
                if res == "accept":
                    open(os.path.join(args.out, f"{hh['seq']:08d}.rec"), "wb").write(rr)
                    got += 1
                else:
                    print(f"record {nxt}: {res}")
            if rx.acked != last_ack:
                await ctrl(b"\x01" + struct.pack("<I", rx.acked))
                last_ack = rx.acked
        await ctrl(b"\x01" + struct.pack("<I", rx.acked))
        json.dump({"next": rx.next}, open(state_path, "w"))
        dt = (t_last - t_first) if t_first else 0
        print(f"received {got} records, {nbytes} B payload in {dt:.2f} s"
              + (f" = {nbytes / dt / 1000:.1f} kB/s" if dt > 0 else "") + f"; acked up to {rx.acked}; {resends} resend request(s)")
        await asyncio.sleep(0.3)
    if getattr(args, "decode", True):
        recs = load_dir([args.out])
        if recs:
            report(recs, os.path.join(args.out, "all.wav"))

async def follow(args):
    import asyncio
    args.decode = False  # a day of records makes a 1.4 GB all.wav; run `decode` on the folder instead
    while True:
        print(time.strftime("%H:%M:%S"), "sync")
        try:
            await sync(args)
        except (Exception, SystemExit) as e:  # pendant out of range, link dropped: try again next round
            print("  skipped:", e)
        await asyncio.sleep(args.every)

def main():
    sys.stdout.reconfigure(line_buffering=True)  # the log is a file when run through mac_bt_run.sh
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sync")
    f = sub.add_parser("follow")
    s.add_argument("--name", default="LifeOS-Pendant")
    s.add_argument("--out", default="rx")
    s.add_argument("--set-time", action="store_true")
    s.add_argument("--drop", type=int, default=0, help="test: drop every Nth fragment and recover by RESEND")
    s.add_argument("--wait", type=float, default=0, help="seconds to wait after connecting before SYNC")
    s.add_argument("--scan", type=float, default=15)
    s.add_argument("--timeout", type=float, default=20)
    for o in s._actions[1:]:  # follow takes the same options as sync
        f._add_action(o)
    f.add_argument("--every", type=float, default=60, help="seconds between syncs (the RRAM ring holds about 3 min)")
    d = sub.add_parser("decode")
    d.add_argument("paths", nargs="+")
    d.add_argument("--wav")
    a = ap.parse_args()
    if a.cmd in ("sync", "follow"):
        import asyncio
        asyncio.run(sync(a) if a.cmd == "sync" else follow(a))
    else:
        report(load_dir(a.paths), a.wav)

if __name__ == "__main__":
    main()
