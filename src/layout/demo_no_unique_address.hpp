// =============================================================================
//  src/layout/demo_no_unique_address.hpp
//
//  EBO e [[no_unique_address]] — não pagar por membros vazios
//  ──────────────────────────────────────────────────────────
//  Um objeto vazio ocupa 1 byte (senão dois objetos teriam o mesmo endereço).
//  Como membro, esse 1 byte vira 8 depois do padding. Numa struct usada aos
//  milhões, é desperdício puro.
//
//  A linguagem tem duas saídas, uma antiga e feia e uma nova e limpa:
//
//    EBO                    — herdar do tipo vazio. Funciona desde sempre,
//                             e é por isso que `std::vector` HERDA do alocador.
//    [[no_unique_address]]  — C++20. Diz ao compilador que o membro pode
//                             compartilhar endereço com outro. Sem herança.
//
//  Tópicos:
//  • O 1 byte que vira 8
//  • EBO — a razão de vector herdar do alocador
//  • [[no_unique_address]] medido
//  • ⚠ dois membros vazios do MESMO tipo
//  • Onde isto aparece de verdade: allocator, deleter, comparador
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace demo_no_unique_address {

struct Vazio { };
struct Comparador { bool operator()(int a, int b) const { return a < b; } };

// ─────────────────────────────────────────────────────────────────────────────
//  1. O 1 byte que vira 8
// ─────────────────────────────────────────────────────────────────────────────
struct PorMembro {
    Vazio  politica;      // 1 byte...
    double dado;          // ...mas `dado` precisa de offset múltiplo de 8
};

struct PorHeranca : Vazio {
    double dado;
};

struct ComAtributo {
    [[no_unique_address]] Vazio politica;
    double                      dado;
};

void demo_custo() {
    fmt::print("\n── 4.1 O membro vazio que custa 8 bytes ──\n");

    fmt::print("      sizeof(Vazio)                       = {}\n", sizeof(Vazio));
    fmt::print("      sizeof(double)                      = {}\n\n", sizeof(double));

    fmt::print("      struct {{ Vazio p; double d; }}        → sizeof {:2}  ← 8 a mais!\n",
               sizeof(PorMembro));
    fmt::print("      struct : Vazio {{ double d; }}         → sizeof {:2}  (EBO)\n",
               sizeof(PorHeranca));
    fmt::print("      struct {{ [[nua]] Vazio p; double d; }}→ sizeof {:2}  (C++20)\n",
               sizeof(ComAtributo));

    fmt::print("\n      offsetof(PorMembro, dado)   = {}\n", offsetof(PorMembro, dado));
    fmt::print("      offsetof(ComAtributo, dado) = {}\n", offsetof(ComAtributo, dado));

    fmt::print("\n      O membro vazio precisa de endereço PRÓPRIO (senão\n");
    fmt::print("      `&p.politica == &p.dado`, e a linguagem proíbe). Com 1 byte\n");
    fmt::print("      ocupado, `dado` só pode começar no offset 8. Resultado:\n");
    fmt::print("      7 bytes de padding e 8 de desperdício total.\n\n");

    fmt::print("      Num `std::vector` de 10 milhões:\n");
    fmt::print("        PorMembro   : {:.1f} MB\n", sizeof(PorMembro)   * 1e7 / 1048576.0);
    fmt::print("        ComAtributo : {:.1f} MB\n", sizeof(ComAtributo) * 1e7 / 1048576.0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. EBO — por que vector herda do alocador
// ─────────────────────────────────────────────────────────────────────────────
void demo_ebo() {
    fmt::print("\n── 4.2 EBO na biblioteca padrão ──\n");

    fmt::print("      sizeof(std::allocator<int>)     = {} (é VAZIO)\n",
               sizeof(std::allocator<int>));
    fmt::print("      sizeof(std::vector<int>)        = {} (3 ponteiros, nada mais)\n",
               sizeof(std::vector<int>));
    fmt::print("      → o alocador ocupa ZERO. Se fosse membro, o vector teria {}.\n",
               sizeof(std::vector<int>) + 8);

    fmt::print("\n      Foi para isso que a libstdc++ escreveu:\n");
    fmt::print("        struct _Vector_base {{\n");
    fmt::print("            struct _Vector_impl : public _Tp_alloc_type {{ ... }};\n");
    fmt::print("        }};                            ▲\n");
    fmt::print("                                      HERANÇA, só pelo layout\n\n");

    fmt::print("      Herdar de algo que não é interface incomoda — mas antes do\n");
    fmt::print("      C++20 era a ÚNICA forma. `boost::compressed_pair` existia\n");
    fmt::print("      exatamente para embrulhar essa feiura.\n\n");

    fmt::print("      O mesmo mecanismo no unique_ptr:\n");
    {
        auto del = [](int* p) { delete p; };
        fmt::print("        unique_ptr<int>                        = {} bytes\n",
                   sizeof(std::unique_ptr<int>));
        fmt::print("        unique_ptr<int, lambda SEM captura>    = {} bytes (EBO)\n",
                   sizeof(std::unique_ptr<int, decltype(del)>));
        fmt::print("        unique_ptr<int, void(*)(int*)>         = {} bytes (ponteiro!)\n",
                   sizeof(std::unique_ptr<int, void(*)(int*)>));
    }
    fmt::print("      ⚠  passar `&minha_funcao` como deleter DOBRA o unique_ptr.\n");
    fmt::print("         Uma lambda sem captura é gratuita. Ver src/smart-pointers/.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ Dois membros vazios do MESMO tipo
// ─────────────────────────────────────────────────────────────────────────────
struct SoIguais     { [[no_unique_address]] Vazio a;
                      [[no_unique_address]] Vazio b; };          // MESMO tipo
struct SoDiferentes { [[no_unique_address]] Vazio      a;
                      [[no_unique_address]] Comparador b; };     // tipos distintos

struct DoisIguais {
    [[no_unique_address]] Vazio a;
    [[no_unique_address]] Vazio b;      // MESMO tipo de `a`
    int dado;
};

void demo_dois_vazios() {
    fmt::print("\n── 4.3 ⚠ Dois membros vazios ──\n");

    fmt::print("      A regra: dois subobjetos do MESMO tipo não podem ter o\n");
    fmt::print("      mesmo endereço. Isso vale mesmo com o atributo.\n\n");

    fmt::print("      struct {{ [[nua]] Vazio a; [[nua]] Vazio b;      }} → sizeof {}\n",
               sizeof(SoIguais));
    fmt::print("      struct {{ [[nua]] Vazio a; [[nua]] Comparador b; }} → sizeof {}\n",
               sizeof(SoDiferentes));
    fmt::print("      Com tipos distintos os dois colapsam para o offset 0.\n");
    fmt::print("      Com o mesmo tipo, um deles é empurrado para o offset 1.\n");

    fmt::print("\n      ⚠ Mas isso NÃO significa que o custo aparece no sizeof da\n");
    fmt::print("        struct que os contém. Basta haver outro membro para\n");
    fmt::print("        absorvê-los:\n\n");
    {
        DoisIguais x{};
        const auto base = reinterpret_cast<std::uintptr_t>(&x);
        fmt::print("        struct {{ [[nua]] Vazio a, b; int dado; }} → sizeof {}\n",
                   sizeof(DoisIguais));
        fmt::print("          offset de a    = {}\n",
                   reinterpret_cast<std::uintptr_t>(&x.a) - base);
        fmt::print("          offset de b    = {}\n",
                   reinterpret_cast<std::uintptr_t>(&x.b) - base);
        fmt::print("          offset de dado = {}\n", offsetof(DoisIguais, dado));
        fmt::print("          &a == &b ? {}   (proibido serem iguais)\n",
                   static_cast<const void*>(&x.a) == static_cast<const void*>(&x.b));
    }
    fmt::print("\n      Os dois membros vazios foram colocados DENTRO do espaço do\n");
    fmt::print("      `int` — endereços diferentes entre si, mas sobrepostos ao\n");
    fmt::print("      `dado`. O sizeof continua sendo o do `int` sozinho.\n");
    fmt::print("      É exatamente o que o atributo autoriza: compartilhar\n");
    fmt::print("      endereço com OUTRO membro.\n\n");

    fmt::print("      ⚠  `[[no_unique_address]]` é uma AUTORIZAÇÃO, não uma\n");
    fmt::print("         garantia. O compilador pode ignorá-la. O MSVC de fato a\n");
    fmt::print("         ignora por compatibilidade de ABI e oferece\n");
    fmt::print("         `[[msvc::no_unique_address]]` no lugar. Se o layout\n");
    fmt::print("         importa, MEÇA com static_assert(sizeof(T) == N).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Onde isto aparece de verdade
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, typename Comp = std::less<T>>
class ConjuntoOrdenado {
    [[no_unique_address]] Comp comp_;      // quase sempre vazio
    std::vector<T>             dados_;

public:
    explicit ConjuntoOrdenado(Comp c = {}) : comp_{std::move(c)} {}

    void inserir(T v) {
        auto it = dados_.begin();
        while (it != dados_.end() && comp_(*it, v)) ++it;
        dados_.insert(it, std::move(v));
    }
    const std::vector<T>& dados() const { return dados_; }
};

void demo_uso_real() {
    fmt::print("\n── 4.4 O padrão em código real ──\n");

    ConjuntoOrdenado<int> c;
    for (int v : {5, 1, 9, 3}) c.inserir(v);
    fmt::print("      ConjuntoOrdenado<int>: ");
    for (int v : c.dados()) fmt::print("{} ", v);
    fmt::print("\n");

    fmt::print("      sizeof(ConjuntoOrdenado<int>)              = {}\n",
               sizeof(ConjuntoOrdenado<int>));
    fmt::print("      sizeof(std::vector<int>)                   = {}\n",
               sizeof(std::vector<int>));
    fmt::print("      → o comparador `std::less<int>` (vazio) custou {} bytes.\n",
               sizeof(ConjuntoOrdenado<int>) - sizeof(std::vector<int>));

    // Com um comparador COM estado, o custo aparece — como deve
    struct ComEstado {
        int fator;
        bool operator()(int a, int b) const { return a * fator < b * fator; }
    };
    fmt::print("      com um comparador COM estado (int fator): {} bytes\n",
               sizeof(ConjuntoOrdenado<int, ComEstado>));
    fmt::print("      → aí o custo é real, e é justo pagá-lo.\n\n");

    fmt::print("      A regra de projeto: TODA política passada por template\n");
    fmt::print("      (comparador, alocador, deleter, hasher, mutex-policy)\n");
    fmt::print("      merece `[[no_unique_address]]`. É uma linha, é gratuito\n");
    fmt::print("      quando a política é vazia — e é o caso comum.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_custo();
    demo_ebo();
    demo_dois_vazios();
    demo_uso_real();
}

}  // namespace demo_no_unique_address
