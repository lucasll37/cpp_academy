// =============================================================================
//  src/vocabulary_types/demo_any.hpp
//
//  std::any — o tipo que aceita qualquer tipo
//  ──────────────────────────────────────────
//  `optional<T>` sabe o tipo e talvez não tenha valor.
//  `variant<Ts...>` sabe o CONJUNTO de tipos possíveis.
//  `any` não sabe nada em compilação: aceita qualquer tipo copiável e só
//  descobre qual é em runtime.
//
//  Essa flexibilidade tem um preço alto, e é por isso que `any` é o menos
//  usado dos três — quase sempre há uma opção melhor. Este demo mostra o
//  mecanismo, mede o custo, e é explícito sobre quando NÃO usar.
//
//  Tópicos:
//  • any_cast: as três formas e o que cada uma faz ao falhar
//  • O custo: alocação medida
//  • any vs variant vs void* vs interface virtual
//  • Os poucos casos legítimos
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <any>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <optional>
#include <utility>
#include <map>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

namespace demo_any {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O básico
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 3.1 any_cast e as formas de falhar ──\n");

    std::any a = 42;
    fmt::print("      a = 42\n");
    fmt::print("        has_value()  = {}\n", a.has_value());
    fmt::print("        type().name()= {} (mangled)\n", a.type().name());
    fmt::print("        type()==int  = {}\n", a.type() == typeid(int));

    // (a) any_cast<T> por VALOR — devolve cópia, lança bad_any_cast
    fmt::print("        any_cast<int>(a) = {}\n", std::any_cast<int>(a));

    try {
        (void)std::any_cast<std::string>(a);
    } catch (const std::bad_any_cast& e) {
        fmt::print("        any_cast<string> → bad_any_cast: {}\n", e.what());
    }

    // (b) any_cast<T*> por PONTEIRO — devolve nullptr, não lança
    if (const int* p = std::any_cast<int>(&a)) {
        fmt::print("        any_cast<int>(&a)    = {} (ponteiro válido)\n", *p);
    }
    if (std::any_cast<std::string>(&a) == nullptr) {
        fmt::print("        any_cast<string>(&a) = nullptr (sem exceção)\n");
    }

    // O tipo é EXATO — não há conversão nem herança
    std::any d = 3.14;
    if (std::any_cast<int>(&d) == nullptr) {
        fmt::print("      ⚠  um any com double NÃO responde a any_cast<int>:\n");
        fmt::print("         o casamento de tipo é EXATO, sem conversão implícita.\n");
    }

    // Trocar o conteúdo troca o tipo
    a = std::string("agora sou texto");
    fmt::print("      após a = string: type()==string = {}\n", a.type() == typeid(std::string));
    a.reset();
    fmt::print("      após reset(): has_value = {}\n", a.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O custo — alocações CONTADAS
//
//     A implementação tem otimização de objeto pequeno (SBO): tipos pequenos e
//     trivialmente copiáveis ficam dentro do próprio `any`. Acima do limite,
//     vão para a heap.
//
//     Para provar isso em vez de afirmar, substituímos o `operator new` global
//     por uma versão que conta. Isso é legítimo aqui porque o demo é uma única
//     unidade de tradução — num projeto real, sobrescrever o `new` global é
//     decisão de arquitetura, não de arquivo.
// ─────────────────────────────────────────────────────────────────────────────
namespace contador {
inline std::size_t alocacoes = 0;
inline std::size_t bytes     = 0;
inline bool        ativo     = false;

inline void zerar() { alocacoes = 0; bytes = 0; }
} // namespace contador

struct Pequeno { int    a;       };   //  4 bytes
struct Medio   { long   a, b;    };   // 16 bytes
struct Grande  { double v[8];    };   // 64 bytes

// Constrói e destrói um objeto do tipo dado, contando as alocações no caminho.
template <typename Fabrica>
std::pair<std::size_t, std::size_t> contar(Fabrica fabrica) {
    contador::zerar();
    contador::ativo = true;
    { auto x = fabrica(); (void)x; }
    contador::ativo = false;
    return {contador::alocacoes, contador::bytes};
}

void demo_custo() {
    fmt::print("\n── 3.2 O custo, contado ──\n");

    fmt::print("      sizeof(std::any)                    = {}\n", sizeof(std::any));
    fmt::print("      sizeof(std::variant<Pequeno,Grande>)= {}  (maior membro + índice)\n",
               sizeof(std::variant<Pequeno, Grande>));
    fmt::print("      sizeof(std::optional<int>)          = {}\n", sizeof(std::optional<int>));

    fmt::print("\n      alocações de heap ao construir UM objeto:\n");

    auto [a1, b1] = contar([] { return std::any{Pequeno{1}}; });
    fmt::print("        any<Pequeno> ({:>2}B) → {} alocação(ões), {} bytes\n",
               sizeof(Pequeno), a1, b1);

    auto [a2, b2] = contar([] { return std::any{Medio{1, 2}}; });
    fmt::print("        any<Medio>   ({:>2}B) → {} alocação(ões), {} bytes\n",
               sizeof(Medio), a2, b2);

    auto [a3, b3] = contar([] { return std::any{Grande{}}; });
    fmt::print("        any<Grande>  ({:>2}B) → {} alocação(ões), {} bytes\n",
               sizeof(Grande), a3, b3);

    auto [a4, b4] = contar([] { return std::variant<Pequeno, Grande>{Grande{}}; });
    fmt::print("        variant<...Grande> ({:>2}B) → {} alocação(ões), {} bytes\n",
               sizeof(Grande), a4, b4);

    fmt::print("\n      → o limite do SBO da libstdc++ fica em sizeof(void*): acima\n");
    fmt::print("        disso, cada `any` construído é um malloc + free.\n");
    fmt::print("      → o `variant` NUNCA aloca: ele reserva o maior membro inline.\n");
    fmt::print("        É essa a diferença estrutural, não a velocidade bruta —\n");
    fmt::print("        em -O0 os tempos até se parecem; o que não se parece é\n");
    fmt::print("        a pressão sobre o alocador num loop quente.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. any vs as alternativas
// ─────────────────────────────────────────────────────────────────────────────
void demo_alternativas() {
    fmt::print("\n── 3.3 Comparando as opções ──\n");

    fmt::print("      | ferramenta       | tipos       | seguro | aloca | despacho    |\n");
    fmt::print("      |------------------|-------------|--------|-------|-------------|\n");
    fmt::print("      | optional<T>      | 1, conhecido| sim    | não   | —           |\n");
    fmt::print("      | variant<Ts...>   | N, fechado  | sim    | não   | visit       |\n");
    fmt::print("      | any              | qualquer    | sim    | talvez| any_cast    |\n");
    fmt::print("      | void*            | qualquer    | NÃO    | não   | você que sabe|\n");
    fmt::print("      | interface virtual| qualquer que| sim    | sim   | vtable      |\n");
    fmt::print("      |                  | implemente  |        |       |             |\n");

    fmt::print("\n      A pergunta que elimina o `any` quase sempre:\n");
    fmt::print("      \"eu consigo listar os tipos possíveis?\"\n");
    fmt::print("        • sim  → `variant`. Ganha exaustividade e zero alocação.\n");
    fmt::print("        • não, mas todos fazem a mesma COISA → interface virtual.\n");
    fmt::print("        • não, e cada um faz coisa diferente → aí sim, `any`.\n");

    fmt::print("\n      ⚠  o sintoma de `any` mal usado é uma cascata de any_cast:\n");
    fmt::print("           if (auto* i = any_cast<int>(&v))        {{ ... }}\n");
    fmt::print("           else if (auto* s = any_cast<string>(&v)){{ ... }}\n");
    fmt::print("         Isso É um variant, escrito à mão e sem exaustividade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Os casos em que `any` é a resposta certa
//
//     Todos têm em comum: o conjunto de tipos é ABERTO e definido por quem
//     usa a biblioteca, não por quem a escreve.
// ─────────────────────────────────────────────────────────────────────────────

// Caso legítimo: um saco de propriedades heterogêneo, cujos tipos são
// escolhidos pelo consumidor da API. É assim que funcionam os "user data"
// de bibliotecas de GUI e de motores de jogo.
class Propriedades {
public:
    template <typename T>
    void definir(const std::string& chave, T valor) {
        dados_[chave] = std::move(valor);
    }

    // Devolve optional<T>: junta as duas ferramentas — "pode não existir"
    // (optional) e "pode ser de qualquer tipo" (any).
    template <typename T>
    std::optional<T> obter(const std::string& chave) const {
        auto it = dados_.find(chave);
        if (it == dados_.end()) return std::nullopt;
        if (const T* p = std::any_cast<T>(&it->second)) return *p;
        return std::nullopt;                    // existe, mas é de outro tipo
    }

    std::size_t tamanho() const { return dados_.size(); }

private:
    std::map<std::string, std::any> dados_;
};

void demo_caso_legitimo() {
    fmt::print("\n── 3.4 Onde `any` é a resposta certa ──\n");

    Propriedades p;
    p.definir("largura",  1920);
    p.definir("titulo",   std::string("Editor"));
    p.definir("opacidade", 0.85);
    p.definir("visivel",  true);

    fmt::print("      {} propriedades guardadas, de 4 tipos diferentes\n", p.tamanho());
    fmt::print("        largura   (int)    = {}\n", p.obter<int>("largura").value_or(-1));
    fmt::print("        titulo    (string) = '{}'\n",
               p.obter<std::string>("titulo").value_or("?"));
    fmt::print("        opacidade (double) = {}\n", p.obter<double>("opacidade").value_or(-1));

    // Tipo errado → nullopt, sem exceção
    fmt::print("        largura como string → {}\n",
               p.obter<std::string>("largura").has_value() ? "achou" : "nullopt (tipo errado)");
    fmt::print("        chave inexistente   → {}\n",
               p.obter<int>("altura").has_value() ? "achou" : "nullopt (não existe)");

    fmt::print("\n      Por que aqui `variant` não serviria: quem escreve a classe\n");
    fmt::print("      `Propriedades` NÃO SABE quais tipos o usuário vai guardar.\n");
    fmt::print("      O conjunto é aberto por design — é a definição do problema.\n");

    fmt::print("\n      Outros casos legítimos:\n");
    fmt::print("        • apagar o tipo numa fronteira de ABI (pimpl genérico)\n");
    fmt::print("        • \"user data\" de callbacks em bibliotecas de C\n");
    fmt::print("        • nós de um grafo de configuração dinâmica\n");
    fmt::print("        • `std::any` como membro de um sistema de eventos\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_custo();
    demo_alternativas();
    demo_caso_legitimo();
}

} // namespace demo_any

// ─────────────────────────────────────────────────────────────────────────────
//  Sobrescrita do operator new/delete global, usada por demo_custo() acima.
//
//  Fica no fim do arquivo de propósito: é infraestrutura do demo, não conteúdo.
//  Precisa ser definida em EXATAMENTE uma unidade de tradução do programa —
//  por isso mora aqui, e não num header incluído por vários demos.
// ─────────────────────────────────────────────────────────────────────────────
void* operator new(std::size_t n) {
    if (demo_any::contador::ativo) {
        ++demo_any::contador::alocacoes;
        demo_any::contador::bytes += n;
    }
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc{};
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
