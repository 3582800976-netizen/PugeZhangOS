#!/bin/sh
# Module behavior checks use mock hardware and do not replace QEMU tests.
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/puge-lab6-host.XXXXXX")
trap 'rm -rf -- "$test_dir"' EXIT HUP INT TERM
cc -std=gnu11 -O2 -mcmodel=large -fno-pie -no-pie -Wall -Wextra -Werror -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -I. tests/kvm_host.c -Wl,--gc-sections -o "$test_dir/kvm"
timeout 20s "$test_dir/kvm"
printf '%s\n' 'kvm: PASS'
cc -std=gnu11 -O1 -Wall -Wextra -Werror -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -pthread -I. tests/mmap_nodes_host.c src/kernel/mem/mmap.c -Wl,--gc-sections -o "$test_dir/mmap"
timeout 10s "$test_dir/mmap"
printf '%s\n' 'mmap-nodes: PASS'
cc -std=gnu11 -O0 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie -no-pie -I. tests/pmem_host.c src/kernel/mem/pmem.c -o "$test_dir/pmem"
timeout 5s "$test_dir/pmem"
printf '%s\n' 'physical-pages: PASS'
cc -std=gnu11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -pthread -I. tests/sleeplock_host.c src/kernel/lock/sleeplock.c -o "$test_dir/sleeplock"
timeout 5s "$test_dir/sleeplock"
printf '%s\n' 'sleeplock: PASS'
cc -std=gnu11 -O0 -Wall -Wextra -Werror -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -I. tests/uvm_copy_host.c src/kernel/mem/uvm.c -Wl,--gc-sections -o "$test_dir/copy"
timeout 5s "$test_dir/copy"
printf '%s\n' 'uvm-copy: PASS'
cc -std=gnu11 -O1 -mcmodel=large -fno-pie -no-pie -Wall -Wextra -Werror -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -I. tests/uvm_regions_host.c -Wl,--gc-sections -o "$test_dir/regions"
timeout 20s "$test_dir/regions"
printf '%s\n' 'uvm-regions: PASS'
