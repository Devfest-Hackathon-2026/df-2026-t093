"""Parts and nets of the Standard board (Fanstel BC15M, CR2477, 1.8 V). Rationale: DESIGN.md.

POWER: nets drawn as power symbols. Each part: ref, sym (stock 'Lib:Name' or custom(...)), value, fp, nets {pin: net or None (no-connect)},
optional dnp and fields. gen_schematic.py turns this into standard.kicad_sch.
"""
from gen_schematic import custom

TITLE = "lifeOS Pendant Standard - Fanstel BC15M (rev 0, generated)"

POWER = ["GND", "VBAT", "VBAT_SYS", "1V8", "NAND_1V8", "MIC_1V8", "RTC_VDD", "VLG"]

R0402, C0402 = "Resistor_SMD:R_0402_1005Metric", "Capacitor_SMD:C_0402_1005Metric"
B, PI, PO, I, O, P = "bidirectional", "power_in", "power_out", "input", "output", "passive"

# ---------- custom symbols (pinouts: DESIGN.md §2) ----------

# GPIOs on the right in the order of the blocks they feed (NAND, mic, sense, RTC, indicator);
# supply, clocks and debug on the left.
# Fanstel BC15M: 6x6 LGA grid, pads named by column letter (A-F) and row (1-6). Crystals and DC/DC inductors are inside.
BC15M = custom("BC15M", [
    ("F6", "VDD", PI), None, ("F3", "~{RESET}", P), None, ("A4", "SWDCLK", I), ("A3", "SWDIO", B), None,
    ("A2", "P0.00", B), ("A1", "P0.01", B), ("F2", "P2.06", B), ("B3", "P2.07", B), ("D3", "P2.08", B), None,
    ("E1", "GND", PI), ("F1", "GND", PI)], [
    ("A6", "P2.05", B), ("C6", "P2.01", B), ("D6", "P2.02", B), ("F5", "P2.04", B), ("E4", "P2.03", B), None,
    ("C4", "P2.00", B), ("D1", "P1.12", B), ("C3", "P1.09", B), ("E2", "P1.13", B), ("D4", "P1.14", B), None,
    ("B1", "P1.08", B), ("C5", "P1.04/AIN0", B), ("B4", "P1.07/AIN3", B), None,
    ("D2", "P1.10", B), ("D5", "P1.11", B), None,
    ("A5", "P2.09", B), None,
    ("C1", "P2.10", B), ("C2", "P0.02", B), ("B2", "P0.03", B), ("E3", "P0.04", B),
    ("E6", "P1.02/NFC1", B), ("F4", "P1.03/NFC2", B), ("B5", "P1.05/AIN1", B), ("B6", "P1.06/AIN2", B), ("E5", "P1.15", B)])

TPS62840 = custom("TPS62840DLC", [("2", "VIN", PI), ("4", "EN", I), ("3", "MODE", I), ("6", "STOP", I), ("5", "VSET", I)],
                  [("7", "SW", PO), ("8", "VOS", I), None, None, ("1", "GND", PI)])
TPS22917 = custom("TPS22917", [("1", "VIN", PI), ("3", "ON", I), ("4", "CT", P)],
                  [("6", "VOUT", PO), ("5", "QOD", P), ("2", "GND", PI)])
NAND = custom("SPI_NAND_WSON8", [("1", "~{CS}", I), None, ("6", "CLK", I), None, ("5", "DI/SIO0", I), None, ("2", "DO/SIO1", O)],
              [("8", "VCC", PI), None, ("3", "~{WP}/SIO2", I), None, ("7", "~{HOLD}/SIO3", I), None, ("4", "GND", PI), ("9", "EP", P)])
T5848 = custom("T5848", [("6", "SCK", I), ("1", "WS", I), ("5", "THSEL", I), ("8", "SD", O), ("4", "WAKE", O)],
               [("7", "VDD", PI), ("2", "LR", I), ("3", "GND", PI)])
AUP3G34 = custom("SN74AUP3G34", [("1", "1A", I), ("3", "2A", I), ("6", "3A", I), ("4", "GND", PI)],
                 [("8", "VCC", PI), ("7", "1Y", O), ("5", "2Y", O), ("2", "3Y", O)])
RV3032 = custom("RV-3032-C7", [("2", "SDA", B), ("8", "SCL", I), ("3", "~{INT}", O), ("7", "CLKOUT", O)],
                [("6", "VDD", PI), ("1", "VBACKUP", P), ("5", "VSS", PI), ("4", "EVI", I)])
SUPV = custom("SUPERVISOR_ACTIVE_HIGH", [("2", "VDD", PI), ("3", "GND", PI)], [("1", "OUT", O), ("4", "~{MR}", I), ("5", "CT", P)])  # TPS3840PH DBV

def R(ref, val, a, b, dnp=False, fp=R0402, **kw):
    return dict(ref=ref, sym="Device:R_Small", value=val, fp=fp, nets={"1": a, "2": b}, dnp=dnp, **kw)

def C(ref, val, a, b="GND", dnp=False, fp=C0402, **kw):
    return dict(ref=ref, sym="Device:C_Small", value=val, fp=fp, nets={"1": a, "2": b}, dnp=dnp, **kw)

def TP(ref, net, group="tp"):
    return dict(ref=ref, sym="Connector:TestPoint", value=net, fp="TestPoint:TestPoint_Pad_D1.0mm", nets={"1": net}, group=group)

def FLAG(n, net, group="flag"):
    return dict(ref=f"#FLG{n}", sym="power:PWR_FLAG", value="PWR_FLAG", nets={"1": net}, group=group)

PARTS = [
    # --- battery, reverse-protection option, reservoir (DESIGN.md §3.1) ---
    dict(ref="BT1", sym="Device:Battery_Cell", value="CR2477", fp="pendant:BatteryHolder_MYOUNG_MY-2477-01",
         nets={"1": "VBAT", "2": "GND"}, group="pwr",
         fields={"MPN": "MYOUNG MY-2477-01 (LCSC C2979173)", "Note": "THT clip 26.8 x 22.6, 8.5 mm tall; - = 8 mm PCB pad (ENIG)"}),
    R("R1", "0R R_BAT", "VBAT", "VBAT_IN", group="pwr"),
    R("R2", "0R", "VBAT_IN", "VBAT_SYS", group="pwr"),
    dict(ref="U1", sym="Power_Management:LM66100DCK", value="LM66100DCK", fp="Package_TO_SOT_SMD:SOT-363_SC-70-6", dnp=True,
         nets={"1": "VBAT_IN", "2": "GND", "3": "GND", "4": None, "5": None, "6": "VBAT_SYS"}, group="pwr"),
    C("C1", "100u", "VBAT_SYS", fp="Capacitor_SMD:C_0805_2012Metric", group="pwr",
      fields={"Spec": "6.3V X5R", "MPN": "Murata GRM21BR60J107ME15L"}),
    C("C2", "220u", "VBAT_SYS", dnp=True, fp="Capacitor_SMD:C_1206_3216Metric", group="pwr",
      fields={"Spec": "6.3V X5R", "MPN": "Murata GRM31CR60J227ME11L"}),
    # --- 1.8 V buck (DESIGN.md §3.2) ---
    dict(ref="U2", sym=TPS62840, value="TPS62840DLCR", fp="Package_SON:Texas_VSON-HR-8_1.5x2mm_P0.5mm", group="buck",
         nets={"2": "VBAT_SYS", "4": "BUCK_EN", "3": "GND", "6": "GND", "5": "GND", "7": "SW_BUCK", "8": "1V8_BUCK", "1": "GND"},
         ds="https://www.ti.com/lit/ds/symlink/tps62840.pdf"),
    R("R3", "0R", "VBAT_SYS", "BUCK_EN", group="buck", fields={"Note": "cut to gate EN from a supervisor later"}),
    C("C3", "4.7u", "VBAT_SYS", group="buck", fields={"Spec": "10V X5R", "MPN": "Murata GRM155R61A475MEAAD"}),
    dict(ref="L1", sym="Device:L_Small", value="2.2u", fp="Inductor_SMD:L_Murata_DFE201610P", nets={"1": "SW_BUCK", "2": "1V8_BUCK"}, group="buck",
         fields={"MPN": "Murata DFE201612E-2R2M=P2 (2016; land of DFE201610P)"}),
    C("C4", "10u", "1V8_BUCK", group="buck", fields={"Spec": "4V X5R", "MPN": "YAGEO CC0402MRX5R4BB106"}),
    R("R4", "0R R_MAIN", "1V8_BUCK", "1V8", group="buck"),
    # --- module (DESIGN.md §3.3, pin map §4) ---
    dict(ref="U3", sym=BC15M, value="BC15M", fp="pendant:Fanstel_BC15M", group="mcu",
         ds="https://fanstel.squarespace.com/s/BC15M-Product-Specifications-675g.pdf",
         fields={"MPN": "Fanstel BC15M (nRF54L15, 技適 201-250527)"},
         nets={"F6": "1V8", "F3": "nRESET", "A4": "SWCLK", "A3": "SWDIO", "A2": "UART_TX", "A1": "UART_RX",
               "F2": "MK_REC", "B3": "MK_STORE", "D3": "MK_BLE", "E1": "GND", "F1": "GND",
               "A6": "NAND_CS", "C6": "NAND_SCK_MCU", "D6": "NAND_MOSI_MCU", "F5": "NAND_MISO_MCU", "E4": "NAND_EN",
               "C4": "MIC_EN", "D1": "I2S_SCK_MCU", "F4": "I2S_WS_MCU", "C3": "THSEL_MCU", "E2": "MIC_SD", "D4": "MIC_WAKE",
               "B1": "BATSENSE_EN", "C5": "VBAT_ADC", "B4": "BTN", "D2": "I2C_SDA", "D5": "I2C_SCL", "A5": "IND_RUN",
               "C1": None, "C2": None, "B2": None, "E3": None, "E6": None, "B5": None, "B6": None, "E5": None}),
    C("C5", "4.7u", "1V8", group="mcu", fields={"Spec": "10V X5R", "MPN": "Murata GRM155R61A475MEAAD"}),
    C("C6", "100n", "1V8", group="mcu"),
    # --- NAND (DESIGN.md §3.4) ---
    dict(ref="U4", sym=TPS22917, value="TPS22917DBVR", fp="Package_TO_SOT_SMD:SOT-23-6", group="nand",
         nets={"1": "1V8", "3": "NAND_EN", "4": None, "6": "NAND_1V8_SW", "5": "NAND_1V8_SW", "2": "GND"}),
    R("R5", "1M", "NAND_EN", "GND", group="nand"),
    R("R6", "0R R_NAND", "NAND_1V8_SW", "NAND_1V8", group="nand"),
    dict(ref="U5", sym=NAND, value="F35UQA512M-WWR", fp="Package_SON:WSON-8-1EP_8x6mm_P1.27mm_EP3.4x4.3mm", group="nand",
         nets={"1": "NAND_CS", "6": "NAND_SCK", "5": "NAND_MOSI", "2": "NAND_MISO", "8": "NAND_1V8",
               "3": "NAND_WP", "7": "NAND_HOLD", "4": "GND", "9": None},
         fields={"Alt": "MX35UF1GE4AD-Z4I6 / GD5F1GQ5REYIGR (same pinout)"}),
    C("C9", "4.7u", "NAND_1V8", group="nand", fields={"Spec": "10V X5R"}),
    C("C10", "100n", "NAND_1V8", group="nand"),
    R("R7", "100k", "NAND_WP", "NAND_1V8", group="nand"),
    R("R8", "100k", "NAND_HOLD", "NAND_1V8", group="nand"),
    R("R9", "100k", "NAND_CS", "NAND_1V8", group="nand"),
    R("R10", "0R", "NAND_SCK_MCU", "NAND_SCK", group="nand"),
    R("R11", "0R", "NAND_MOSI_MCU", "NAND_MOSI", group="nand"),
    R("R12", "0R", "NAND_MISO", "NAND_MISO_MCU", group="nand"),
    # --- microphone, privacy latch, buffer (DESIGN.md §3.5) ---
    dict(ref="U6", sym=TPS22917, value="TPS22917DBVR", fp="Package_TO_SOT_SMD:SOT-23-6", group="mic",
         nets={"1": "1V8", "3": "MIC_EN", "4": None, "6": "MIC_1V8_SW", "5": None, "2": "GND"}),
    R("R13", "1M", "MIC_EN", "GND", group="mic"),
    R("R14", "0R R_MIC", "MIC_1V8_SW", "MIC_1V8_PRE", group="mic"),
    dict(ref="SW1", sym="Switch:SW_SPST", value="PRIVACY", fp="Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm",
         nets={"1": "MIC_1V8_PRE", "2": "MIC_1V8"}, group="mic",
         fields={"Note": "privacy latch; solder jumper until the slide/magnetic part is chosen"}),
    dict(ref="U7", sym=AUP3G34, value="SN74AUP3G34DCUR", fp="Package_SO:VSSOP-8_2.3x2mm_P0.5mm", group="mic",
         nets={"1": "I2S_SCK_MCU", "3": "I2S_WS_MCU", "6": "THSEL_MCU", "4": "GND",
               "7": "MIC_SCK", "5": "MIC_WS", "2": "MIC_THSEL", "8": "MIC_1V8"},
         ds="https://www.ti.com/lit/ds/symlink/sn74aup3g34.pdf"),
    C("C11", "100n", "MIC_1V8", group="mic"),
    dict(ref="U8", sym=T5848, value="MMICT5848-00-012", fp="pendant:TDK_T5848_LGA-8_3.5x2.65mm", group="mic",
         nets={"6": "MIC_SCK", "1": "MIC_WS", "5": "MIC_THSEL", "8": "MIC_SD", "2": "MIC_SEL", "7": "MIC_1V8",
               "4": "MIC_WAKE", "3": "GND"},
         fields={"Note": "bottom port I2S; pads from DS-000479 Fig. 3 + 33"}),
    R("R26", "100k", "MIC_SD", "GND", group="mic", fields={"Note": "SD pull-down (T5848 datasheet)"}),
    C("C12", "100n", "MIC_1V8", group="mic", fields={"Spec": "X7R"}),
    R("R15", "0R", "MIC_SEL", "GND", group="mic"),
    R("R16", "0R", "MIC_SEL", "MIC_1V8", dnp=True, group="mic"),
    # --- battery voltage sense, button (DESIGN.md §3.6) ---
    dict(ref="U9", sym=TPS22917, value="TPS22917DBVR", fp="Package_TO_SOT_SMD:SOT-23-6", group="sense",
         nets={"1": "VBAT_SYS", "3": "BATSENSE_EN", "4": None, "6": "VDIV_TOP", "5": None, "2": "GND"}),
    R("R17", "1M", "BATSENSE_EN", "GND", group="sense"),
    R("R18", "1M", "VDIV_TOP", "VBAT_ADC", group="sense", fields={"Spec": "1%"}),
    R("R19", "1M", "VBAT_ADC", "GND", group="sense", fields={"Spec": "1%"}),
    C("C13", "10n", "VBAT_ADC", group="sense"),
    dict(ref="SW2", sym="Switch:SW_Push", value="B3U-1000P", fp="Button_Switch_SMD:SW_SPST_B3U-1000P",
         nets={"1": "BTN", "2": "GND"}, group="sense"),
    # --- RTC footprint, not populated (DESIGN.md §3.7) ---
    dict(ref="U10", sym=RV3032, value="RV-3032-C7", fp="Package_SON:MicroCrystal_C7_SON-8_1.5x3.2mm_P0.9mm", dnp=True, group="rtc",
         nets={"6": "RTC_VDD", "1": "RTC_VBAK", "5": "GND", "4": None, "2": "I2C_SDA", "8": "I2C_SCL", "3": None, "7": None}),
    R("R20", "0R R_RTC", "1V8", "RTC_VDD", dnp=True, group="rtc"),
    R("R21", "10k", "RTC_VBAK", "GND", dnp=True, group="rtc"),
    C("C14", "100n", "RTC_VDD", dnp=True, group="rtc"),
    R("R22", "10k", "I2C_SDA", "RTC_VDD", dnp=True, group="rtc"),
    R("R23", "10k", "I2C_SCL", "RTC_VDD", dnp=True, group="rtc"),
    # --- terminal indicator, provisional and not populated (DESIGN.md §3.8) ---
    R("R24", "10k", "VBAT_SYS", "VLG_CHG", dnp=True, group="ind"),
    dict(ref="D1", sym="Device:D_Schottky_Small", value="Schottky", fp="Diode_SMD:D_SOD-323", dnp=True,
         nets={"1": "VLG", "2": "VLG_CHG"}, group="ind"),
    C("C15", "TBD", "VLG", dnp=True, fp="Capacitor_SMD:C_1206_3216Metric", group="ind"),
    dict(ref="U11", sym=SUPV, value="TPS3840PH (TBD)", fp="Package_TO_SOT_SMD:SOT-23-5", dnp=True, group="ind",
         nets={"2": "VBAT_SYS", "3": "GND", "1": "TERM_TRIG", "4": None, "5": None}),  # MR, CT open
    dict(ref="U12", sym="Driver_Motor:DRV8837C", value="DRV8837DSGR", dnp=True, group="ind",
         fp="Package_SON:Texas_DSG0008A_WSON-8-1EP_2x2mm_P0.5mm_EP0.9x1.6mm",
         nets={"1": "VLG", "2": "IND_A", "3": "IND_B", "4": "GND", "5": "TERM_TRIG", "6": "IND_RUN", "7": "IND_SLEEP",
               "8": "VLG", "9": "GND"}),
    dict(ref="D2", sym="Device:D_Schottky_Small", value="Schottky", fp="Diode_SMD:D_SOD-323", dnp=True,
         nets={"1": "IND_SLEEP", "2": "TERM_TRIG"}, group="indor"),
    dict(ref="D3", sym="Device:D_Schottky_Small", value="Schottky", fp="Diode_SMD:D_SOD-323", dnp=True,
         nets={"1": "IND_SLEEP", "2": "IND_RUN"}, group="indor"),
    R("R25", "1M", "IND_SLEEP", "GND", dnp=True, group="indor"),
    # --- debug and test points (DESIGN.md §3.9) ---
    dict(ref="J1", sym="Connector:Conn_ARM_SWD_TagConnect_TC2030", value="SWD pogo", group="dbg",
         fp="pendant:SWD_Pogo_2x03_P1.27mm_NoHoles",
         nets={"1": "1V8", "2": "SWDIO", "3": "nRESET", "4": "SWCLK", "5": "GND", "6": None}),
    TP("TP1", "VBAT"), TP("TP2", "VBAT_SYS"), TP("TP3", "1V8_BUCK", "buck"), TP("TP4", "1V8"), TP("TP5", "NAND_1V8"),
    TP("TP6", "MIC_1V8"), TP("TP7", "GND"), TP("TP8", "GND"), 
    TP("TP12", "UART_TX", "tpsig"), TP("TP13", "UART_RX", "tpsig"), TP("TP14", "MK_REC", "tpsig"),
    TP("TP15", "MK_STORE", "tpsig"), TP("TP16", "MK_BLE", "tpsig"), TP("TP17", "VBAT_ADC", "sense"), TP("TP18", "IND_A", "ind"), TP("TP19", "IND_B", "ind"),
    TP("TP20", "MIC_1V8_PRE", "mic"),
    # --- power flags for ERC (VBAT_SYS is driven by U1's VOUT) ---
    *[FLAG(i + 1, n) for i, n in enumerate(["GND", "VBAT", "1V8", "NAND_1V8", "MIC_1V8", "RTC_VDD", "VLG"])],
    FLAG(8, "VBAT_IN", "pwr"),
]
