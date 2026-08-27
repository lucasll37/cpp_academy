// =============================================================================
//  src/compile_time/demo_constexpr.hpp
//
//  constexpr — mover trabalho da execução para a compilação
//  ────────────────────────────────────────────────────────
//  `constexpr` não é uma promessa de "isto SEMPRE roda em compilação". É uma
//  permissão: "isto PODE rodar em compilação, se os argumentos permitirem".
//  A mesma função serve para os dois mundos.
//
//  A evolução do que cabe numa função constexpr é uma das linhas mais visíveis
//  da história recente do C++:
//
//    C++11 — praticamente só `return expr;` (uma única instrução)
//    C++14 — laços, variáveis locais, if/switch, múltiplos returns
//    C++17 — `if constexpr`, lambdas constexpr
//    C++20 — alocação dinâmica transitória, try/catch, virtual, std::vector,
//            std::string, e a maior parte dos algoritmos
//    C++23 — `if consteval`, goto, variáveis static/thread_local
//
//  Tópicos:
//  • constexpr é permissão, não garantia
//  • Como FORÇAR a avaliação em compilação
//  • O que mudou em C++20: vector e string em constexpr
//  • Alocação transitória — a regra que a torna possível
//  • constexpr em classes, membros e construtores
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace demo_constexpr {

// ─────────────────────────────────────────────────────────────────────────────
//  1. `constexpr` é permissão, não garantia
//
//     A MESMA função roda nos dois contextos. Quem decide é o chamador:
//     se o resultado é exigido em compilação, o compilador avalia lá; se não,
//     ele emite código normal.
// ─────────────────────────────────────────────────────────────────────────────
constexpr int fatorial(int n) {
    int r = 1;
    for (int i = 2; i <= n; ++i) r *= i;      // laço: legal desde C++14
    return r;
}

void demo_permissao() {
    fmt::print("\n── 1.1 constexpr é permissão, não garantia ──\n");

    // Contexto que EXIGE compilação: o compilador avalia agora
    constexpr int em_compilacao = fatorial(10);
    static_assert(em_compilacao == 3'628'800);
    fmt::print("      constexpr int x = fatorial(10);  → {} (avaliado na compilação)\n",
               em_compilacao);

    // Mesma função, argumento de runtime: vira código de máquina normal
    int n = 5;                                 // não é constexpr
    const int em_runtime = fatorial(n);
    fmt::print("      int n = 5; fatorial(n);          → {} (avaliado na execução)\n",
               em_runtime);

    fmt::print("\n      É a MESMA função nos dois casos. `constexpr` na assinatura\n");
    fmt::print("      apenas garante que ela PODE ser avaliada em compilação —\n");
    fmt::print("      não que ela SERÁ.\n");

    // O array de tamanho constexpr é a prova mais direta: o tamanho de um
    // array precisa ser constante de compilação.
    std::array<int, fatorial(5)> arr{};
    fmt::print("      std::array<int, fatorial(5)> → {} elementos\n", arr.size());
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Como FORÇAR a avaliação em compilação
//
//     Três formas, da mais fraca para a mais forte.
// ─────────────────────────────────────────────────────────────────────────────
constexpr int soma_ate(int n) {
    int s = 0;
    for (int i = 1; i <= n; ++i) s += i;
    return s;
}

void demo_forcar() {
    fmt::print("\n── 1.2 Forçando a avaliação em compilação ──\n");

    int argumento_runtime = 100;

    // (a) variável `constexpr` — exige, mas só naquela declaração
    constexpr int a = soma_ate(100);
    fmt::print("      (a) constexpr int a = soma_ate(100);        → {}\n", a);

    // (b) `static_assert` — exige e ainda verifica o valor
    static_assert(soma_ate(100) == 5050);
    fmt::print("      (b) static_assert(soma_ate(100) == 5050);   → passou\n");

    // (c) NTTP — o argumento de template é o contexto mais exigente que existe
    fmt::print("      (c) std::array<int, soma_ate(4)>            → {} elementos\n",
               std::array<int, soma_ate(4)>{}.size());

    // Sem nenhuma dessas, o compilador PODE avaliar em compilação (e geralmente
    // avalia com -O2), mas não é obrigado.
    const int talvez = soma_ate(argumento_runtime);
    fmt::print("      sem forçar: soma_ate(variável) → {} (o compilador decide)\n", talvez);

    fmt::print("\n      Para GARANTIR que uma função nunca rode em runtime,\n");
    fmt::print("      a ferramenta é `consteval` — ver demo_consteval.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. C++20: std::vector e std::string em constexpr
//
//     Até o C++17, alocar em contexto constexpr era proibido — o que excluía
//     todos os containers dinâmicos. O C++20 permitiu, com uma regra:
//
//       TODA alocação feita durante a avaliação em compilação precisa ser
//       LIBERADA antes de a avaliação terminar. É a "alocação transitória".
//
//     Ou seja: você pode usar um vector como memória de rascunho, mas não pode
//     devolver um vector de uma função consteval para o mundo do runtime.
// ─────────────────────────────────────────────────────────────────────────────

// ✓ Legal: o vector nasce e morre dentro da avaliação
constexpr int soma_dos_primos_ate(int limite) {
    std::vector<bool> composto(limite + 1, false);      // aloca

    int soma = 0;
    for (int i = 2; i <= limite; ++i) {
        if (composto[i]) continue;
        soma += i;
        for (int j = i * i; j <= limite; j += i) composto[j] = true;
    }
    return soma;                                        // o vector é destruído aqui
}

// ✓ Legal: usa string como rascunho e devolve um tipo de tamanho fixo
constexpr std::size_t contar_vogais(std::string_view s) {
    std::string minusculo{s};                            // aloca
    std::transform(minusculo.begin(), minusculo.end(), minusculo.begin(),
                   [](char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; });
    return static_cast<std::size_t>(std::count_if(minusculo.begin(), minusculo.end(),
        [](char c) { return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }));
}

// ✗ ILEGAL em contexto constexpr: a alocação escaparia para o runtime.
//   constexpr std::vector<int> v = {1,2,3};   // não compila
//   O motivo: o vector alocado durante a compilação não teria como sobreviver
//   até a execução — a memória do compilador não existe mais.
//
// ✓ A alternativa: devolver std::array, cujo armazenamento é o próprio objeto.
template <std::size_t N>
constexpr std::array<int, N> primeiros_quadrados() {
    std::array<int, N> a{};
    for (std::size_t i = 0; i < N; ++i) a[i] = static_cast<int>((i + 1) * (i + 1));
    return a;
}

void demo_cpp20() {
    fmt::print("\n── 1.3 C++20: vector e string em constexpr ──\n");

    // A criba de Eratóstenes inteira, resolvida antes de o programa existir
    constexpr int soma_primos = soma_dos_primos_ate(1000);
    static_assert(soma_primos == 76127);
    fmt::print("      soma dos primos até 1000 = {} (crivo rodou na COMPILAÇÃO)\n",
               soma_primos);

    constexpr auto vogais = contar_vogais("Programacao Orientada a Objetos");
    static_assert(vogais == 14);
    fmt::print("      vogais em 'Programacao Orientada a Objetos' = {}\n", vogais);

    constexpr auto quadrados = primeiros_quadrados<8>();
    fmt::print("      primeiros 8 quadrados: ");
    for (int q : quadrados) fmt::print("{} ", q);
    fmt::print("\n");

    fmt::print("\n      A REGRA da alocação transitória:\n");
    fmt::print("        toda alocação feita na avaliação em compilação precisa\n");
    fmt::print("        ser LIBERADA antes de a avaliação terminar.\n");
    fmt::print("      Consequência: `constexpr std::vector<int> v = ...;` NÃO\n");
    fmt::print("      compila — a memória do compilador não sobrevive até o\n");
    fmt::print("      runtime. Use `std::array` para devolver resultados.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. constexpr em classes
//
//     Um construtor constexpr permite que objetos do tipo existam em contexto
//     de compilação. Os métodos constexpr operam neles.
//
//     Desde o C++14, métodos constexpr não são implicitamente `const` — por
//     isso `avancar()` abaixo pode modificar o objeto durante a avaliação.
// ─────────────────────────────────────────────────────────────────────────────
class Racional {
public:
    constexpr Racional(int num, int den) : num_(num), den_(den) {
        // Normaliza no construtor — em compilação, inclusive
        const int g = mdc(num < 0 ? -num : num, den < 0 ? -den : den);
        if (g > 1) { num_ /= g; den_ /= g; }
        if (den_ < 0) { num_ = -num_; den_ = -den_; }
    }

    constexpr int numerador()   const { return num_; }
    constexpr int denominador() const { return den_; }

    constexpr Racional operator*(const Racional& o) const {
        return {num_ * o.num_, den_ * o.den_};
    }
    constexpr Racional operator+(const Racional& o) const {
        return {num_ * o.den_ + o.num_ * den_, den_ * o.den_};
    }

    // Não-const: modifica o próprio objeto. Legal em constexpr desde C++14.
    constexpr void inverter() {
        const int t = num_; num_ = den_; den_ = t;
    }

private:
    static constexpr int mdc(int a, int b) { return b == 0 ? a : mdc(b, a % b); }
    int num_, den_;
};

void demo_classes() {
    fmt::print("\n── 1.4 constexpr em classes ──\n");

    // O objeto inteiro existe em tempo de compilação
    constexpr Racional meio{2, 4};          // normaliza para 1/2 na compilação
    static_assert(meio.numerador()   == 1);
    static_assert(meio.denominador() == 2);
    fmt::print("      Racional{{2,4}} normalizado → {}/{}\n",
               meio.numerador(), meio.denominador());

    constexpr Racional soma = Racional{1, 3} + Racional{1, 6};
    static_assert(soma.numerador() == 1 && soma.denominador() == 2);
    fmt::print("      1/3 + 1/6 = {}/{}  (calculado na compilação)\n",
               soma.numerador(), soma.denominador());

    // Modificação durante a avaliação: legal, num objeto local da função
    constexpr auto invertido = [] {
        Racional r{3, 4};
        r.inverter();
        return r;
    }();
    static_assert(invertido.numerador() == 4);
    fmt::print("      (3/4).inverter() = {}/{}  (lambda constexpr, C++17)\n",
               invertido.numerador(), invertido.denominador());

    fmt::print("\n      O que a classe precisa para ser \"constexpr-friendly\":\n");
    fmt::print("        • construtor constexpr (e um destrutor trivial, ou\n");
    fmt::print("          constexpr desde C++20)\n");
    fmt::print("        • membros de tipo literal\n");
    fmt::print("        • nenhum membro virtual antes do C++20\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O que a avaliação em compilação NÃO pode fazer
// ─────────────────────────────────────────────────────────────────────────────
void demo_limites() {
    fmt::print("\n── 1.5 Os limites ──\n");

    fmt::print("      Uma avaliação constexpr NÃO pode:\n");
    fmt::print("        • fazer I/O (não há sistema operacional na compilação)\n");
    fmt::print("        • chamar função não-constexpr (inclui a maioria da libc)\n");
    fmt::print("        • ler variável não-constexpr\n");
    fmt::print("        • reinterpret_cast, nem type punning\n");
    fmt::print("        • deixar alocação viva ao fim da avaliação\n");
    fmt::print("        • causar undefined behavior — e ESTA é a mais interessante\n");

    fmt::print("\n      A regra do UB é uma ferramenta poderosa: em contexto\n");
    fmt::print("      constexpr, qualquer UB vira ERRO DE COMPILAÇÃO.\n");
    fmt::print("        constexpr int x = 1 / 0;            // erro, não crash\n");
    fmt::print("        constexpr int y = arr[10];          // erro se fora dos limites\n");
    fmt::print("        constexpr int z = *(int*)nullptr;   // erro\n");
    fmt::print("      Ou seja: código testado sob `static_assert` é código\n");
    fmt::print("      PROVADAMENTE livre de UB naquele caminho.\n");

    fmt::print("\n      Há também um limite prático: o compilador impõe um teto\n");
    fmt::print("      de passos de avaliação (GCC: -fconstexpr-ops-limit,\n");
    fmt::print("      padrão 33.554.432). Recursão profunda demais falha com\n");
    fmt::print("      \"constexpr evaluation operation count exceeded\".\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_permissao();
    demo_forcar();
    demo_cpp20();
    demo_classes();
    demo_limites();
}

} // namespace demo_constexpr
