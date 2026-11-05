#include "mod.h"

// 这个文件通过make build生成, 是proczero对应的ELF文件
#include "../../user/initcode.h"
#define initcode target_user_initcode
#define initcode_len target_user_initcode_len

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap/trap_user.c
extern void trap_user_return();

// 第一个用户进程
static proc_t proczero;

// 获得一个初始化过的用户页表
// 完成trapframe和trampoline的映射
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    pgtbl_t pgtbl = pmem_alloc(true);
    if (pgtbl == NULL) return NULL;
    if (vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X) < 0 ||
        vm_mappages(pgtbl, TRAPFRAME, trapframe, PGSIZE, PTE_R | PTE_W) < 0) {
        vm_freewalk(pgtbl);
        return NULL;
    }
    return pgtbl;
}

/*
    第一个用户态进程的创建
    它的代码和数据位于initcode.h的initcode数组

    第一个进程的用户地址空间布局:
    trapoline   (1 page)
    trapframe   (1 page)
    ustack      (1 page)
    .......
                        <--heap_top
    code + data (1 page)
    empty space (1 page) 最低的4096字节 不分配物理页，同时不可访问

	注意: 用用户空间的地址映射需要标记 PTE_U
*/
void proc_make_first()
{
    proczero.pid = 1;
    proczero.kstack = KSTACK(0);
    void *code = NULL, *stack = NULL;
    bool code_mapped = false, stack_mapped = false;
    proczero.tf = pmem_alloc(true);
    if (proczero.tf == NULL) goto failed;
    proczero.pgtbl = proc_pgtbl_init((uint64)proczero.tf);
    if (proczero.pgtbl == NULL || initcode_len == 0 || initcode_len > PGSIZE) goto failed;
    code = pmem_alloc(false);
    stack = pmem_alloc(false);
    if (code == NULL || stack == NULL) goto failed;
    memmove(code, initcode, initcode_len);
    // 本阶段课程映像代码和常量占一页；用户代码页只读、可执行。
    if (vm_mappages(proczero.pgtbl, USER_BASE, (uint64)code, PGSIZE,
                    PTE_U | PTE_R | PTE_X) < 0) goto failed;
    code_mapped = true;
    if (vm_mappages(proczero.pgtbl, TRAPFRAME - PGSIZE, (uint64)stack, PGSIZE,
                    PTE_U | PTE_R | PTE_W) < 0) goto failed;
    stack_mapped = true;
    proczero.heap_top = USER_BASE + PGSIZE;
    proczero.ustack_npage = 1;
    proczero.tf->user_to_kern_epc = USER_BASE;
    proczero.tf->sp = TRAPFRAME;
    proczero.ctx.ra = (uint64)trap_user_return;
    proczero.ctx.sp = proczero.kstack + PGSIZE;
    cpu_t *cpu = mycpu();
    cpu->proc = &proczero;
    swtch(&cpu->ctx, &proczero.ctx);
    cpu->proc = NULL;
    // 用户异常退出后，回收独占数据和页表；保留共享 trampoline。
    vm_unmappages(proczero.pgtbl, USER_BASE, PGSIZE, true);
    vm_unmappages(proczero.pgtbl, TRAPFRAME - PGSIZE, PGSIZE, true);
    vm_freewalk(proczero.pgtbl);
    pmem_free((uint64)proczero.tf, true);
    proczero.pgtbl = NULL;
    proczero.tf = NULL;
    return;
failed:
    if (code_mapped) vm_unmappages(proczero.pgtbl, USER_BASE, PGSIZE, true);
    else if (code != NULL) pmem_free((uint64)code, false);
    if (stack_mapped) vm_unmappages(proczero.pgtbl, TRAPFRAME - PGSIZE, PGSIZE, true);
    else if (stack != NULL) pmem_free((uint64)stack, false);
    if (proczero.pgtbl != NULL) vm_freewalk(proczero.pgtbl);
    if (proczero.tf != NULL) pmem_free((uint64)proczero.tf, true);
    proczero.pgtbl = NULL;
    proczero.tf = NULL;
    panic("proc_make_first: resource failure");
}
