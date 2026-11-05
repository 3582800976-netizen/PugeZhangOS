#!/usr/bin/env python3
"""Build temporary Lab 4 copies and exercise serial and user-fault paths."""
from pathlib import Path
import os
import selectors
import shutil
import subprocess
import tempfile
import time

HERE = Path(__file__).resolve().parent
COURSE = HERE.parents[2] / 'course'


def wait_for(proc, selector, output, predicate, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline and not predicate(output):
        for key, _ in selector.select(0.1):
            output += os.read(key.fd, 4096)
        if proc.poll() is not None:
            break
    return output


def check(kind):
    with tempfile.TemporaryDirectory(prefix='lab4-check-') as tmp:
        dst = Path(tmp) / 'course'
        shutil.copytree(COURSE, dst, ignore=shutil.ignore_patterns('target', 'initcode.h'))
        subprocess.run(['patch', '-p1', '-i', str(HERE / (kind + '-only.patch'))], cwd=dst,
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['make', 'build'], cwd=dst, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        command = ['qemu-system-riscv64', '-machine', 'virt', '-bios', 'default',
                   '-kernel', 'target/kernel/kernel-qemu.elf', '-m', '130M',
                   '-smp', '2', '-nographic']
        proc = subprocess.Popen(command, cwd=dst, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        selector = selectors.DefaultSelector()
        selector.register(proc.stdout, selectors.EVENT_READ)
        output = b''
        if kind == 'test':
            predicate = lambda s: s.count(b'proczero: hello world') == 2 and b'[lab4-user-timer]' in s
            send, expected = b'Lab4Z\x7f!\r', b'Lab4Z\x08 \x08!\n'
        else:
            predicate = lambda s: (b'proczero: hello world' in s and
                                   b'user fault: cause=0x000000000000000d' in s and
                                   b'[lab4] user process stopped; kernel remains alive' in s)
            send, expected = b'after!\r', b'after!\n'
        try:
            output = wait_for(proc, selector, output, predicate, 10)
            if not predicate(output):
                raise RuntimeError('expected user-mode output missing')
            proc.stdin.write(send)
            proc.stdin.flush()
            output = wait_for(proc, selector, output, lambda s: expected in s, 3)
            if expected not in output:
                raise RuntimeError('expected UART echo missing')
            print(kind + ': PASS')
        finally:
            proc.terminate()
            try:
                tail, _ = proc.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                tail, _ = proc.communicate()
            log_dir = os.environ.get('LAB4_LOG_DIR')
            if log_dir:
                Path(log_dir).mkdir(parents=True, exist_ok=True)
                (Path(log_dir) / (kind + '-runtime.log')).write_bytes(output + tail)


if __name__ == '__main__':
    check('test')
    check('fault')
