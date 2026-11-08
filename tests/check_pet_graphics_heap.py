"""Verify actual graphics heap placement and allocator calls in the target ELF.

Host CLIB image decoding alone cannot prove an embedded system heap is large
enough. This check establishes PSRAM reservation; board initialization/FPS still
need physical evidence.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--elf', required=True, type=Path)
parser.add_argument('--toolchain', required=True, type=Path)
parser.add_argument('--expected-kib', type=int, default=4096)
args = parser.parse_args()
symbols = subprocess.check_output([
    str(args.toolchain / 'arm-none-eabi-nm.exe'), '-S', str(args.elf)], text=True, errors='replace')
match = re.search(r'^([0-9a-f]+) ([0-9a-f]+) [bB] lv_psram_heap_buffer$', symbols, re.M)
errors = []
address = size = None
if not match:
    errors.append('No dedicated PSRAM graphics heap in final ELF')
else:
    address, size = (int(value, 16) for value in match.groups())
    if not (size == args.expected_kib * 1024 and 0x60000000 <= address < address + size <= 0x60800000):
        errors.append('Graphics reservation must match expected KiB inside 8 MiB PSRAM')
for function, required_call in [('lv_mem_init', 'rt_memheap_init'),
                                 ('lv_malloc_core', 'rt_memheap_alloc')]:
    disassembly = subprocess.check_output([
        str(args.toolchain / 'arm-none-eabi-objdump.exe'), '-d',
        '--disassemble=' + function, str(args.elf)], text=True, errors='replace')
    if '<' + required_call + '>' not in disassembly:
        errors.append(function + ' does not call ' + required_call)
print(json.dumps({'result': 'FAIL' if errors else 'PASS',
                  'heap_address': hex(address) if address is not None else None,
                  'reserved_bytes': size, 'errors': errors}, indent=2))
raise SystemExit(1 if errors else 0)
