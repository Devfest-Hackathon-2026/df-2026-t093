#!/usr/bin/env python3
"""Generate a bench-only Premium power circuit. No real cell or hybrid capacitor."""
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'standard'))
from gen_schematic import custom, build, check
from core_parts import make_core
import sexpdata as sx

P='passive'
BQ=custom('BQ25505RGR', [('2','VIN_DC',P),('20','LBOOST',P),('3','VOC_SAMP',P),('4','VREF_SAMP',P),('5','EN',P),('14','VBAT_PRI',P),('18','VBAT_SEC',P),('19','VSTOR',P),('1','VSS',P),('15','VSS',P),('21','EP',P)], [('8','VRDIV',P),('7','VBAT_OV',P),('12','OK_PROG',P),('11','OK_HYST',P),('13','VBAT_OK',P),('9','VB_SEC_ON',P),('10','VB_PRI_ON',P),('6','NC_GND',P),('16','NC_GND',P),('17','NC_GND',P)])
BUCK=custom('TPS62842DGR', [('6','VIN',P),('5','EN',P),('3','MODE',P),('4','VSET',P),('8','GND',P),('9','EP',P)], [('2','SW',P),('1','VOS',P),('7','NC','no_connect')])
# Assign types based on the device pin function; MOSFET conduction nodes remain passive.
def assign_types(symbol, types):
    def walk(node):
        if not isinstance(node, list) or not node: return
        if str(node[0]) == 'pin':
            number = next(x[1] for x in node if isinstance(x, list) and x and str(x[0]) == 'number')
            node[1] = sx.Symbol(types[str(number)])
        for child in node: walk(child)
    walk(symbol[0])
assign_types(BQ, {'1':'power_in','2':'power_in','3':'input','4':'input','5':'input','6':'passive','7':'input','8':'output','9':'output','10':'output','11':'input','12':'input','13':'output','14':'input','15':'power_in','16':'passive','17':'passive','18':'passive','19':'power_out','20':'passive','21':'power_in'})
assign_types(BUCK, {'1':'input','2':'power_out','3':'input','4':'input','5':'input','6':'power_in','7':'no_connect','8':'power_in','9':'power_in'})
# WLP top view: A1=D2, A2=G2, B1/B2=S, C1=G1, C2=D1.
DUAL=custom('CSD75208W1015', [('A1','D2',P),('A2','G2',P),('B1','S',P)], [('C2','D1',P),('C1','G1',P),('B2','S',P)],ref='Q')
LM=custom('LM66100DCK', [('1','VIN',P),('2','GND',P),('3','CE',P)], [('6','VOUT',P),('5','ST',P),('4','NC',P)])
assign_types(LM, {'1':'power_in','2':'power_in','3':'input','4':'no_connect','5':'open_collector','6':'power_out'})
BUF=custom('SN74LVC1G17DBV', [('1','NC','no_connect'),('2','A','input'),('3','GND','power_in')], [('5','VCC','power_in'),('4','Y','output')])
parts=[]
def add(ref,sym,value,nets,fp='',ds=''):
    parts.append(dict(ref=ref,sym=sym,value=value,nets=nets,fp=fp,ds=ds))
def R(ref,v,a,b,fp='Resistor_SMD:R_0402_1005Metric'): add(ref,'Device:R_Small',v,{'1':a,'2':b},fp)
def C(ref,v,a,fp='Capacitor_SMD:C_0402_1005Metric'): add(ref,'Device:C_Small',v,{'1':a,'2':'GND'},fp)
def L(ref,v,a,b,fp): add(ref,'Device:L_Small',v,{'1':a,'2':b},fp)
def J(ref,v,a,b): add(ref,'Connector_Generic:Conn_01x02',v,{'1':a,'2':b},'Connector_Wire:SolderWire-0.15sqmm_1x02_P4mm_D0.5mm_OD1.5mm')
add('U1',BQ,'BQ25505RGR',{'1':'GND','2':'SOLAR','3':'VSTOR','4':'VREF','5':'GND','6':'GND','7':'OV','8':'VRDIV','9':'SEC_GATE','10':'PRI_GATE','11':'OK_HYST','12':'OK_PROG','13':'VBAT_OK_RAW','14':'COIN','15':'GND','16':'GND','17':'GND','18':'HSL','19':'VSTOR','20':'LBOOST','21':'GND'},'premium:TI_RGR0020A_BQ25505_Eval','https://www.ti.com/lit/ds/symlink/bq25505.pdf')
add('U2',BUCK,'TPS62842DGRR EVAL',{'1':'1V8_BUCK','2':'SW','3':'GND','4':'GND','5':'VOR','6':'VOR','7':None,'8':'GND','9':'GND'},'Package_SO:HVSSOP-8-1EP_3x3mm_P0.65mm_EP1.57x1.89mm','https://www.ti.com/lit/ds/symlink/tps62840.pdf')
add('U5',BUF,'SN74LVC1G17DBVR',{'1':None,'2':'VBAT_OK_RAW','3':'GND','4':'VBAT_OK_1V8','5':'1V8'},'Package_TO_SOT_SMD:SOT-23-5','https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf')
for ref,src,dst,gate,mid in [('Q1','VSTOR_D','VOR','SEC_GATE','SEC_MID'),('Q2','COIN_D','VOR','PRI_GATE','PRI_MID')]:
    add(ref,DUAL,'CSD75208W1015',{'A1':src,'A2':gate,'B1':mid,'B2':mid,'C1':gate,'C2':dst},'premium:TI_CSD75208W1015_Eval', 'https://www.ti.com/lit/gpn/csd75208w1015')
for ref,src,dst in [('U3','VSTOR','VSTOR_D'),('U4','COIN','COIN_D')]:
    add(ref,LM,'LM66100DCK',{'1':src,'2':'GND','3':dst,'4':None,'5':'GND','6':dst},'Package_TO_SOT_SMD:SOT-363_SC-70-6','https://www.ti.com/lit/ds/symlink/lm66100.pdf')
R('R1','6.49M 0.1%','VRDIV','OV'); R('R2','6.34M 0.1%','OV','GND')
R('R3','787k 0.1%','VRDIV','OK_HYST'); R('R4','6.34M 0.1%','OK_HYST','OK_PROG'); R('R5','4.87M 0.1%','OK_PROG','GND')
# 0805 footprints and 10 uF nominal are provisional; DC-bias derating is not verified.
C('C1','10u 16V EVAL','SOLAR','Capacitor_SMD:C_0805_2012Metric'); C('C2','10n low leak','VREF'); C('C3','10u 16V EVAL','VSTOR','Capacitor_SMD:C_0805_2012Metric'); C('C4','100n','VSTOR'); C('C5','10u 16V EVAL','VOR','Capacitor_SMD:C_0805_2012Metric'); C('C6','10u 16V EVAL','1V8_BUCK','Capacitor_SMD:C_0805_2012Metric'); C('C7','100n','1V8')
C('C8','BULK TBD - NO BUILD','COIN','Capacitor_SMD:C_1210_3225Metric')
L('L1','22u LPS4018 EVAL','SOLAR','LBOOST','Inductor_SMD:L_Coilcraft_LPS4018'); L('L2','2.2u DFE201612E EVAL','SW','1V8_BUCK','premium:L_Murata_DFE201612E_Eval')
R('R7','0R R_MAIN','1V8_BUCK','1V8','Resistor_SMD:R_0603_1608Metric')
R('R8','0R R_CAP','CAP_EXT','HSL','Resistor_SMD:R_0603_1608Metric')
J('J1','SOLAR INPUT','SOLAR','GND')
# 30F bottom-side body projection and lead bends are an unverified evaluation hypothesis.
add('J2','Connector_Generic:Conn_01x02','CAP EMULATOR ONLY',{'1':'CAP_EXT','2':'GND'},'premium:Vina_VEL08203R8306G_Bottom_Horizontal_Eval')
# EVALUATION HYPOTHESIS: two-lead positive. Verify polarity and B-side mirror on the actual CH005 before fabrication.
add('J3','Connector_Generic:Conn_01x02','PRIMARY EMULATOR ONLY',{'1':'COIN_RAW','2':'GND'},'premium:CH005_2032LF_Bottom_Eval_PolarityHypothesis')
# Emulator-only screening candidates; real-cell reverse charge and faults are not qualified.
add('D1','Device:D_Small','BAT41ZFILM EVAL ONLY',{'1':'COIN_DIODE','2':'COIN_RAW'},'Diode_SMD:D_SOD-123','https://www.st.com/resource/en/datasheet/bat41.pdf')
R('R6','1.2k 1% EVAL ONLY','COIN_DIODE','COIN','Resistor_SMD:R_0603_1608Metric')
for i,net in enumerate(('SOLAR','HSL','VSTOR','COIN','VOR','1V8_BUCK','SEC_GATE','PRI_GATE','CAP_EXT','MIC_EN','VBAT_OK_1V8','VBAT_OK_RAW'),1):
    add(f'TP{i}','Connector:TestPoint',net,{'1':net},'TestPoint:TestPoint_Pad_D1.0mm')
# Flags identify external supplies and power paths through passive switches.
for i, net in enumerate(('GND','SOLAR','COIN','VOR','1V8','NAND_1V8','MIC_1V8'), 1):
    add(f'#FLG{i}', 'power:PWR_FLAG', 'PWR_FLAG', {'1':net})
core_parts, core_layout = make_core()
parts.extend(core_parts)
# On this draft sheet every connection is a named net; no routing to unrelated sheet areas.
nets={n for p in parts for n in p['nets'].values() if n}
positions = {
    'J1':(10,25),'L1':(10,34),'C1':(10,43),'C2':(10,52),
    'U1':(30,33),'C3':(54,28),'C4':(54,38),
    'R1':(68,25),'R2':(68,35),'R3':(85,25),'R4':(85,35),'R5':(85,45),
    'J2':(10,63),'U3':(32,63),'Q1':(62,63),
    'J3':(10,78),'D1':(24,78),'R6':(37,78),'C8':(47,86),'U4':(60,78),'Q2':(90,78),'C5':(110,78),
    'U2':(26,96),'L2':(52,92),'C6':(67,92),
    'R7':(83,102),'R8':(10,71),
    'U5':(105,96),'C7':(125,96),
    '#FLG1':(145,25),'#FLG2':(145,33),'#FLG3':(145,41),'#FLG4':(145,49),'#FLG5':(145,57),
    '#FLG6':(145,65),'#FLG7':(145,73),
    **{f'TP{i}':(180, 17 + 8*i) for i in range(1,11)},
    'TP11':(195,25),'TP12':(195,33),
}
layout={ref:('at',*position,0) for ref, position in positions.items()}
layout.update({ref: ('at', x + 10, y + 115, rotation) for ref, (_, x, y, rotation) in core_layout.items()})
sch=HERE/'premium_power.kicad_sch'
build(parts,layout,nets,str(sch),str(HERE/'pendant.kicad_sym'),'Premium power and shared core - BENCH DRAFT ONLY',paper='A1',titles=[(8,7,'DRAFT: NO REAL CELL / CAP - SOURCE EMULATORS ONLY'),(8,10,'RAW VBAT_OK IS NOT 1V8 LOGIC - LEVEL SHIFT REQUIRED'),(8,13,'REVIEW REVERSE CHARGE, STARTUP AND SOLAR BEFORE HARDWARE'),(8,110,'SHARED CORE - POWER STATE AND PRIVACY NOT QUALIFIED')])
# The reused Standard generator hard-codes the project instance name.
sch.write_text(sch.read_text().replace('(project "standard"','(project "premium_power"'))
# Replace the generator's named VCC symbols with true same-sheet net labels.
# Keep GND symbols. Labels do not assert that every signal is a power input.
root = sx.loads(sch.read_text())
new = []
for node in root:
    if isinstance(node, list) and node and node[0] == sx.Symbol('symbol'):
        attrs = {str(x[0]): x for x in node[1:] if isinstance(x, list) and x}
        if attrs.get('lib_id', [None, None])[1] == 'power:VCC':
            props = [x for x in node if isinstance(x, list) and x and x[0] == sx.Symbol('property')]
            name = next(x[2] for x in props if x[1] == 'Value')
            at = attrs['at']; uid = attrs['uuid'][1]
            new.append(sx.loads(f'(label {sx.dumps(name)} (at {at[1]} {at[2]} 0) (effects (font (size 1 1))) (uuid {sx.dumps(uid)}))'))
            continue
    new.append(node)
sch.write_text(sx.dumps(new) + '\n')
if not check(parts,str(sch)): raise SystemExit('netlist mismatch')
print('Netlist matches parts definitions')
