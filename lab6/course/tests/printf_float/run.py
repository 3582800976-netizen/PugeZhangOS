#!/usr/bin/env python3
"""Check the real kernel printf using host reference and a RISC-V QEMU probe."""
import random
import re
import struct
import subprocess
import tempfile
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--course', type=Path, default=Path(__file__).resolve().parents[2])
course = parser.parse_args().course.resolve()
test = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='lab6-float-check-') as tmp:
    tmp = Path(tmp)
    host = tmp / 'host-test'
    subprocess.run(['cc', '-O2', '-ffreestanding', '-fno-builtin', '-I.',
                    str(test / 'host_print.c'), str(test / 'host_test.c'), '-o', str(host)],
                   cwd=course, check=True)
    rng = random.Random(7319)
    values = []
    for _ in range(3000):
        value = rng.getrandbits(64)
        if ((value >> 52) & 0x7ff) >= 1087:
            value = (value & ~(0x7ff << 52)) | (rng.randrange(1087) << 52)
        values.append(value)
    result = subprocess.run([str(host)], input=b''.join(struct.pack('<Q', v) for v in values),
                            stdout=subprocess.PIPE, check=True)
    lines = result.stdout.decode().splitlines()
    if len(lines) != len(values):
        raise SystemExit('host output count mismatch')
    for bits, output in zip(values, lines):
        absolute = struct.unpack('<d', struct.pack('<Q', bits & ((1 << 63) - 1)))[0]
        num, den = absolute.as_integer_ratio()
        scaled = num * 1000000 // den
        expected = ('-' if bits >> 63 else '') + str(scaled // 1000000) + '.' + str(scaled % 1000000).zfill(6)
        if output != expected:
            raise SystemExit(f'float mismatch: {bits:016x}: {output} != {expected}')
    print('float-host: PASS (3000 finite binary64 values)')

    obj = tmp / 'print.o'
    base = ['riscv64-linux-gnu-gcc', '-march=rv64gc', '-mabi=lp64d', '-mcmodel=medany',
            '-O', '-ffreestanding', '-fno-builtin', '-fno-pie', '-no-pie', '-I.']
    subprocess.run(base + ['-c', 'src/kernel/lib/print.c', '-o', str(obj)], cwd=course, check=True)
    assembly = subprocess.check_output(['riscv64-linux-gnu-objdump', '-d', str(obj)], text=True)
    mnemonics = re.findall(r'^\s*[0-9a-f]+:\s+[0-9a-f]+\s+([a-z][a-z0-9.]*)',
                           assembly, re.MULTILINE)
    fp = [op for op in mnemonics if op.startswith('f') and op not in ('fence', 'fence.i')]
    if fp:
        raise SystemExit('unexpected floating-point instruction in print.c: ' + ', '.join(fp))
    print('float-riscv-disassembly: PASS (no floating-point instructions)')
    elf = tmp / 'float-test.elf'
    subprocess.run(base + ['-nostdlib', '-nostartfiles', '-Wl,--build-id=none',
                   '-Wl,-T,' + str(test / 'linker.ld'), str(test / 'entry.S'), str(test / 'probe.S'),
                   str(test / 'riscv_test.c'), 'src/kernel/lib/print.c', '-o', str(elf)], cwd=course, check=True)
    command = ['timeout', '3s', 'qemu-system-riscv64', '-machine', 'virt', '-bios', 'none',
               '-kernel', str(elf), '-m', '128M', '-smp', '1', '-nographic']
    result = subprocess.run(command, cwd=course, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    required = ('case0=0.000000 next=42', 'case1=-0.000000 next=42', 'case2=1.500000 next=42',
                'case3=-1.500000 next=42', 'case4=0.100000 next=42',
                # The binary64 value nearest to 0.000001 is slightly smaller;
                # the course format truncates rather than rounds six digits.
                'case5=0.000000 next=42', 'case6=18446744073709549568.000000 next=42',
                'case7=0.000000 next=42', 'probe=1.500000 next=42',
                'registers=1', '[float-test] done')
    if result.returncode != 124 or not all(item in result.stdout for item in required):
        raise SystemExit('RISC-V QEMU float test failed:\n' + result.stdout)
    print(result.stdout, end='')
    print('float-riscv-qemu: PASS (timeout stopped the waiting test kernel)')
