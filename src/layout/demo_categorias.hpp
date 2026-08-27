// =============================================================================
//  src/layout/demo_categorias.hpp
//
//  Trivial, standard-layout, POD e agregado
//  ────────────────────────────────────────
//  Quatro palavras que soam parecidas e significam coisas diferentes. Elas não
//  são curiosidade acadêmica: cada uma AUTORIZA uma operação concreta.
//
//    trivially copyable → pode usar `memcpy` no objeto inteiro
//    trivially default
//      constructible    → `T x;` não gera código nenhum
//    standard-layout    → o layout é previsível; `offsetof` é legal;
//                         interopera com C
//    agregado           → aceita inicialização por chaves e designated
//                         initializers
//    POD                → trivial E standard-layout (o termo foi APOSENTADO
//                         no C++20 — use os dois traits separados)
//
//  Tópicos:
//  • O que cada categoria autoriza
//  • ⚠ o que quebra cada uma
//  • memcpy: quando é legal e quando é UB
//  • Agregados e designated initializers (C++20)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_categorias {

// ─── Os cobaias ──────────────────────────────────────────────────────────────
struct Simples      { int a; double b; };
struct ComCtor      { int a; ComCtor() : a{1} {} };                 // não trivial
struct ComVirtual   { int a; virtual ~ComVirtual() = default; };    // vptr
struct Privado      { public: int a; private: int b; };             // membros mistos
struct HerdaDados   : Simples { int c; };                           // dados em 2 níveis
struct ComString    { int a; std::string s; };                      // membro não trivial

// ─────────────────────────────────────────────────────────────────────────────
//  1. A tabela
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
void linha(const char* nome) {
    fmt::print("      | {:<12} |   {:<3}   |    {:<3}    |    {:<3}     |   {:<3}    |\n",
               nome,
               std::is_trivially_copyable_v<T>            ? "sim" : "não",
               std::is_trivially_default_constructible_v<T> ? "sim" : "não",
               std::is_standard_layout_v<T>               ? "sim" : "não",
               std::is_aggregate_v<T>                     ? "sim" : "não");
}

void demo_tabela() {
    fmt::print("\n── 3.1 As quatro categorias ──\n");

    fmt::print("      | tipo         | triv.cp | triv.ctor | std.layout | agregado |\n");
    fmt::print("      |--------------|---------|-----------|------------|----------|\n");
    linha<int>         ("int");
    linha<Simples>     ("Simples");
    linha<ComCtor>     ("ComCtor");
    linha<ComVirtual>  ("ComVirtual");
    linha<Privado>     ("Privado");
    linha<HerdaDados>  ("HerdaDados");
    linha<ComString>   ("ComString");
    linha<std::string> ("std::string");

    fmt::print("\n      Lendo a tabela:\n");
    fmt::print("        `ComCtor`     perdeu trivially-default-constructible (tem\n");
    fmt::print("                      construtor escrito à mão) mas continua\n");
    fmt::print("                      trivially-copyable — copiar é memcpy.\n");
    fmt::print("        `ComVirtual`  perdeu tudo: o vptr precisa ser preenchido\n");
    fmt::print("                      pelo construtor, e um memcpy o corromperia.\n");
    fmt::print("        `Privado`     perdeu standard-layout: com membros de dados\n");
    fmt::print("                      em access specifiers DIFERENTES, o compilador\n");
    fmt::print("                      pode reordená-los.\n");
    fmt::print("        `HerdaDados`  perdeu standard-layout: membros de dados em\n");
    fmt::print("                      mais de um nível da hierarquia.\n");
    fmt::print("        `ComString`   nada de trivial — mas AINDA é agregado e\n");
    fmt::print("                      aceita `ComString{{1, \"x\"}}`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O que cada categoria AUTORIZA
// ─────────────────────────────────────────────────────────────────────────────
void demo_autoriza() {
    fmt::print("\n── 3.2 O que cada uma autoriza ──\n");

    // (a) trivially copyable → memcpy é legal
    static_assert(std::is_trivially_copyable_v<Simples>);
    Simples origem{42, 3.14};
    Simples destino;
    std::memcpy(&destino, &origem, sizeof(Simples));      // LEGAL
    fmt::print("      (a) trivially copyable → memcpy legal:\n");
    fmt::print("          copiado por bytes: a={}, b={}\n", destino.a, destino.b);
    fmt::print("          É o que autoriza `std::vector<T>` a usar memmove no\n");
    fmt::print("          realloc em vez de mover elemento a elemento.\n");

    // (b) standard-layout → offsetof e interop com C
    static_assert(std::is_standard_layout_v<Simples>);
    fmt::print("\n      (b) standard-layout → offsetof legal, interopera com C:\n");
    fmt::print("          offsetof(Simples, b) = {}\n", offsetof(Simples, b));
    fmt::print("          Também garante que o endereço do PRIMEIRO membro é o\n");
    fmt::print("          endereço da struct — a base do \"herança por composição\"\n");
    fmt::print("          em C (struct Derivada {{ struct Base b; ... }}).\n");
    fmt::print("          `reinterpret_cast<int*>(&s) == &s.a`? {}\n",
               static_cast<const void*>(reinterpret_cast<const int*>(&origem))
                 == static_cast<const void*>(&origem.a));

    // (c) agregado → designated initializers
    fmt::print("\n      (c) agregado → chaves e designated initializers (C++20):\n");
    Simples s1{7, 2.5};
    Simples s2{.a = 7, .b = 2.5};
    fmt::print("          Simples s{{7, 2.5}}          → a={}, b={}\n", s1.a, s1.b);
    fmt::print("          Simples s{{.a=7, .b=2.5}}    → a={}, b={}\n", s2.a, s2.b);
    fmt::print("          ⚠  em C++ a ORDEM tem que seguir a declaração (em C, não).\n");
    fmt::print("             Membros omitidos são value-initialized.\n");

    // (d) trivially destructible → o vetor não chama destrutor
    fmt::print("\n      (d) trivially destructible → destruir um array é NADA:\n");
    fmt::print("          is_trivially_destructible<Simples> = {}\n",
               std::is_trivially_destructible_v<Simples>);
    fmt::print("          `vector<Simples>::clear()` só zera o tamanho. Com\n");
    fmt::print("          `vector<std::string>` ({}) ele percorre e destrói.\n",
               std::is_trivially_destructible_v<std::string>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ memcpy: quando é UB
// ─────────────────────────────────────────────────────────────────────────────
void demo_memcpy_ub() {
    fmt::print("\n── 3.3 ⚠ memcpy sobre o tipo errado ──\n");

    fmt::print("      is_trivially_copyable<std::string> = {}\n",
               std::is_trivially_copyable_v<std::string>);
    fmt::print("      is_trivially_copyable<ComString>   = {}\n",
               std::is_trivially_copyable_v<ComString>);
    fmt::print("      is_trivially_copyable<ComVirtual>  = {}\n\n",
               std::is_trivially_copyable_v<ComVirtual>);

    fmt::print("      `memcpy` sobre um `std::string` copia o PONTEIRO interno.\n");
    fmt::print("      Os dois objetos passam a apontar para o mesmo buffer, e os\n");
    fmt::print("      dois destrutores vão liberá-lo: double free.\n");
    fmt::print("      (com SSO e string curta, pode até \"funcionar\" — e é isso\n");
    fmt::print("       que torna o bug traiçoeiro: some nos testes)\n\n");

    fmt::print("      Sobre `ComVirtual`, o memcpy copia o VPTR. Se a origem e o\n");
    fmt::print("      destino forem de classes derivadas diferentes, o destino\n");
    fmt::print("      passa a despachar para os métodos da classe errada.\n\n");

    fmt::print("      A defesa que compila:\n");
    fmt::print("        template <typename T>\n");
    fmt::print("        void copia_bruta(T& d, const T& o) {{\n");
    fmt::print("            static_assert(std::is_trivially_copyable_v<T>,\n");
    fmt::print("                          \"memcpy exige trivially copyable\");\n");
    fmt::print("            std::memcpy(&d, &o, sizeof(T));\n");
    fmt::print("        }}\n\n");

    fmt::print("      Em C++20, `std::bit_cast<To>(from)` faz isso com verificação\n");
    fmt::print("      em COMPILAÇÃO e sem UB de aliasing — ver src/bits/demo_bit.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. POD foi aposentado
// ─────────────────────────────────────────────────────────────────────────────
void demo_pod() {
    fmt::print("\n── 3.4 POD foi aposentado no C++20 ──\n");

    fmt::print("      \"POD\" (Plain Old Data) significava: trivial E standard-layout.\n");
    fmt::print("      `std::is_pod` está DEPRECIADO desde o C++20 (P0767), porque\n");
    fmt::print("      juntar as duas propriedades numa só escondia qual delas você\n");
    fmt::print("      realmente precisava:\n\n");

    fmt::print("        quero memcpy?          → is_trivially_copyable\n");
    fmt::print("        quero offsetof?        → is_standard_layout\n");
    fmt::print("        quero `T x;` de graça? → is_trivially_default_constructible\n");
    fmt::print("        quero chaves?          → is_aggregate\n\n");

    fmt::print("      As quatro perguntas são independentes, e `is_pod` respondia\n");
    fmt::print("      \"sim\" só quando TODAS eram sim — recusando tipos\n");
    fmt::print("      perfeitamente memcpy-áveis. Peça exatamente o que precisa.\n\n");

    fmt::print("      Exemplo concreto de `ComCtor` (tem construtor):\n");
    fmt::print("        is_pod                                = {} (depreciado)\n",
               std::is_trivially_copyable_v<ComCtor>
                 && std::is_standard_layout_v<ComCtor>
                 && std::is_trivially_default_constructible_v<ComCtor>);
    fmt::print("        is_trivially_copyable                 = {}  ← o que importa\n",
               std::is_trivially_copyable_v<ComCtor>);
    fmt::print("        is_standard_layout                    = {}\n",
               std::is_standard_layout_v<ComCtor>);
    fmt::print("      Ele NÃO seria POD, e ainda assim memcpy sobre ele é legal.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_tabela();
    demo_autoriza();
    demo_memcpy_ub();
    demo_pod();
}

}  // namespace demo_categorias
