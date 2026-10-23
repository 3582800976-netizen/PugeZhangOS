#ifndef PUGE_USER_VM_H
#define PUGE_USER_VM_H

#include "types.h"
#include "vm.h"

/* These four fields are owned physical addresses, not user pointers.
 * The trapframe, kernel stack and trampoline are borrowed leaf mappings.
 * Dynamic heap pages are owned by marked leaves in this object's tree. */
struct user_vm {
    uint64 root;
    uint64 code;
    uint64 data;
    uint64 stack;
};

/* out must be empty. Production uses budget=-1; a nonnegative budget limits
 * allocation attempts. All failures leave out empty and release owned pages.
 * code must have 1..4096 bytes; data may be empty. Both borrowed pages must
 * be aligned kernel-pool pages. The caller keeps those pages alive. */
int user_vm_create(struct user_vm *out, const void *code, uint64 code_size,
                   const void *data, uint64 data_size,
                   uint64 trapframe_pa, uint64 kstack_pa, int budget);

/* No hart may still be using vm->root in satp. Empty objects are harmless. */
void user_vm_destroy(struct user_vm *vm);
int user_vm_lookup(const struct user_vm *vm, uint64 va, struct vm_mapping *out);

/* Page-aligned, nonempty ranges within USER_HEAP_BASE..USER_HEAP_LIMIT only.
 * alloc creates zeroed R/W/U, nonexecutable pages; budget=-1 is unlimited.
 * Both operations preflight the complete range. On failure mappings and
 * free-page counts are unchanged, including new page-table allocations.
 * free releases physical pages and prunes empty tables, retaining root.
 * The owner hart must use the kernel root throughout these operations. */
int user_vm_heap_alloc(struct user_vm *vm, uint64 va, uint64 npages, int budget);
int user_vm_heap_free(struct user_vm *vm, uint64 va, uint64 npages);

/* dst is a trusted kernel buffer with len bytes of capacity. Validation of
 * the complete range precedes every copy, so failure leaves dst unchanged.
 * len=0 succeeds without accessing vm, dst or the user source address. */
int user_vm_copyin(const struct user_vm *vm, void *dst, uint64 srcva, uint64 len);

/* src is a trusted kernel buffer with len readable bytes. All user pages
 * must be writable U mappings backed by user-pool memory. Failure leaves
 * the complete destination unchanged; len=0 accesses nothing. */
int user_vm_copyout(const struct user_vm *vm, uint64 dstva, const void *src, uint64 len);

#endif
