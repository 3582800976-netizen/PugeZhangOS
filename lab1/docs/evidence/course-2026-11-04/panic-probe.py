from pathlib import Path
import os, selectors, subprocess, time
source=Path('src/kernel/main.c')
cases={
    'recursive': ('spinlock_t lk; spinlock_init(&lk, "lk"); spinlock_acquire(&lk); spinlock_acquire(&lk);', b'panic! recursive spinlock acquire'),
    'bad-pop': ('pop_off();', b'panic! push_off: 2'),
}
for name,(body,needle) in cases.items():
    source.write_text('#include "arch/mod.h"\n#include "lib/mod.h"\nint main(void) { if (r_tp() == 0) { print_init(); '+body+' } for (;;) asm volatile("wfi"); }\n')
    build=subprocess.run(['make','build'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    if build.returncode: raise SystemExit(build.stdout)
    p=subprocess.Popen(['qemu-system-riscv64','-machine','virt','-bios','none','-kernel','target/kernel/kernel-qemu.elf','-m','128M','-smp','2','-nographic'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL)
    sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ)
    out=b'';end=time.monotonic()+5
    while time.monotonic()<end and needle not in out:
        for key,_ in sel.select(.1):
            data=os.read(key.fd,4096)
            if data:out+=data
        if p.poll() is not None:break
    p.terminate()
    try:tail,_=p.communicate(timeout=3)
    except subprocess.TimeoutExpired:p.kill();tail,_=p.communicate()
    out+=tail;Path('panic-'+name+'.log').write_bytes(out)
    print(name, 'PASS' if needle in out else 'FAIL')
    if needle not in out:raise SystemExit(1)
