#!/usr/bin/env python3
"""Inject allocation failures into a temporary Lab 4 copy and check page counts."""
from pathlib import Path
import os
import selectors
import shutil
import subprocess
import tempfile
import time

HERE = Path(__file__).resolve().parent
COURSE = HERE.parents[2] / 'course'


def main():
    logs = []
    with tempfile.TemporaryDirectory(prefix='lab4-oom-') as tmp:
        dst = Path(tmp) / 'course'
        shutil.copytree(COURSE, dst, ignore=shutil.ignore_patterns('target', 'initcode.h'))
        pmem = dst / 'src/kernel/mem/pmem.c'
        text = pmem.read_text()
        anchor = 'static uint64 dtb_begin, dtb_end;'
        assert text.count(anchor) == 1
        text = text.replace(anchor, anchor + '''
static int lab4_test_budget = -1;
void lab4_set_budget(int n) { lab4_test_budget = n; }
void lab4_get_counts(uint32 *k, uint32 *u)
{
    spinlock_acquire(&kern_region.lk); *k = kern_region.allocable; spinlock_release(&kern_region.lk);
    spinlock_acquire(&user_region.lk); *u = user_region.allocable; spinlock_release(&user_region.lk);
}
''')
        anchor = 'void *pmem_alloc(bool in_kernel)\n{'
        assert text.count(anchor) == 1
        text = text.replace(anchor, anchor + '''
    if (lab4_test_budget == 0) return NULL;
    if (lab4_test_budget > 0) lab4_test_budget--;
''')
        pmem.write_text(text)
        proc_src = dst / 'src/kernel/proc/proc.c'
        original = proc_src.read_text()
        anchor = 'void proc_make_first()\n{'
        assert original.count(anchor) == 1
        original = original.replace(anchor, '''extern void lab4_set_budget(int);
extern void lab4_get_counts(uint32 *, uint32 *);
#define LAB4_TEST_BUDGET 0
void proc_make_first()
{
    uint32 before_k, before_u, after_k, after_u;
    lab4_get_counts(&before_k, &before_u);
    lab4_set_budget(LAB4_TEST_BUDGET);
''')
        anchor = '    panic("proc_make_first: resource failure");'
        assert original.count(anchor) == 1
        original = original.replace(anchor, '''    lab4_set_budget(-1);
    lab4_get_counts(&after_k, &after_u);
    printf("[oom-check] budget=%d kernel=%d/%d user=%d/%d restored=%d\\n",
           LAB4_TEST_BUDGET, before_k, after_k, before_u, after_u,
           before_k == after_k && before_u == after_u);
    return;''')
        for budget in range(10):
            proc_src.write_text(original.replace('#define LAB4_TEST_BUDGET 0',
                                                 f'#define LAB4_TEST_BUDGET {budget}'))
            build = subprocess.run(['make', 'build'], cwd=dst, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
            if build.returncode:
                raise RuntimeError(f'budget {budget} build failed:\n{build.stdout}')
            cmd = ['qemu-system-riscv64', '-machine', 'virt', '-bios', 'default',
                   '-kernel', 'target/kernel/kernel-qemu.elf', '-m', '130M',
                   '-smp', '2', '-nographic']
            p = subprocess.Popen(cmd, cwd=dst, stdin=subprocess.DEVNULL,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            sel = selectors.DefaultSelector()
            sel.register(p.stdout, selectors.EVENT_READ)
            out = b''
            expected = f'[oom-check] budget={budget} '.encode()
            deadline = time.monotonic() + 8
            try:
                while time.monotonic() < deadline and not (expected in out and b'restored=1\n' in out):
                    for key, _ in sel.select(0.1): out += os.read(key.fd, 4096)
                    if p.poll() is not None: break
                if expected not in out or b'restored=1' not in out:
                    raise RuntimeError(f'budget {budget}: unexpected output {out!r}')
            finally:
                p.terminate()
                try: tail, _ = p.communicate(timeout=3)
                except subprocess.TimeoutExpired:
                    p.kill(); tail, _ = p.communicate()
                out += tail
            lines = [line for line in out.decode(errors='replace').splitlines()
                     if '[oom-check]' in line]
            logs.extend(lines)
            print(lines[-1])
    log_dir = os.environ.get('LAB4_LOG_DIR')
    if log_dir:
        Path(log_dir).mkdir(parents=True, exist_ok=True)
        (Path(log_dir) / 'oom-runtime.log').write_text('\n'.join(logs) + '\n')


if __name__ == '__main__':
    main()
