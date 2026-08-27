// =============================================================================
//  src/cpp23/demo_disponivel.hpp
//
//  As adições que este GCC tem e que quase ninguém usa
//  ───────────────────────────────────────────────────
//  O inventário (demo_inventario.hpp) revelou features presentes que não
//  estavam no radar. Este demo usa cada uma delas de verdade.
//
//  Tópicos:
//  • std::move_only_function — o std::function que aceita move-only
//  • std::spanstream — stream sobre buffer, sem alocar
//  • views::chunk e views::slide — janelas
//  • std::stacktrace — e a biblioteca extra que ele exige
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <cstddef>
#include <algorithm>
#include <functional>
#include <memory>
#include <ranges>
#include <iterator>
#include <span>
#include <spanstream>
#include <string>
#include <string_view>
#include <vector>
#include <version>

namespace demo_disponivel {

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::move_only_function
//
//     `std::function` exige que o chamável seja COPIÁVEL — mesmo quando ninguém
//     vai copiá-lo. Isso impede guardar uma lambda que capturou um `unique_ptr`,
//     uma `std::thread` ou um `std::promise`.
// ─────────────────────────────────────────────────────────────────────────────
void demo_move_only_function() {
    fmt::print("\n── 4.1 std::move_only_function ──\n");

    auto recurso = std::make_unique<std::string>("recurso caro");

    // ✗ std::function não aceita: a lambda não é copiável
    // std::function<std::string()> f = [r = std::move(recurso)] { return *r; };

    // ✓ move_only_function aceita
    std::move_only_function<std::string()> f =
        [r = std::move(recurso)] { return *r; };

    fmt::print("      lambda capturando unique_ptr:\n");
    fmt::print("        std::function<...>            → NÃO COMPILA\n");
    fmt::print("        std::move_only_function<...>  → '{}'\n", f());

    fmt::print("\n      Por que `std::function` exige copiável:\n");
    fmt::print("        ele PROMETE ser copiável, e para cumprir precisa que o\n");
    fmt::print("        chamável embrulhado também seja — mesmo que você nunca\n");
    fmt::print("        copie. É uma exigência do TIPO, não do uso.\n");

    fmt::print("\n      Onde isso morde na prática:\n");
    fmt::print("        • fila de tarefas de um thread pool\n");
    fmt::print("          (a tarefa captura um promise, que é move-only)\n");
    fmt::print("        • callbacks que possuem um recurso\n");
    fmt::print("        • continuações que capturam um socket\n");

    fmt::print("\n      A API também permite qualificar a chamada:\n");
    fmt::print("        move_only_function<void() const>       → operator() const\n");
    fmt::print("        move_only_function<void() &&>          → só em rvalue\n");
    fmt::print("        move_only_function<void() noexcept>    → promete noexcept\n");
    fmt::print("      `std::function` nunca teve nada disso.\n");

    // Uma fila de tarefas move-only, que é o caso de uso canônico
    std::vector<std::move_only_function<int()>> tarefas;
    for (int i = 1; i <= 3; ++i) {
        auto dado = std::make_unique<int>(i * 10);
        tarefas.emplace_back([d = std::move(dado)] { return *d; });
    }
    fmt::print("\n      fila de {} tarefas, cada uma dona de um unique_ptr:\n        ",
               tarefas.size());
    for (auto& t : tarefas) fmt::print("{} ", t());
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::spanstream — stream sobre um buffer que já existe
//
//     `stringstream` sempre ALOCA. Para parsear um buffer que você já tem,
//     isso é uma cópia inteira desnecessária. O `strstream` resolvia e foi
//     depreciado (a API era insegura); o `spanstream` é o substituto correto.
// ─────────────────────────────────────────────────────────────────────────────
void demo_spanstream() {
    fmt::print("\n── 4.2 std::spanstream ──\n");

    // Parsear um buffer existente, SEM copiar.
    //
    // ⚠ `std::span{dados}` sobre um `char[]` inclui o '\0' final — o array
    //   tem 14 bytes, não 13. Passar isso ao stream faz o terminador virar
    //   um caractere de dado, e a última string sai com um byte a mais.
    //   `std::size(dados) - 1` corta o terminador.
    char dados[] = "42 3.14 texto";
    std::ispanstream entrada{std::span<char>{dados, std::size(dados) - 1}};

    int         i = 0;
    double      d = 0;
    std::string s;
    entrada >> i >> d >> s;

    fmt::print("      buffer: \"{}\"\n", dados);
    fmt::print("      lido  : int={} double={} string='{}' ({} chars)\n",
               i, d, s, s.size());
    fmt::print("        → nenhuma cópia do buffer foi feita\n");
    fmt::print("        ⚠  o `- 1` no span não é detalhe: sem ele, `s` viria\n");
    fmt::print("           com 6 caracteres, incluindo o '\\0' do literal.\n");

    // Escrever num buffer da pilha, sem alocar
    char saida[64]{};
    std::ospanstream escrita{std::span<char>{saida}};
    escrita << "id=" << 7 << " v=" << 1.5;
    const auto escrito = escrita.span();

    fmt::print("\n      escrita num char[64] da pilha:\n");
    fmt::print("        '{}' ({} bytes usados de {})\n",
               std::string_view{escrito.data(), escrito.size()},
               escrito.size(), sizeof(saida));

    fmt::print("\n      | classe          | buffer          | aloca |\n");
    fmt::print("      |-----------------|-----------------|-------|\n");
    fmt::print("      | stringstream    | próprio, dinâm. | SIM   |\n");
    fmt::print("      | strstream       | externo         | não   | (DEPRECIADO)\n");
    fmt::print("      | spanstream      | externo, span   | não   |\n");

    fmt::print("\n      O `strstream` foi depreciado no C++98 e removido em\n");
    fmt::print("      espírito porque a API não deixava claro quem era dono do\n");
    fmt::print("      buffer nem quando ele podia ser liberado. O `span` resolve\n");
    fmt::print("      isso por construção: ele NUNCA é dono.\n");
    fmt::print("      ⚠  e traz a mesma regra: o buffer precisa sobreviver ao\n");
    fmt::print("         stream (src/vocabulary_types/demo_span.hpp, 5.6).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. views::chunk e views::slide
// ─────────────────────────────────────────────────────────────────────────────
void demo_chunk_slide() {
    fmt::print("\n── 4.3 views::chunk e views::slide ──\n");

    const std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    fmt::print("      base: {}\n", fmt::join(v, " "));

    fmt::print("\n      chunk(3) — blocos DISJUNTOS:\n");
    for (auto bloco : v | std::views::chunk(3)) {
        fmt::print("        [{}]\n", fmt::join(bloco, " "));
    }

    fmt::print("\n      slide(3) — janela DESLIZANTE:\n");
    for (auto janela : v | std::views::slide(3)) {
        fmt::print("        [{}]\n", fmt::join(janela, " "));
    }

    fmt::print("\n      A diferença: `chunk` avança de N em N (o último bloco\n");
    fmt::print("      pode ser menor); `slide` avança de 1 em 1 e todas as\n");
    fmt::print("      janelas têm o mesmo tamanho.\n");

    // Uso real: média móvel com slide
    fmt::print("\n      média móvel de 3 elementos (slide + fold):\n        ");
    for (auto janela : v | std::views::slide(3)) {
        const auto soma = std::ranges::fold_left(janela, 0, std::plus{});
        fmt::print("{:.1f} ", soma / 3.0);
    }
    fmt::print("\n");

    // Uso real: processar em lotes com chunk
    fmt::print("\n      processar em lotes de 4 (chunk):\n");
    int lote = 1;
    for (auto b : v | std::views::chunk(4)) {
        fmt::print("        lote {}: {} elementos, soma {}\n", lote++,
                   std::ranges::distance(b),
                   std::ranges::fold_left(b, 0, std::plus{}));
    }

    fmt::print("\n      Antes do C++23, os dois eram laços com índice e\n");
    fmt::print("      aritmética de fronteira fácil de errar no último bloco.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::stacktrace — presente, mas com uma pegadinha de link
// ─────────────────────────────────────────────────────────────────────────────
void demo_stacktrace() {
    fmt::print("\n── 4.4 std::stacktrace ──\n");

#if defined(__cpp_lib_stacktrace)
    fmt::print("      __cpp_lib_stacktrace = {} → a API EXISTE\n",
               __cpp_lib_stacktrace);
#else
    fmt::print("      __cpp_lib_stacktrace ausente\n");
#endif

    fmt::print("\n      ⚠  MAS ELA NÃO LINKA com as flags padrão:\n");
    fmt::print("        $ g++ -std=c++23 usa_stacktrace.cpp\n");
    fmt::print("        undefined reference to `__glibcxx_backtrace_simple'\n\n");
    fmt::print("      É preciso a biblioteca extra:\n");
    fmt::print("        $ g++ -std=c++23 usa_stacktrace.cpp -lstdc++_libbacktrace\n");
    fmt::print("        frames=5   ✓\n");

    fmt::print("\n      Esta é a lição mais útil do subprojeto inteiro:\n");
    fmt::print("      a feature-test macro diz que a API EXISTE. Ela NÃO diz que\n");
    fmt::print("      o programa vai linkar. Sempre há um passo de verificação\n");
    fmt::print("      além da macro — e ele é COMPILAR DE VERDADE.\n");

    fmt::print("\n      Para habilitar neste subprojeto, seria preciso no\n");
    fmt::print("      meson.build:\n");
    fmt::print("        link_args: ['-lstdc++_libbacktrace']\n");
    fmt::print("      Optamos por não fazer, porque a biblioteca não é padrão em\n");
    fmt::print("      toda distribuição e quebraria o build de quem clonar.\n");

    fmt::print("\n      O que ele daria, quando disponível:\n");
    fmt::print("        auto t = std::stacktrace::current();\n");
    fmt::print("        for (const auto& f : t)\n");
    fmt::print("            std::cout << f.source_file() << ':'\n");
    fmt::print("                      << f.source_line() << ' '\n");
    fmt::print("                      << f.description() << '\\n';\n");
    fmt::print("      → o stack trace PORTÁVEL que o C++ nunca teve.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_move_only_function();
    demo_spanstream();
    demo_chunk_slide();
    demo_stacktrace();
}

} // namespace demo_disponivel
