from pathlib import Path
import os
import re
import selectors
import shutil
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[4]
outdir = Path('/tmp/puge-recheck-123')
outdir.mkdir(exist_ok=True)

def command(args, cwd, name, expect=0):
    result = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=60)
    (outdir / name).write_text(result.stdout)
    if result.returncode != expect:
        raise RuntimeError(f'{name}: exit {result.returncode}, expected {expect}\n{result.stdout[-2000:]}')
    return result.stdout

def qemu(cwd, bios, ram, name, markers, payload=b'', echoed=b'', cpus=2):
    command_line = ['qemu-system-riscv64','-machine','virt','-bios',bios,
                    '-kernel','target/kernel/kernel-qemu.elf','-m',ram,'-smp',str(cpus),'-nographic']
    p = subprocess.Popen(command_line, cwd=cwd, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    selector = selectors.DefaultSelector()
    selector.register(p.stdout, selectors.EVENT_READ)
    data = b''
    def receive(deadline, predicate):
        nonlocal data
        while time.monotonic() < deadline and not predicate(data):
            for key, _ in selector.select(0.1):
                chunk = os.read(key.fd, 4096)
                if chunk:
                    data += chunk
            if p.poll() is not None:
                break
        return predicate(data)
    try:
        good = receive(time.monotonic() + 40, lambda output: all(m in output for m in markers))
        if good and cpus == 3:
            receive(time.monotonic() + 0.5, lambda output: False)
            good = b'cpu 2' not in data
        if good and payload:
            p.stdin.write(payload)
            p.stdin.flush()
            good = receive(time.monotonic() + 5, lambda output: echoed in output)
        if good and payload:
            length = len(data)
            receive(time.monotonic() + 1, lambda output: False)
            good = len(data) == length
    finally:
        p.terminate()
        try:
            more, _ = p.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            p.kill()
            more, _ = p.communicate()
        data += more
        (outdir / name).write_bytes(data)
    if not good:
        raise RuntimeError(f'{name}: expected output missing, tail={data[-1000:]!r}')
    if b'panic!' in data:
        raise RuntimeError(f'{name}: unexpected panic')
    return data

with tempfile.TemporaryDirectory(prefix='puge-recheck-') as temp:
    for n in (1, 2, 3):
        work = Path(temp) / f'lab{n}'
        shutil.copytree(root / f'lab{n}/course', work,
                        ignore=shutil.ignore_patterns('target'))
        ev = root / f'lab{n}/docs/evidence/course-2026-11-04'
        command(['make','build'], work, f'lab{n}-build.log')
        if n == 1:
            qemu(work,'none','128M','lab1-extra-hart.log',
                 [b'cpu 0 is booting!', b'cpu 1 is booting!'], cpus=3)
        if n in (2, 3):
            command(['make','clean','build'], work, f'lab{n}-clean-build.log')
            command(['make','clean'], work, f'lab{n}-clean.log')
            command(['make','-j8','build'], work, f'lab{n}-parallel-build.log')
            rejected = subprocess.run(['make','-n','run','CPUNUM=3'], cwd=work,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            (outdir / f'lab{n}-invalid-cpus.log').write_text(rejected.stdout)
            if rejected.returncode == 0 or 'CPUNUM must be 2' not in rejected.stdout:
                raise RuntimeError(f'lab{n}: invalid CPUNUM not rejected')
            dry = subprocess.run(['make','-n'], cwd=work, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True)
            (outdir / f'lab{n}-default.log').write_text(dry.stdout)
            if dry.returncode or 'qemu-system-riscv64' not in dry.stdout:
                raise RuntimeError(f'lab{n}: default make does not start QEMU')
        if n == 1:
            shutil.copy2(ev / 'main-locked.c', work / 'src/kernel/main.c')
            os.utime(work / 'src/kernel/main.c', None)
        else:
            command(['patch','-p1','-i',str(ev / 'test-only.patch')], work, f'lab{n}-patch.log')
        command(['make','build'], work, f'lab{n}-test-build.log')
        if n == 1:
            data = qemu(work,'none','128M','lab1-runtime.log',
                        [b'[lock-test] hart=0 errors=0', b'[lock-test] hart=1 errors=0',
                         b'[sum] 2000000', b'[done]', b'[format] -2147483648 ffffffff 0x0000000000000012 Q (null) %'])
            if len(re.findall(rb'\[line\] hart=[01] index=\d+ AAAABBBBCCCCDDDDEEEE', data)) != 40:
                raise RuntimeError('lab1: interleaved or missing output lines')
        elif n == 2:
            qemu(work,'default','130M','lab2-runtime.log',
                 [b'[lab2-check] dual-hart Sv39, concurrent pages, 10-page count/zero/reuse, inherited lock and interrupt nesting, PTE map/unmap, hardware alias, DTB PASS'])
        else:
            payload = b'abcdefghijklmnopqrstuvwxyz0123456789\x7f!\r'
            echo = b'abcdefghijklmnopqrstuvwxyz0123456789\x08 \x08!\n'
            qemu(work,'default','130M','lab3-runtime.log',
                 [b'[timer-end] hart=0', b'[timer-end] hart=1', b'regs=PASS'], payload, echo)
        print(f'Lab {n}: build and instrumented QEMU PASS', flush=True)
