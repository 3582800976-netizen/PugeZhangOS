#!/usr/bin/env python3
"""Verify the local-only GDB endpoint reaches the Lab 4 kernel entry."""
from pathlib import Path
import os
import signal
import subprocess
import time

COURSE = Path(__file__).resolve().parents[3] / 'course'
port = 26144
process = subprocess.Popen(['make', 'debug', f'GDBPORT={port}'], cwd=COURSE,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           start_new_session=True)
try:
    time.sleep(0.5)
    result = subprocess.run(['gdb-multiarch', '-q', '-batch',
                             'target/kernel/kernel-qemu.elf',
                             '-ex', 'set pagination off',
                             '-ex', f'target remote 127.0.0.1:{port}',
                             '-ex', 'hbreak _entry', '-ex', 'continue',
                             '-ex', 'info registers pc', '-ex', 'detach'],
                            cwd=COURSE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=18)
finally:
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.communicate(timeout=4)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
if result.returncode or 'hit Breakpoint' not in result.stdout or '0x80200000' not in result.stdout:
    raise SystemExit('Lab 4 GDB FAILED: ' + result.stdout[-500:])
print('Lab 4 GDB _entry 0x80200000: PASS')
