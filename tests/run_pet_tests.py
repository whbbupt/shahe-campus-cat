"""Build native model tests; --sdk also renders the production UI using real LVGL.

Run from a native compiler environment. Windows: --cc cl --vcvars PATH_TO_VCVARS64.
Outputs/cache stay under ignored build/pet-host unless --out is supplied.
"""
import argparse
import concurrent.futures
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cc', default=os.environ.get('CC', 'cl' if os.name == 'nt' else 'cc'))
p.add_argument('--vcvars', type=Path)
p.add_argument('--sdk', type=Path)
p.add_argument('--out', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = (a.out or root / 'build/pet-host').resolve()
out.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if a.vcvars:
    # cmd uses its own quoting rules; list2cmdline's backslash-escaped quotes
    # would be interpreted literally by CALL, so pass one Windows command line.
    command = f'cmd /d /s /c ""{a.vcvars.resolve()}" >nul && set"'
    r = subprocess.run(command, capture_output=True)
    if r.returncode:
        raise RuntimeError(r.stderr.decode('mbcs', errors='replace'))
    # Environment text stays local; never print the complete environment.
    for line in r.stdout.decode('mbcs', errors='replace').splitlines():
        key, separator, value = line.partition('=')
        if separator and key:
            env[key.upper()] = value
cc = shutil.which(a.cc, path=env.get('Path', env.get('PATH'))) or a.cc
msvc = Path(cc).name.lower() in ('cl', 'cl.exe')

def run(command, **kwargs):
    r = subprocess.run([str(x) for x in command], env=env, capture_output=True, **kwargs)
    if r.returncode:
        print(r.stdout.decode('utf-8', errors='replace'))
        print(r.stderr.decode('utf-8', errors='replace'))
        raise SystemExit(r.returncode)
    return r.stdout.decode('utf-8', errors='replace')

def build(sources, name, includes=(), defines=()):
    include_paths = [root/'src', *includes]
    base = (['/nologo', '/std:c11', '/utf-8', '/O1', '/D_CRT_SECURE_NO_WARNINGS'] if msvc
            else ['-std=c11', '-O1', '-Wall', '-Wextra'])
    flags = base + [(('/I' if msvc else '-I') + str(x)) for x in include_paths]
    flags += [(('/D' if msvc else '-D') + x) for x in defines]
    # Cache key includes all production/header/config content relevant to these tests.
    headers = list((root/'src').glob('*.h')) + list((root/'tests/include').glob('*.h'))
    if a.sdk:
        headers += list((a.sdk/'external/lvgl_v9').rglob('*.h'))
    newest = max((x.stat().st_mtime for x in headers), default=0)
    def compile_one(source):
        key = hashlib.sha256((str(source)+repr(flags)+cc).encode()).hexdigest()[:20]
        obj = out/(key + ('.obj' if msvc else '.o'))
        if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, newest):
            command = [cc, *flags, '/c', source, '/Fo'+str(obj)] if msvc else [cc, *flags, '-c', source, '-o', obj]
            run(command, cwd=out)
        return obj
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        objects = list(pool.map(compile_one, sources))
    executable = out/(name + ('.exe' if os.name == 'nt' else ''))
    if msvc:
        response = out/(name+'.rsp')
        response.write_text('\n'.join('"'+str(x)+'"' for x in objects), encoding='utf-8')
        run([cc, '/nologo', '@'+str(response), '/Fe'+str(executable)], cwd=out)
    else:
        run([cc, *objects, '-lm', '-o', executable], cwd=out)
    print(run([executable], cwd=out, timeout=90))

build([root/'src/pet_model.c', root/'src/pet_selftest.c', root/'tests/pet_model_test.c'], 'pet-model-test')
build([root/'src/pet_world.c', root/'src/pet_world_selftest.c', root/'tests/pet_world_test.c'], 'pet-world-test')
build([root/'src/pet_world.c', root/'src/pet_save.c', root/'src/pet_save_selftest.c', root/'tests/pet_save_test.c'], 'pet-save-test')
build([root/'src/pet_model.c', root/'src/pet_world.c', root/'src/pet_world_selftest.c', root/'src/pet_save.c', root/'src/pet_save_selftest.c', root/'src/pet_save_flash.c', root/'src/pet_app.c', root/'tests/pet_save_app_test.c'], 'pet-save-app-test', [root/'tests/save_include'])
if a.sdk:
    lvgl = a.sdk.resolve()/'external/lvgl_v9'
    sources = sorted((lvgl/'src').rglob('*.c'))
    sources += [root/'src/pet_world.c', root/'src/pet_ui.c', root/'src/pet_world_assets.c', root/'src/pet_campus_assets.c', root/'src/pet_shahe_assets.c', root/'src/pet_world_font.c', root/'tests/pet_world_ui_test.c']
    build(sources, 'pet-ui-test', [root/'tests/include', lvgl], ['LV_KCONFIG_IGNORE', 'LV_CONF_INCLUDE_SIMPLE'])
    print('Actual LVGL software previews:', out)
