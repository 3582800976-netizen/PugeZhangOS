#!/usr/bin/env python3
"""Check the actual RISC-V memmove in isolated Lab 2-6 QEMU kernels."""
import argparse
import os
from pathlib import Path
import re
import selectors
import shutil
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out-dir', required=True, type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
out = args.out_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
for lab in range(2, 7):
    with tempfile.TemporaryDirectory(prefix=f'memmove-qemu-lab{lab}-') as tmp:
        course = Path(tmp) / 'course'
        shutil.copytree(repo / f'lab{lab}/course', course,
                        ignore=shutil.ignore_patterns('target'))
        shutil.copyfile(repo / 'scripts/tests/memmove_kernel.h',
                        course / 'memmove_probe.h')
        main = course / 'src/kernel/main.c'
        source, replaced = re.subn(r'(?m)^void main\(',
                                   '#include "memmove_probe.h"\n\nvoid main(',
                                   main.read_text(), count=1)
        if replaced != 1 or source.count('print_init();') != 1:
            raise SystemExit(f'Lab {lab}: main entry changed; review test insertion')
        source = source.replace('print_init();', 'print_init();\n        memmove_probe();', 1)
        main.write_text(source)
        build = subprocess.run(['make', 'build'], cwd=course, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, timeout=60)
        (out / f'lab{lab}-build.log').write_bytes(build.stdout)
        if build.returncode:
            raise SystemExit(f'Lab {lab}: build failed')
        kernel = ('target/kernel/kernel-qemu.elf' if lab <= 4 else
                  'target/qemu/kernel/kernel-qemu.elf')
        command = [os.environ.get('QEMU', 'qemu-system-riscv64'), '-machine', 'virt',
                   '-bios', 'default', '-kernel', kernel, '-m', '130M', '-smp', '2', '-nographic']
        process = subprocess.Popen(command, cwd=course, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = b''
        marker = b'[memmove] RV64 PASS cases=3570'
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                deadline = time.monotonic() + 15
                while marker not in output and time.monotonic() < deadline:
                    for key, _ in selector.select(.1):
                        output += os.read(key.fd, 65536)
                    if process.poll() is not None:
                        break
        finally:
            process.terminate()
            try:
                tail, _ = process.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                tail, _ = process.communicate()
            output += tail
            (out / f'lab{lab}-runtime.log').write_bytes(output)
        if marker not in output or b'panic' in output.lower():
            raise SystemExit(f'Lab {lab}: RISC-V memmove check failed; see runtime log')
        print(f'Lab {lab}: RV64 QEMU memmove PASS (3570 snapshot cases)', flush=True)
