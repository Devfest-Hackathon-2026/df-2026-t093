#!/usr/bin/env python3
"""Check critical connections in KiCad's exported netlist, not electrical safety."""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET


SCHEMATIC = Path(__file__).with_name('premium_power.kicad_sch')

# Independent review points: source isolation, TI pin mapping and the shared
# 1.8 V NAND rail. These assertions are not a simulation or hardware approval.
EXPECTED = {
    ('U1', '2'): 'SOLAR', ('U1', '18'): 'HSL', ('U1', '19'): 'VSTOR',
    ('U1', '9'): 'SEC_GATE', ('U1', '10'): 'PRI_GATE',
    ('U1', '13'): 'VBAT_OK_RAW', ('U1', '14'): 'COIN',
    ('J2', '1'): 'CAP_EXT', ('R8', '1'): 'CAP_EXT', ('R8', '2'): 'HSL',
    ('J3', '1'): 'COIN_RAW', ('D1', '2'): 'COIN_RAW',
    ('D1', '1'): 'COIN_DIODE', ('R6', '1'): 'COIN_DIODE',
    ('R6', '2'): 'COIN', ('C8', '1'): 'COIN',
    ('U3', '1'): 'VSTOR', ('U3', '3'): 'VSTOR_D', ('U3', '6'): 'VSTOR_D',
    ('U4', '1'): 'COIN', ('U4', '3'): 'COIN_D', ('U4', '6'): 'COIN_D',
    ('U3', '5'): 'GND', ('U4', '5'): 'GND',
    ('Q1', 'A1'): 'VSTOR_D', ('Q1', 'C2'): 'VOR',
    ('Q1', 'A2'): 'SEC_GATE', ('Q1', 'C1'): 'SEC_GATE',
    ('Q1', 'B1'): 'SEC_MID', ('Q1', 'B2'): 'SEC_MID',
    ('Q2', 'A1'): 'COIN_D', ('Q2', 'C2'): 'VOR',
    ('Q2', 'A2'): 'PRI_GATE', ('Q2', 'C1'): 'PRI_GATE',
    ('Q2', 'B1'): 'PRI_MID', ('Q2', 'B2'): 'PRI_MID',
    ('U2', '6'): 'VOR', ('U2', '5'): 'VOR', ('U2', '4'): 'GND',
    ('U2', '2'): 'SW', ('U2', '1'): '1V8_BUCK',
    ('U2', '8'): 'GND', ('U2', '9'): 'GND',
    ('L2', '1'): 'SW', ('L2', '2'): '1V8_BUCK',
    ('R7', '1'): '1V8_BUCK', ('R7', '2'): '1V8',
    ('U5', '2'): 'VBAT_OK_RAW', ('U5', '4'): 'VBAT_OK_1V8',
    ('U5', '5'): '1V8', ('U103', 'B2'): 'VBAT_OK_1V8',
    ('U103', 'F6'): '1V8', ('R106', '1'): '1V8',
    ('R106', '2'): 'NAND_1V8', ('U105', '8'): 'NAND_1V8',
    ('U103', 'A6'): 'NAND_CS', ('U105', '1'): 'NAND_CS',
    ('U103', 'C6'): 'NAND_SCK_MCU', ('R110', '1'): 'NAND_SCK_MCU',
    ('R110', '2'): 'NAND_SCK', ('U105', '6'): 'NAND_SCK',
    ('U103', 'D6'): 'NAND_MOSI_MCU', ('R111', '1'): 'NAND_MOSI_MCU',
    ('R111', '2'): 'NAND_MOSI', ('U105', '5'): 'NAND_MOSI',
    ('U105', '2'): 'NAND_MISO', ('R112', '1'): 'NAND_MISO',
    ('R112', '2'): 'NAND_MISO_MCU', ('U103', 'F5'): 'NAND_MISO_MCU',
    ('SW101', '1'): 'MIC_1V8_PRE', ('SW101', '2'): 'MIC_1V8',
    ('U107', '8'): 'MIC_1V8', ('U108', '7'): 'MIC_1V8',
    ('U103', 'D1'): 'I2S_SCK_MCU', ('U107', '1'): 'I2S_SCK_MCU',
    ('U107', '7'): 'MIC_SCK', ('U108', '6'): 'MIC_SCK',
    ('U103', 'E2'): 'MIC_SD', ('U108', '8'): 'MIC_SD',
}


def main():
    with tempfile.TemporaryDirectory(prefix='premium-netlist-') as directory:
        netlist = Path(directory) / 'netlist.xml'
        subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '--format',
                        'kicadxml', '-o', str(netlist), str(SCHEMATIC)],
                       check=True, capture_output=True, text=True)
        root = ET.parse(netlist).getroot()

    nets = {}
    unconnected = set()
    for net in root.findall('./nets/net'):
        name = net.get('name').lstrip('/')
        nodes = net.findall('node')
        for node in nodes:
            pin = (node.get('ref'), node.get('pin'))
            nets[pin] = name
            if len(nodes) == 1 and 'no_connect' in node.get('pintype', ''):
                unconnected.add(pin)

    errors = [f'{ref}.{pin}: expected {name}, got {nets.get((ref, pin))}'
              for (ref, pin), name in EXPECTED.items()
              if nets.get((ref, pin)) != name]
    for pin in (('U103', 'E4'), ('U103', 'B5'), ('U2', '7')):
        if pin not in unconnected:
            errors.append(f'{pin[0]}.{pin[1]} must remain unconnected')
    if {p for p, net in nets.items() if net == 'COIN_RAW'} != {('J3', '1'), ('D1', '2')}:
        errors.append('COIN_RAW must connect only to J3 and D1')
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'Checked {len(EXPECTED)} critical net assignments; physical safety remains unverified.')


if __name__ == '__main__':
    main()
