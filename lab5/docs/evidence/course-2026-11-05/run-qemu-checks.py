#!/usr/bin/env python3
"""Rebuild Lab 5 probes in isolated copies and check real QEMU output."""
from pathlib import Path
import selectors
import shutil
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[4]
course = repo / 'lab5/course'
evidence = Path(__file__).resolve().parent
cases = [
    ('baseline', None, (b'lab5: heap and mmap OK',)),
    ('usercheck', 'usercheck.c', (b'[lab5] user copy brk mmap stack PASS',)),
    ('codefault', 'codefault.c', (b'[lab5] write-code probe', b'cause=0x000000000000000f', b'[lab5] user stopped; kernel remains alive')),
]
for name, user, markers in cases:
    with tempfile.TemporaryDirectory(prefix='lab5-course-check-') as tmp:
        dest = Path(tmp) / 'course'
        shutil.copytree(course, dest, ignore=shutil.ignore_patterns('target'))
        if user:
            shutil.copyfile(evidence / user, dest / 'src/user/initcode.c')
        build = subprocess.run(['make', 'build'], cwd=dest, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (evidence / f'{name}-build.log').write_bytes(build.stdout)
        if build.returncode:
            raise SystemExit(f'{name}: build failed')
        command = ['qemu-system-riscv64', '-machine', 'virt', '-bios', 'default', '-kernel',
                   'target/qemu/kernel/kernel-qemu.elf', '-m', '130M', '-smp', '2', '-nographic']
        process = subprocess.Popen(command, cwd=dest, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        output = b''
        end = time.monotonic() + 15
        try:
            while time.monotonic() < end and not all(m in output for m in markers):
                for key, _ in selector.select(.1):
                    data = key.fileobj.read1(65536)
                    if data: output += data
                if process.poll() is not None: break
            if all(m in output for m in markers) and process.poll() is None:
                grace = time.monotonic() + .25
                while time.monotonic() < grace and process.poll() is None:
                    for key, _ in selector.select(.05):
                        data = key.fileobj.read1(65536)
                        if data: output += data
            premature = process.poll() is not None
        finally:
            if process.poll() is None: process.terminate()
            try: tail, _ = process.communicate(timeout=3)
            except subprocess.TimeoutExpired: process.kill(); tail, _ = process.communicate()
            output += tail
            (evidence / f'{name}-runtime.log').write_bytes(output)
        okay = all(m in output for m in markers) and not premature and b'panic!' not in output and b'FAIL' not in output
        if name == 'baseline': okay &= output.count(b'proczero: hello world') == 2
        if not okay: raise SystemExit(f'{name}: FAIL; see runtime log')
        print(f'{name}: PASS', flush=True)
