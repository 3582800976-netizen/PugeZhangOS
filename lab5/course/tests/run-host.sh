#!/bin/sh
# Host probes use mock physical pages and locks; they do not replace QEMU.
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/puge-lab5-host.XXXXXX")
trap 'rm -rf -- "$test_dir"' EXIT HUP INT TERM
common='-std=gnu11 -O0 -Wall -Wextra -Werror -ffreestanding -fno-builtin -ffunction-sections -fdata-sections'
cc $common -I. -mcmodel=large -fno-pie -no-pie tests/kvm_host.c -Wl,--gc-sections -o "$test_dir/kvm"
timeout 20s "$test_dir/kvm"
printf '%s\n' 'kvm: PASS'
cc $common -I. -pthread tests/mmap_nodes_host.c src/kernel/mem/mmap.c -Wl,--gc-sections -o "$test_dir/mmap"
timeout 10s "$test_dir/mmap"
printf '%s\n' 'mmap-nodes: PASS'
cc $common -I. -fno-pie -no-pie tests/pmem_host.c src/kernel/mem/pmem.c -Wl,--gc-sections -o "$test_dir/pmem"
timeout 5s "$test_dir/pmem"
printf '%s\n' 'physical-pages: PASS'
cc $common -I. tests/uvm_copy_host.c src/kernel/mem/uvm.c -Wl,--gc-sections -o "$test_dir/copy"
timeout 5s "$test_dir/copy"
printf '%s\n' 'uvm-copy: PASS'
cc $common -I. tests/uvm_regions_host.c -Wl,--gc-sections -o "$test_dir/regions"
timeout 20s "$test_dir/regions"
printf '%s\n' 'uvm-regions: PASS'
