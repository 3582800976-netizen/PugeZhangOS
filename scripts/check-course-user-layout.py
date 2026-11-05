#!/usr/bin/env python3
"""Exercise Lab 5/6 user linker scripts without changing the course trees."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[1]
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
        subprocess.run(['riscv64-linux-gnu-as', str(asm), '-o', str(obj)], check=True)
        for lab in (5, 6):
            script = repo / f'lab{lab}/course/user.ld'
            result = subprocess.run(['riscv64-linux-gnu-ld', '-T', str(script), str(obj),
                                     '-o', str(tmp / f'{name}-{lab}.elf')],
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            expected = result.returncode == 0 if name == 'code-only' else (
                result.returncode != 0 and 'user image contains writable static data' in result.stdout)
            if not expected:
                raise SystemExit(f'Lab {lab} {name}: FAIL\n{result.stdout}')
            print(f'Lab {lab} {name}: PASS')
