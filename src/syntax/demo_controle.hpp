// =============================================================================
//  src/syntax/demo_controle.hpp
//
//  Controle de fluxo — if, switch, loops e saltos
//  ─────────────────────────────────────────────
//  São apenas seis construções, e todo programa é feito delas:
//
//      seleção   →  if / else       switch
//      iteração  →  while  do-while  for  range-for
//      salto     →  break  continue  return  goto
//
//  O que muda entre C e C++ moderno não é a lista, é a POSSIBILIDADE de limitar
//  o escopo: `if (auto x = f(); x)` faz `x` morrer no fim do `if`. Cada nome que
//  não escapa do bloco é um bug a menos.
//
//  Tópicos:
//  • `if`, `else if`, e o init-statement do C++17
//  • `if constexpr` — o galho descartado em compile-time
//  • `switch`: fallthrough, `default`, escopo, e o `case` com init
//  • `while`, `do-while` e as três cláusulas opcionais do `for`
//  • range-for: como o compilador o reescreve, e o init do C++20
//  • `break`, `continue`, `return`, `goto` e labels
//  • Ternário, curto-circuito e o operador vírgula
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_controle {

// ─────────────────────────────────────────────────────────────────────────────
//  1. if / else — e o init-statement
// ─────────────────────────────────────────────────────────────────────────────
std::optional<int> procurar(const std::map<std::string, int>& m, const std::string& chave) {
    if (auto it = m.find(chave); it != m.end()) return it->second;
    return std::nullopt;
}

void demo_if() {
    fmt::print("\n── 2.1 if / else ──\n");

    int nota = 78;

    // A cadeia clássica: else-if encadeado
    if (nota >= 90) {
        fmt::print("      nota {} → A\n", nota);
    } else if (nota >= 80) {
        fmt::print("      nota {} → B\n", nota);
    } else if (nota >= 70) {
        fmt::print("      nota {} → C\n", nota);
    } else {
        fmt::print("      nota {} → reprovado\n", nota);
    }

    // Sem chaves, o `if` governa UMA instrução só — origem de bugs históricos
    fmt::print("\n      ⚠ Sem chaves, o if governa apenas UMA instrução:\n");
    fmt::print("          if (cond)\n");
    fmt::print("              a();\n");
    fmt::print("              b();   ← executa SEMPRE (o `goto fail;` da Apple, 2014)\n");
    fmt::print("        → use chaves sempre. Custo zero, um bug de segurança a menos.\n");

    // init-statement (C++17): a variável existe só dentro do if/else
    std::map<std::string, int> estoque{{"parafuso", 120}, {"porca", 45}};

    fmt::print("\n      init-statement (C++17) — o nome morre no fim do if:\n");
    if (auto it = estoque.find("porca"); it != estoque.end()) {
        fmt::print("        encontrado: {} = {}\n", it->first, it->second);
    } else {
        fmt::print("        não encontrado\n");
    }
    // `it` já não existe aqui — impossível reusá-lo por engano.

    if (auto achado = procurar(estoque, "parafuso"); achado.has_value()) {
        fmt::print("        procurar(\"parafuso\") → {}\n", *achado);
    }
    if (auto achado = procurar(estoque, "arruela"); !achado) {
        fmt::print("        procurar(\"arruela\")  → nullopt\n");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. if constexpr — o galho que nem chega a ser compilado
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
std::string descrever(const T& valor) {
    if constexpr (std::is_integral_v<T>) {
        return fmt::format("inteiro de {} bytes, valor {}", sizeof(T), valor);
    } else if constexpr (std::is_floating_point_v<T>) {
        return fmt::format("ponto flutuante, valor {:.2f}", valor);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return fmt::format("string de {} caracteres", valor.size());   // .size() só existe aqui
    } else {
        return "tipo desconhecido";
    }
}

void demo_if_constexpr() {
    fmt::print("\n── 2.2 if constexpr (C++17) ──\n");

    fmt::print("      descrever(42)        → {}\n", descrever(42));
    fmt::print("      descrever(3.14159)   → {}\n", descrever(3.14159));
    fmt::print("      descrever(\"abc\"s)    → {}\n", descrever(std::string{"abc"}));
    fmt::print("      descrever('x')       → {}\n", descrever('x'));

    fmt::print("\n      A diferença para o `if` normal:\n");
    fmt::print("        if        → os DOIS galhos são compilados, um é escolhido em runtime\n");
    fmt::print("        if constexpr → o galho falso é DESCARTADO; nem precisa ser válido\n");
    fmt::print("      Por isso `valor.size()` compila mesmo quando T é int: naquele\n");
    fmt::print("      caso o galho nem existe. Ver src/templates e src/compile_time.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. switch — todas as particularidades
// ─────────────────────────────────────────────────────────────────────────────
enum class Comando { Iniciar, Pausar, Retomar, Parar, Reiniciar };

const char* executar(Comando c) {
    switch (c) {
        case Comando::Iniciar:
            return "motor ligado";

        // Fallthrough INTENCIONAL: Retomar faz o mesmo que Iniciar mais um log.
        // Sem [[fallthrough]] o compilador avisa (-Wimplicit-fallthrough).
        case Comando::Retomar:
            fmt::print("        (log: retomando de uma pausa)\n");
            [[fallthrough]];

        case Comando::Pausar:
            return "estado alterado";

        case Comando::Parar: {
            // Precisa de CHAVES para declarar variáveis dentro de um case:
            // sem elas, o escopo é o switch inteiro e o salto pularia a init.
            const char* motivo = "pedido do usuário";
            fmt::print("        (log: parando — {})\n", motivo);
            return "motor desligado";
        }

        // Sem `default`, o compilador AVISA se um enumerador ficar de fora —
        // e esse aviso é justamente o que se quer ao adicionar um comando novo.
        case Comando::Reiniciar:
            return "reiniciando";
    }
    return "impossível";
}

void demo_switch() {
    fmt::print("\n── 2.3 switch ──\n");

    for (auto c : {Comando::Iniciar, Comando::Pausar, Comando::Retomar,
                   Comando::Parar, Comando::Reiniciar}) {
        const char* resultado = executar(c);          // roda antes de imprimir,
        fmt::print("      executar(...) → {}\n", resultado);   // para o log vir antes
    }

    // Fallthrough como agrupamento de casos (sem código entre eles)
    fmt::print("\n      Agrupando casos (fallthrough sem corpo, não precisa de atributo):\n");
    for (char ch : {'a', 'e', 'k', 'u', '7'}) {
        switch (ch) {
            case 'a':
            case 'e':
            case 'i':
            case 'o':
            case 'u':
                fmt::print("        '{}' → vogal\n", ch);
                break;
            default:
                fmt::print("        '{}' → não é vogal\n", ch);
                break;
        }
    }

    // switch com init-statement (C++17)
    std::vector<int> v{5, 3, 9};
    switch (const auto tamanho = v.size(); tamanho) {
        case 0:  fmt::print("\n      switch com init: vazio\n");           break;
        case 1:  fmt::print("\n      switch com init: um elemento\n");     break;
        default: fmt::print("\n      switch com init: {} elementos\n", tamanho); break;
    }

    fmt::print("\n      Regras do switch:\n");
    fmt::print("        • a condição tem de ser INTEGRAL ou enum (nada de string/double)\n");
    fmt::print("        • cada `case` precisa de uma constante de compile-time e ÚNICA\n");
    fmt::print("        • sem `break`, o controle CAI para o próximo case\n");
    fmt::print("        • declarar variável num case exige {{ chaves }}\n");
    fmt::print("        • em enum class, OMITIR o `default` faz o compilador avisar\n");
    fmt::print("          quando um enumerador novo não for tratado — isso é uma feature.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. while, do-while e for
// ─────────────────────────────────────────────────────────────────────────────
void demo_loops() {
    fmt::print("\n── 2.4 while, do-while, for ──\n");

    // while: testa ANTES → pode executar zero vezes
    int n = 3;
    fmt::print("      while (n-- > 0):");
    while (n-- > 0) fmt::print(" {}", n);
    fmt::print("\n");

    // do-while: testa DEPOIS → executa pelo menos uma vez
    int m = 0;
    fmt::print("      do{{...}}while(m > 0):");
    do { fmt::print(" corpo(m={})", m); } while (m > 0);
    fmt::print("   ← rodou mesmo com a condição falsa\n");

    // for canônico: init; condição; incremento
    fmt::print("      for (int i = 0; i < 5; ++i):");
    for (int i = 0; i < 5; ++i) fmt::print(" {}", i);
    fmt::print("\n");

    // As três cláusulas são TODAS opcionais
    fmt::print("\n      As três cláusulas são opcionais:\n");
    fmt::print("        for (;;) {{ }}         ← laço infinito\n");
    int contador = 0;
    for (;;) {                       // sem init, sem condição, sem incremento
        if (++contador == 3) break;  // a saída fica no corpo
    }
    fmt::print("        saiu por break após {} voltas\n", contador);

    // Múltiplas variáveis por vírgula; duas variáveis de controle
    fmt::print("\n      Duas variáveis, andando em direções opostas:\n        ");
    for (int i = 0, j = 9; i < j; ++i, --j) fmt::print("({},{}) ", i, j);
    fmt::print("\n");

    // Laço aninhado + break/continue
    fmt::print("\n      Aninhado, com continue e break:\n");
    for (int linha = 1; linha <= 3; ++linha) {
        fmt::print("        linha {}:", linha);
        for (int col = 1; col <= 5; ++col) {
            if (col == 2) continue;   // pula ESTA iteração do laço interno
            if (col == 4) break;      // encerra o laço INTERNO (só ele!)
            fmt::print(" {}x{}", linha, col);
        }
        fmt::print("\n");
    }
    fmt::print("        ⚠ `break` sai de UM nível. Para sair de dois, use uma\n");
    fmt::print("          flag, um `return` (extraindo a função) ou `goto`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. range-for — e como o compilador o reescreve
// ─────────────────────────────────────────────────────────────────────────────
void demo_range_for() {
    fmt::print("\n── 2.5 range-based for ──\n");

    std::vector<int> v{1, 2, 3, 4, 5};

    fmt::print("      por valor  `for (int x : v)`        → cópia de cada elemento:");
    for (int x : v) fmt::print(" {}", x);
    fmt::print("\n");

    fmt::print("      por const& `for (const auto& x : v)`→ sem cópia, só leitura:");
    for (const auto& x : v) fmt::print(" {}", x);
    fmt::print("\n");

    fmt::print("      por ref    `for (auto& x : v)`      → MODIFICA o container:");
    for (auto& x : v) x *= 10;
    for (const auto& x : v) fmt::print(" {}", x);
    fmt::print("\n");

    fmt::print("\n      O compilador reescreve isto:\n");
    fmt::print("        for (auto& x : v) {{ corpo }}\n");
    fmt::print("      aproximadamente como:\n");
    fmt::print("        auto&& __r = v;\n");
    fmt::print("        for (auto __b = begin(__r), __e = end(__r); __b != __e; ++__b) {{\n");
    fmt::print("            auto& x = *__b;  corpo\n");
    fmt::print("        }}\n");
    fmt::print("      → basta ter `begin()`/`end()` para um tipo funcionar aqui.\n");

    // Structured bindings + range-for: o idioma para percorrer mapas
    std::map<std::string, int> pontos{{"ana", 10}, {"bruno", 7}};
    fmt::print("\n      Com structured bindings (o idioma para mapas):\n");
    for (const auto& [nome, valor] : pontos) {
        fmt::print("        {} → {}\n", nome, valor);
    }

    // init-statement no range-for (C++20)
    fmt::print("\n      init-statement no range-for (C++20):\n");
    for (auto dobro = [](int x) { return x * 2; }; const auto& x : v) {
        fmt::print("        {} → {}\n", x, dobro(x));
    }

    fmt::print("\n      ⚠ Modificar o container DENTRO do range-for invalida os\n");
    fmt::print("        iteradores → UB. Ver as armadilhas em src/stl.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Saltos: break, continue, return, goto
// ─────────────────────────────────────────────────────────────────────────────
void demo_saltos() {
    fmt::print("\n── 2.6 break, continue, return, goto ──\n");

    // O caso em que `goto` ainda é defensável: sair de laços aninhados
    const int alvo = 7;
    int achou_i = -1, achou_j = -1;

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            if (i * j == alvo - 1) {   // 6 == 2*3
                achou_i = i;
                achou_j = j;
                goto encontrado;        // sai dos DOIS laços de uma vez
            }
        }
    }
    fmt::print("      não encontrado\n");

encontrado:
    fmt::print("      goto para sair de laços aninhados: i={} j={}\n", achou_i, achou_j);

    fmt::print("\n      A alternativa preferida — extrair uma função e usar `return`:\n");
    auto busca = [](int a) -> std::pair<int, int> {
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 5; ++j)
                if (i * j == a - 1) return {i, j};
        return {-1, -1};
    };
    auto [bi, bj] = busca(alvo);
    fmt::print("        busca({}) → ({}, {})   ← sem goto, sem flag\n", alvo, bi, bj);

    fmt::print("\n      Regras do goto (por isso quase nunca vale a pena):\n");
    fmt::print("        • não pode SALTAR SOBRE uma inicialização (erro de compilação)\n");
    fmt::print("        • não sai da função (para isso: return, ou exceções)\n");
    fmt::print("        • destrutores dos escopos abandonados AINDA rodam (RAII salva)\n");
    fmt::print("        • em C++ o lugar do `goto cleanup;` do C é o RAII\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Ternário, curto-circuito e o operador vírgula
// ─────────────────────────────────────────────────────────────────────────────
bool caro(const char* nome) {
    fmt::print("        (avaliou {})\n", nome);
    return true;
}

void demo_expressoes_de_controle() {
    fmt::print("\n── 2.7 Ternário, curto-circuito e vírgula ──\n");

    int  idade = 20;
    auto rotulo = idade >= 18 ? "adulto" : "menor";
    fmt::print("      cond ? a : b  → \"{}\"\n", rotulo);
    fmt::print("        é uma EXPRESSÃO (tem valor) — dá para inicializar const:\n");
    const int limite = idade >= 18 ? 100 : 10;
    fmt::print("        const int limite = idade >= 18 ? 100 : 10;  → {}\n", limite);
    fmt::print("        ⚠ os dois galhos precisam ter um tipo comum\n");

    fmt::print("\n      Curto-circuito — o segundo operando pode nem ser avaliado:\n");
    fmt::print("        false && caro(\"B\"):\n");
    bool r1 = false && caro("B");
    fmt::print("          resultado {} — \"B\" não apareceu acima\n", r1);
    fmt::print("        true || caro(\"C\"):\n");
    bool r2 = true || caro("C");
    fmt::print("          resultado {} — \"C\" também não\n", r2);
    fmt::print("        → o idioma `if (p && p->campo)` depende disto para ser seguro\n");

    // Operador vírgula: avalia tudo, devolve o ÚLTIMO
    int a = (fmt::print("        (efeito colateral) "), 42);
    fmt::print("\n      Operador vírgula: `(f(), 42)` → devolve o último = {}\n", a);
    fmt::print("        Usado quase só na cláusula de incremento do `for`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_if();
    demo_if_constexpr();
    demo_switch();
    demo_loops();
    demo_range_for();
    demo_saltos();
    demo_expressoes_de_controle();
}

} // namespace demo_controle
