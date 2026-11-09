"""Writes the hand-made footprints in pendant.pretty that are generated from datasheet numbers (DESIGN.md §5).

BC15M: Fanstel Eagle library BC15M-V3-LIB.lbr (BC15-LIB-2026-08.zip) and datasheet Ver 1.05 (pin table).
T5848: TDK DS-000479 Fig. 33 / 34 (land, stencil) and Fig. 3 (pin numbers, top view), rotated 90° CW into Fig. 33.
"""
import math
from pathlib import Path

OUT = Path(__file__).with_name("pendant.pretty")
LINE = '(stroke (width {w}) (type solid))'


def rect(x0, y0, x1, y1, layer, w):
    return f'\t(fp_rect (start {x0:g} {y0:g}) (end {x1:g} {y1:g}) {LINE.format(w=w)} (fill none) (layer "{layer}"))'


def smd(num, x, y, sx, sy, layers='"F.Cu" "F.Paste" "F.Mask"'):
    return f'\t(pad "{num}" smd rect (at {x:g} {y:g}) (size {sx:g} {sy:g}) (layers {layers}))'


def fp(name, descr, body, ref_y, val_y, attr="smd"):
    return "\n".join([
        f'(footprint "{name}"', '\t(version 20240108)', '\t(generator "gen_footprints.py")', '\t(layer "F.Cu")',
        f'\t(descr "{descr}")', f'\t(attr {attr})',
        f'\t(property "Reference" "REF**" (at 0 {ref_y:g} 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
        f'\t(property "Value" "{name}" (at 0 {val_y:g} 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))',
        *body, ")", ""])


def bc15m():
    # Fanstel's Eagle library BC15M-V3-LIB.lbr (BC15-LIB-2026-08.zip), copied number for number. Eagle frame: mm, y up,
    # module outline x -3.0..3.0, y -0.787..8.788; KiCad: origin at the module centre, y down.
    # Columns A-F, rows 1-6 (row 6 at the bottom edge). Columns A and F and row 6 run out to the module edge;
    # B1-E5 sit under the module. Datasheet Ver 1.05: pitch 0.9 mm, pads R 0.2, total height 1.9 mm.
    X0, X1, Y0, Y1 = -2.9972, 2.9972, -0.7874, 8.7884
    cy = (Y0 + Y1) / 2
    k = lambda x, y: (round(x, 4), round(cy - y, 4))
    xs = dict(zip("ABCDEF", (-2.54, -1.3462, -0.4572, 0.4572, 1.3462, 2.54)))
    ys = (4.4958, 3.6068, 2.6924, 1.8034, 0.889)
    pads = []
    for c, x in xs.items():
        for r, y in enumerate(ys, 1):
            if c in "AF":
                pads.append(smd(f"{c}{r}", *k(x, y), 0.9398, 0.4064))
            else:
                pads.append(f'\t(pad "{c}{r}" smd circle (at {k(x, y)[0]:g} {k(x, y)[1]:g}) (size 0.4064 0.4064) '
                            f'(layers "F.Cu" "F.Paste" "F.Mask"))')
        x6 = {"A": -2.2606, "F": 2.2606}.get(c, x)
        pads.append(smd(f"{c}6", *k(x6, -0.254), 0.4064, 0.889))
    # Antenna: everything above the 6.0 mm shield. Fanstel: the shield's top edge on the ground-plane edge, nothing under
    # the antenna. Copper-free on every layer.
    ya = Y0 + 6.0
    (x0, y0), (x1, y1) = k(X0, Y1), k(X1, ya)
    keepout = (f'\t(zone (net 0) (net_name "") (layers "*.Cu") (name "Antenna") (hatch edge 0.5) '
               f'(connect_pads (clearance 0)) (min_thickness 0.25) '
               f'(keepout (tracks not_allowed) (vias not_allowed) (pads not_allowed) (copperpour not_allowed) (footprints allowed)) '
               f'(fill (thermal_gap 0.5) (thermal_bridge_width 0.5)) '
               f'(polygon (pts (xy {x0:g} {y0:g}) (xy {x1:g} {y0:g}) (xy {x1:g} {y1:g}) (xy {x0:g} {y1:g}))))')
    (fx0, fy0), (fx1, fy1) = k(X0, Y1), k(X1, Y0)
    pin1 = k(X0 - 0.45, 4.4958)
    body = [rect(fx0, fy0, fx1, fy1, "F.Fab", 0.1),
            f'\t(fp_line (start {fx0:g} {y1:g}) (end {fx1:g} {y1:g}) {LINE.format(w=0.1)} (layer "F.Fab"))',
            f'\t(fp_text user "ANTENNA" (at 0 {(y0 + y1) / 2:g} 0) (layer "F.Fab") (effects (font (size 0.6 0.6) (thickness 0.1))))',
            rect(fx0 - 0.12, fy0 - 0.12, fx1 + 0.12, fy1 + 0.12, "F.SilkS", 0.12),
            f'\t(fp_circle (center {pin1[0]:g} {pin1[1]:g}) (end {pin1[0] + 0.2:g} {pin1[1]:g}) {LINE.format(w=0.12)} (fill solid) (layer "F.SilkS"))',
            rect(fx0 - 0.25, fy0 - 0.25, fx1 + 0.25, fy1 + 0.25, "F.CrtYd", 0.05),
            keepout, *pads]
    return fp("Fanstel_BC15M", "Fanstel BC15M (nRF54L15, PCB antenna, crystals and DC/DC inductors inside), 6.0 x 9.6 x 1.9 mm, "
              "36-pad LGA, pitch 0.9 mm. From Fanstel BC15M-V3-LIB.lbr. Antenna end on the board edge, no copper under it.",
              body, fy0 - 1.0, fy1 + 1.0)


def swd_pogo():
    # Tag-Connect TC2030 pad pattern (KiCad Tag-Connect_TC2030-IDC-NL_2x03_P1.27mm_Vertical) without its three 0.99 mm
    # alignment holes: the back of the board is the battery holder (decision H6). A pogo jig located by the board
    # outline presses on the pads; the TC2030 cable itself also fits by hand. Pin numbers follow the TC2030 symbol.
    pads = [f'\t(pad "{n}" connect circle (at {x:g} {y:g}) (size 0.7874 0.7874) (layers "F.Cu" "F.Mask"))'
            for n, x, y in ((1, -1.27, 0.635), (2, -1.27, -0.635), (3, 0, 0.635), (4, 0, -0.635), (5, 1.27, 0.635), (6, 1.27, -0.635))]
    body = [rect(-2.0, -1.5, 2.0, 1.5, "F.CrtYd", 0.05),
            f'\t(fp_line (start -1.905 1.27) (end -1.905 0.635) {LINE.format(w=0.12)} (layer "F.SilkS"))',
            f'\t(fp_line (start -1.27 1.27) (end -1.905 1.27) {LINE.format(w=0.12)} (layer "F.SilkS"))',
            *pads]
    return fp("SWD_Pogo_2x03_P1.27mm_NoHoles", "SWD pogo pads, TC2030 pattern (2x3, 1.27 mm, 0.79 mm pads) without alignment holes",
              body, 2.3, -2.3, attr="smd exclude_from_pos_files exclude_from_bom")


def t5848():
    # Origin = sound hole / GND ring centre. DS-000479 Fig. 33: big pads 0.522 x 0.60 in two columns 1.252 and 2.072
    # left of the ring (left column 3 pads at 0.90 pitch, right column 2 pads at +/-0.90); small pads 0.30 x 0.30 at
    # x = 0.765, y = -/+1.05. Pin numbers from Fig. 3 rotated 90 deg CW onto Fig. 33 (same as the T5838 family).
    # Paste (Fig. 34): big 0.422 x 0.50, small 0.27 x 0.27, ring split in four between 1.125 and 1.625.
    def pad(n, x, y, sx, sy, margin):
        return (f'\t(pad "{n}" smd rect (at {x:g} {y:g}) (size {sx:g} {sy:g}) (layers "F.Cu" "F.Paste" "F.Mask") '
                f'(solder_paste_margin {margin:g}))')
    big, small = (0.522, 0.60, -0.05), (0.3, 0.3, -0.015)
    pads = [pad(7, -2.072, -0.90, *big), pad(8, -2.072, 0, *big), pad(1, -2.072, 0.90, *big),
            pad(6, -1.252, -0.90, *big), pad(2, -1.252, 0.90, *big),
            pad(5, 0.765, -1.05, *small), pad(4, 0.765, 1.05, *small)]
    r, rw = (1.625 + 1.025) / 4, (1.625 - 1.025) / 2
    ring = (f'\t(pad "3" smd custom (at {r:g} 0) (size 0.3 0.3) (layers "F.Cu" "F.Mask") '
            f'(options (clearance outline) (anchor circle)) '
            f'(primitives (gr_circle (center {-r:g} 0) (end 0 0) (width {rw:g}) (fill none))))')
    pr, pw = (1.625 + 1.125) / 4, (1.625 - 1.125) / 2
    arcs = []
    for q in range(4):
        a0, a1 = math.radians(90 * q + 4.5), math.radians(90 * q + 85.5)
        pt = lambda a: f"{pr * math.cos(a):.4f} {pr * math.sin(a):.4f}"
        arcs.append(f'\t(fp_arc (start {pt(a0)}) (mid {pt((a0 + a1) / 2)}) (end {pt(a1)}) {LINE.format(w=pw)} (layer "F.Paste"))')
    hole = '\t(pad "" np_thru_hole circle (at 0 0) (size 0.6 0.6) (drill 0.6) (layers "*.Cu" "*.Mask"))'
    cx = (-2.072 - 0.261 + 0.765 + 0.15) / 2  # package outline centred on the land pattern (3.5 x 2.65)
    body = [rect(cx - 1.75, -1.325, cx + 1.75, 1.325, "F.Fab", 0.1),
            rect(cx - 1.75 - 0.25, -1.575, cx + 1.75 + 0.25, 1.575, "F.CrtYd", 0.05),
            f'\t(fp_circle (center {-2.072:g} 1.45) (end {-2.072 + 0.08:g} 1.45) {LINE.format(w=0.12)} (fill solid) (layer "F.SilkS"))',
            *pads, ring, *arcs, hole]
    return fp("TDK_T5848_LGA-8_3.5x2.65mm", "TDK InvenSense T5848 bottom-port I2S mic, 3.5 x 2.65 mm LGA-8. DS-000479 Fig. 33/34. "
              "Origin = sound hole (0.6 mm NPTH: TDK 0.5 to 1.0, JLCPCB NPTH >= 0.5, 0.21 mm to the GND ring). No paste in the hole.", body, -2.4, 2.4)


if __name__ == "__main__":
    for f in (bc15m, t5848, swd_pogo):
        text = f()
        name = text.split('"')[1]
        (OUT / f"{name}.kicad_mod").write_text(text)
        print("wrote", name)
