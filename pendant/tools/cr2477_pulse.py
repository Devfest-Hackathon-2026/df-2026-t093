#!/usr/bin/env python3
"""CR2477 pulse sag and reservoir sizing (docs/spec/power.md §4.4, issue #18).

Model: open-circuit Voc -> Rbat -> node with reservoir C -> load.  A constant-current pulse I of
length T starting from a full node gives V(T) = Voc - I*Rbat*(1 - exp(-T/(Rbat*C))).
The node must stay >= VREQ so the 1.8 V buck (VIN min 1.8 V, TPS62840) keeps regulating.
All pulse currents and resistances below are assumptions (see docs/spec/power.md §4.4 and the issue #18 log).
"""
import math
import sys

VREQ = 1.85            # V: TPS62840 VIN min 1.8 V + 50 mV margin
ETA = 0.9              # 1.8 V buck efficiency during a pulse (ASSUME)
V_NRF_MIN = 1.75       # nRF54L15 VDD,POR (PS v1.0 sec 11.14.1)
R_U5 = (0.26 + 0.12)   # TPS628438 RDSON(HS) max + inductor DCR, ohm (kit: nRF on the U5 path)

# name: (I on the nRF rail [A], I on the 1.8 V rail [A], T [s]).
# BLE and the CPU share are ASSUME; the NOR numbers are the W25Q256JW typ / max of docs/archive/32_NOR_W25Q.md (12 / 20 mA, tPP 0.8 / 5 ms,
# 64 KB erase 200 ms / 2 s).
PULSES = {
    "BLE": (8e-3, 0.3e-3, 3e-3),
    "PROG": (3e-3, 12e-3, 0.8e-3),
    "PROG max": (3e-3, 20e-3, 5e-3),
    "BLE+PROG": (8e-3, 12.3e-3, 0.8e-3),
    "ERASE": (3e-3, 12e-3, 0.2),
    "ERASE max": (3e-3, 20e-3, 2.0),
    "BLE+ERASE": (8e-3, 12.3e-3, 0.2),
}
RBAT = {"15 ohm (ms pulse, ASSUME)": 15.0, "40 ohm (Jauch 5 s curve)": 40.0, "80 ohm (cold x2)": 80.0}


def i_bat(i_nrf, i_18, v):
    """Battery-side current: nRF rail passes through (100 % mode), 1.8 V rail is a buck."""
    return i_nrf + i_18 * 1.8 / (v * ETA)


def cap_needed(voc, rbat, i, t, vreq):
    """Smallest C that keeps the node >= vreq for the whole pulse; 0 if none needed, inf if impossible."""
    if voc - i * rbat >= vreq:
        return 0.0
    if voc <= vreq:
        return math.inf
    x = (voc - vreq) / (i * rbat)
    return t / -math.log(1 - x) / rbat


def main():
    print(f"VREQ={VREQ} V  eta={ETA}  nRF kit path: R_U5={R_U5:.2f} ohm  nRF min {V_NRF_MIN} V")
    for rname, rb in RBAT.items():
        print(f"\n## Rbat = {rname}")
        print("pulse      Voc  I_bat[mA]  V_node(no C)[V]  V_nRF(kit,no C)[V]  C_min[uF]")
        for pname, (i_n, i_18, t) in PULSES.items():
            for voc in (3.0, 2.5, 2.0):
                i = i_bat(i_n, i_18, max(voc - 0.3, VREQ))  # v used only for the buck input current
                v = voc - i * rb
                vn = v - i_n * R_U5
                c = cap_needed(voc, rb, i, t, VREQ)
                cs = "-" if c == 0 else ("impossible" if c == math.inf else f"{c * 1e6:.0f}")
                print(f"{pname:10s} {voc:.1f}  {i * 1e3:8.1f}  {v:15.2f}  {vn:18.2f}  {cs}")
        print("lowest Voc that needs no reservoir (node >= VREQ for the whole pulse):")
        for pname, (i_n, i_18, t) in PULSES.items():
            i = i_bat(i_n, i_18, 2.2)
            print(f"  {pname:10s} {VREQ + i * rb:.2f} V  (I_bat {i * 1e3:.1f} mA)")
    print("\n## reservoir sweep (Rbat 40 ohm): node minimum for fixed C")
    for pname, (i_n, i_18, t) in PULSES.items():
        for voc in (2.5, 2.2):
            i = i_bat(i_n, i_18, max(voc - 0.3, VREQ))
            row = []
            for c in (47e-6, 100e-6, 220e-6, 470e-6):
                tau = 40.0 * c
                row.append(f"{c * 1e6:.0f}uF:{voc - i * 40.0 * (1 - math.exp(-t / tau)):.2f}V")
            print(f"{pname:10s} Voc={voc:.1f}  " + "  ".join(row))


if __name__ == "__main__":
    sys.exit(main())
