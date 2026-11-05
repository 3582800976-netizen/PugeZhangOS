#!/usr/bin/env python3
"""Run user-mode page permission probes in disposable course copies."""
from pathlib import Path
import os
import selectors
import shutil
import subprocess
import tempfile
import time

HERE = Path(__file__).resolve().parent
COURSE = HERE.parents[2] / 'course'
CASES = {
    'uart-read': ('volatile unsigned *p = (volatile unsigned *)0x10000000UL; unsigned x = *p; (void)x;', 13),
    'trapframe-read': ('volatile unsigned *p = (volatile unsigned *)((1UL << 38) - 8192); unsigned x = *p; (void)x;', 13),
    'code-write': ('volatile unsigned char *p = (volatile unsigned char *)(unsigned long)&main; *p = 0;', 15),
    'stack-exec': ('char stack_code[16]; void (*fn)(void) = (void (*)(void))stack_code; fn();', 12),
}


def check(name, statement, cause):
    with tempfile.TemporaryDirectory(prefix='lab4-protection-') as tmp:
        dst = Path(tmp) / 'course'
        shutil.copytree(COURSE, dst, ignore=shutil.ignore_patterns('target', 'initcode.h'))
        program = '#include "sys.h"\nint main(void) { syscall(SYS_helloworld); ' + statement + ' for (;;) ; }\n'
        (dst / 'src/user/initcode.c').write_text(program)
        subprocess.run(['make', 'build'], cwd=dst, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        cmd = ['qemu-system-riscv64', '-machine', 'virt', '-bios', 'default',
               '-kernel', 'target/kernel/kernel-qemu.elf', '-m', '130M', '-smp', '2', '-nographic']
        p = subprocess.Popen(cmd, cwd=dst, stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        sel = selectors.DefaultSelector()
        sel.register(p.stdout, selectors.EVENT_READ)
        out = b''
        marker = f'user fault: cause=0x{cause:016x}'.encode()
        deadline = time.monotonic() + 8
        try:
            while time.monotonic() < deadline and b'[lab4] user process stopped; kernel remains alive' not in out:
                for key, _ in sel.select(0.1):
                    out += os.read(key.fd, 4096)
                if p.poll() is not None:
                    break
            if marker not in out or b'proczero: hello world' not in out or b'[lab4] user process stopped; kernel remains alive' not in out:
                raise RuntimeError(name + ': unexpected output: ' + repr(out[-500:]))
            print(name + ': PASS')
        finally:
            p.terminate()
            try:
                tail, _ = p.communicate(timeout=3)
            except subprocess.TimeoutExpired:
                p.kill(); tail, _ = p.communicate()
            log_dir = os.environ.get('LAB4_LOG_DIR')
            if log_dir:
                Path(log_dir).mkdir(parents=True, exist_ok=True)
                (Path(log_dir) / (name + '.log')).write_bytes(out + tail)


if __name__ == '__main__':
    for key, (probe, reason) in CASES.items():
        check(key, probe, reason)
