// =============================================================================
//  src/undefined_behavior/demo_defesas.hpp
//
//  As camadas de defesa
//  ────────────────────
//  Nenhuma ferramenta sozinha resolve. A estratégia que funciona é em
//  camadas, da mais barata para a mais cara — e a mais barata é também a
//  mais eficaz, o que costuma surpreender:
//
//    1. ESCREVER código que não tem como ter UB    (custo zero, pega mais)
//    2. os avisos do compilador                     (custo zero)
//    3. análise estática                            (minutos no CI)
//    4. sanitizers nos testes                       (2× no tempo de teste)
//    5. hardening em produção                       (custo em runtime)
//
//  Tópicos:
//  • A camada 1: construções que eliminam a categoria
//  • Os avisos que valem cada um
//  • Hardening da libstdc++
//  • ⚠ A checklist honesta
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <span>
#include <string>
#include <vector>

namespace demo_defesas {

void demo_camada_1() {
    fmt::print("\n── 5.1 Camada 1: não dar chance ──\n");

    fmt::print("      | em vez de...            | use...                      |\n");
    fmt::print("      |-------------------------|-----------------------------|\n");
    fmt::print("      | new / delete            | unique_ptr, vector          |\n");
    fmt::print("      | T* + tamanho            | std::span                   |\n");
    fmt::print("      | array C                 | std::array                  |\n");
    fmt::print("      | v[i] em código duvidoso | v.at(i)                     |\n");
    fmt::print("      | reinterpret_cast        | std::bit_cast               |\n");
    fmt::print("      | int para tamanhos       | std::size_t / ssize_t       |\n");
    fmt::print("      | strcpy / sprintf        | std::string, std::format    |\n");
    fmt::print("      | T* que pode ser nulo    | std::optional<T>, T&        |\n");
    fmt::print("      | union para punning      | std::variant / bit_cast     |\n");
    fmt::print("      | laço com índice         | for (auto& x : c)           |\n\n");

    // span elimina a categoria inteira de "ponteiro e tamanho desencontrados"
    {
        std::vector<int> v{1, 2, 3, 4, 5};

        auto soma_c = [](const int* p, std::size_t n) {          // ❌ dois args
            long s = 0; for (std::size_t i = 0; i < n; ++i) s += p[i]; return s;
        };
        auto soma_span = [](std::span<const int> s) {            // ✓ um arg
            long r = 0; for (int x : s) r += x; return r;
        };

        fmt::print("      soma_c(v.data(), v.size())  = {}\n", soma_c(v.data(), v.size()));
        fmt::print("      soma_span(v)                = {}\n", soma_span(v));
        fmt::print("      A primeira aceita `soma_c(v.data(), 100)` sem reclamar.\n");
        fmt::print("      A segunda não tem onde errar o tamanho.\n");
        fmt::print("      Ver src/vocabulary_types/demo_span.hpp.\n");
    }

    fmt::print("\n      ⚠  `at()` vs `[]`: `at()` LANÇA em vez de ser UB. O custo\n");
    fmt::print("         é uma comparação por acesso — irrelevante fora do laço\n");
    fmt::print("         mais quente. Use `[]` só onde o índice é provadamente\n");
    fmt::print("         válido (um `for` sobre `size()`).\n");
}

void demo_avisos() {
    fmt::print("\n── 5.2 Camada 2: os avisos ──\n");

    fmt::print("      A base, e não é negociável:\n");
    fmt::print("        -Wall -Wextra -Wpedantic\n\n");

    fmt::print("      Os que valem acrescentar, com o que cada um pega:\n");
    fmt::print("        -Wshadow              variável interna escondendo outra\n");
    fmt::print("        -Wconversion          conversão que perde valor\n");
    fmt::print("        -Wsign-conversion     int ↔ unsigned silencioso\n");
    fmt::print("        -Wcast-qual           cast que remove const\n");
    fmt::print("        -Wold-style-cast      (int)x em vez de static_cast\n");
    fmt::print("        -Wnull-dereference    o caso do CVE do kernel (2.1)\n");
    fmt::print("        -Wdouble-promotion    float virando double sem querer\n");
    fmt::print("        -Wuseless-cast        cast que não faz nada\n");
    fmt::print("        -Wnon-virtual-dtor    base polimórfica sem ~virtual\n");
    fmt::print("        -Wduplicated-cond     if/else if com a MESMA condição\n");
    fmt::print("        -Wlogical-op          `&` onde era `&&`\n\n");

    fmt::print("      Os que devem ser ERRO, não aviso:\n");
    fmt::print("        -Werror=return-type       função sem return → é UB\n");
    fmt::print("        -Werror=uninitialized\n");
    fmt::print("        -Werror=implicit-fallthrough\n\n");

    fmt::print("      ⚠  `-Werror` no projeto inteiro é controverso: uma versão\n");
    fmt::print("         nova do compilador acrescenta avisos e quebra o build de\n");
    fmt::print("         quem só queria compilar. O meio-termo usual: -Werror no\n");
    fmt::print("         CI, avisos comuns no build local.\n\n");

    fmt::print("      Este repositório compila com warning_level=3 do Meson\n");
    fmt::print("      (= -Wall -Wextra -Wpedantic) e ZERO avisos em todos os\n");
    fmt::print("      alvos — as poucas supressões são locais, com #pragma e um\n");
    fmt::print("      comentário explicando por que o aviso É o conteúdo.\n");
}

void demo_estatica_e_hardening() {
    fmt::print("\n── 5.3 Camadas 3 e 5 ──\n");

    fmt::print("      ANÁLISE ESTÁTICA (camada 3) — encontra o caminho que os\n");
    fmt::print("      testes não executaram, que é justamente o ponto cego do\n");
    fmt::print("      sanitizer:\n");
    fmt::print("        g++ -fanalyzer                  (GCC 10+, C melhor que C++)\n");
    fmt::print("        clang-tidy --checks='bugprone-*,cert-*,clang-analyzer-*'\n");
    fmt::print("        cppcheck --enable=all\n");
    fmt::print("        include-what-you-use\n\n");
    fmt::print("      ⚠  Falso positivo é a norma. Adote INCREMENTALMENTE: ligue\n");
    fmt::print("         em código novo, e vá limpando o antigo.\n\n");

    fmt::print("      HARDENING (camada 5) — checagens que ficam em PRODUÇÃO:\n");
    fmt::print("        -D_GLIBCXX_ASSERTIONS       verifica v[i], .front(), etc.\n");
    fmt::print("        -D_FORTIFY_SOURCE=3         memcpy/strcpy com tamanho\n");
    fmt::print("                                    conhecido em compilação\n");
    fmt::print("        -fstack-protector-strong    canário na pilha\n");
    fmt::print("        -fstack-clash-protection\n");
    fmt::print("        -fcf-protection             control-flow integrity\n");
    fmt::print("        -D_LIBCPP_HARDENING_MODE=...  (libc++)\n\n");

    fmt::print("      `_GLIBCXX_ASSERTIONS` transforma `v[i]` fora de faixa de UB\n");
    fmt::print("      em abort com mensagem. O custo é pequeno (uma comparação),\n");
    fmt::print("      e a Red Hat e o Google já o ligam por padrão.\n\n");

    fmt::print("      ⚠  ATENÇÃO DE ABI: `_GLIBCXX_ASSERTIONS` muda o layout de\n");
    fmt::print("         alguns tipos. Ele precisa valer para TODAS as unidades\n");
    fmt::print("         e para todas as bibliotecas linkadas — misturar é a\n");
    fmt::print("         violação de ODR de src/linkage/ 2.4, com corrupção\n");
    fmt::print("         silenciosa.\n");
}

void demo_checklist() {
    fmt::print("\n── 5.4 ⚠ A checklist honesta ──\n");

    fmt::print("      Para um projeto que leva UB a sério:\n\n");
    fmt::print("        [ ] -Wall -Wextra -Wpedantic, zero avisos\n");
    fmt::print("        [ ] -Werror=return-type\n");
    fmt::print("        [ ] um job de CI com -fsanitize=address,undefined\n");
    fmt::print("            e -fno-sanitize-recover=all\n");
    fmt::print("        [ ] um job separado com -fsanitize=thread (se há threads)\n");
    fmt::print("        [ ] a suíte sob sanitizer em -O0 E em -O2\n");
    fmt::print("        [ ] clang-tidy com bugprone-* no código novo\n");
    fmt::print("        [ ] -D_GLIBCXX_ASSERTIONS uniformemente\n");
    fmt::print("        [ ] fuzzing no que recebe entrada não confiável\n");
    fmt::print("        [ ] `at()` em vez de `[]` fora dos laços quentes\n\n");

    fmt::print("      E a parte honesta: mesmo com tudo isso, C++ não é uma\n");
    fmt::print("      linguagem segura por construção. As camadas reduzem a\n");
    fmt::print("      probabilidade; elas não a zeram. É por isso que o comitê\n");
    fmt::print("      discute profiles de segurança e que Rust ganhou tração em\n");
    fmt::print("      código de segurança crítica.\n\n");

    fmt::print("      Isso não é argumento para abandonar C++ — é argumento para\n");
    fmt::print("      levar as camadas a sério, e para reconhecer que \"eu tomo\n");
    fmt::print("      cuidado\" nunca foi uma delas.\n\n");

    fmt::print("      Neste repositório: todo subprojeto compila sem avisos e é\n");
    fmt::print("      validado sob ASan+UBSan; `concurrency2` também sob TSan.\n");
    fmt::print("      O que cada README chama de \"Como validar\" é exatamente\n");
    fmt::print("      esta checklist aplicada.\n");
}

inline void run() {
    demo_camada_1();
    demo_avisos();
    demo_estatica_e_hardening();
    demo_checklist();
}

}  // namespace demo_defesas
