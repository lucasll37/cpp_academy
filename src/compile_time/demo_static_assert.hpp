// =============================================================================
//  src/compile_time/demo_static_assert.hpp
//
//  static_assert e type_traits — programar o compilador
//  ────────────────────────────────────────────────────
//  `static_assert` é a única forma de fazer o compilador FALHAR de propósito,
//  com uma mensagem sua. Combinado com `<type_traits>`, ele transforma
//  suposições implícitas sobre tipos em contratos verificados.
//
//  A regra que orienta o uso: se você comentou "T precisa ser copiável",
//  esse comentário deveria ser um static_assert.
//
//  Tópicos:
//  • static_assert e as mensagens que ajudam vs as que não ajudam
//  • O catálogo de type_traits que importa
//  • Escrever um trait próprio (detection idiom)
//  • Concepts como substituto — e quando ainda usar static_assert
//  • Verificar layout, tamanho e ABI
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_static_assert {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A mensagem é o produto
//
//     Um static_assert sem mensagem devolve a EXPRESSÃO, que raramente ajuda.
//     A mensagem deve dizer o que fazer, não o que falhou.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
void serializar_ruim(const T&) {
    static_assert(std::is_trivially_copyable_v<T>);
    // erro: "static assertion failed" — e o usuário fica sem saber o que fazer
}

template <typename T>
void serializar_bom(const T&) {
    static_assert(std::is_trivially_copyable_v<T>,
        "serializar() copia bytes crus e só aceita tipos trivialmente "
        "copiáveis. Tipos com ponteiros, std::string ou destrutor próprio "
        "precisam de uma sobrecarga específica.");
}

struct Pacote { std::uint32_t id; float valor; };

void demo_mensagens() {
    fmt::print("\n── 3.1 A mensagem é o produto ──\n");

    serializar_bom(Pacote{1, 2.0f});
    fmt::print("      serializar_bom(Pacote) → ok (trivialmente copiável)\n");

    // serializar_bom(std::string{"x"});
    // → error: static assertion failed: serializar() copia bytes crus e só
    //   aceita tipos trivialmente copiáveis. Tipos com ponteiros, std::string
    //   ou destrutor próprio precisam de uma sobrecarga específica.
    fmt::print("      serializar_bom(std::string) → NÃO COMPILA, com instruções\n");

    fmt::print("\n      Compare as duas mensagens de erro:\n");
    fmt::print("        ruim: \"static assertion failed\"\n");
    fmt::print("        bom : \"serializar() copia bytes crus e só aceita…\"\n");
    fmt::print("      A segunda diz o que fazer. Custa uma linha.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O catálogo de type_traits que realmente aparece
// ─────────────────────────────────────────────────────────────────────────────
struct ComPonteiro   { int* p; };
struct ComDestrutor  { ~ComDestrutor() {} };
struct Polimorfico   { virtual ~Polimorfico() = default; };
struct Agregado      { int a; double b; };

template <typename T>
void inspecionar(const char* nome) {
    fmt::print("      {:<14} triv.copiável={:<5} triv.destr={:<5} "
               "polimórfico={:<5} agregado={:<5} tamanho={}\n",
               nome,
               std::is_trivially_copyable_v<T>,
               std::is_trivially_destructible_v<T>,
               std::is_polymorphic_v<T>,
               std::is_aggregate_v<T>,
               sizeof(T));
}

void demo_catalogo() {
    fmt::print("\n── 3.2 O catálogo de type_traits ──\n");

    inspecionar<int>("int");
    inspecionar<Pacote>("Pacote");
    inspecionar<ComPonteiro>("ComPonteiro");
    inspecionar<ComDestrutor>("ComDestrutor");
    inspecionar<Polimorfico>("Polimorfico");
    inspecionar<Agregado>("Agregado");
    inspecionar<std::string>("std::string");

    fmt::print("\n      | pergunta                       | trait                        |\n");
    fmt::print("      |--------------------------------|------------------------------|\n");
    fmt::print("      | dá para memcpy?                | is_trivially_copyable_v      |\n");
    fmt::print("      | tem vtable?                    | is_polymorphic_v             |\n");
    fmt::print("      | o move é barato E seguro?      | is_nothrow_move_constructible|\n");
    fmt::print("      | dá para inicializar com {{}}?    | is_aggregate_v               |\n");
    fmt::print("      | é o mesmo tipo?                | is_same_v                    |\n");
    fmt::print("      | herda de X?                    | is_base_of_v                 |\n");
    fmt::print("      | converte para X?               | is_convertible_v             |\n");
    fmt::print("      | é chamável com estes args?     | is_invocable_v               |\n");

    // Os traits de TRANSFORMAÇÃO, que produzem outro tipo
    static_assert(std::is_same_v<std::remove_reference_t<int&>,  int>);
    static_assert(std::is_same_v<std::remove_cv_t<const int>,    int>);
    static_assert(std::is_same_v<std::decay_t<const int&>,       int>);
    static_assert(std::is_same_v<std::decay_t<int[5]>,           int*>);
    fmt::print("\n      Traits de transformação (produzem um tipo, não um bool):\n");
    fmt::print("        remove_reference_t<int&>  → int\n");
    fmt::print("        remove_cv_t<const int>    → int\n");
    fmt::print("        decay_t<const int&>       → int\n");
    fmt::print("        decay_t<int[5]>           → int*   (array decai p/ ponteiro)\n");
    fmt::print("      `decay_t` é o que o `auto` faz na dedução por valor.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Escrever um trait próprio — o detection idiom
//
//     A pergunta "este tipo tem o método X?" não tem trait pronto. O padrão
//     clássico usa SFINAE com void_t; em C++20, um concept é bem mais direto.
// ─────────────────────────────────────────────────────────────────────────────

// ── Forma clássica (C++17): void_t + especialização parcial ────────────────
template <typename, typename = void>
struct tem_serializar : std::false_type {};

// Esta especialização só é viável se `declval<T>().serializar()` for válido.
// Se não for, o SFINAE descarta e sobra a primária (false_type).
template <typename T>
struct tem_serializar<T, std::void_t<decltype(std::declval<const T&>().serializar())>>
    : std::true_type {};

template <typename T>
inline constexpr bool tem_serializar_v = tem_serializar<T>::value;

// ── Forma moderna (C++20): um concept diz a mesma coisa em 3 linhas ────────
template <typename T>
concept Serializavel = requires(const T& t) {
    { t.serializar() } -> std::convertible_to<std::string>;
};

struct ComMetodo    { std::string serializar() const { return "ok"; } };
struct SemMetodo    { int x; };
struct TipoErrado   { int serializar() const { return 0; } };   // devolve int, não string

void demo_trait_proprio() {
    fmt::print("\n── 3.3 Um trait próprio ──\n");

    fmt::print("      detection idiom (C++17, void_t):\n");
    fmt::print("        tem_serializar_v<ComMetodo>  = {}\n", tem_serializar_v<ComMetodo>);
    fmt::print("        tem_serializar_v<SemMetodo>  = {}\n", tem_serializar_v<SemMetodo>);
    fmt::print("        tem_serializar_v<TipoErrado> = {}  ← só checa que EXISTE\n",
               tem_serializar_v<TipoErrado>);

    fmt::print("      concept (C++20):\n");
    fmt::print("        Serializavel<ComMetodo>      = {}\n", Serializavel<ComMetodo>);
    fmt::print("        Serializavel<SemMetodo>      = {}\n", Serializavel<SemMetodo>);
    fmt::print("        Serializavel<TipoErrado>     = {}  ← checa o TIPO DE RETORNO\n",
               Serializavel<TipoErrado>);

    fmt::print("\n      O concept ganha em três frentes:\n");
    fmt::print("        • 3 linhas em vez de 8\n");
    fmt::print("        • verifica também o tipo de retorno\n");
    fmt::print("        • dá mensagem de erro na CHAMADA, não na instanciação\n");
    fmt::print("      Ver src/concepts/ para a mecânica completa.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. static_assert ou concept?
//
//     Não são substitutos perfeitos: eles falham em MOMENTOS diferentes, e
//     isso muda a resolução de overload.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
void com_assert(T) {
    static_assert(std::is_integral_v<T>, "só aceita inteiros");
}

template <std::integral T>
void com_concept(T) {}

// Overload para não-inteiros: só o concept permite isso.
void com_concept(double) { }

void demo_assert_vs_concept() {
    fmt::print("\n── 3.4 static_assert ou concept? ──\n");

    com_assert(42);
    com_concept(42);
    com_concept(3.14);            // vai para a sobrecarga de double

    fmt::print("      com_assert(3.14)  → ERRO. O template foi ESCOLHIDO e só\n");
    fmt::print("                          então o assert falhou. Não há como\n");
    fmt::print("                          outra sobrecarga assumir.\n");
    fmt::print("      com_concept(3.14) → ok. A constraint removeu o template do\n");
    fmt::print("                          conjunto de candidatos, e a sobrecarga\n");
    fmt::print("                          `void com_concept(double)` venceu.\n");

    fmt::print("\n      | ferramenta    | quando falha        | afeta overload? |\n");
    fmt::print("      |---------------|---------------------|-----------------|\n");
    fmt::print("      | static_assert | após a escolha      | NÃO             |\n");
    fmt::print("      | concept       | durante a escolha   | SIM             |\n");

    fmt::print("\n      Use `concept` para RESTRINGIR (afeta a resolução).\n");
    fmt::print("      Use `static_assert` para AFIRMAR fatos que não têm\n");
    fmt::print("      alternativa: invariantes de layout, tamanho, ABI.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Verificar layout e ABI — onde static_assert é insubstituível
//
//     Estruturas que atravessam uma fronteira binária (protocolo de rede,
//     formato de arquivo, FFI) têm requisitos de tamanho e offset. Um
//     static_assert transforma esses requisitos em erro de build.
// ─────────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct CabecalhoDeRede {
    std::uint8_t  versao;
    std::uint8_t  tipo;
    std::uint16_t comprimento;
    std::uint32_t timestamp;
};
#pragma pack(pop)

// O contrato do protocolo, verificado no build:
static_assert(sizeof(CabecalhoDeRede) == 8,
    "O cabeçalho do protocolo tem 8 bytes por especificação. "
    "Se este assert falhou, o padding mudou e os pacotes gerados "
    "serão incompatíveis com a outra ponta.");
static_assert(offsetof(CabecalhoDeRede, comprimento) == 2,
    "O campo 'comprimento' precisa estar no offset 2.");
static_assert(std::is_trivially_copyable_v<CabecalhoDeRede>,
    "O cabeçalho é enviado com memcpy — precisa ser trivialmente copiável.");

// Sem o pack, o mesmo struct tem outro tamanho — é o que o assert protege
struct CabecalhoSemPack {
    std::uint8_t  versao;
    std::uint8_t  tipo;
    std::uint16_t comprimento;
    std::uint32_t timestamp;
};

void demo_layout() {
    fmt::print("\n── 3.5 Verificando layout e ABI ──\n");

    fmt::print("      sizeof(CabecalhoDeRede)  = {} (com #pragma pack(1))\n",
               sizeof(CabecalhoDeRede));
    fmt::print("      sizeof(CabecalhoSemPack) = {} (o compilador alinha)\n",
               sizeof(CabecalhoSemPack));

    fmt::print("      offsets no cabeçalho empacotado:\n");
    fmt::print("        versao      → {}\n", offsetof(CabecalhoDeRede, versao));
    fmt::print("        tipo        → {}\n", offsetof(CabecalhoDeRede, tipo));
    fmt::print("        comprimento → {}\n", offsetof(CabecalhoDeRede, comprimento));
    fmt::print("        timestamp   → {}\n", offsetof(CabecalhoDeRede, timestamp));

    fmt::print("\n      Os três static_assert deste arquivo garantem que, se\n");
    fmt::print("      alguém acrescentar um campo ou trocar um tipo, o BUILD\n");
    fmt::print("      quebra — em vez de o protocolo quebrar em produção.\n");
    fmt::print("      É a única categoria em que static_assert não tem\n");
    fmt::print("      substituto: concepts restringem TEMPLATES, não layout.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_mensagens();
    demo_catalogo();
    demo_trait_proprio();
    demo_assert_vs_concept();
    demo_layout();
}

} // namespace demo_static_assert
