"""Losslessly pack generated Shahe arrays using this SDK's own LVGL LZ4.

Run after make_shahe_assets.py. The default is a dry run; --write applies it.
No SDK files are changed and no additional Python packages are needed.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--vcvars', type=Path, required=True)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    cache = root / 'build/pet-asset-codec'
    cache.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    command = f'cmd /d /s /c ""{args.vcvars.resolve()}" >nul && set"'
    setup = subprocess.run(command, capture_output=True, check=True)
    for line in setup.stdout.decode('mbcs', errors='replace').splitlines():
        key, separator, value = line.partition('=')
        if separator and key:
            env[key.upper()] = value
    source = args.sdk / 'external/lvgl_v9/src/libs/lz4/lz4.c'
    dll = cache / 'pet-lz4.dll'
    if not dll.exists() or dll.stat().st_mtime < source.stat().st_mtime:
        compiler = shutil.which('cl', path=env.get('PATH', env.get('Path')))
        if not compiler:
            raise RuntimeError('Native compiler missing after vcvars setup')
        # Same libc behavior as LVGL's clib backend, without linking its heap.
        support = cache / 'codec_memory.c'
        support.write_text('#include <string.h>\n#include <stdint.h>\n'
                           'void *lv_memcpy(void *d,const void *s,size_t n){return memcpy(d,s,n);}\n'
                           'void lv_memset(void *d,uint8_t c,size_t n){memset(d,c,n);}\n'
                           'void *lv_memmove(void *d,const void *s,size_t n){return memmove(d,s,n);}\n',
                           encoding='utf-8')
        result = subprocess.run([
            compiler, '/nologo', '/O2', '/LD', '/DLZ4_DLL_EXPORT=1',
            '/DLV_USE_LZ4=1', '/DLV_USE_LZ4_INTERNAL=1', '/DLV_KCONFIG_IGNORE',
            '/DLV_CONF_INCLUDE_SIMPLE', '/I' + str(root / 'tests/include'),
            str(source), str(support), '/Fe' + str(dll)],
            env=env, cwd=cache, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stdout.decode('mbcs', errors='replace') +
                               result.stderr.decode('mbcs', errors='replace'))
    codec = ctypes.CDLL(str(dll))
    codec.LZ4_compressBound.argtypes = [ctypes.c_int]
    for name in ['LZ4_compress_default', 'LZ4_decompress_safe']:
        function = getattr(codec, name)
        function.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        function.restype = ctypes.c_int
    target = root / 'src/pet_shahe_assets.c'
    text = target.read_text(encoding='utf-8')
    if 'LV_IMAGE_FLAGS_COMPRESSED' in text:
        raise RuntimeError('Already packed: regenerate raw arrays before packing again')
    entries = []

    def pack(match):
        name, body = match.group(1), match.group(2)
        raw = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', body))
        capacity = codec.LZ4_compressBound(len(raw))
        buffer = ctypes.create_string_buffer(capacity)
        length = codec.LZ4_compress_default(raw, buffer, len(raw), capacity)
        if length <= 0:
            raise RuntimeError('Compression failed: ' + name)
        compressed = buffer.raw[:length]
        decoded = ctypes.create_string_buffer(len(raw))
        assert codec.LZ4_decompress_safe(compressed, decoded, length, len(raw)) == len(raw)
        assert decoded.raw == raw, name
        # Official LVGLImage.py layout: method, compressed bytes, raw bytes, data.
        encoded = (2).to_bytes(4, 'little') + length.to_bytes(4, 'little')
        encoded += len(raw).to_bytes(4, 'little') + compressed
        use = len(encoded) < len(raw)
        entries.append({'name': name, 'raw_bytes': len(raw),
                        'packed_bytes': len(encoded) if use else len(raw),
                        'compressed': use, 'roundtrip': True,
                        'raw_sha256': hashlib.sha256(raw).hexdigest().upper()})
        if not use:
            return match.group(0)
        rows = [' ' + ','.join(f'0x{x:02x}' for x in encoded[i:i + 24]) + ','
                for i in range(0, len(encoded), 24)]
        return ('static const LV_ATTRIBUTE_MEM_ALIGN uint8_t ' + name +
                '_data[] = {\n' + '\n'.join(rows) + '\n};')

    pattern = r'static const LV_ATTRIBUTE_MEM_ALIGN uint8_t (\w+)_data\[\] = \{(.*?)\};'
    output = re.sub(pattern, pack, text, flags=re.S)
    assert entries, 'No generated arrays found'
    for entry in entries:
        if entry['compressed']:
            marker = 'const lv_image_dsc_t ' + entry['name'] + ' = {\n.header={'
            assert output.count(marker) == 1, entry['name']
            output = output.replace(marker, marker + '.flags=LV_IMAGE_FLAGS_COMPRESSED,', 1)
    raw_total = sum(x['raw_bytes'] for x in entries)
    packed_total = sum(x['packed_bytes'] for x in entries)
    record = {'method': 'SDK LVGL LZ4 lossless', 'write': args.write,
              'raw_bytes': raw_total, 'packed_bytes': packed_total,
              'saved_image_bytes': raw_total - packed_total,
              'largest_decoded_image_bytes': max(x['raw_bytes'] for x in entries),
              'entries': entries}
    if args.write:
        target.write_text(output, encoding='utf-8')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in record.items() if k != 'entries'}, indent=2))


if __name__ == '__main__':
    main()
