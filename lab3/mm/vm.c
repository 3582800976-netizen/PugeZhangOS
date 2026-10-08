/* Sv39 from first principles: 3 arrays of 512 entries, all 4 KiB mappings.
 * Each pointer entry names another physical page; each leaf names a frame.
 * Ordinary kernel mappings are identity mappings, so the kernel can
 * dereference page-table physical addresses before and after enabling the
 * MMU. The diagnostic window deliberately provides a VA != PA example.
 */
#include "vm.h"
#include "page.h"
#include "memory.h"
#include "riscv.h"
#include "console.h"
#include "trap.h"

#define UART_BASE 0x10000000UL
#define SYSCON_BASE 0x00100000UL
#define PLIC_BASE 0x0c000000UL
#define PLIC_SIZE 0x04000000UL
#define SV39_MODE (8UL << 60)
#define PTE_FLAG_MASK 0x3ffUL
#define PTE_PPN_MASK (((1UL << 44) - 1) << 10)

extern char _text_start[], _text_end[], _rodata_start[], _rodata_end[];
static uint64 *root;
static uint64 *diag_frame;

static int canonical(uint64 va)
{
    /* bits 63:39 must repeat bit 38; both Sv39 halves are accepted. */
    uint64 high = va >> 39;
    return high == ((va & (1UL << 38)) ? ((1UL << 25) - 1) : 0);
}

static uint64 entry_address(uint64 entry)
{
    return ((entry & PTE_PPN_MASK) >> 10) << 12;
}

static uint64 address_entry(uint64 address)
{
    return (address >> 12) << 10;
}

static uint32 index_at(uint64 va, int level)
{
    return (uint32)((va >> (12 + level * 9)) & 511);
}

static int map_page(uint64 va, uint64 pa, uint64 flags)
{
    if (!canonical(va) || ((va | pa) & (PAGE_SIZE - 1)) ||
        (pa >> 56) || !(flags & (VM_R | VM_X)) ||
        ((flags & VM_W) && !(flags & VM_R)) ||
        (flags & ~(uint64)(VM_R | VM_W | VM_X)))
        return VM_INVALID_MAP;
    uint64 *table = root;
    for (int level = 2; level > 0; level--) {
        uint64 *entry = &table[index_at(va, level)];
        if (!(*entry & VM_V)) {
            uint64 *next = page_alloc(PAGE_KERNEL);
            if (!next)
                return VM_NO_MEMORY;
            *entry = address_entry((uint64)next) | VM_V;
        } else if (*entry & (VM_R | VM_W | VM_X)) {
            return VM_ALREADY_MAPPED;
        }
        table = (uint64 *)entry_address(*entry);
    }
    uint64 *entry = &table[index_at(va, 0)];
    if (*entry & VM_V)
        return VM_ALREADY_MAPPED;
    /* Set A/D explicitly; do not depend on hardware updating them. */
    *entry = address_entry(pa) | flags | VM_V | VM_A;
    if (flags & VM_W)
        *entry |= VM_D;
    return VM_OK;
}

static int map_identity(uint64 begin, uint64 end, uint64 flags)
{
    for (uint64 address = begin; address < end; address += PAGE_SIZE) {
        int result = map_page(address, address, flags);
        if (result != VM_OK)
            return result;
    }
    return VM_OK;
}

int vm_init(void)
{
    uint64 begin = (uint64)_text_start, end = (uint64)_text_end;
    uint64 ro_begin = (uint64)_rodata_start, ro_end = (uint64)_rodata_end;
    if (((begin | end) & (PAGE_SIZE - 1)) || begin != 0x80200000UL ||
        end <= begin || end >= PAGE_RAM_END || ro_begin != end ||
        (ro_end & (PAGE_SIZE - 1)) || ro_end < ro_begin || ro_end >= PAGE_RAM_END)
        return VM_INVALID_MAP;
    root = page_alloc(PAGE_KERNEL);
    if (!root)
        return VM_NO_MEMORY;
    int result = map_identity(UART_BASE, UART_BASE + PAGE_SIZE, VM_R | VM_W);
    /* The kernel closes this QEMU machine with a direct 32-bit write to its
     * test/syscon finisher, so the device page must be mapped in S-mode. */
    if (result == VM_OK)
        result = map_identity(SYSCON_BASE, SYSCON_BASE + PAGE_SIZE, VM_R | VM_W);
    if (result == VM_OK)
        result = map_identity(PLIC_BASE, PLIC_BASE + PLIC_SIZE, VM_R | VM_W);
    if (result == VM_OK)
        result = map_identity(begin, end, VM_R | VM_X);
    if (result == VM_OK)
        result = map_identity(ro_begin, ro_end, VM_R);
    if (result == VM_OK)
        result = map_identity(ro_end, PAGE_RAM_END, VM_R | VM_W);
    if (result == VM_OK) {
        diag_frame = page_alloc(PAGE_KERNEL);
        if (!diag_frame)
            return VM_NO_MEMORY;
        result = map_page(VM_DIAG_ALIAS, (uint64)diag_frame, VM_R | VM_W);
    }
    return result;
}

void vm_enable(void)
{
    /* satp is per hart. Ordering fences surround the switch to a new root. */
    asm volatile("sfence.vma zero, zero" ::: "memory");
    w_satp(SV39_MODE | ((uint64)root >> 12));
    asm volatile("sfence.vma zero, zero" ::: "memory");
}

uint64 vm_root_address(void)
{
    return (uint64)root;
}

uint64 vm_diag_frame_address(void)
{
    return (uint64)diag_frame;
}

int vm_lookup(uint64 va, struct vm_mapping *out)
{
    if (!canonical(va))
        return VM_BAD_ADDRESS;
    if (!root)
        return VM_UNMAPPED;
    if (out) {
        memset(out, 0, sizeof *out);
        for (int level = 0; level < 3; level++)
            out->vpn[level] = index_at(va, level);
    }
    uint64 *table = root;
    for (int level = 2; level >= 0; level--) {
        uint64 entry = table[index_at(va, level)];
        if (!(entry & VM_V))
            return VM_UNMAPPED;
        /* Sv39 reserves bits 63:54; W=1,R=0 is an invalid encoding. */
        if ((entry >> 54) || ((entry & VM_W) && !(entry & VM_R)))
            return VM_BAD_ENTRY;
        uint64 address = entry_address(entry);
        if (entry & (VM_R | VM_X)) {
            uint64 size = 1UL << (12 + 9 * level);
            if (address & (size - 1))
                return VM_BAD_ENTRY;
            if (out) {
                out->pa = address | (va & (size - 1));
                out->pte = entry;
                out->flags = entry & PTE_FLAG_MASK;
                out->level = (uint32)level;
            }
            return VM_OK;
        }
        /* Non-leaves must not carry U/A/D, and must name managed RAM. */
        if (level == 0 || (entry & (VM_U | VM_A | VM_D)) ||
            address < 0x80200000UL || address >= PAGE_RAM_END)
            return VM_BAD_ENTRY;
        table = (uint64 *)address;
    }
    return VM_UNMAPPED;
}

static int check(const char *name, int passed)
{
    console_printf("[%s] lab2 vm %s\n", passed ? "PASS" : "FAIL", name);
    return !passed;
}

static int mapping_has(uint64 address, uint64 permissions)
{
    struct vm_mapping m;
    return vm_lookup(address, &m) == VM_OK && m.pa == address &&
           (m.flags & (VM_R | VM_W | VM_X | VM_U)) == permissions &&
           m.level == 0;
}

int vm_selftest(void)
{
    int failures = 0;
    failures += check("UART identity RW", mapping_has(UART_BASE + 5, VM_R | VM_W));
    failures += check("shutdown syscon identity RW", mapping_has(SYSCON_BASE, VM_R | VM_W));
    failures += check("PLIC identity RW", mapping_has(PLIC_BASE + 0x201000, VM_R | VM_W));
    failures += check("kernel text identity RX", mapping_has((uint64)_text_start, VM_R | VM_X));
    failures += check("last text page RX", mapping_has((uint64)_text_end - 1, VM_R | VM_X));
    failures += check("kernel rodata identity R", mapping_has((uint64)_rodata_start, VM_R));
    failures += check("kernel data identity RW", mapping_has((uint64)_rodata_end, VM_R | VM_W));
    failures += check("root table identity RW", mapping_has((uint64)root, VM_R | VM_W));
    failures += check("last RAM byte identity RW", mapping_has(PAGE_RAM_END - 1, VM_R | VM_W));
    failures += check("OpenSBI excluded", vm_lookup(0x80000000UL, 0) == VM_UNMAPPED);
    failures += check("CLINT excluded", vm_lookup(0x02000000UL, 0) == VM_UNMAPPED);
    failures += check("address zero unmapped", vm_lookup(0, 0) == VM_UNMAPPED);
    failures += check("beyond RAM unmapped", vm_lookup(PAGE_RAM_END, 0) == VM_UNMAPPED);
    failures += check("reject noncanonical bit 38", vm_lookup(1UL << 38, 0) == VM_BAD_ADDRESS);
    failures += check("reject noncanonical high bits", vm_lookup(1UL << 63, 0) == VM_BAD_ADDRESS);
    failures += check("canonical upper half accepted", vm_lookup(~((1UL << 38) - 1), 0) == VM_UNMAPPED);
    failures += check("reject write-only leaf", map_page(0x40000000UL, 0x80200000UL, VM_W) == VM_INVALID_MAP);
    failures += check("reject unaligned mapping", map_page(0x40000001UL, 0x80200000UL, VM_R) == VM_INVALID_MAP);
    failures += check("reject duplicate mapping", map_page(UART_BASE, UART_BASE, VM_R | VM_W) == VM_ALREADY_MAPPED);
    struct vm_mapping alias;
    failures += check("diagnostic alias points to dedicated frame",
                      vm_lookup(VM_DIAG_ALIAS, &alias) == VM_OK &&
                      alias.pa == (uint64)diag_frame &&
                      (alias.flags & (VM_R | VM_W | VM_X | VM_U)) == (VM_R | VM_W));
    int enabled = (r_satp() >> 60) == 8;
    failures += check("hardware MMU is Sv39", enabled);
    if (enabled && diag_frame) {
        volatile uint64 *physical = diag_frame;
        volatile uint64 *virtual = (volatile uint64 *)VM_DIAG_ALIAS;
        uint64 saved = *physical;
        uint64 pattern = 0x5041474557494e44UL;
        *virtual = pattern;
        asm volatile("fence rw, rw" ::: "memory");
        failures += check("hardware alias write visible at PA", *physical == pattern);
        *physical = ~pattern;
        asm volatile("fence rw, rw" ::: "memory");
        failures += check("hardware PA write visible at alias", *virtual == ~pattern);
        *physical = saved;
        failures += check("hardware zero-page load rejected", trap_test_load_fault(0));
        failures += check("hardware firmware load rejected", trap_test_load_fault(0x80000000UL));
        failures += check("hardware text write rejected", trap_test_store_fault((uint64)_text_start));
        failures += check("hardware rodata write rejected", trap_test_store_fault((uint64)_rodata_start));
    }
    return failures;
}
