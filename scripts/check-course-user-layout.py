#!/usr/bin/env python3
"""Exercise Lab 5/6 user linker scripts without changing the course trees."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

def select_toolprefix():
    """Match the course Makefiles' supported toolchains, or honor TOOLPREFIX."""
    requested = os.environ.get('TOOLPREFIX')
    prefixes = [requested] if requested else [
        'riscv64-linux-gnu-', 'riscv64-unknown-elf-',
        'riscv64-elf-', 'riscv64-none-elf-',
    ]
    for prefix in prefixes:
        if all(shutil.which(prefix + tool) for tool in ('as', 'ld')):
            return prefix
    raise SystemExit('RISC-V assembler and linker not found; set TOOLPREFIX to their common prefix')

repo = Path(__file__).resolve().parents[1]
toolprefix = select_toolprefix()
with tempfile.TemporaryDirectory(prefix='course-user-layout-') as tmp:
    tmp = Path(tmp)
    cases = {
        'code-only': '.section .text.entry\n.globl main\nmain: ret\n',
        'writable-data': '.section .text.entry\n.globl main\nmain: ret\n.data\n.quad 1\n',
        'writable-bss': '.section .text.entry\n.globl main\nmain: ret\n.bss\n.zero 8\n',
    }
    for name, source in cases.items():
        asm = tmp / (name + '.S')
        obj = tmp / (name + '.o')
        asm.write_text(source)
        subprocess.run([toolprefix + 'as', str(asm), '-o', str(obj)], check=True)
        for lab in (5, 6):
            script = repo / f'lab{lab}/course/user.ld'
            result = subprocess.run([toolprefix + 'ld', '-T', str(script), str(obj),
                                     '-o', str(tmp / f'{name}-{lab}.elf')],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            expected = result.returncode == 0 if name == 'code-only' else (
                result.returncode != 0 and 'user image contains writable static data' in result.stdout)
            if not expected:
                raise SystemExit(f'Lab {lab} {name}: FAIL\n{result.stdout}')
            print(f'Lab {lab} {name}: PASS')
