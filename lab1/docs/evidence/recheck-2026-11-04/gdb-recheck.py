from pathlib import Path
import os
import signal
import subprocess
import time

root = Path(__file__).resolve().parents[4]
output = Path('/tmp/puge-recheck-123')
output.mkdir(exist_ok=True)
for lab in (1, 2, 3):
    work = root / f'lab{lab}/course'
    port = 26140 + lab
    process = subprocess.Popen(['make', 'debug', f'GDBPORT={port}'], cwd=work,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        time.sleep(0.8)
        result = subprocess.run(
            ['gdb-multiarch', '-q', '-batch', 'target/kernel/kernel-qemu.elf',
             '-ex', 'set pagination off', '-ex', f'target remote 127.0.0.1:{port}',
             '-ex', 'hbreak _entry', '-ex', 'continue', '-ex', 'info registers pc',
             '-ex', 'detach'], cwd=work, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, timeout=18)
    finally:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            qemu_output, _ = process.communicate(timeout=4)
        except subprocess.TimeoutExpired:
            process.kill()
            qemu_output, _ = process.communicate()
    (output / f'lab{lab}-gdb.log').write_text(result.stdout)
    (output / f'lab{lab}-gdb-qemu.log').write_bytes(qemu_output)
    address = '0x80000000' if lab == 1 else '0x80200000'
    if result.returncode or 'hit Breakpoint' not in result.stdout or address not in result.stdout:
        raise SystemExit(f'Lab {lab} GDB FAILED: {result.stdout[-500:]}')
    print(f'Lab {lab}: GDB _entry {address} PASS')
