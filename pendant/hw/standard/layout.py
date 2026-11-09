"""Placement for standard.kicad_sch, in 2.54 mm grid units (x right, y down).

("at", x, y, rot) places a part's origin. ("attach", my_pin, ref, pin, dx, dy, rot) puts my_pin dx, dy grid
units from another part's pin, so the wire between them is straight.

The MCU (U3) sits left of centre. Its right-hand GPIOs run, top to bottom, to the NAND, mic, battery sense,
RTC and indicator blocks, which are stacked in the same order to its right. Supply and debug (SWD,
UART, markers) are on its left. Power input and the buck are along the top.
"""

def at(x, y, r=0):
    return ("at", x, y, r)

def att(mp, ref, pin, dx, dy, r=0):
    return ("attach", mp, ref, pin, dx, dy, r)

H, V, Hr, Vr = 90, 0, 270, 180  # two-pin parts: H = pin 1 left, Hr = pin 1 right, V = pin 1 top, Vr = pin 1 bottom

LAYOUT = {
    # --- battery, reverse protection, reservoir (top left) ---
    "BT1": at(8, 16),
    "TP1": at(12, 14, 180),
    "R1": at(18, 14, V),
    "U1": at(28, 26),
    "R2": att("1", "U1", "1", 0, -6, H),
    "#FLG8": att("1", "U1", "1", -3, -3, 180),
    "C1": at(40, 18, V),
    "C2": at(46, 18, V),
    "TP2": at(50, 26, 180),

    # --- 1.8 V buck (top middle) ---
    "U2": at(64, 18),
    "R3": att("2", "U2", "4", -4, 0, H),
    "L1": att("1", "U2", "7", 3, 0, H),
    "C4": att("1", "U2", "8", 8, 0, V),
    "TP3": att("1", "U2", "8", 5, 2, 180),
    "R4": att("1", "U2", "8", 12, 0, H),
    "C3": at(56, 26, V),

    # --- module ---
    "U3": at(50, 80),
    "C5": at(30, 56, V),
    "C6": at(36, 56, V),
    "TP4": at(42, 54, 180),
    # SWD pogo pads (TC2030 pattern, no holes) on the module's left, lined up with RESET / SWDCLK / SWDIO
    "J1": att("3", "U3", "F3", -16, 0, 0),
    "TP12": att("1", "U3", "A2", -12, 0, 90),
    "TP13": att("1", "U3", "A1", -12, 0, 90),
    "TP14": att("1", "U3", "F2", -12, 0, 90),
    "TP15": att("1", "U3", "B3", -12, 0, 90),
    "TP16": att("1", "U3", "D3", -12, 0, 90),
    "TP7": at(22, 92), "TP8": at(26, 92),

    # --- NAND (right, top) ---
    "U4": at(84, 50),
    "R5": att("1", "U4", "3", -3, 2, V),
    "R6": att("1", "U4", "6", 3, 0, H),
    "TP5": att("1", "R6", "2", 2, 2, 180),
    "U5": at(112, 62),
    "R9": att("1", "U5", "1", -12, 0, Vr),
    "R10": att("2", "U5", "6", -6, 0, H),
    "R11": att("2", "U5", "5", -6, 0, H),
    "R12": att("1", "U5", "2", -6, 0, Hr),
    "R7": att("1", "U5", "3", 4, 0, Vr),
    "R8": att("1", "U5", "7", 9, 0, Vr),
    "C9": at(136, 58, V),
    "C10": at(142, 58, V),

    # --- microphone (right, second) ---
    "U6": at(84, 73),
    "R13": att("1", "U6", "3", -3, 2, V),
    "R14": att("1", "U6", "6", 3, 0, H),
    "SW1": att("1", "R14", "2", 4, 0, 0),
    "TP20": att("1", "R14", "2", 2, 2, 180),
    "U7": at(92, 84),
    "U8": att("6", "U7", "7", 14, 0, 0),
    "R26": att("1", "U8", "8", -6, 1, V),
    "R15": att("1", "U8", "2", 3, 0, V),
    "R16": att("1", "U8", "2", 7, 0, Vr),
    "C11": at(132, 78, V),
    "C12": at(138, 78, V),
    "TP6": at(144, 76, 180),

    # --- battery voltage sense and button (right, third) ---
    "U9": at(84, 96),
    "R17": att("1", "U9", "3", -3, 2, V),
    "R18": att("1", "U9", "6", 4, 0, V),
    "R19": att("1", "R18", "2", 0, 2, V),
    "C13": att("1", "R18", "2", 4, 2, V),
    "TP17": att("1", "R18", "2", 8, 2, 180),
    "SW2": at(84, 106),

    # --- RTC, not populated (right, fourth) ---
    "U10": at(92, 116),
    "R22": att("1", "U10", "2", -4, 0, Vr),
    "R23": att("1", "U10", "8", -7, 0, Vr),
    "R21": att("1", "U10", "1", 3, 0, H),
    "R20": at(108, 110, H),
    "C14": at(116, 112, V),

    # --- terminal indicator, provisional, not populated (right, bottom) ---
    "R24": at(92, 126, H),
    "D1": att("2", "R24", "2", 3, 0, 180),
    "C15": at(106, 128, V),
    "U12": at(112, 140),
    "U11": att("1", "U12", "5", -10, 0, 0),
    "D2": att("2", "U12", "5", -6, 0, 90),
    "D3": att("1", "U12", "7", -3, 3, 180),
    "R25": att("1", "U12", "7", -2, 6, V),
    "TP18": att("1", "U12", "2", 4, 0, 270),
    "TP19": att("1", "U12", "3", 7, 0, 270),

    # --- power flags (bottom left) ---
    "#FLG1": at(10, 120), **{f"#FLG{i}": at(10 + 6 * (i - 1), 120, 180) for i in range(2, 8)},
}

# block titles: (x, y, text) in grid units
TITLES = [
    (4, 8, "電源入力・逆流防止（DNP）・reservoir"), (52, 8, "1.8 V 降圧（TPS62840）"),
    (18, 52, "モジュール（Fanstel BC15M）・SWD・テストポイント"),
    (76, 44, "NAND（電源ゲート + SPI）"), (106, 69, "マイク（電源ゲート・privacy latch・Ioff バッファ・T5848）"),
    (96, 91, "電池電圧の測定・前面ボタン"), (104, 120, "RTC（footprint のみ・DNP）"),
    (80, 123, "終端表示（仮・DNP）"), (6, 114, "PWR_FLAG（ERC 用）"),
]
