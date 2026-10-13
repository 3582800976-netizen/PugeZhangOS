/* Only this user file is assembled into the kernel. Others form a U ELF. */
    .section .rodata.user_image,"a",@progbits
    .balign 8
    .globl _user_code_start, _user_code_end
_user_code_start:
    .incbin USER_CODE_IMAGE
_user_code_end:
    .balign 8
    .globl _user_data_start, _user_data_end
_user_data_start:
    .incbin USER_DATA_IMAGE
_user_data_end:
