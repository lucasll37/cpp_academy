# ============================================================
# src/asm_course/11_syscall_asm.s
# ============================================================
    .section .text

    .globl asm_getpid
    .type  asm_getpid, @function
asm_getpid:
    movq    $39, %rax           # nr = 39 (sys_getpid)
    syscall                     # executa — retorno em rax
    ret
    .size asm_getpid, .-asm_getpid

    .globl asm_write_direct
    .type  asm_write_direct, @function
asm_write_direct:
    # rdi=fd, rsi=buf, rdx=len
    movq    %rdi, %rax          # rax = fd (temporário)
    movq    $1,   %rax          # rax = 1 (sys_write) — sobrescreve fd
    # rdi já é fd, rsi já é buf, rdx já é len
    movq    %rdi, %rdi          # fd permanece em rdi (noop)
    syscall
    ret
    .size asm_write_direct, .-asm_write_direct

    # Wrapper genérico: syscall com 1 arg
    .globl asm_syscall1
    .type  asm_syscall1, @function
asm_syscall1:
    movq    %rdi, %rax          # rax = nr
    movq    %rsi, %rdi          # rdi = a1
    syscall
    ret
    .size asm_syscall1, .-asm_syscall1

    # Wrapper genérico: syscall com 3 args
    .globl asm_syscall3
    .type  asm_syscall3, @function
asm_syscall3:
    movq    %rdi, %rax          # rax = nr
    movq    %rsi, %rdi          # rdi = a1
    movq    %rdx, %rsi          # rsi = a2
    movq    %rcx, %rdx          # rdx = a3  (atenção: rcx muda no syscall!)
    syscall
    ret
    .size asm_syscall3, .-asm_syscall3
