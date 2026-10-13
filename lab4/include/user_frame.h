#ifndef PUGE_USER_FRAME_H
#define PUGE_USER_FRAME_H
/* Register xN is at N*8. The assembly and C share one layout. */
#define UF_EPC          256
#define UF_STATUS       264
#define UF_KERNEL_SATP  272
#define UF_KERNEL_SP    280
#define UF_KERNEL_HANDLER 288
#define UF_KERNEL_HART  296
#define UF_SIZE         304
#ifndef __ASSEMBLER__
#include "types.h"
struct user_frame {
    uint64 x[32];
    uint64 epc, status;
    uint64 kernel_satp, kernel_sp, kernel_handler, kernel_hartid;
};
struct context { uint64 ra, sp, s[12]; };
_Static_assert(sizeof(struct user_frame) == UF_SIZE, "user frame size");
_Static_assert(__builtin_offsetof(struct user_frame, epc) == UF_EPC, "epc offset");
_Static_assert(__builtin_offsetof(struct user_frame, status) == UF_STATUS, "status offset");
_Static_assert(__builtin_offsetof(struct user_frame, kernel_satp) == UF_KERNEL_SATP, "satp offset");
_Static_assert(__builtin_offsetof(struct user_frame, kernel_sp) == UF_KERNEL_SP, "sp offset");
_Static_assert(__builtin_offsetof(struct user_frame, kernel_handler) == UF_KERNEL_HANDLER, "handler offset");
_Static_assert(__builtin_offsetof(struct user_frame, kernel_hartid) == UF_KERNEL_HART, "hart offset");
_Static_assert(sizeof(struct context) == 112, "context size");
void context_switch(struct context *from, struct context *to);
#endif
#endif
