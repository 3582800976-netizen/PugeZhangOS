#!/usr/bin/env python3
"""Build isolated Lab 6 QEMU probes; never modifies the course source tree."""
import argparse
import re
import selectors
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--course', type=Path, required=True)
p.add_argument('--out-dir', type=Path, required=True)
args = p.parse_args()
course = args.course.resolve()
out_dir = args.out_dir.resolve()
out_dir.mkdir(parents=True, exist_ok=True)
evidence = Path(__file__).resolve().parent
cases = [
    ('baseline-cpu1', 1, None, None, b'proczero: hello world!'),
    ('baseline', 2, None, None, b'proczero: hello world!'),
    ('fork', 2, 'fork', None, b'L3\n'),
    ('wait', 2, 'wait', None, b'WAIT-PASS'),
    ('copy', 2, 'copy', None, b'COPY-PASS'),
    ('user-memory', 2, 'user-memory', None, b'MEMABI-PASS'),
    ('waitretry', 2, 'waitretry', None, b'RETRY-PASS'),
    ('fault', 2, 'fault', None, b'FAULT-PASS'),
    ('codefault', 2, 'codefault', None, b'CODE-PASS'),
    ('uart', 1, None, 'uart', b'[user-uart-irq]'),
    ('limit', 2, 'limit', None, b'LIMIT-PASS'),
    ('failfork', 1, 'failfork', 'failfork', b'INJECT-PASS'),
    ('lock', 2, 'lock', 'lock', b'LOCK-PASS'),
    ('sleep', 2, 'sleep', None, b'SLEEP-PASS'),
    ('preempt', 1, 'preempt', 'preempt', b'PREEMPT-DONE'),
    ('sleepticks', 2, 'sleep', 'sleepticks', b'SLEEP-PASS'),
    ('ownership', 2, 'fork', 'ownership', b'[ownership]'),
]
for name, cpus, user, patch, marker in cases:
    with tempfile.TemporaryDirectory(prefix='lab6-qemu-audit-') as tmp:
        dest = Path(tmp) / 'course'
        shutil.copytree(course, dest, ignore=shutil.ignore_patterns('target'))
        if user:
            shutil.copyfile(evidence / (user + '.c'), dest / 'src/user/initcode.c')
        if patch:
            apply = subprocess.run(['patch', '-p1', '-i', str(evidence / (patch + '.patch'))], cwd=dest, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if apply.returncode:
                raise SystemExit(f'{name}: patch failed: {apply.stdout.decode(errors="replace")}')
        build = subprocess.run(['make', 'build', f'CPUNUM={cpus}'], cwd=dest, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out_dir / (name + '-build.log')).write_bytes(build.stdout)
        if build.returncode:
            raise SystemExit(f'{name}: build failed; see {name}-build.log')
        command = ['qemu-system-riscv64', '-machine', 'virt', '-bios', 'default', '-kernel', 'target/qemu/kernel/kernel-qemu.elf', '-m', '130M', '-smp', str(cpus), '-nographic']
        proc = subprocess.Popen(command, cwd=dest, stdin=subprocess.PIPE if name == 'uart' else subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        selector = selectors.DefaultSelector()
        selector.register(proc.stdout, selectors.EVENT_READ)
        output = b''
        uart_sent = False
        end = time.monotonic() + 15
        def complete(buf):
            if name == 'fork': return buf.count(b'L1\n') == 1 and buf.count(b'L2\n') >= 2 and buf.count(b'L3\n') >= 4
            if name == 'ownership': return re.search(rb'\[ownership\] switches=(\d+) collisions=(\d+)', buf) is not None
            return marker in buf
        try:
            while time.monotonic() < end and not complete(output):
                for key, _ in selector.select(0.1):
                    part = key.fileobj.read1(65536)
                    if part: output += part
                if name == 'uart' and not uart_sent and b'proczero: hello world!' in output:
                    proc.stdin.write(b'hiZ\x7f!\r')
                    proc.stdin.flush()
                    uart_sent = True
                if proc.poll() is not None: break
            # Continue briefly after the expected output to catch an immediate panic.
            if complete(output) and proc.poll() is None:
                grace = time.monotonic() + 0.25
                while time.monotonic() < grace and proc.poll() is None:
                    for key, _ in selector.select(0.05):
                        part = key.fileobj.read1(65536)
                        if part: output += part
            premature_exit = proc.poll() is not None
        finally:
            if proc.poll() is None: proc.terminate()
            try: tail, _ = proc.communicate(timeout=3)
            except subprocess.TimeoutExpired: proc.kill(); tail, _ = proc.communicate()
            output += tail
            (out_dir / (name + '-runtime.log')).write_bytes(output)
        okay = complete(output) and not premature_exit and b'panic!' not in output and b'-FAIL' not in output
        if name == 'fork':
            okay &= output.count(b'L1\n') == 1 and output.count(b'L2\n') == 2 and output.count(b'L3\n') == 4
        if name == 'uart':
            okay &= b'hiZ\x08 \x08!\r\n' in output and b'[user-uart-irq]' in output
        if name == 'lock':
            waits = [int(n) for n in re.findall(rb'\[lock-probe\] waits=(\d+)', output)]
            okay &= len(waits) == 2 and max(waits) >= 1
        if name == 'preempt':
            a, b, c = output.find(b'[busy-start]'), output.find(b'CHILD-RAN'), output.find(b'[busy-end]')
            okay &= 0 <= a < b < c
        if name == 'sleepticks':
            m = re.search(rb'\[sleep-ticks\] requested=30 actual=(\d+)', output)
            okay &= bool(m and int(m.group(1)) >= 30)
        if name == 'ownership':
            m = re.search(rb'\[ownership\] switches=(\d+) collisions=(\d+)', output)
            okay &= bool(m and int(m.group(1)) >= 2 and int(m.group(2)) == 0)
        print(f'{name}: {"PASS" if okay else "FAIL"}', flush=True)
        if not okay: raise SystemExit(f'{name}: inspect {name}-runtime.log')
