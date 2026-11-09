#!/usr/bin/env python3
"""Recheck default, overridden, and missing toolchain selection."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[3]
script = repo / 'scripts/check-course-user-layout.py'
clean_env = os.environ.copy()
clean_env.pop('TOOLPREFIX', None)
print('Automatic toolchain detection:', flush=True)
subprocess.run(['python3', str(script)], env=clean_env, check=True)
with tempfile.TemporaryDirectory(prefix='course-toolchain-') as temp:
    prefix = str(Path(temp) / 'custom-riscv-')
    choices = ('riscv64-linux-gnu-', 'riscv64-unknown-elf-',
               'riscv64-elf-', 'riscv64-none-elf-')
    chosen = next((p for p in choices if all(shutil.which(p + n) for n in ('as', 'ld'))), None)
    if not chosen:
        raise SystemExit('Installed RISC-V assembler and linker missing')
    for name in ('as', 'ld'):
        os.symlink(shutil.which(chosen + name), prefix + name)
    custom_env = clean_env | {'TOOLPREFIX': prefix}
    print('Explicit TOOLPREFIX using a temporary toolchain prefix:', flush=True)
    subprocess.run(['python3', str(script)], env=custom_env, check=True)
    bad_env = clean_env | {'TOOLPREFIX': str(Path(temp) / 'missing-')}
    result = subprocess.run(['python3', str(script)], env=bad_env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    if result.returncode == 0 or 'assembler and linker not found' not in result.stdout:
        raise SystemExit('Missing TOOLPREFIX did not fail clearly')
    print('Missing TOOLPREFIX: PASS (clear error, nonzero exit)', flush=True)
