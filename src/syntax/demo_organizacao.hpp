// =============================================================================
//  src/syntax/demo_organizacao.hpp
//
//  Organização do código — namespaces, pré-processador, atributos e linkage
//  ───────────────────────────────────────────────────────────────────────
//  Antes de o compilador ver uma linha de C++, o PRÉ-PROCESSADOR já reescreveu
//  o arquivo: colou os `#include`, expandiu os `#define`, apagou os galhos de
//  `#if` falsos. O resultado dessa colagem é a *unidade de tradução*.
//
//      .hpp + .cpp ──[pré-processador]──> unidade de tradução
//                  ──[compilador]───────> objeto (.o)
//                  ──[linker]───────────> executável
//
//  Namespaces evitam colisões de nomes; `static`/namespace anônimo evitam
//  colisões de SÍMBOLOS no linker. São problemas diferentes.
//
//  Tópicos:
//  • Namespaces: aninhados, anônimos, alias, `using`, `inline`
//  • ADL — por que `std::` some quando você menos espera
//  • `#include`, `#pragma once` vs include guards
//  • Macros: objeto, função, `#`, `##`, variádicas — e suas armadilhas
//  • Compilação condicional e macros predefinidas
//  • Atributos padrão `[[...]]`
//  • Linkage, ODR, `inline`, `extern "C"`
//  • `static_assert` e comentários
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdlib>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Macros — declaradas fora do namespace: o pré-processador NÃO conhece escopo
// ─────────────────────────────────────────────────────────────────────────────

// (a) Macro de objeto: substituição textual pura
#define VERSAO_SINTAXE "1.0.0"

// (b) Macro de função: parênteses em CADA parâmetro e em volta de TUDO
#define QUADRADO(x) ((x) * (x))

// (c) Sem os parênteses, a precedência do operador destrói o resultado
#define QUADRADO_ERRADO(x) x* x

// (d) `#` transforma o argumento em string literal (stringify)
#define MOSTRAR(expr) fmt::print("        {} = {}\n", #expr, (expr))

// (e) `##` cola tokens (token pasting)
#define DECLARAR_VAR(sufixo) int variavel_##sufixo = 0

// (f) Macro variádica (C++11) — encaminha tudo para fmt
#define LOG(fmt_str, ...) fmt::print("        [log] " fmt_str "\n" __VA_OPT__(,) __VA_ARGS__)

// (g) O idioma do do-while(0): garante que a macro se comporte como UMA instrução
#define TROCAR(a, b) do { auto tmp_ = (a); (a) = (b); (b) = tmp_; } while (0)

namespace demo_organizacao {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Namespaces
// ─────────────────────────────────────────────────────────────────────────────
namespace empresa {
    namespace produto {
        namespace v1 {
            std::string versao() { return "produto v1"; }
        }
    }
}

// Aninhamento numa linha só (C++17)
namespace outra::camada::interna {
    std::string onde() { return "outra::camada::interna"; }
}

// Namespace ANÔNIMO: tudo aqui dentro tem linkage INTERNA — invisível a
// outras unidades de tradução. É o substituto moderno do `static` global.
namespace {
    int contador_privado = 0;
    int so_deste_arquivo() { return ++contador_privado; }
}

// Namespace INLINE: os membros vazam para o namespace pai. É o mecanismo de
// versionamento de ABI usado pela própria libstdc++.
namespace biblioteca {
    inline namespace v2 {
        std::string api() { return "v2 (a atual)"; }
    }
    namespace v1 {
        std::string api() { return "v1 (legada)"; }
    }
}

// Alias de namespace: encurta um caminho comprido
namespace pv1 = empresa::produto::v1;

// ADL (argument-dependent lookup): o compilador procura a função também nos
// namespaces dos ARGUMENTOS. É por isso que `swap(a, b)` acha `std::swap`.
namespace geometria {
    struct Vetor { double x, y; };
    double norma_quadrada(const Vetor& v) { return v.x * v.x + v.y * v.y; }
}

void demo_namespaces() {
    fmt::print("\n── 6.1 Namespaces ──\n");

    fmt::print("      aninhado clássico:  empresa::produto::v1::versao() → {}\n", pv1::versao());
    fmt::print("      aninhado C++17:     namespace outra::camada::interna {{ }} → {}\n",
               outra::camada::interna::onde());
    fmt::print("      alias:              namespace pv1 = empresa::produto::v1;\n");

    fmt::print("\n      Namespace anônimo — linkage interna:\n");
    const int primeira = so_deste_arquivo();
    const int segunda  = so_deste_arquivo();
    fmt::print("        so_deste_arquivo() = {}, depois {} ← invisível a outras .cpp\n",
               primeira, segunda);

    fmt::print("\n      Namespace inline — versionamento transparente:\n");
    fmt::print("        biblioteca::api()     → {}  ← resolve para o inline\n",
               biblioteca::api());
    fmt::print("        biblioteca::v1::api() → {}  ← a antiga continua acessível\n",
               biblioteca::v1::api());

    // using-declaration: traz UM nome. using-directive: traz TODOS.
    {
        using empresa::produto::v1::versao;          // um nome só — recomendado
        fmt::print("\n      using-declaration: `using ns::versao;` → {}\n", versao());
    }
    {
        using namespace outra::camada::interna;      // tudo — perigoso em header
        fmt::print("      using-directive:   `using namespace ns;` → {}\n", onde());
    }
    fmt::print("      ⚠ NUNCA escreva `using namespace` no escopo global de um HEADER:\n");
    fmt::print("        você impõe a decisão a todos os arquivos que o incluem.\n");

    fmt::print("\n      ADL — a busca vai também aos namespaces dos ARGUMENTOS:\n");
    geometria::Vetor v{3.0, 4.0};
    fmt::print("        norma_quadrada(v) = {} — chamado SEM qualificar `geometria::`\n",
               norma_quadrada(v));
    fmt::print("        (o argumento é geometria::Vetor, então lá se procura também)\n");
    fmt::print("        É o que faz `std::cout << x` e o idioma `using std::swap; swap(a,b)`\n");
    fmt::print("        funcionarem.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O pré-processador
// ─────────────────────────────────────────────────────────────────────────────
int contador_de_efeitos = 0;
int proximo() { return ++contador_de_efeitos; }

void demo_preprocessador() {
    fmt::print("\n── 6.2 O pré-processador ──\n");

    fmt::print("      #include <fmt/core.h>   → procura nos diretórios do sistema\n");
    fmt::print("      #include \"local.hpp\"    → procura primeiro ao lado do arquivo\n");
    fmt::print("      #pragma once            → guarda de inclusão (o que este arquivo usa)\n");
    fmt::print("      #ifndef X / #define X   → a guarda portável equivalente\n");

    fmt::print("\n      Macro de objeto:  VERSAO_SINTAXE = {}\n", VERSAO_SINTAXE);

    fmt::print("\n      Macro de função — parênteses são obrigatórios:\n");
    fmt::print("        QUADRADO(3)          = {}\n", QUADRADO(3));
    fmt::print("        QUADRADO(1 + 2)      = {}   ← ((1+2)*(1+2))\n", QUADRADO(1 + 2));
    fmt::print("        QUADRADO_ERRADO(1+2) = {}   ← vira 1+2*1+2 = 5. Precedência!\n",
               QUADRADO_ERRADO(1 + 2));

    fmt::print("\n      ⚠ A armadilha da DUPLA AVALIAÇÃO:\n");
    contador_de_efeitos = 0;
    int r = QUADRADO(proximo());     // `proximo()` é chamado DUAS vezes: 1 * 2
    fmt::print("        QUADRADO(proximo()) → resultado {} (esperava-se 1), \n"
               "          e o contador avançou para {} — a expressão foi COLADA duas vezes\n",
               r, contador_de_efeitos);
    fmt::print("        → uma função `constexpr int quadrado(int)` não tem nenhum\n");
    fmt::print("          desses problemas: tem tipos, escopo e avalia uma vez só.\n");

    fmt::print("\n      `#` stringify — o texto do argumento vira string:\n");
    int a = 6, b = 7;
    MOSTRAR(a * b);
    MOSTRAR(a > b);

    fmt::print("\n      `##` token pasting — cola nomes:\n");
    DECLARAR_VAR(temporaria);        // gera: int variavel_temporaria = 0;
    variavel_temporaria = 99;
    fmt::print("        DECLARAR_VAR(temporaria) criou `variavel_temporaria` = {}\n",
               variavel_temporaria);

    fmt::print("\n      Macro variádica (__VA_ARGS__):\n");
    LOG("sem argumentos");
    LOG("com {} e {}", 1, "dois");

    fmt::print("\n      do {{ ... }} while (0) — para a macro virar UMA instrução:\n");
    int x = 1, y = 2;
    TROCAR(x, y);
    fmt::print("        TROCAR(x, y) → x={} y={}  (funciona mesmo em `if (c) TROCAR(x,y);`)\n",
               x, y);

    fmt::print("\n      Compilação condicional:\n");
#ifdef NDEBUG
    fmt::print("        NDEBUG definido    → build de RELEASE (assert desativado)\n");
#else
    fmt::print("        NDEBUG não definido → build de DEBUG (assert ativo)\n");
#endif
#if __cplusplus >= 202002L
    fmt::print("        __cplusplus = {}L → C++20 ou superior\n", __cplusplus);
#elif __cplusplus >= 201703L
    fmt::print("        __cplusplus = {}L → C++17\n", __cplusplus);
#else
    fmt::print("        __cplusplus = {}L → anterior ao C++17\n", __cplusplus);
#endif
#if defined(__GNUC__) && !defined(__clang__)
    fmt::print("        __GNUC__ = {}.{} → GCC\n", __GNUC__, __GNUC_MINOR__);
#endif

    fmt::print("\n      Macros predefinidas úteis:\n");
    fmt::print("        __FILE__ = {}\n", __FILE__);
    fmt::print("        __LINE__ = {}\n", __LINE__);
    fmt::print("        __func__ = {}   ← não é macro: é uma variável implícita\n", __func__);
    fmt::print("        #error \"mensagem\" → aborta a compilação de propósito\n");

    fmt::print("\n      Regra geral: em C++, quase toda macro tem substituto melhor —\n");
    fmt::print("        #define PI 3.14   → constexpr double pi = 3.14;\n");
    fmt::print("        #define MAX(a,b)  → template <class T> T max(T,T)\n");
    fmt::print("        #define DEBUG_ON  → if constexpr / constexpr bool\n");
    fmt::print("        Sobram os casos que EXIGEM texto: guardas, __LINE__, portabilidade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Atributos padrão
// ─────────────────────────────────────────────────────────────────────────────
[[nodiscard]] int calcular_importante() { return 42; }

[[noreturn]] void abortar_de_vez(const char* motivo) {
    fmt::print("        (abortaria: {})\n", motivo);
    std::abort();
}

int classificar(int n) {
    if (n > 0) [[likely]]   { return 1; }        // C++20: dica ao otimizador
    else if (n < 0) [[unlikely]] { return -1; }
    return 0;
}

void demo_atributos() {
    fmt::print("\n── 6.3 Atributos padrão ──\n");

    [[maybe_unused]] int nao_usado = 0;
    const int importante = calcular_importante();

    fmt::print("      [[nodiscard]]    → warning se o retorno for ignorado ({})\n", importante);
    fmt::print("      [[maybe_unused]] → silencia o warning de variável não usada\n");
    fmt::print("      [[deprecated(\"use X\")]] → warning em cada uso\n");
    fmt::print("      [[noreturn]]     → \"esta função não volta\" (abort, throw, exit)\n");
    fmt::print("      [[fallthrough]]  → \"a queda no switch é intencional\" (ver demo_controle)\n");
    fmt::print("      [[likely]]/[[unlikely]] (C++20) → dica de ramificação: classificar(5)={}\n",
               classificar(5));
    fmt::print("      [[no_unique_address]] (C++20) → membro vazio pode ocupar 0 bytes\n");

    fmt::print("\n      Atributos são PADRÃO e portáveis — ao contrário de\n");
    fmt::print("      __attribute__((...)) do GCC ou __declspec(...) do MSVC.\n");
    fmt::print("      Um atributo desconhecido é ignorado, não é erro.\n");

    (void)&abortar_de_vez;
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Linkage, ODR e extern "C"
// ─────────────────────────────────────────────────────────────────────────────

// `inline` numa variável (C++17): uma única instância, definida no header
inline int variavel_global_do_header = 100;

// `extern "C"`: desliga o name mangling do C++ → o símbolo fica linkável por C
extern "C" int soma_para_c(int a, int b) { return a + b; }

void demo_linkage() {
    fmt::print("\n── 6.4 Linkage, ODR e extern \"C\" ──\n");

    fmt::print("      | construção            | linkage  | consequência                |\n");
    fmt::print("      |-----------------------|----------|-----------------------------|\n");
    fmt::print("      | int x;      (global)  | externa  | visível a todo o programa   |\n");
    fmt::print("      | static int x;         | interna  | só esta unidade de tradução |\n");
    fmt::print("      | namespace {{ int x; }}  | interna  | idem, o jeito C++           |\n");
    fmt::print("      | extern int x;         | externa  | declara; define-se noutra .cpp|\n");
    fmt::print("      | inline int x = 1;     | externa  | UMA instância, mesmo em header|\n");
    fmt::print("      | const int x = 1;      | interna  | ⚠ const global é interno em C++|\n");

    fmt::print("\n      inline int variavel_global_do_header = {};\n", variavel_global_do_header);
    fmt::print("        → antes do C++17 isto exigia `extern` no .hpp + definição no .cpp\n");

    fmt::print("\n      ODR (One Definition Rule):\n");
    fmt::print("        • cada entidade pode ter N declarações\n");
    fmt::print("        • e exatamente UMA definição no programa\n");
    fmt::print("        • exceção: classes, templates e `inline` podem repetir-se em\n");
    fmt::print("          várias unidades, DESDE QUE sejam token a token idênticas\n");

    fmt::print("\n      extern \"C\" — o C não tem sobrecarga, logo não tem name mangling:\n");
    fmt::print("        extern \"C\" int soma_para_c(int, int);  → soma_para_c(2,3) = {}\n",
               soma_para_c(2, 3));
    fmt::print("        o símbolo sai como `soma_para_c`, não `_Z11soma_para_cii`\n");
    fmt::print("        → é o que permite expor uma biblioteca C++ a C, Python (ctypes)\n");
    fmt::print("          ou a um carregador de plugins. Ver src/plugin_registry.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. static_assert e comentários
// ─────────────────────────────────────────────────────────────────────────────
void demo_assercoes() {
    fmt::print("\n── 6.5 static_assert e comentários ──\n");

    static_assert(sizeof(int) >= 4, "este código assume int de 32 bits ou mais");
    static_assert(sizeof(void*) == 8);                      // mensagem opcional (C++17)

    fmt::print("      static_assert(cond, \"msg\") → verificação em COMPILE-TIME\n");
    fmt::print("        falhar não gera um crash: gera um ERRO DE COMPILAÇÃO\n");
    fmt::print("        custo em runtime: zero. Ver src/compile_time.\n");
    fmt::print("      assert(cond) (<cassert>)  → verificação em RUNTIME, some com NDEBUG\n");

    fmt::print("\n      Comentários:\n");
    fmt::print("        // até o fim da linha            (C++, o usual)\n");
    fmt::print("        /* em bloco */                   (herdado do C, NÃO aninha)\n");
    fmt::print("        /// ou /** */                    (Doxygen — ver src/doxygen)\n");
    fmt::print("      O melhor comentário explica o PORQUÊ; o código já diz o QUÊ.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_namespaces();
    demo_preprocessador();
    demo_atributos();
    demo_linkage();
    demo_assercoes();
}

} // namespace demo_organizacao
