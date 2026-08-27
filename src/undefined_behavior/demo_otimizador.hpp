// =============================================================================
//  src/undefined_behavior/demo_otimizador.hpp
//
//  Como o UB licencia o otimizador
//  ───────────────────────────────
//  O compilador raciocina assim, e a lógica é impecável:
//
//      "Este programa não tem UB — o padrão diz que programas corretos não
//       têm. Logo, se `p` foi desreferenciado, `p != nullptr`. Logo, a
//       checagem `if (p)` três linhas abaixo é sempre verdadeira. Logo,
//       posso apagá-la."
//
//  O resultado é que o UB não causa "um valor errado". Ele causa a REMOÇÃO
//  de código que você escreveu, ou a manutenção de código que você achou que
//  estava protegido.
//
//  Tópicos:
//  • O padrão de raciocínio "isto não pode acontecer"
//  • ⚠ MEDIDO: o mesmo UAF, detectado com -O0 e invisível com -O1
//  • Time travel: o efeito antes da causa
//  • Os casos famosos
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <climits>

namespace demo_otimizador {

void demo_raciocinio() {
    fmt::print("\n── 2.1 \"Isto não pode acontecer\" ──\n");

    fmt::print("      (a) A checagem removida — o bug do kernel Linux, 2009:\n\n");
    fmt::print("        void tun_chr_poll(struct file* file) {{\n");
    fmt::print("            struct sock* sk = tun->sk;      // ← desreferencia tun\n");
    fmt::print("            if (!tun) return POLLERR;       // ← checa DEPOIS\n");
    fmt::print("            ...\n");
    fmt::print("        }}\n\n");
    fmt::print("      O GCC raciocinou: `tun->sk` só é legal se `tun != nullptr`,\n");
    fmt::print("      logo `!tun` é sempre falso, logo o `if` inteiro pode sumir.\n");
    fmt::print("      E sumiu. Virou o CVE-2009-1897, escalação de privilégio —\n");
    fmt::print("      um atacante conseguia mapear a página zero.\n\n");

    fmt::print("      (b) O laço que não termina:\n");
    fmt::print("        for (int i = 1; i > 0; i *= 2) {{ ... }}\n");
    fmt::print("      Overflow com sinal é UB, logo `i` \"nunca\" fica negativo,\n");
    fmt::print("      logo a condição é sempre verdadeira, logo é `while (true)`.\n\n");

    fmt::print("      (c) A checagem de overflow que não checa:\n");
    fmt::print("        if (a + b < a) trata_overflow();     // ❌ some com -O2\n");
    fmt::print("      A forma correta:\n");
    fmt::print("        if (__builtin_add_overflow(a, b, &r)) trata_overflow();\n");
    fmt::print("      ou, sem extensão, comparar ANTES:\n");
    fmt::print("        if (b > 0 && a > INT_MAX - b) trata_overflow();\n");

    // A versão correta, executada
    {
        auto soma_segura = [](int a, int b, int& out) {
            return __builtin_add_overflow(a, b, &out);   // true = estourou
        };
        int r = 0;
        fmt::print("\n      medido: soma_segura(INT_MAX, 1) → estourou? {}\n",
                   soma_segura(INT_MAX, 1, r));
        fmt::print("              soma_segura(2, 3)         → estourou? {}, r = {}\n",
                   soma_segura(2, 3, r), r);
        fmt::print("      `__builtin_add_overflow` (GCC/Clang) usa o flag do\n");
        fmt::print("      processador: é uma instrução, não uma comparação extra.\n");
    }
}

void demo_medido() {
    fmt::print("\n── 2.2 ⚠ MEDIDO: o UAF que some com -O1 ──\n");

    fmt::print("      O arquivo ub_exemplos.cpp deste subprojeto tem:\n\n");
    fmt::print("        int caso_uaf(int n) {{\n");
    fmt::print("            int* p = new int[4];\n");
    fmt::print("            p[0] = n;\n");
    fmt::print("            delete[] p;\n");
    fmt::print("            return p[0];            // ← use-after-free\n");
    fmt::print("        }}\n\n");

    fmt::print("      Compilado com -fsanitize=address, o MESMO código:\n\n");
    fmt::print("        -O0 → ERROR: AddressSanitizer: heap-use-after-free\n");
    fmt::print("        -O1 → imprime 42, sem diagnóstico nenhum\n\n");

    fmt::print("      Por quê? Com -O1 o compilador viu `p[0] = n; ... return p[0]`\n");
    fmt::print("      e propagou a constante: `return n`. A LEITURA da memória\n");
    fmt::print("      liberada desapareceu antes de o sanitizer poder\n");
    fmt::print("      instrumentá-la. Não há acesso, logo não há o que reportar.\n\n");

    fmt::print("      Duas conclusões práticas:\n");
    fmt::print("        1. rode a suíte sob sanitizer em -O0 E em -O1/-O2. Cada\n");
    fmt::print("           nível esconde e revela coisas diferentes.\n");
    fmt::print("        2. \"o ASan não acusou\" NÃO é prova de que não há UB.\n\n");

    fmt::print("      (a recomendação oficial do ASan é -O1: bom equilíbrio entre\n");
    fmt::print("       velocidade e fidelidade. Este exemplo mostra que ela\n");
    fmt::print("       também tem custo.)\n");
}

void demo_time_travel() {
    fmt::print("\n── 2.3 Time travel ──\n");

    fmt::print("      O efeito do UB pode aparecer ANTES da linha que o causa:\n\n");
    fmt::print("        void f(int* p, bool cond) {{\n");
    fmt::print("            printf(\"antes\\n\");      // ← pode NÃO ser impresso\n");
    fmt::print("            if (cond) *p = 1;       // ← o UB está aqui\n");
    fmt::print("        }}\n\n");
    fmt::print("      Se o compilador provar que `cond` é sempre true e `p` é\n");
    fmt::print("      sempre nullptr, o corpo inteiro vira \"inalcançável\", e ele\n");
    fmt::print("      pode remover TUDO — inclusive o printf que vinha antes.\n\n");

    fmt::print("      O nome vem do fato de o padrão dizer que a execução INTEIRA\n");
    fmt::print("      fica indefinida — não a partir do UB, e sim desde o começo.\n");
    fmt::print("      É o que torna a depuração de UB tão desagradável: o último\n");
    fmt::print("      `printf` que apareceu não delimita onde o problema está.\n\n");

    fmt::print("      Corolário: `std::unreachable()` (C++23) e `[[assume(x)]]`\n");
    fmt::print("      são UB DELIBERADO. Você promete que aquilo não acontece, e\n");
    fmt::print("      o compilador otimiza em cima. Mentir tem esse mesmo efeito.\n");
    fmt::print("      Ver src/cpp23/demo_biblioteca.hpp.\n");
}

void demo_famosos() {
    fmt::print("\n── 2.4 Os casos famosos ──\n");

    fmt::print("      SSL/TLS, 2014 — a checagem de nulo removida no OpenSSL,\n");
    fmt::print("      pelo mesmo mecanismo do bug do kernel.\n\n");

    fmt::print("      `memset` para apagar senha:\n");
    fmt::print("        void limpa(char* senha, size_t n) {{\n");
    fmt::print("            memset(senha, 0, n);\n");
    fmt::print("        }}   // ← senha não é mais lida: o memset é REMOVIDO\n");
    fmt::print("      Não é UB, é dead-store elimination legítima — mas a senha\n");
    fmt::print("      fica na memória. A correção: `explicit_bzero` (BSD/glibc),\n");
    fmt::print("      `SecureZeroMemory` (Windows) ou um ponteiro `volatile`.\n\n");

    fmt::print("      `this` nunca é nulo:\n");
    fmt::print("        void Widget::f() {{ if (this == nullptr) return; }}\n");
    fmt::print("      Chamar um método já pressupõe `this != nullptr`, então a\n");
    fmt::print("      checagem some. O GCC até avisa: -Wnonnull-compare.\n\n");

    fmt::print("      A função sem `return`:\n");
    fmt::print("        int f(int x) {{ if (x > 0) return 1; }}   // sem else\n");
    fmt::print("      Cair fora de uma função não-void é UB. Na prática o\n");
    fmt::print("      compilador emite código que \"continua\" para a função\n");
    fmt::print("      seguinte no binário. Ligue -Werror=return-type.\n\n");

    fmt::print("      A defesa comum a todos: os avisos do compilador JÁ pegam a\n");
    fmt::print("      maioria. -Wall -Wextra não é opcional — é a primeira linha.\n");
}

inline void run() {
    demo_raciocinio();
    demo_medido();
    demo_time_travel();
    demo_famosos();
}

}  // namespace demo_otimizador
