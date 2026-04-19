# ============================================================
# src/asm_course/10_string_asm.s
# ============================================================
    .section .text

    .globl asm_rep_movsq
    .type  asm_rep_movsq, @function
asm_rep_movsq:
    # rdi=dst, rsi=src, rdx=qwords
    movq    %rdx, %rcx
    cld
    rep     movsq
    ret
    .size asm_rep_movsq, .-asm_rep_movsq

    .globl asm_rep_stosb
    .type  asm_rep_stosb, @function
asm_rep_stosb:
    # rdi=dst, sil=byte, rdx=n
    movzbl  %sil, %eax
    movq    %rdx, %rcx
    cld
    rep     stosb
    ret
    .size asm_rep_stosb, .-asm_rep_stosb

    .globl asm_repne_scasb
    .type  asm_repne_scasb, @function
asm_repne_scasb:
    # rdi=s, retorna comprimento
    movq    %rdi, %rcx          # salva início
    xorb    %al, %al            # al = '\0'
    movq    $-1, %rdx
    movq    %rdx, %rcx          # rcx = "infinito"
    cld
    repne   scasb               # busca '\0'
    notq    %rcx
    leaq    -2(%rcx), %rax      # ajuste
    ret
    .size asm_repne_scasb, .-asm_repne_scasb

    .globl asm_repe_cmpsb
    .type  asm_repe_cmpsb, @function
asm_repe_cmpsb:
    # rdi=a, rsi=b, rdx=n
    movq    %rdx, %rcx
    xorl    %eax, %eax
    testq   %rcx, %rcx
    jz      .cmps_done
    cld
    repe    cmpsb               # compara [rdi] vs [rsi] até diferença ou rcx=0
    je      .cmps_done          # chegou ao fim iguais
    # Calcula diferença do byte onde parou
    movzbl  -1(%rdi), %eax      # byte de a
    movzbl  -1(%rsi), %ecx
    subl    %ecx, %eax
.cmps_done:
    ret
    .size asm_repe_cmpsb, .-asm_repe_cmpsb
