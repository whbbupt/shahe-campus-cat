"""Check the actual SF32LB52 HCPU ELF embeds the freshly built LCPU image.

This detects the weak empty installer that still lets an HCPU-only build pass.
It does not establish that the radio works on a physical board.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', required=True, type=Path)
    parser.add_argument('--lcpu-bin', required=True, type=Path)
    parser.add_argument('--toolchain', required=True, type=Path)
    args = parser.parse_args()
    nm = args.toolchain / 'arm-none-eabi-nm.exe'
    output = subprocess.check_output([str(nm), '-S', str(args.elf)], text=True)
    symbols = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 4 and fields[3] in ('lcpu_img_install', 'g_lcpu_bin'):
            symbols[fields[3]] = (int(fields[0], 16), int(fields[1], 16), fields[2])

    errors = []
    installer = symbols.get('lcpu_img_install')
    if not installer or installer[2] != 'T' or installer[1] <= 2:
        errors.append('lcpu_img_install must be a nonempty strong implementation')
    blob = symbols.get('g_lcpu_bin')
    if not blob or blob[1] < 4:
        errors.append('g_lcpu_bin is missing or empty')

    digest = None
    if blob:
        address, length, _ = blob
        if not 0x12020000 <= address < address + length <= 0x12820000:
            errors.append('LCPU image must be inside the 8 MiB application region')
        data = args.elf.read_bytes()
        if data[:6] != b'\x7fELF\x01\x01':
            errors.append('expected little-endian ELF32')
        else:
            phoff = struct.unpack_from('<I', data, 28)[0]
            phsize, phcount = struct.unpack_from('<HH', data, 42)
            embedded = None
            for index in range(phcount):
                kind, offset, vaddr, _, filesz = struct.unpack_from(
                    '<IIIII', data, phoff + phsize * index)
                if kind == 1 and vaddr <= address and address + length <= vaddr + filesz:
                    start = offset + address - vaddr
                    embedded = data[start:start + length]
                    break
            expected = args.lcpu_bin.read_bytes()
            if embedded != expected:
                errors.append('embedded LCPU bytes differ from this build lcpu.bin')
            else:
                digest = hashlib.sha256(embedded).hexdigest().upper()

    print(json.dumps({'result': 'FAIL' if errors else 'PASS', 'symbols': symbols,
                      'lcpu_sha256': digest, 'errors': errors}, indent=2))
    return 1 if errors else 0


if __name__ == '__main__':
    raise SystemExit(main())
