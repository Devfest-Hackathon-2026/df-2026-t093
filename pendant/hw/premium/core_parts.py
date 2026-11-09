"""Reusable Premium digital core; coordinates are schematic-grid units.

Use make_core() after the Standard directory is on sys.path (as in the
Premium generator). This is a bench-evaluation core, not a completed product.
The Standard part definitions are the source for symbols, pin names and nets.

Measurement proposal, deliberately NOT connected: HSL -> TPS22917 VIN,
VOUT -> 3.3 MOhm -> ADC -> 1 MOhm -> GND, with 10 nF at ADC;
an analogous independently switched COIN branch. 3.8 V gives 0.884 V
nominal; resistor tolerances, ADC gain/reference, 1.8 V GPIO limits and
transients must be checked. Connect EN through an MCU GPIO and a pulldown;
QOD open or separately designed. However the retained ADC capacitor and
switch leakage can power an unpowered MCU through its input. Add a proven
powered-off isolation/clamp scheme, validate all transitions and faults, and
measure leakage before connecting B5 (P1.05/AIN1) or any other ADC. The
switch's 1--5.5 V VIN range and reverse blocking do not establish whole-circuit
power-off safety: https://www.ti.com/lit/ds/symlink/tps22917.pdf .
Use a source-specific operating budget and test MCU input current, HSL/COIN
leakage, switch-off settling and ADC calibration. BQ UV can isolate HSL from
VSTOR, so sense HSL itself, not VSTOR:
https://www.ti.com/lit/ds/symlink/bq25505.pdf .

VBAT_OK_1V8 is connected to P0.03 (B2), subject to validating U5's input
threshold and power-off behavior. The NAND shares 1V8 with the MCU in this
demo draft: it cannot be power-gated independently, and its standby current
must be included in the energy budget. The latch is a bridged solder jumper,
not the final physical privacy control. Check startup/shutdown and the mic's
off-state back-power, and measure the privacy path.
"""
from copy import deepcopy

from parts import PARTS

# Keep only the shared core. Do not copy Standard battery, buck, RTC or display.
_REFS = {
    'U3', 'C5', 'C6', 'R6', 'U5', 'C9', 'C10',
    'R7', 'R8', 'R9', 'R10', 'R11', 'R12',
    'U6', 'R13', 'R14', 'SW1', 'U7', 'C11', 'U8', 'R26',
    'C12', 'R15', 'R16', 'SW2', 'J1',
    'TP4', 'TP5', 'TP6', 'TP7', 'TP12', 'TP13', 'TP14', 'TP15', 'TP16', 'TP20',
}


def make_core():
    """Return (parts, layout); caller may translate layout to fit its sheet."""
    parts = []
    for original in PARTS:
        if original['ref'] not in _REFS:
            continue
        part = deepcopy(original)
        ref = part['ref']
        prefix = ref.rstrip('0123456789')
        part['ref'] = prefix + str(int(ref[len(prefix):]) + 100)
        # Every pin gets its own named-net label in the Premium generator.
        part.pop('group', None)
        if ref == 'U3':
            part['nets']['B2'] = 'VBAT_OK_1V8'  # P0.03, conditional candidate
            for pin in ('B1', 'C5', 'D2', 'D5', 'A5', 'E4'):
                part['nets'][pin] = None  # omitted Standard sense/RTC/display
            # B5/P1.05 reserved for HSL; do not connect an unvalidated divider.
        if ref == 'R6':
            # The link still allows separate NAND current measurements.
            part['nets']['1'] = '1V8'
        parts.append(part)

    # Local schematic coordinates only, not PCB positions. Translation is expected.
    placements = {
        'U103': (30, 45), 'C105': (7, 20), 'C106': (7, 30),
        'R106': (85, 18),
        'U105': (112, 20), 'C109': (135, 8), 'C110': (135, 18),
        'R107': (135, 28), 'R108': (135, 38), 'R109': (135, 48),
        'R110': (155, 8), 'R111': (155, 18), 'R112': (155, 28),
        'U106': (65, 45), 'R113': (85, 40), 'R114': (85, 50),
        'SW101': (105, 45), 'U107': (125, 55), 'C111': (145, 55),
        'U108': (105, 80), 'R126': (125, 75), 'C112': (125, 85),
        'R115': (145, 75), 'R116': (145, 85), 'SW102': (65, 75),
        'J101': (65, 95),
        'TP104': (8, 80), 'TP105': (8, 88), 'TP106': (8, 96),
        'TP107': (8, 104), 'TP112': (25, 95), 'TP113': (25, 103),
        'TP114': (40, 95), 'TP115': (40, 103), 'TP116': (55, 103),
        'TP120': (80, 103),
    }
    assert {p['ref'] for p in parts} == placements.keys()
    return parts, {ref: ('at', x, y, 0) for ref, (x, y) in placements.items()}
