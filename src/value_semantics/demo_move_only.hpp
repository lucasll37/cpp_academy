// =============================================================================
//  src/value_semantics/demo_move_only.hpp
//
//  Tipos move-only, ref-qualifiers e sink parameters
//  ─────────────────────────────────────────────────
//  Nem todo recurso pode ser duplicado: um descritor de arquivo, um lock, uma
//  conexão de socket, uma thread. Para esses, a cópia não é apenas cara — ela
//  é SEMANTICAMENTE ERRADA. A resposta do C++ é o tipo move-only.
//
//  Tópicos:
//  • Construir um tipo move-only correto
//  • Ref-qualifiers (`&` e `&&`) em métodos
//  • Sink parameters — por valor + std::move
//  • Quando passar por valor vence passar por referência
//  • Move-only dentro de containers
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_move_only {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Um handle move-only — a forma canônica
//
//     Modela um recurso do SO que só pode ter UM dono. Copiar seria
//     criar dois donos do mesmo descritor: fechamento duplo garantido.
// ─────────────────────────────────────────────────────────────────────────────
class Conexao {
public:
    explicit Conexao(std::string host) : host_(std::move(host)), aberta_(true) {
        fmt::print("      abrindo conexão com '{}'\n", host_);
    }

    // Cópia PROIBIDA — e o `= delete` documenta a intenção no próprio tipo.
    Conexao(const Conexao&)            = delete;
    Conexao& operator=(const Conexao&) = delete;

    // Move PERMITIDO — transfere a posse
    Conexao(Conexao&& o) noexcept
        : host_(std::move(o.host_)), aberta_(o.aberta_) {
        o.aberta_ = false;            // a origem deixa de ser dona
    }

    Conexao& operator=(Conexao&& o) noexcept {
        if (this != &o) {
            fechar();                 // libera o recurso atual antes de assumir outro
            host_     = std::move(o.host_);
            aberta_   = o.aberta_;
            o.aberta_ = false;
        }
        return *this;
    }

    ~Conexao() { fechar(); }

    bool aberta() const noexcept { return aberta_; }
    const std::string& host() const noexcept { return host_; }

private:
    void fechar() {
        if (aberta_) {
            fmt::print("      fechando conexão com '{}'\n", host_);
            aberta_ = false;
        }
    }

    std::string host_;
    bool        aberta_ = false;
};

void demo_move_only_basico() {
    fmt::print("\n── 5.1 Um tipo que só pode ser movido ──\n");

    static_assert(!std::is_copy_constructible_v<Conexao>);
    static_assert(std::is_nothrow_move_constructible_v<Conexao>);

    {
        Conexao a("db.exemplo.com");

        // Conexao b = a;             // ✗ erro de compilação — e é isso que queremos
        Conexao b = std::move(a);     // ✓ transferência de posse

        fmt::print("      a.aberta() = {}  ← deixou de ser dona\n", a.aberta());
        fmt::print("      b.aberta() = {}  ← assumiu o recurso\n",  b.aberta());
        fmt::print("      saindo do escopo — só UM fechamento deve aparecer:\n");
    }

    fmt::print("      ✓ o tipo torna o erro impossível de escrever\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Ref-qualifiers — sobrecarregar método pela categoria do *this
//
//     Um método pode perguntar "estou sendo chamado sobre um objeto que ainda
//     vai ser usado, ou sobre um temporário descartável?" — e responder com
//     uma cópia ou com um move.
// ─────────────────────────────────────────────────────────────────────────────
class Documento {
public:
    explicit Documento(std::string t) : texto_(std::move(t)) {}

    // Chamado sobre um lvalue → o objeto continua vivo → devolve CÓPIA
    const std::string& texto() const& {
        fmt::print("      texto() const&  → devolvendo referência (sem cópia)\n");
        return texto_;
    }

    // Chamado sobre um rvalue → o objeto é descartável → pode ROUBAR
    std::string texto() && {
        fmt::print("      texto() &&      → roubando o buffer (move)\n");
        return std::move(texto_);
    }

private:
    std::string texto_;
};

void demo_ref_qualifiers() {
    fmt::print("\n── 5.2 Ref-qualifiers: `&` e `&&` em métodos ──\n");

    Documento d("conteúdo do documento");

    const std::string& r = d.texto();          // lvalue → overload const&
    fmt::print("      leitura sem cópia: '{}'\n", r);

    std::string roubado = Documento("temporário").texto();   // rvalue → overload &&
    fmt::print("      obtido do temporário: '{}'\n", roubado);

    fmt::print("      Ref-qualifiers também servem para PROIBIR chamadas:\n");
    fmt::print("      declarar `T& operator[](int) && = delete;` impede devolver\n");
    fmt::print("      referência para dentro de um temporário que já morreu.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Sink parameter — "por valor + std::move"
//
//     Quando a função vai GUARDAR uma cópia do argumento, receber por valor
//     e mover para dentro é o padrão mais simples e eficiente:
//       • chamador passa lvalue → 1 cópia (inevitável, ele quer guardar)
//       • chamador passa rvalue → 1 move  (barato)
//     Um único overload cobre os dois. A alternativa é escrever
//     `const T&` E `T&&`, e o número de overloads explode com o de parâmetros.
// ─────────────────────────────────────────────────────────────────────────────
class Agenda {
public:
    // ✓ sink parameter
    void adicionar(std::string nome) {
        nomes_.push_back(std::move(nome));    // o move aqui é obrigatório
    }

    // ✗ a alternativa: 2 overloads para 1 parâmetro, 4 para 2, 8 para 3...
    // void adicionar(const std::string& n) { nomes_.push_back(n); }
    // void adicionar(std::string&& n)      { nomes_.push_back(std::move(n)); }

    std::size_t tamanho() const noexcept { return nomes_.size(); }
    const std::vector<std::string>& nomes() const noexcept { return nomes_; }

private:
    std::vector<std::string> nomes_;
};

void demo_sink_parameter() {
    fmt::print("\n── 5.3 Sink parameter — por valor + std::move ──\n");

    Agenda a;
    std::string nome = "Grace Hopper";

    a.adicionar(nome);                 // lvalue → cópia no parâmetro, move no push
    fmt::print("      após adicionar(lvalue): nome = '{}'  ← preservado\n", nome);

    a.adicionar(std::move(nome));      // rvalue → move no parâmetro, move no push
    fmt::print("      após adicionar(rvalue): nome = '{}'  ← movido\n", nome);

    a.adicionar("Alan Turing");        // literal → constrói direto no parâmetro

    fmt::print("      agenda tem {} nomes:", a.tamanho());
    for (const auto& n : a.nomes()) fmt::print(" '{}'", n);
    fmt::print("\n");

    fmt::print("      ⚠  esquecer o `std::move` dentro da função anula o ganho:\n");
    fmt::print("         o parâmetro tem nome, logo é lvalue, logo copiaria de novo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Quando NÃO usar sink parameter
//
//     Se a função só LÊ o argumento, `const T&` é estritamente melhor:
//     o sink cobraria uma cópia que ninguém pediu.
// ─────────────────────────────────────────────────────────────────────────────
std::size_t contar_vogais(const std::string& s) {     // ✓ só lê → const&
    std::size_t n = 0;
    for (char c : s) {
        if (c=='a'||c=='e'||c=='i'||c=='o'||c=='u') ++n;
    }
    return n;
}

void demo_quando_nao_usar() {
    fmt::print("\n── 5.4 Escolhendo a assinatura ──\n");

    fmt::print("      contar_vogais(\"semântica de valor\") = {}\n",
               contar_vogais("semantica de valor"));

    fmt::print("\n      | intenção                    | assinatura        |\n");
    fmt::print("      |-----------------------------|-------------------|\n");
    fmt::print("      | só ler                      | const T&          |\n");
    fmt::print("      | ler e modificar do chamador | T&                |\n");
    fmt::print("      | guardar uma cópia           | T (sink) + move   |\n");
    fmt::print("      | repassar em template        | T&& + forward<T>  |\n");
    fmt::print("      | tipo pequeno e trivial      | T (por valor)     |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Move-only em containers
//
//     std::vector aceita tipos move-only sem problema, desde que você não
//     peça uma operação que exija cópia (como `v1 = v2`).
// ─────────────────────────────────────────────────────────────────────────────
void demo_containers() {
    fmt::print("\n── 5.5 Move-only dentro de containers ──\n");

    std::vector<Conexao> pool;
    pool.reserve(3);                       // evita realocação (que exigiria move)

    pool.emplace_back("nó-1");             // constrói in-place: nenhum move
    pool.emplace_back("nó-2");
    pool.push_back(Conexao("nó-3"));       // constrói e MOVE para dentro

    fmt::print("      pool com {} conexões\n", pool.size());

    // std::vector<Conexao> copia = pool;  // ✗ erro: exigiria copiar Conexao

    // Tirar um elemento de dentro exige move explícito:
    Conexao extraida = std::move(pool.back());
    pool.pop_back();
    fmt::print("      extraída: '{}' (aberta={})\n", extraida.host(), extraida.aberta());

    fmt::print("      encerrando o pool:\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_move_only_basico();
    demo_ref_qualifiers();
    demo_sink_parameter();
    demo_quando_nao_usar();
    demo_containers();
}

} // namespace demo_move_only
