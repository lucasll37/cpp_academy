// =============================================================================
//  src/layout/demo_polimorfico.hpp
//
//  O que `virtual` acrescenta ao objeto
//  ────────────────────────────────────
//  `src/oop/demo_virtual.hpp` mostra o COMPORTAMENTO do despacho dinâmico.
//  Aqui olhamos o outro lado: o que ele custa em BYTES e em ENDEREÇOS.
//
//  Três fatos que mudam o desenho de estruturas de dados:
//    • um único método virtual acrescenta 8 bytes a TODO objeto;
//    • com herança múltipla, `this` MUDA de valor conforme o ponteiro;
//    • herança virtual acrescenta mais indireção e um offset em runtime.
//
//  Tópicos:
//  • O vptr no início do objeto
//  • Herança simples: o layout é uma extensão
//  • ⚠ Herança múltipla: o `this` que se desloca
//  • Herança virtual e o custo do diamante
//  • Quando isto importa (e quando não)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace demo_polimorfico {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O vptr
// ─────────────────────────────────────────────────────────────────────────────
struct SemVirtual { int a; int b; };
struct ComVirtual { int a; int b; virtual ~ComVirtual() = default; };

void demo_vptr() {
    fmt::print("\n── 5.1 O vptr ──\n");

    fmt::print("      struct {{ int a; int b; }}                  → sizeof {:2}\n",
               sizeof(SemVirtual));
    fmt::print("      struct {{ int a; int b; virtual ~T(); }}     → sizeof {:2}\n",
               sizeof(ComVirtual));
    fmt::print("      alignof passou de {} para {} (o vptr é um ponteiro)\n",
               alignof(SemVirtual), alignof(ComVirtual));

    fmt::print("\n      Um ÚNICO método virtual custa {} bytes por objeto.\n",
               sizeof(ComVirtual) - sizeof(SemVirtual));
    fmt::print("      Dez métodos virtuais custam os mesmos {} — o vptr é um só,\n",
               sizeof(ComVirtual) - sizeof(SemVirtual));
    fmt::print("      e a vtable (que cresce) fica no binário, não no objeto.\n");

    // Onde exatamente o vptr está
    {
        ComVirtual x{};
        x.a = 0x11111111; x.b = 0x22222222;
        const auto* p = reinterpret_cast<const unsigned char*>(&x);
        fmt::print("\n      os {} bytes de um ComVirtual{{a=0x11111111, b=0x22222222}}:\n",
                   sizeof(ComVirtual));
        fmt::print("        ");
        for (std::size_t i = 0; i < sizeof(ComVirtual); ++i) {
            fmt::print("{:02X} ", p[i]);
            if (i == 7) fmt::print("| ");
        }
        fmt::print("\n        └─── vptr (8 bytes) ───┘  └─── a ────┘└─── b ────┘\n");

        // ⚠ `offsetof` em tipo NÃO standard-layout é "conditionally-supported":
        //   o padrão não obriga o compilador a aceitar. GCC e Clang aceitam e
        //   dão a resposta certa; o -Winvalid-offsetof avisa com razão, e aqui
        //   o aviso É o conteúdo. Suprimido localmente.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        fmt::print("      offsetof(ComVirtual, a) = {} — o vptr vem ANTES.\n",
                   offsetof(ComVirtual, a));
#pragma GCC diagnostic pop
        fmt::print("      (⚠ `offsetof` num tipo não standard-layout é\n");
        fmt::print("        conditionally-supported — o GCC aceita e avisa)\n");
    }

    fmt::print("\n      A posição no início é convenção do Itanium ABI (Linux,\n");
    fmt::print("      macOS), não exigência do padrão. É o que permite despachar\n");
    fmt::print("      sem saber o tipo: o vptr está sempre no offset 0.\n\n");

    fmt::print("      Consequência prática: num `vector<Particula>` de 10 milhões,\n");
    fmt::print("      um `virtual ~Particula()` que ninguém usa custa {:.0f} MB.\n",
               8.0 * 1e7 / 1048576.0);
    fmt::print("      Se a classe não é destruída por ponteiro-para-base, NÃO\n");
    fmt::print("      declare o destrutor virtual. (Torne-o protegido e não-virtual\n");
    fmt::print("      para impedir o uso errado — ver src/oop/.)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Herança simples: extensão
// ─────────────────────────────────────────────────────────────────────────────
struct Base1   { int x = 1; virtual ~Base1() = default; virtual void f() {} };
struct Deriv1  : Base1 { int y = 2; void f() override {} };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"   // ver a nota em 5.1
void demo_simples() {
    fmt::print("\n── 5.2 Herança simples ──\n");

    Deriv1 d;
    Base1* pb = &d;

    fmt::print("      sizeof(Base1)  = {:2}   sizeof(Deriv1) = {:2}\n",
               sizeof(Base1), sizeof(Deriv1));
    fmt::print("      &d       = {}\n", static_cast<const void*>(&d));
    fmt::print("      (Base1*)&d = {}\n", static_cast<const void*>(pb));
    fmt::print("      diferença = {} bytes\n",
               reinterpret_cast<std::uintptr_t>(pb) - reinterpret_cast<std::uintptr_t>(&d));

    fmt::print("\n      Com herança SIMPLES, o subobjeto da base fica no início e\n");
    fmt::print("      o derivado ACRESCENTA depois dele. O ponteiro não muda de\n");
    fmt::print("      valor, o vptr é compartilhado (aponta para a vtable de\n");
    fmt::print("      Deriv1), e o upcast é gratuito.\n\n");

    fmt::print("      ⚠  Repare que sizeof(Deriv1) == sizeof(Base1) == {}, apesar\n",
               sizeof(Base1));
    fmt::print("         de Deriv1 ter um `int y` a mais. O layout de Base1 é\n");
    fmt::print("           [vptr 8][x 4][padding 4]\n");
    fmt::print("         e o `y` do derivado foi colocado DENTRO desse padding de\n");
    fmt::print("         cauda. offsetof: x={}, y={}\n",
               offsetof(Deriv1, x), offsetof(Deriv1, y));
    fmt::print("         O compilador só pode fazer isso porque Base1 NÃO é\n");
    fmt::print("         standard-layout (tem virtual). Se fosse, o padrão\n");
    fmt::print("         proibiria — e é por isso que `memcpy` de um objeto para\n");
    fmt::print("         o subobjeto-base de outro pode corromper o derivado.\n\n");

    fmt::print("      É esse layout que torna o object slicing possível: copiar\n");
    fmt::print("      `*pb` para um `Base1` leva só a parte-base, e o resultado\n");
    fmt::print("      é um Base1 legítimo — sem aviso nenhum.\n");
    fmt::print("      Ver src/oop/demo_slicing.hpp.\n");
}
#pragma GCC diagnostic pop

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ Herança múltipla: o `this` que se desloca
// ─────────────────────────────────────────────────────────────────────────────
struct A { int a = 1; virtual ~A() = default; virtual void fa() {} };
struct B { int b = 2; virtual ~B() = default; virtual void fb() {} };
struct AB : A, B { int c = 3; };

void demo_multipla() {
    fmt::print("\n── 5.3 ⚠ Herança múltipla ──\n");

    AB obj;
    A* pa = &obj;
    B* pb = &obj;

    const auto base = reinterpret_cast<std::uintptr_t>(&obj);
    fmt::print("      sizeof(A)={}  sizeof(B)={}  sizeof(AB)={}\n",
               sizeof(A), sizeof(B), sizeof(AB));
    fmt::print("      &obj       offset  0\n");
    fmt::print("      (A*)&obj   offset {:2}   ← igual ao início\n",
               reinterpret_cast<std::uintptr_t>(pa) - base);
    fmt::print("      (B*)&obj   offset {:2}   ← DESLOCADO!\n",
               reinterpret_cast<std::uintptr_t>(pb) - base);

    fmt::print("\n      Layout: [vptr-A][a][pad][vptr-B][b][pad][c][pad]\n");
    fmt::print("      Há DOIS vptr: a parte-B precisa da sua própria vtable, e\n");
    fmt::print("      um `B*` tem que apontar para o começo DELA.\n\n");

    fmt::print("      Isto tem três consequências que mordem:\n\n");

    fmt::print("      (a) `static_cast<B*>(&obj)` NÃO é um no-op: o compilador\n");
    fmt::print("          soma {} ao ponteiro. Um `reinterpret_cast` no lugar\n",
               reinterpret_cast<std::uintptr_t>(pb) - base);
    fmt::print("          produziria um ponteiro ERRADO que compila sem aviso.\n\n");

    fmt::print("      (b) comparar ponteiros de tipos diferentes engana:\n");
    fmt::print("          (void*)pa == (void*)pb ? {}\n",
               static_cast<const void*>(pa) == static_cast<const void*>(pb));
    fmt::print("          pa == &obj && pb == &obj ? {}\n",
               (pa == &obj) && (pb == &obj));
    fmt::print("          Os dois apontam para o MESMO objeto, com endereços\n");
    fmt::print("          numéricos diferentes. Compare com `dynamic_cast<void*>`,\n");
    fmt::print("          que devolve o início do objeto mais derivado:\n");
    fmt::print("          dynamic_cast<void*>(pa) == dynamic_cast<void*>(pb) ? {}\n",
               dynamic_cast<void*>(pa) == dynamic_cast<void*>(pb));

    fmt::print("\n      (c) `delete pb` precisa AJUSTAR o ponteiro antes de chamar\n");
    fmt::print("          `free`. Quem faz isso é o \"thunk\" gerado pelo compilador\n");
    fmt::print("          na vtable de B. Sem destrutor virtual em B, seria UB.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Herança virtual — o diamante
// ─────────────────────────────────────────────────────────────────────────────
struct V           { int v = 1; virtual ~V() = default; };
struct E1 : virtual V { int e1 = 2; };
struct E2 : virtual V { int e2 = 3; };
struct Diamante : E1, E2 { int d = 4; };

struct N1 : V  { int n1 = 2; };      // herança NÃO virtual, para comparar
struct N2 : V  { int n2 = 3; };
struct NaoDiamante : N1, N2 { int n = 4; };

void demo_virtual_base() {
    fmt::print("\n── 5.4 Herança virtual ──\n");

    fmt::print("      Sem `virtual` no diamante, a base aparece DUAS vezes:\n");
    fmt::print("        sizeof(NaoDiamante) = {}\n", sizeof(NaoDiamante));
    {
        NaoDiamante n;
        fmt::print("        n.N1::v e n.N2::v são objetos DIFERENTES: {} e {}\n",
                   static_cast<const void*>(&n.N1::v),
                   static_cast<const void*>(&n.N2::v));
        fmt::print("        `n.v` sozinho seria ambíguo e nem compila.\n");
    }

    fmt::print("\n      Com `virtual`, a base é COMPARTILHADA:\n");
    fmt::print("        sizeof(Diamante) = {}\n", sizeof(Diamante));
    {
        Diamante d;
        fmt::print("        &d.E1::v == &d.E2::v ? {}  (um só subobjeto)\n",
                   static_cast<const void*>(&d.E1::v)
                     == static_cast<const void*>(&d.E2::v));
        fmt::print("        `d.v` = {} — sem ambiguidade\n", d.v);
    }

    fmt::print("\n      O preço: {} bytes contra {} do não-virtual, apesar de haver\n",
               sizeof(Diamante), sizeof(NaoDiamante));
    fmt::print("      MENOS dados. A diferença é o mecanismo:\n");
    fmt::print("        • `E1` e `E2` não sabem, em compilação, ONDE `V` vai estar\n");
    fmt::print("          (depende de quem herda delas, e a mais derivada decide);\n");
    fmt::print("        • cada uma ganha um ponteiro extra — o vtt / virtual base\n");
    fmt::print("          offset — consultado em RUNTIME para achar `V`;\n");
    fmt::print("        • o `V` compartilhado vai para o FIM do objeto.\n\n");

    fmt::print("      Acessar `d.v` é uma indireção a mais que acessar `d.d`.\n");
    fmt::print("      E o construtor da classe mais derivada passa a ser\n");
    fmt::print("      responsável por construir `V` — pulando os construtores de\n");
    fmt::print("      E1 e E2 (ver src/oop/demo_multipla.hpp).\n\n");

    fmt::print("      Quando vale: interfaces puras (sem dados). Aí o custo é\n");
    fmt::print("      pequeno e o diamante é inevitável. Com DADOS na base\n");
    fmt::print("      virtual, reconsidere o desenho — composição costuma ser\n");
    fmt::print("      mais simples e mais rápida.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Onde isto importa
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_importa() {
    fmt::print("\n── 5.5 Quando o layout importa ──\n");

    fmt::print("      IMPORTA quando:\n");
    fmt::print("        • há milhões de objetos (partículas, entidades, nós);\n");
    fmt::print("        • os dados atravessam um limite de ABI ou vão para disco;\n");
    fmt::print("        • você faz interop com C, com uma GPU ou com hardware;\n");
    fmt::print("        • o laço quente percorre um array e o cache é o gargalo.\n\n");

    fmt::print("      NÃO importa quando:\n");
    fmt::print("        • há dezenas ou centenas de objetos;\n");
    fmt::print("        • o objeto vive atrás de um ponteiro que você segue uma\n");
    fmt::print("          vez por frame;\n");
    fmt::print("        • a clareza do desenho vale mais que 8 bytes.\n\n");

    fmt::print("      Ferramentas para inspecionar sem adivinhar:\n");
    fmt::print("        g++ -Xclang -fdump-record-layouts   (clang)\n");
    fmt::print("        g++ -fdump-lang-class               (GCC; gera .class)\n");
    fmt::print("        pahole -C NomeDaStruct ./binario    (mostra os buracos!)\n");
    fmt::print("        static_assert(sizeof(T) == 16);     (trava o contrato)\n\n");

    fmt::print("      O `static_assert` é o mais valioso dos quatro: transforma\n");
    fmt::print("      uma suposição sobre layout numa falha de compilação no dia\n");
    fmt::print("      em que alguém acrescentar um membro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_vptr();
    demo_simples();
    demo_multipla();
    demo_virtual_base();
    demo_quando_importa();
}

}  // namespace demo_polimorfico
