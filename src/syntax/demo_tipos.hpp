// =============================================================================
//  src/syntax/demo_tipos.hpp
//
//  Tipos definidos pelo usuário — struct, class, enum, union
//  ────────────────────────────────────────────────────────
//  A linguagem oferece quatro maneiras de criar um tipo novo:
//
//      struct  →  dados agrupados; membros PÚBLICOS por padrão
//      class   →  o mesmo, mas membros PRIVADOS por padrão
//      enum    →  um conjunto fechado de valores nomeados
//      union   →  vários membros ocupando o MESMO espaço (um de cada vez)
//
//  `struct` e `class` são literalmente a mesma construção: a única diferença
//  é o acesso padrão (e a herança padrão). A escolha é uma CONVENÇÃO —
//  `struct` para agregados de dados, `class` quando há invariante a proteger.
//
//  Tópicos:
//  • struct vs class — a única diferença real
//  • Inicialização agregada e designated initializers (C++20)
//  • `enum` clássico vs `enum class` — e por que o segundo venceu
//  • Tipo subjacente, `std::underlying_type`, `enum` como flags
//  • `union` — e por que `std::variant` a substituiu
//  • Bitfields — empacotando bits com nome
//  • Tipos aninhados, forward declaration e tipos incompletos
//  • O que é POD, trivial, standard-layout
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace demo_tipos {

// ─────────────────────────────────────────────────────────────────────────────
//  1. struct vs class
// ─────────────────────────────────────────────────────────────────────────────
struct ComoStruct {
    int publico_por_padrao = 0;
private:
    int escondido = 0;
public:
    int ver() const { return escondido; }
};

class ComoClass {
    int privado_por_padrao = 0;      // sem rótulo → private
public:
    int ver() const { return privado_por_padrao; }
};

void demo_struct_vs_class() {
    fmt::print("\n── 4.1 struct vs class ──\n");

    ComoStruct s;
    ComoClass  c;

    fmt::print("      struct → membros PÚBLICOS por padrão   (s.publico = {})\n",
               s.publico_por_padrao);
    fmt::print("      class  → membros PRIVADOS por padrão   (c.ver()   = {})\n", c.ver());
    fmt::print("      Fora isso são idênticos: ambos podem ter métodos, construtores,\n");
    fmt::print("      herança, templates e operadores. `s.ver()` = {}\n", s.ver());

    fmt::print("\n      A convenção que a comunidade adotou:\n");
    fmt::print("        struct → agregado de dados, tudo público, sem invariante\n");
    fmt::print("        class  → há um estado a proteger e métodos que o mantêm válido\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Inicialização agregada e designated initializers
// ─────────────────────────────────────────────────────────────────────────────
// Um AGREGADO é um tipo sem construtor definido pelo usuário, sem membros
// privados, sem virtuais e sem base privada. Pode ser inicializado com { }.
struct Config {
    std::string host    = "localhost";
    int         porta   = 8080;
    bool        tls     = false;
    int         timeout = 30;
};

struct Retangulo { int largura; int altura; };

void demo_agregados() {
    fmt::print("\n── 4.2 Inicialização agregada ──\n");

    Retangulo r1{10, 20};               // por ORDEM dos membros
    Retangulo r2{};                     // todos value-initialized → zero
    // O aviso -Wmissing-field-initializers é exatamente o assunto desta linha:
    // é legal omitir campos, e é bom o compilador lembrar disso.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    Retangulo r3{5};                    // o resto vira zero
#pragma GCC diagnostic pop

    fmt::print("      Retangulo r1{{10, 20}} → {}x{}\n", r1.largura, r1.altura);
    fmt::print("      Retangulo r2{{}}       → {}x{}   ← tudo zerado\n", r2.largura, r2.altura);
    fmt::print("      Retangulo r3{{5}}      → {}x{}   ← faltantes viram zero\n",
               r3.largura, r3.altura);
    fmt::print("        (e o compilador AVISA: -Wmissing-field-initializers)\n");

    // Designated initializers (C++20): nomear os campos na inicialização
    Config padrao{};
    Config producao{.host = "api.exemplo.com", .porta = 443, .tls = true};

    fmt::print("\n      Designated initializers (C++20) — o chamador LÊ o que está fazendo:\n");
    fmt::print("        Config{{}}                                  → {}:{} tls={}\n",
               padrao.host, padrao.porta, padrao.tls);
    fmt::print("        Config{{.host=\"api...\", .porta=443, .tls=true}}\n");
    fmt::print("                                                   → {}:{} tls={} timeout={}\n",
               producao.host, producao.porta, producao.tls, producao.timeout);
    fmt::print("        ⚠ em C++ a ordem tem de seguir a DECLARAÇÃO dos membros\n");
    fmt::print("          (o C permite qualquer ordem; o C++ não)\n");
    fmt::print("        → é a resposta do C++ à falta de argumentos nomeados\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. enum clássico vs enum class
// ─────────────────────────────────────────────────────────────────────────────
enum CorAntiga { Vermelho, Verde, Azul };            // C++98 — "unscoped"
enum Fruta     { Maca, Banana /*, Verde ← COLIDIRIA */ };

enum class Cor  : std::uint8_t { Vermelho, Verde, Azul };   // C++11 — "scoped"
enum class Nota : char         { A = 'A', B = 'B', C = 'C' };

// Enum como conjunto de flags: valores potência de dois
enum class Permissao : unsigned {
    Nenhuma  = 0,
    Ler      = 1u << 0,     // 0b001
    Escrever = 1u << 1,     // 0b010
    Executar = 1u << 2,     // 0b100
};

constexpr Permissao operator|(Permissao a, Permissao b) {
    return static_cast<Permissao>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr bool contem(Permissao conjunto, Permissao flag) {
    return (static_cast<unsigned>(conjunto) & static_cast<unsigned>(flag)) != 0u;
}

void demo_enums() {
    fmt::print("\n── 4.3 enum vs enum class ──\n");

    CorAntiga antiga = Verde;
    int       vazou  = antiga;          // converte SOZINHO para int — o problema

    fmt::print("      enum clássico:  `enum Cor {{ Vermelho, Verde, Azul }};`\n");
    fmt::print("        os nomes vazam para o escopo de fora: `Verde`, não `Cor::Verde`\n");
    fmt::print("        converte implicitamente para int: int x = Verde; → {}\n", vazou);
    fmt::print("        → dois enums com um nome igual COLIDEM\n");

    Cor moderna = Cor::Verde;
    // int nao_compila = moderna;       // ✗ precisa de static_cast
    fmt::print("\n      enum class (C++11):\n");
    fmt::print("        exige qualificação: Cor::Verde\n");
    fmt::print("        NÃO converte sozinho: `int x = moderna;` não compila\n");
    fmt::print("        static_cast<int>(Cor::Verde) = {}\n", static_cast<int>(moderna));
    fmt::print("        valores começam em 0 e incrementam de 1 se você não disser outra coisa\n");

    // Tipo subjacente explícito: controla tamanho e permite forward declaration
    fmt::print("\n      Tipo subjacente (`: uint8_t`):\n");
    fmt::print("        sizeof(CorAntiga) = {}   (int, por padrão)\n", sizeof(CorAntiga));
    fmt::print("        sizeof(Cor)       = {}   (fixado em uint8_t)\n", sizeof(Cor));
    fmt::print("        std::underlying_type_t<Cor> é uint8_t? {}\n",
               std::is_same_v<std::underlying_type_t<Cor>, std::uint8_t>);
    fmt::print("        Nota::A tem valor '{}' — enums podem ter valores explícitos\n",
               static_cast<char>(Nota::A));

    // Flags
    auto perms = Permissao::Ler | Permissao::Escrever;
    fmt::print("\n      Enum como flags (valores 1, 2, 4, 8...):\n");
    fmt::print("        Ler | Escrever = 0b{:03b}\n", static_cast<unsigned>(perms));
    fmt::print("        contem(perms, Ler)      = {}\n", contem(perms, Permissao::Ler));
    fmt::print("        contem(perms, Executar) = {}\n", contem(perms, Permissao::Executar));
    fmt::print("        → sobrecarregar `|` e `&` devolve a SEGURANÇA que o cast tirou\n");

    (void)Maca; (void)Banana;
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. union — e por que std::variant a substituiu
// ─────────────────────────────────────────────────────────────────────────────
union Bruto {
    std::int32_t inteiro;
    float        real;
    std::uint8_t bytes[4];
};

// A união "com etiqueta" (tagged union): o padrão manual que o variant automatiza
struct ValorEtiquetado {
    enum class Qual { Inteiro, Real } qual;
    union { int i; double d; };          // união anônima: membros entram na struct
};

void demo_union() {
    fmt::print("\n── 4.4 union ──\n");

    Bruto b{};
    b.inteiro = 0x41424344;

    fmt::print("      union {{ int32; float; uint8[4]; }} — todos no MESMO endereço\n");
    fmt::print("        sizeof(Bruto) = {} = o maior membro, não a soma\n", sizeof(Bruto));
    fmt::print("        b.inteiro = 0x41424344\n");
    fmt::print("        b.bytes   = {:02X} {:02X} {:02X} {:02X}  ← little-endian: invertido\n",
               b.bytes[0], b.bytes[1], b.bytes[2], b.bytes[3]);
    fmt::print("        ⚠ ler um membro DIFERENTE do último escrito é UB formalmente\n");
    fmt::print("          (\"type punning\"). Para reinterpretar bits: std::bit_cast (C++20).\n");

    ValorEtiquetado v{};
    v.qual = ValorEtiquetado::Qual::Real;
    v.d    = 3.5;
    fmt::print("\n      Tagged union — o padrão manual:\n");
    fmt::print("        struct {{ enum Qual qual; union {{ int i; double d; }}; }}\n");
    fmt::print("        quem escreve a etiqueta é VOCÊ; errar é UB silencioso\n");
    fmt::print("        v.d = {}\n", v.d);

    std::variant<int, double, std::string> moderno = 3.5;
    fmt::print("\n      std::variant (C++17) — a mesma ideia, com segurança:\n");
    fmt::print("        guarda a etiqueta sozinho: index() = {}\n", moderno.index());
    fmt::print("        std::get<double> = {}\n", std::get<double>(moderno));
    fmt::print("        acessar o tipo errado → LANÇA std::bad_variant_access\n");
    fmt::print("        aceita tipos não-triviais (std::string!), que a union crua não aceita\n");
    fmt::print("        Detalhes → src/vocabulary_types.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Bitfields
// ─────────────────────────────────────────────────────────────────────────────
struct RegistradorStatus {
    unsigned pronto      : 1;    // 1 bit
    unsigned erro        : 1;    // 1 bit
    unsigned codigo_erro : 4;    // 4 bits → 0..15
    unsigned versao      : 8;    // 8 bits → 0..255
    unsigned reservado   : 18;
};

void demo_bitfields() {
    fmt::print("\n── 4.5 Bitfields ──\n");

    RegistradorStatus reg{};
    reg.pronto      = 1;
    reg.erro        = 1;
    reg.codigo_erro = 9;
    reg.versao      = 3;

    fmt::print("      struct {{ unsigned pronto:1; erro:1; codigo:4; versao:8; ... }}\n");
    fmt::print("        sizeof = {} bytes — 32 bits com NOME, em vez de máscaras\n",
               sizeof(RegistradorStatus));
    fmt::print("        reg.pronto={} erro={} codigo_erro={} versao={}\n",
               +reg.pronto, +reg.erro, +reg.codigo_erro, +reg.versao);
    fmt::print("        (o `+` acima é necessário: um bitfield não se liga a `unsigned&`)\n");
    fmt::print("        ⚠ codigo_erro:4 só guarda 0..15 — atribuir 20 TRUNCA em silêncio\n");
    fmt::print("        ⚠ o LAYOUT é definido pela implementação: não confie nele para\n");
    fmt::print("          protocolo de rede ou formato de arquivo entre plataformas\n");
    fmt::print("        ⚠ não existe `&reg.pronto` — bitfield não tem endereço\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Tipos aninhados, forward declaration e tipos incompletos
// ─────────────────────────────────────────────────────────────────────────────
class Motor;                        // forward declaration → tipo INCOMPLETO

class Carro {
public:
    // Tipo aninhado: só existe dentro do escopo de Carro
    struct Rodas { int quantidade = 4; };
    enum class Cambio { Manual, Automatico };
    using Placa = std::string;

    Carro() : cambio_(Cambio::Manual) {}
    Cambio cambio() const { return cambio_; }

private:
    Rodas   rodas_{};
    Cambio  cambio_;
    Motor*  motor_ = nullptr;       // ✓ ponteiro para tipo incompleto é permitido
    // Motor  motor_;               // ✗ membro por VALOR exigiria o tipo completo
};

void demo_aninhados() {
    fmt::print("\n── 4.6 Tipos aninhados e forward declaration ──\n");

    Carro::Rodas r;
    Carro::Placa p = "ABC-1234";
    Carro c;

    fmt::print("      Tipos aninhados vivem no escopo da classe:\n");
    fmt::print("        Carro::Rodas r;   → r.quantidade = {}\n", r.quantidade);
    fmt::print("        Carro::Placa p;   → alias interno = \"{}\"\n", p);
    fmt::print("        Carro::Cambio::Manual → índice {}\n", static_cast<int>(c.cambio()));

    fmt::print("\n      Forward declaration (`class Motor;`) cria um tipo INCOMPLETO:\n");
    fmt::print("        ✓ pode-se declarar ponteiro, referência, parâmetro e retorno\n");
    fmt::print("        ✗ não se pode ter membro por valor, herdar, nem chamar métodos\n");
    fmt::print("        → é o que quebra dependências circulares e acelera a compilação\n");
    fmt::print("          (o header pesado só entra na .cpp). Ver o pimpl em src/smart-pointers.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Categorias de tipo: trivial, standard-layout, POD
// ─────────────────────────────────────────────────────────────────────────────
struct Trivial       { int a; double b; };
struct ComConstrutor { int a; ComConstrutor() : a(42) {} };
struct ComVirtual    { virtual ~ComVirtual() = default; int a; };

void demo_categorias() {
    fmt::print("\n── 4.7 Categorias de tipo ──\n");

    fmt::print("      | tipo          | trivial | standard-layout | memcpy seguro |\n");
    fmt::print("      |---------------|---------|-----------------|---------------|\n");
    fmt::print("      | Trivial       | {:^7} | {:^15} | {:^13} |\n",
               std::is_trivial_v<Trivial>, std::is_standard_layout_v<Trivial>,
               std::is_trivially_copyable_v<Trivial>);
    fmt::print("      | ComConstrutor | {:^7} | {:^15} | {:^13} |\n",
               std::is_trivial_v<ComConstrutor>, std::is_standard_layout_v<ComConstrutor>,
               std::is_trivially_copyable_v<ComConstrutor>);
    fmt::print("      | ComVirtual    | {:^7} | {:^15} | {:^13} |\n",
               std::is_trivial_v<ComVirtual>, std::is_standard_layout_v<ComVirtual>,
               std::is_trivially_copyable_v<ComVirtual>);

    fmt::print("\n      trivial         → construção/cópia sem trabalho extra\n");
    fmt::print("      standard-layout → layout previsível, compatível com C\n");
    fmt::print("      POD             = trivial + standard-layout (termo aposentado no C++20)\n");
    fmt::print("      Só um tipo trivially-copyable pode ir para `memcpy`, arquivo ou socket.\n");
    fmt::print("        sizeof(ComVirtual) = {} — o vptr custou {} bytes (+ padding)\n",
               sizeof(ComVirtual), sizeof(void*));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_struct_vs_class();
    demo_agregados();
    demo_enums();
    demo_union();
    demo_bitfields();
    demo_aninhados();
    demo_categorias();
}

} // namespace demo_tipos
