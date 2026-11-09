#!/usr/bin/env python3
"""BQ25505 VBAT_OV / VBAT_OK resistor picker (docs/spec/power.md §3.3, issue #25).

Equations (BQ25505 datasheet SLUSBJ3F, 7.3.3 and 7.3.4):
  VBAT_OV       = 1.5 * VBIAS * (1 + ROV2 / ROV1)
  VBAT_OK_PROG  = VBIAS * (1 + ROK2 / ROK1)             (falling)
  VBAT_OK_HYST  = VBIAS * (1 + (ROK2 + ROK3) / ROK1)    (rising)
  VBIAS 1.205 / 1.21 / 1.217 V (min / typ / max); ROV1 + ROV2 and ROK1 + ROK2 + ROK3 each 11 to 15 MOhm (13 recommended)
  threshold accuracy +-2 % overall with 0.1 % resistors
"""
import itertools
import argparse
import math

E96 = [100, 102, 105, 107, 110, 113, 115, 118, 121, 124, 127, 130, 133, 137, 140, 143, 147, 150, 154, 158, 162, 165,
       169, 174, 178, 182, 187, 191, 196, 200, 205, 210, 215, 221, 226, 232, 237, 243, 249, 255, 261, 267, 274, 280,
       287, 294, 301, 309, 316, 324, 332, 340, 348, 357, 365, 374, 383, 392, 402, 412, 422, 432, 442, 453, 464, 475,
       487, 499, 511, 523, 536, 549, 562, 576, 590, 604, 619, 634, 649, 665, 681, 698, 715, 732, 750, 768, 787, 806,
       825, 845, 866, 887, 909, 931, 953, 976]
VBIAS = (1.205, 1.21, 1.217)
RMIN, RMAX = 11e6, 15e6
VALUES = [v * 10 ** d * 1e3 for d in range(0, 2) for v in E96]   # 100 kOhm .. 9.76 MOhm (and below)


def ov(r1, r2, vb):
    return 1.5 * vb * (1 + r2 / r1)


def ok_prog(r1, r2, r3, vb):
    return vb * (1 + r2 / r1)


def ok_hyst(r1, r2, r3, vb):
    return vb * (1 + (r2 + r3) / r1)


def spread(fn, rs, tol):
    """min / max of fn over VBIAS and every +-tol corner of the resistors."""
    out = []
    for vb in VBIAS:
        for signs in itertools.product((-1, 1), repeat=len(rs)):
            out.append(fn(*[r * (1 + s * tol) for r, s in zip(rs, signs)], vb))
    return min(out), max(out)


def pick_ov(target):
    best = min(((abs(ov(a, b, VBIAS[1]) - target), a, b) for a in VALUES for b in VALUES if RMIN <= a + b <= RMAX))
    return best[1], best[2]


def pick_ok(lo, hi):
    best = None
    for a in VALUES:
        for b in VALUES:
            for c in VALUES:
                if not RMIN <= a + b + c <= RMAX or c > 3e6:
                    continue
                e = abs(ok_prog(a, b, c, VBIAS[1]) - lo) + abs(ok_hyst(a, b, c, VBIAS[1]) - hi)
                if best is None or e < best[0]:
                    best = (e, a, b, c)
    return best[1:]


def fmt(r):
    return f"{r / 1e6:.3f} M"


OV_RESISTORS = (6.34e6, 6.49e6)
PROFILES = (
    ("LEGACY prototype reference (2.5 / 2.7 V targets)", (6.34e6, 6.81e6, 1e6)),
    ("PROPOSAL rated-floor profile; NOT adopted", (4.87e6, 5.62e6, .787e6)),
    ("DEMO draft for Vina 30 F; NOT hardware-qualified", (4.87e6, 6.34e6, .787e6)),
)
ETA_BUCK, CAP_FIXED_MW, ETA_HARVEST = .9, .037, .85


def energy_mwh(cap_f, high_v, low_v):
    return cap_f * (high_v ** 2 - low_v ** 2) / 7.2


def cap_load_mw(rail_mw):
    return rail_mw / ETA_BUCK + CAP_FIXED_MW


def check():
    high = ov(*OV_RESISTORS, VBIAS[1])
    assert math.isclose(high, 3.672941640378549, abs_tol=1e-12)
    for (_, rs), expected in zip(PROFILES, ((119.86507700, 59.76336374), (111.62443659, 54.96216886), (95.54933234, 45.59642732))):
        fall = ok_prog(*rs, VBIAS[1])
        rise = ok_hyst(*rs, VBIAS[1])
        assert RMIN <= sum(rs) <= RMAX
        assert high * 1.02 < 3.8 and fall * 1.02 < rise * .98 - .05
        assert math.isclose(energy_mwh(120, high, fall), expected[0], abs_tol=1e-5)
        assert math.isclose(energy_mwh(67.2, high * .98, fall * 1.02), expected[1], abs_tol=1e-5)
    legacy = ok_prog(*PROFILES[0][1], VBIAS[1])
    rated = ok_prog(*PROFILES[1][1], VBIAS[1])
    assert 2.2 < legacy * .98 < 2.5 < rated * .98
    demo = ok_prog(*PROFILES[2][1], VBIAS[1])
    assert math.isclose(demo, 2.78523614, abs_tol=1e-8) and demo * .98 > 2.5
    assert not math.isclose(legacy * .98, 2.5 * .98, abs_tol=.001)  # actual divider, not target
    assert math.isclose(energy_mwh(120, 3, 2), 600 / 7.2)  # joules / 3.6 -> mWh
    assert math.isclose(energy_mwh(120, 3, 2) / cap_load_mw(.9), (600 / 7.2) / 1.037)
    assert math.isclose((cap_load_mw(.5) * 72) / (5 * ETA_HARVEST - cap_load_mw(.5)), 11.6649755446)
    print("PASS: actual divider thresholds, tolerance boundaries, energy, runtime and solar recovery")


def main():
    high = ov(*OV_RESISTORS, VBIAS[1])
    print("Frozen OV resistors (not a fresh nearest-target search):")
    print(f"  ROV1/ROV2 = {' / '.join(map(fmt, OV_RESISTORS))}; sum {fmt(sum(OV_RESISTORS))}")
    print(f"  nominal {high:.9f} V; overall +-2% {high * .98:.9f} .. {high * 1.02:.9f} V")
    print("Assumptions: 0.1% resistors; datasheet overall +-2% applied to actual nominal (not added to VBIAS corners).")
    print("Energy model: eta_buck=0.9, cap-side fixed load=0.037 mW; estimates, not hardware qualification.")
    for name, rs in PROFILES:
        fall, rise = ok_prog(*rs, VBIAS[1]), ok_hyst(*rs, VBIAS[1])
        print(f"\n{name}")
        print(f"  ROK1/ROK2/ROK3 = {' / '.join(map(fmt, rs))}; sum {fmt(sum(rs))}")
        print(f"  nominal falling/rising {fall:.9f} / {rise:.9f} V")
        print(f"  overall +-2%: falling {fall * .98:.9f} .. {fall * 1.02:.9f} V; "
              f"rising {rise * .98:.9f} .. {rise * 1.02:.9f} V")
        if fall * .98 < 2.5:
            print("  Legacy falling minimum is below the HSL rated 2.5 V floor, above the allowed 2.2 V floor.")
        for label, c, vh, vl in (("nominal 120 F", 120, high, fall),
                                  ("67.2 F, OV -2% / falling +2%", 67.2, high * .98, fall * 1.02)):
            e = energy_mwh(c, vh, vl)
            print(f"  {label}: E={e:.8f} mWh; 72 h rail ceiling={(e / 72 - CAP_FIXED_MW) * ETA_BUCK:.6f} mW")
            print("    rail mW -> runtime h: " + "; ".join(
                f"{p:g} -> {e / cap_load_mw(p):.4f}" for p in (.3, .5, .8, 1, 3, 5, 10, 20)))
    load = cap_load_mw(.5)
    deficit = load * 72
    print(f"\nSolar recovery: 3-day deficit at rail 0.5 mW = {deficit:.6f} mWh; eta_harvest=0.85.")
    print("Panel power means actual available power; live load is subtracted, constant-light estimate.")
    for panel in (5, 10, 20):
        net = panel * ETA_HARVEST - load
        print(f"  panel {panel} mW: net {net:.6f} mW, recovery {deficit / net:.6f} h")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="run the reproducibility self-check")
    args = parser.parse_args()
    check() if args.check else main()
