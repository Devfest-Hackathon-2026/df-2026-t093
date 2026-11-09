#!/usr/bin/env python3
"""Standard (CR2477 + TPS62840 + NOR) average power and battery life (docs/spec/power.md §7.6, issue #17).

Every input is a constant below with its source; change one and rerun, or override on the command line:
  standard_power.py --ble 100 --nor 331 --kbps 16 --floor 60
"""
import argparse

BYTES_PER_S_FULL = 4000     # 32 kbps IMA-ADPCM, 8 kHz (docs/spec/formats.md §3)
ETA = 0.9                   # TPS62840 efficiency (ASSUME, same convention as docs/spec/power.md §7.2)
# rail-side power in uW at 1.8 V; (a) = always on, (b) = scales with the stored fraction p
MIC_ACTIVE_UW = 216         # T5838 LP mode 120 uA x 1.8 V (docs/spec/power.md §7.2, T5838 datasheet)
MIC_AAD_UW = 36             # T5838 AAD 20 uA x 1.8 V (docs/spec/audio.md §4.7)
MCU_AUDIO_UW = 65           # ADPCM + CRC + DMA at p = 1 (docs/spec/power.md §7.2, ASSUME; ADPCM alone measured 42 uW, docs/spec/audio.md §5.2)
MCU_FLOOR_UW = 5.2          # nRF54L15 System ON IDLE, GRTC, 256 KB RAM: 2.9 uA x 1.8 V (PS v1.0 front page)
NOR_PD_UW = 1.8             # W25Q256JW power-down 1 uA typ x 1.8 V (W25Q256JW datasheet)
NOR_NJ_PER_BYTE = 133 + 1.7  # 64 KB block erase + program (W25Q256JW datasheet, typ) + 50 MHz read for the sync (6 mA)
BLE_MJ_PER_MB = 87          # docs/spec/power.md §7.2 (design hypothesis, 60 to 100)
RTC_UW = 0.3                # RV-3032 160 nA x 1.8 V (docs/spec/firmware.md §5.3)
REG_IQ_UW = 0.3             # TPS62840 IQ ~120 nA (data sheet) plus margin
SELF_DISCHARGE_PER_YEAR = 0.01  # Li/MnO2 (ASSUME)
V_MEAN = 2.9                # mean voltage to a 2.0 V cutoff, read from the Jauch CR2477J curves (rough)
UV2_FRACTION = 0.95         # capacity above the 2.40 V stage (docs/spec/power.md §4.2), ASSUME (knee is narrow)


def power_uw(p, a):
    scale = a.kbps / 32
    byte_s = BYTES_PER_S_FULL * scale
    rail = (MIC_AAD_UW * (1 - p) + MIC_ACTIVE_UW * p + MCU_FLOOR_UW + MCU_AUDIO_UW * p * scale
            + NOR_PD_UW + a.nor * 1e-3 * byte_s * p + a.ble * byte_s * p * 1e-3
            + RTC_UW + REG_IQ_UW + a.floor + a.shifter_ua * 2.5)
    return rail / ETA


def life_days(energy_wh, p_uw):
    p_total = p_uw + energy_wh * 1e6 * SELF_DISCHARGE_PER_YEAR / 8760
    return energy_wh * 1e6 / p_total / 24


def p_max(energy_wh, days, a):
    """Largest p that still reaches `days` (P is linear in p)."""
    target = energy_wh * 1e6 / (days * 24) - energy_wh * 1e6 * SELF_DISCHARGE_PER_YEAR / 8760
    p0, p1 = power_uw(0, a), power_uw(1, a)
    return (target - p0) / (p1 - p0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ble", type=float, default=BLE_MJ_PER_MB, help="BLE energy [mJ/MB]")
    ap.add_argument("--nor", type=float, default=NOR_NJ_PER_BYTE, help="NOR program+erase+read [nJ/B]")
    ap.add_argument("--kbps", type=float, default=32, help="stored bitrate (32 or 16)")
    ap.add_argument("--floor", type=float, default=0, help="extra rail-side floor [uW] (XIAO board, unmeasured)")
    ap.add_argument("--shifter-ua", type=float, default=0, help="level shifter ICC [uA] at 2.5 V (kit only)")
    a = ap.parse_args()
    print(f"inputs: BLE {a.ble} mJ/MB, NOR {a.nor} nJ/B, {a.kbps} kbps, floor {a.floor} uW, shifter {a.shifter_ua} uA")
    caps = {"1000 mAh, 2.0 V": 1.0 * V_MEAN, "950 mAh, 2.0 V": 0.95 * V_MEAN,
            "950 mAh, 2.4 V (UV 2)": 0.95 * V_MEAN * UV2_FRACTION, "3.0 Wh (docs/spec/product.md)": 3.0}
    print("p     P[uW]  " + "  ".join(f"{k:>22s}" for k in caps))
    for p in (0.25, 0.4, 0.6, 1.0):
        pw = power_uw(p, a)
        print(f"{p:4.2f}  {pw:6.0f}  " + "  ".join(f"{life_days(e, pw):19.0f} d" for e in caps.values()))
    print("p upper bound for 365 d: " + ", ".join(f"{k}: {p_max(e, 365, a):.2f}" for k, e in caps.items()))


if __name__ == "__main__":
    main()
