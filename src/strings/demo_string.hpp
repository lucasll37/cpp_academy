// =============================================================================
//  src/strings/demo_string.hpp
//
//  std::string por dentro — SSO, capacity e o custo de concatenar
//  ──────────────────────────────────────────────────────────────
//  `std::string` parece simples e esconde três decisões de implementação que
//  aparecem no perfil de qualquer programa que mexe com texto:
//
//    1. SSO (Small String Optimization) — strings curtas não alocam
//    2. crescimento amortizado — a mesma política do vector
//    3. o operador `+` cria temporários; o `+=` não
//
//  Este demo MEDE as três, contando alocações de verdade.
//
//  Tópicos:
//  • SSO: onde está o limite, medido
//  • size, capacity, reserve e shrink_to_fit
//  • `+` vs `+=` vs `append` — alocações contadas
//  • Iterar, comparar e as armadilhas de encoding
//  • char_traits e por que `std::string` não sabe o que é UTF-8
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace demo_string {

// ─────────────────────────────────────────────────────────────────────────────
//  Contagem de alocações — a infraestrutura que torna este demo verificável
// ─────────────────────────────────────────────────────────────────────────────
namespace contador {
inline std::size_t alocacoes = 0;
inline std::size_t bytes     = 0;
inline bool        ativo     = false;

inline void zerar() { alocacoes = 0; bytes = 0; }

// Executa `f` contando as alocações que ela provoca
template <typename F>
std::pair<std::size_t, std::size_t> medir(F f) {
    zerar();
    ativo = true;
    f();
    ativo = false;
    return {alocacoes, bytes};
}
} // namespace contador

// ─────────────────────────────────────────────────────────────────────────────
//  1. SSO — onde está o limite
//
//     A libstdc++ reserva 16 bytes dentro do próprio objeto `string`. Strings
//     de até 15 caracteres (+ o '\0') cabem ali e NÃO alocam.
// ─────────────────────────────────────────────────────────────────────────────
void demo_sso() {
    fmt::print("\n── 1.1 SSO — o limite, medido ──\n");

    fmt::print("      sizeof(std::string) = {} bytes\n", sizeof(std::string));
    fmt::print("      (ponteiro + tamanho + buffer interno de 16 bytes)\n\n");

    fmt::print("      alocações ao construir strings de tamanhos crescentes:\n");
    std::size_t limite_sso = 0;
    for (std::size_t n : {1u, 8u, 14u, 15u, 16u, 17u, 32u}) {
        const auto [a, b] = contador::medir([n] {
            std::string s(n, 'x');
            // `volatile` impede o compilador de eliminar a construção
            const volatile auto tam = s.size();
            (void)tam;
        });
        fmt::print("        {:>2} chars → {} alocação(ões){}\n",
                   n, a, a == 0 ? "  ← cabe no SSO" : "");
        if (a == 0) limite_sso = n;
    }

    fmt::print("\n      → o limite do SSO na libstdc++ é {} caracteres.\n", limite_sso);

    // A prova estrutural: numa string curta, os dados vivem DENTRO do objeto
    std::string curta = "abc";
    std::string longa(100, 'y');

    const auto* obj_curta   = reinterpret_cast<const char*>(&curta);
    const auto* obj_longa   = reinterpret_cast<const char*>(&longa);
    const auto  dist_curta  = curta.data() - obj_curta;
    const auto  dist_longa  = longa.data() - obj_longa;

    fmt::print("\n      distância entre &string e .data():\n");
    fmt::print("        curta ({} chars): {:>3} bytes  ← DENTRO do objeto\n",
               curta.size(), dist_curta);
    fmt::print("        longa ({} chars): {} bytes  ← na heap, longe\n",
               longa.size(), dist_longa > 1000 || dist_longa < -1000 ? "muitos" : "poucos");

    fmt::print("\n      Consequências práticas do SSO:\n");
    fmt::print("        • strings curtas são quase de graça — não evite `std::string`\n");
    fmt::print("          por medo de alocação em nomes, chaves e identificadores\n");
    fmt::print("        • mas o MOVE de uma string curta COPIA os bytes (não há\n");
    fmt::print("          ponteiro a roubar); o move só é O(1) acima do limite\n");
    fmt::print("        • `sizeof(string)` é 32 bytes: um vector<string> de 1M\n");
    fmt::print("          elementos gasta 32 MB só de cabeçalhos\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. size, capacity e o crescimento
// ─────────────────────────────────────────────────────────────────────────────
void demo_capacity() {
    fmt::print("\n── 1.2 size, capacity e crescimento ──\n");

    std::string s;
    std::size_t cap = s.capacity();
    fmt::print("      string vazia: size={} capacity={}  ← o SSO já está reservado\n",
               s.size(), cap);

    fmt::print("      crescendo caractere a caractere, marcando as realocações:\n      ");
    for (int i = 0; i < 200; ++i) {
        s += 'x';
        if (s.capacity() != cap) {
            fmt::print("{}→{} ", cap, s.capacity());
            cap = s.capacity();
        }
    }
    fmt::print("\n      size={} capacity={}\n", s.size(), s.capacity());
    fmt::print("      ↑ o buffer DOBRA a cada estouro, igual ao vector\n");

    // reserve elimina as realocações
    const auto [sem_reserve, _] = contador::medir([] {
        std::string t;
        for (int i = 0; i < 200; ++i) t += 'x';
        const volatile auto n = t.size(); (void)n;
    });
    const auto [com_reserve, __] = contador::medir([] {
        std::string t;
        t.reserve(200);
        for (int i = 0; i < 200; ++i) t += 'x';
        const volatile auto n = t.size(); (void)n;
    });

    fmt::print("\n      200 caracteres, um a um:\n");
    fmt::print("        sem reserve  → {} alocação(ões)\n", sem_reserve);
    fmt::print("        com reserve  → {} alocação(ões)\n", com_reserve);

    // clear não devolve memória
    s.clear();
    fmt::print("\n      após clear(): size={} capacity={}  ← memória MANTIDA\n",
               s.size(), s.capacity());
    s.shrink_to_fit();
    fmt::print("      após shrink_to_fit(): capacity={}  (voltou ao SSO)\n", s.capacity());
    fmt::print("      Para forçar a liberação: `std::string{{}}.swap(s);`\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Concatenação — `+` vs `+=` vs append
//
//     `a + b + c` cria temporários intermediários. Numa cadeia longa, isso
//     vira alocação por operação.
// ─────────────────────────────────────────────────────────────────────────────
void demo_concatenacao() {
    fmt::print("\n── 1.3 Concatenação: o custo de cada forma ──\n");

    const std::string a = "primeiro trecho de texto ";
    const std::string b = "segundo trecho de texto ";
    const std::string c = "terceiro trecho de texto ";
    const std::string d = "quarto trecho de texto";

    const auto [n_plus, by_plus] = contador::medir([&] {
        const std::string r = a + b + c + d;
        const volatile auto x = r.size(); (void)x;
    });

    const auto [n_pluseq, by_pluseq] = contador::medir([&] {
        std::string r;
        r += a; r += b; r += c; r += d;
        const volatile auto x = r.size(); (void)x;
    });

    const auto [n_reserva, by_reserva] = contador::medir([&] {
        std::string r;
        r.reserve(a.size() + b.size() + c.size() + d.size());
        r += a; r += b; r += c; r += d;
        const volatile auto x = r.size(); (void)x;
    });

    const auto [n_append, by_append] = contador::medir([&] {
        std::string r;
        r.reserve(a.size() + b.size() + c.size() + d.size());
        r.append(a).append(b).append(c).append(d);
        const volatile auto x = r.size(); (void)x;
    });

    fmt::print("      juntando 4 trechos ({} bytes no total):\n",
               a.size() + b.size() + c.size() + d.size());
    fmt::print("        a + b + c + d        → {} aloc, {:>3} bytes\n", n_plus,   by_plus);
    fmt::print("        r += a; r += b; ...  → {} aloc, {:>3} bytes\n", n_pluseq, by_pluseq);
    fmt::print("        reserve + +=         → {} aloc, {:>3} bytes\n", n_reserva, by_reserva);
    fmt::print("        reserve + append     → {} aloc, {:>3} bytes\n", n_append, by_append);

    fmt::print("\n      Por que `+` custa mais: `a + b + c + d` é\n");
    fmt::print("        ((a + b) + c) + d\n");
    fmt::print("      e cada `+` produz uma string TEMPORÁRIA nova.\n");
    fmt::print("      (a libstdc++ otimiza parte disso quando o operando da\n");
    fmt::print("       esquerda é um rvalue, reaproveitando o buffer — por isso\n");
    fmt::print("       o número não explode)\n");

    fmt::print("\n      Regra: numa cadeia com N trechos, `reserve` + `append`.\n");
    fmt::print("      Para montar texto formatado, `fmt::format` já faz isso\n");
    fmt::print("      internamente — ver demo_format.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. As operações que aparecem no dia a dia
// ─────────────────────────────────────────────────────────────────────────────
void demo_operacoes() {
    fmt::print("\n── 1.4 Operações essenciais ──\n");

    std::string s = "  /caminho/para/arquivo.tar.gz  ";

    // Trim — a STL não tem, e é o utilitário mais reescrito do mundo
    const auto trim = [](std::string_view v) {
        const auto ini = v.find_first_not_of(" \t\n\r");
        if (ini == std::string_view::npos) return std::string_view{};
        const auto fim = v.find_last_not_of(" \t\n\r");
        return v.substr(ini, fim - ini + 1);
    };

    const auto limpo = trim(s);
    fmt::print("      original : '{}'\n", s);
    fmt::print("      trim     : '{}'\n", limpo);

    fmt::print("      find('/')            = {}\n", limpo.find('/'));
    fmt::print("      rfind('/')           = {}\n", limpo.rfind('/'));
    fmt::print("      find_last_of(\".\")    = {}\n", limpo.find_last_of('.'));
    fmt::print("      substr após o último '/': '{}'\n",
               limpo.substr(limpo.rfind('/') + 1));

    // C++20: starts_with / ends_with — antes disso era substr + comparação
    fmt::print("      starts_with(\"/caminho\") = {}  (C++20)\n",
               limpo.starts_with("/caminho"));
    fmt::print("      ends_with(\".gz\")        = {}  (C++20)\n", limpo.ends_with(".gz"));
    fmt::print("      contains(\"para\")        = {}  (C++23)\n", limpo.contains("para"));

    // replace, insert, erase
    std::string caminho{limpo};
    caminho.replace(0, 8, "/novo");
    fmt::print("      replace(0, 8, \"/novo\")  → '{}'\n", caminho);

    // Split — outro utilitário ausente da STL antes do <ranges>
    const auto dividir = [](std::string_view v, char sep) {
        std::vector<std::string_view> partes;
        std::size_t ini = 0;
        while (ini <= v.size()) {
            const auto pos = v.find(sep, ini);
            partes.push_back(v.substr(ini, pos - ini));
            if (pos == std::string_view::npos) break;
            ini = pos + 1;
        }
        return partes;
    };

    fmt::print("      split por '/': ");
    for (auto p : dividir(limpo, '/')) fmt::print("['{}'] ", p);
    fmt::print("\n");
    fmt::print("      (em C++20 há `views::split` — ver src/ranges/)\n");

    fmt::print("\n      ⚠  `npos` é `size_t(-1)`, o maior valor possível. Um\n");
    fmt::print("         `if (s.find(x) > 0)` é SEMPRE verdadeiro quando não\n");
    fmt::print("         acha. Compare sempre com `!= std::string::npos`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. char_traits e o que `std::string` NÃO sabe
//
//     `std::string` é `basic_string<char>`: uma sequência de BYTES. Ela não
//     sabe o que é um caractere Unicode, não sabe maiúsculas fora do ASCII, e
//     `size()` conta bytes — não letras.
// ─────────────────────────────────────────────────────────────────────────────
void demo_encoding() {
    fmt::print("\n── 1.5 O que std::string não sabe ──\n");

    const std::string ascii = "cafe";
    const std::string utf8  = "café";           // o 'é' ocupa 2 bytes em UTF-8

    fmt::print("      \"cafe\".size() = {}  (4 letras, 4 bytes)\n", ascii.size());
    fmt::print("      \"café\".size() = {}  (4 letras, {} BYTES)\n", utf8.size(), utf8.size());
    fmt::print("      ↑ `size()` conta BYTES, não caracteres.\n");

    fmt::print("      iterando \"café\" byte a byte: ");
    for (unsigned char ch : utf8) fmt::print("{:02X} ", ch);
    fmt::print("\n      ↑ os dois últimos (C3 A9) são UM caractere só.\n");

    fmt::print("\n      Consequências:\n");
    fmt::print("        • `s[i]` pode cair no MEIO de um caractere\n");
    fmt::print("        • `substr` pode cortar um caractere ao meio\n");
    fmt::print("        • `std::reverse` sobre UTF-8 produz lixo\n");
    fmt::print("        • comparar sem considerar normalização Unicode dá\n");
    fmt::print("          resultados errados (é vs e+acento combinam diferente)\n");

    fmt::print("\n      O que a biblioteca padrão oferece: quase nada.\n");
    fmt::print("        u8string/u16string/u32string  → só marcam a intenção\n");
    fmt::print("        <codecvt>                     → DEPRECIADO no C++17\n");
    fmt::print("      Para Unicode de verdade: ICU, utf8cpp ou Boost.Locale.\n");

    fmt::print("\n      `char_traits<char>` é o que define comparação e cópia:\n");
    fmt::print("        compare · length · copy · eq · lt · find\n");
    fmt::print("      Trocá-lo permite uma string case-insensitive sem alocar:\n");
    fmt::print("        struct TraitsSemCaso : std::char_traits<char> {{ ... }};\n");
    fmt::print("        using StringSemCaso = std::basic_string<char, TraitsSemCaso>;\n");
    fmt::print("      ⚠  mas o resultado NÃO converte para std::string — é outro\n");
    fmt::print("         tipo. Na prática, quase sempre é melhor normalizar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_sso();
    demo_capacity();
    demo_concatenacao();
    demo_operacoes();
    demo_encoding();
}

} // namespace demo_string

// ─────────────────────────────────────────────────────────────────────────────
//  operator new/delete instrumentados — infraestrutura do demo, em UMA TU só
// ─────────────────────────────────────────────────────────────────────────────
void* operator new(std::size_t n) {
    if (demo_string::contador::ativo) {
        ++demo_string::contador::alocacoes;
        demo_string::contador::bytes += n;
    }
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc{};
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
