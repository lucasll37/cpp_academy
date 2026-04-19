// ============================================================
// src/asm_course/11_syscalls.cpp
//
// MÓDULO 11 — System Calls Diretas (sem libc)
//
// ┌─────────────────────────────────────────────────────────┐
// │  Um syscall é a fronteira entre userspace e kernel.     │
// │                                                         │
// │  Instrução SYSCALL (x86-64 Linux):                      │
// │    rax = número do syscall                              │
// │    rdi = arg1,  rsi = arg2,  rdx = arg3                 │
// │    r10 = arg4,  r8  = arg5,  r9  = arg6                 │
// │    (retorna em rax; negativo = errno)                    │
// │                                                         │
// │  Syscalls comuns:                                       │
// │    0  = read(fd, buf, count)                            │
// │    1  = write(fd, buf, count)                           │
// │    2  = open(path, flags, mode)                         │
// │    3  = close(fd)                                       │
// │    9  = mmap(...)                                       │
// │   39  = getpid()                                        │
// │   60  = exit(status)                                    │
// │   231 = exit_group(status)                              │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <cstddef>
#include <fmt/core.h>

extern "C" {
    int64_t asm_syscall1(int64_t nr, int64_t a1);
    int64_t asm_syscall3(int64_t nr, int64_t a1, int64_t a2, int64_t a3);
    void    asm_write_direct(int fd, const char* msg, size_t len);
    int64_t asm_getpid();
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Demo 1: getpid() sem libc ─────────────────────────────────
void demo_getpid() {
    section("11.1 getpid() — syscall 39");

    int64_t pid_asm = asm_getpid();
    fmt::print("  PID via syscall direto: {}\n", pid_asm);

    // Compara com o getpid() da libc
    int64_t pid_libc;
    asm volatile (
        "syscall"
        : "=a"(pid_libc)
        : "0"(39LL)  // syscall nr 39 = getpid
        : "rcx", "r11", "memory"
    );
    fmt::print("  PID via asm inline:     {}\n", pid_libc);
    fmt::print("  Iguais? {}\n", (pid_asm == pid_libc) ? "SIM ✓" : "NÃO ✗");
}

// ── Demo 2: write() sem libc ──────────────────────────────────
void demo_write() {
    section("11.2 write(fd=1, buf, len) — syscall 1");

    const char* msg = "  [escrito via syscall 1 direto, sem libc]\n";
    size_t len = __builtin_strlen(msg);

    // Syscall inline diretamente
    asm volatile (
        "syscall"
        :
        : "a"(1LL),           // rax = 1 (sys_write)
          "D"(1LL),           // rdi = 1 (stdout)
          "S"(msg),           // rsi = buf
          "d"((int64_t)len)   // rdx = len
        : "rcx", "r11", "memory"
    );

    fmt::print("  (mensagem acima foi escrita diretamente via syscall)\n");
}

// ── Demo 3: Clobbers do SYSCALL ───────────────────────────────
void demo_syscall_clobbers() {
    section("11.3 Clobbers importantes do SYSCALL");
    //
    // A instrução SYSCALL modifica automaticamente:
    //   RCX ← RIP (endereço de retorno — o kernel usa para voltar)
    //   R11 ← RFLAGS (o kernel salva as flags)
    //
    // Por isso, nas constraints do asm inline você DEVE incluir:
    //   : "rcx", "r11", "memory"
    //
    // E o kernel pode modificar qualquer registrador caller-saved.
    // Use "memory" para garantir que o compilador não cache valores
    // em registradores através do boundary do syscall.

    fmt::print("  Clobbers obrigatórios do SYSCALL:\n");
    fmt::print("    rcx   ← kernel salva RIP aqui (para SYSRET)\n");
    fmt::print("    r11   ← kernel salva RFLAGS aqui\n");
    fmt::print("    memory← barreia de memória (evita reordenamento)\n");
    fmt::print("  NUNCA esqueça esses clobbers!\n");

    // Demonstração: ler rcx antes e depois de syscall
    uint64_t rcx_before, rcx_after;
    asm volatile ("movq %%rcx, %0" : "=r"(rcx_before));

    asm volatile (
        "syscall"
        :
        : "a"(39LL)   // getpid
        : "rcx", "r11", "memory"
    );

    asm volatile ("movq %%rcx, %0" : "=r"(rcx_after));
    fmt::print("\n  rcx antes do syscall: 0x{:016X}\n", rcx_before);
    fmt::print("  rcx após  do syscall: 0x{:016X}  (modificado pelo kernel!)\n",
               rcx_after);
}

// ── Demo 4: Verificação de erro de syscall ────────────────────
void demo_syscall_error() {
    section("11.4 Verificação de erro — retorno negativo = -errno");
    //
    // O kernel retorna um valor negativo quando o syscall falha.
    // Por convenção: retorno < 0 → erro, e -retorno é o código errno.
    //
    // A libc converte isso: if (rax < 0) { errno = -rax; rax = -1; }

    // Tenta abrir um arquivo inexistente (syscall 2 = open)
    const char* path = "/arquivo_que_nao_existe";
    int64_t ret;
    asm volatile (
        "syscall"
        : "=a"(ret)
        : "0"(2LL),    // sys_open
          "D"(path),   // rdi = pathname
          "S"(0LL),    // rsi = flags = O_RDONLY = 0
          "d"(0LL)     // rdx = mode = 0
        : "rcx", "r11", "memory"
    );

    fmt::print("  open(\"/arquivo_que_nao_existe\") → {}\n", ret);
    if (ret < 0) {
        fmt::print("  Erro! errno = {} (ENOENT=2, EACCES=13, etc.)\n", -ret);
        fmt::print("  -ret = {} = ENOENT (arquivo não encontrado)\n", (int)-ret);
    }
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 11 · System Calls Diretas      ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_getpid();
    demo_write();
    demo_syscall_clobbers();
    demo_syscall_error();

    fmt::print("\n\033[1;32m✓ Módulo 11 completo\033[0m\n\n");
}
