// =============================================================================
//  src/vocabulary_types/demo_optional.hpp
//
//  std::optional<T> — "pode não haver valor"
//  ─────────────────────────────────────────
//  Antes do C++17, "sem valor" era codificado por convenção: um ponteiro nulo,
//  um índice -1, uma string vazia, um `bool` de saída. Todas essas convenções
//  compartilham o mesmo defeito — nada no TIPO diz que a ausência é possível,
//  então o compilador não pode cobrar a verificação.
//
//  `optional<T>` põe a ausência no tipo. Ele guarda o T INLINE (sem heap) mais
//  um bool, e o acesso ao valor é uma decisão explícita de quem chama.
//
//  Tópicos:
//  • Construir, testar, extrair — e as três formas de acessar
//  • Onde o T mora: sizeof e a ausência de alocação
//  • Operações monádicas (C++23): and_then, transform, or_else
//  • Por que optional<T&> NÃO existe
//  • A armadilha do optional<bool>
//  • optional como retorno vs como parâmetro vs como membro
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace demo_optional {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O básico — e as três formas de acessar o valor
// ─────────────────────────────────────────────────────────────────────────────
std::optional<int> buscar_idade(const std::string& nome) {
    static const std::unordered_map<std::string, int> pessoas{
        {"Ada", 36}, {"Grace", 45}, {"Alan", 41},
    };
    auto it = pessoas.find(nome);
    if (it == pessoas.end()) return std::nullopt;   // ausência explícita
    return it->second;                              // conversão implícita para optional
}

void demo_basico() {
    fmt::print("\n── 1.1 Construir, testar, extrair ──\n");

    for (const std::string nome : {"Ada", "Turing"}) {
        auto r = buscar_idade(nome);

        // (a) operator bool + operator* — sem verificação no operator*
        if (r) {
            fmt::print("      {:<7} → {} anos\n", nome, *r);
        } else {
            fmt::print("      {:<7} → ausente\n", nome);
        }
    }

    // (b) value_or — o default na mesma expressão
    fmt::print("      value_or(-1) para 'Turing' → {}\n",
               buscar_idade("Turing").value_or(-1));

    // (c) value() — lança std::bad_optional_access se vazio
    try {
        (void)buscar_idade("Turing").value();
    } catch (const std::bad_optional_access& e) {
        fmt::print("      .value() vazio lança bad_optional_access: {}\n", e.what());
    }

    fmt::print("\n      | forma        | se vazio             | quando usar        |\n");
    fmt::print("      |--------------|----------------------|--------------------|\n");
    fmt::print("      | *opt         | UNDEFINED BEHAVIOR   | já testou com if   |\n");
    fmt::print("      | opt.value()  | lança bad_optional…  | quer rede de segur.|\n");
    fmt::print("      | value_or(x)  | devolve x            | tem um default óbvio|\n");
    fmt::print("      ⚠  `*opt` num optional vazio é UB — não é exceção, não é\n");
    fmt::print("         crash garantido. É lixo silencioso. Teste antes.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Onde o valor mora — nenhuma alocação
//
//     `optional<T>` contém o T por VALOR, num buffer alinhado dentro de si.
//     Custo: sizeof(T) arredondado + 1 byte de flag + padding. Zero heap.
// ─────────────────────────────────────────────────────────────────────────────
struct Vec3 { double x, y, z; };

void demo_layout() {
    fmt::print("\n── 1.2 Onde o valor mora ──\n");

    fmt::print("      sizeof(int)                 = {:>3}\n", sizeof(int));
    fmt::print("      sizeof(optional<int>)       = {:>3}  (int + flag + padding)\n",
               sizeof(std::optional<int>));
    fmt::print("      sizeof(Vec3)                = {:>3}\n", sizeof(Vec3));
    fmt::print("      sizeof(optional<Vec3>)      = {:>3}\n", sizeof(std::optional<Vec3>));
    fmt::print("      sizeof(std::string)         = {:>3}\n", sizeof(std::string));
    fmt::print("      sizeof(optional<string>)    = {:>3}\n", sizeof(std::optional<std::string>));

    // Prova de que o T vive DENTRO do optional: os endereços coincidem.
    std::optional<Vec3> o{Vec3{1, 2, 3}};
    const auto* endereco_optional = static_cast<const void*>(&o);
    const auto* endereco_valor    = static_cast<const void*>(&*o);
    fmt::print("      &optional = {}\n", endereco_optional);
    fmt::print("      &*optional= {}  ← dentro do próprio objeto\n", endereco_valor);
    fmt::print("      → nenhuma alocação. `optional` não é um ponteiro.\n");

    // `emplace` constrói in-place, sem temporário
    std::optional<std::string> s;
    s.emplace(10, 'x');                  // constrói std::string(10, 'x') no lugar
    fmt::print("      emplace(10,'x') → '{}'\n", *s);
    s.reset();                           // destrói o valor, volta a vazio
    fmt::print("      após reset(): has_value = {}\n", s.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Operações monádicas (C++23)
//
//     Sem elas, cada passo de um encadeamento vira um `if (!o) return {};`.
//     Com elas, a ausência propaga sozinha — a mesma ideia do std::expected
//     (ver src/errors/demo_expected.hpp).
// ─────────────────────────────────────────────────────────────────────────────
std::optional<std::string> nome_do_id(int id) {
    if (id == 1) return "Ada";
    if (id == 2) return "Grace";
    return std::nullopt;
}

std::optional<int> comprimento_se_curto(const std::string& s) {
    if (s.size() > 10) return std::nullopt;
    return static_cast<int>(s.size());
}

void demo_monadico() {
    fmt::print("\n── 1.3 Operações monádicas (C++23) ──\n");

    auto pipeline = [](int id) {
        return nome_do_id(id)                                  // optional<string>
            .and_then(comprimento_se_curto)                    // optional<int>
            .transform([](int n) { return n * 100; })           // optional<int>
            .or_else([] { return std::optional<int>{-1}; });    // nunca vazio no fim
    };

    for (int id : {1, 2, 99}) {
        fmt::print("      id={:<3} → {}\n", id, *pipeline(id));
    }

    fmt::print("      A versão sem monádicas, para comparar:\n");
    fmt::print("        auto n = nome_do_id(id);\n");
    fmt::print("        if (!n) return -1;\n");
    fmt::print("        auto c = comprimento_se_curto(*n);\n");
    fmt::print("        if (!c) return -1;\n");
    fmt::print("        return *c * 100;\n");
    fmt::print("      Mesma lógica, o dobro das linhas, e dois `if` de propagação.\n");

    fmt::print("\n      | operação      | recebe | devolve      | papel           |\n");
    fmt::print("      |---------------|--------|--------------|-----------------|\n");
    fmt::print("      | and_then(f)   | T      | optional<U>  | encadeia        |\n");
    fmt::print("      | transform(f)  | T      | U            | mapeia o valor  |\n");
    fmt::print("      | or_else(f)    | (nada) | optional<T>  | fornece default |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Por que optional<T&> não existe
//
//     A pergunta natural é: o que `opt = outro` deveria fazer com uma
//     referência dentro? RELIGAR a referência (mudar para onde aponta) ou
//     ATRIBUIR ao objeto referido? As duas respostas são defensáveis, e o
//     comitê não escolheu — então proibiu.
//
//     Na prática, um ponteiro JÁ é o "optional reference" do C++.
// ─────────────────────────────────────────────────────────────────────────────
struct Config { int timeout = 30; };

// ✓ A forma idiomática de "referência que pode faltar": ponteiro cru.
//   Aqui `T*` não significa posse — significa "opcional e não-proprietário".
const Config* achar_config(const std::vector<Config>& v, int timeout) {
    for (const auto& c : v) if (c.timeout == timeout) return &c;
    return nullptr;
}

// ✓ Alternativa quando você quer a intenção explícita no tipo:
//   optional<reference_wrapper<T>> compila e funciona.
std::optional<std::reference_wrapper<const Config>>
achar_config_ref(const std::vector<Config>& v, int timeout) {
    for (const auto& c : v) if (c.timeout == timeout) return std::cref(c);
    return std::nullopt;
}

void demo_sem_referencia() {
    fmt::print("\n── 1.4 Por que optional<T&> não existe ──\n");

    // std::optional<int&> o;        // ✗ não compila
    fmt::print("      `std::optional<int&>` NÃO COMPILA.\n");
    fmt::print("      Motivo: `opt = outro` seria ambíguo — religar a referência\n");
    fmt::print("      ou atribuir ao objeto referido? Sem resposta óbvia → proibido.\n");

    std::vector<Config> configs{{10}, {30}, {60}};

    if (const Config* c = achar_config(configs, 30)) {
        fmt::print("      via ponteiro cru:  timeout={} (nullptr = ausente)\n", c->timeout);
    }

    if (auto r = achar_config_ref(configs, 60)) {
        fmt::print("      via reference_wrapper: timeout={}\n", r->get().timeout);
    }

    fmt::print("      Prefira `T*` — é mais curto e todo mundo entende.\n");
    fmt::print("      Use `optional<reference_wrapper<T>>` só quando a assinatura\n");
    fmt::print("      precisa dizer \"opcional\" de forma inequívoca.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. A armadilha do optional<bool>
//
//     `optional<bool>` tem TRÊS estados: vazio, false, true. E o `operator bool`
//     do optional responde "tem valor?", não "o valor é true?". A confusão é
//     quase inevitável.
// ─────────────────────────────────────────────────────────────────────────────
void demo_optional_bool() {
    fmt::print("\n── 1.5 ⚠ A armadilha do optional<bool> ──\n");

    const std::optional<bool> vazio;
    const std::optional<bool> falso{false};
    const std::optional<bool> verdadeiro{true};

    auto mostrar = [](const char* rotulo, const std::optional<bool>& o) {
        fmt::print("      {:<12} if(o) = {:<5}  o.value_or(false) = {}\n",
                   rotulo, static_cast<bool>(o), o.value_or(false));
    };

    mostrar("vazio",      vazio);
    mostrar("false",      falso);
    mostrar("true",       verdadeiro);

    fmt::print("      ↑ repare na linha 'false': `if (o)` é TRUE porque HÁ valor.\n");
    fmt::print("        Quem lê `if (flag)` esperando o valor booleano se engana.\n");

    fmt::print("\n      Alternativas mais claras:\n");
    fmt::print("        enum class Resposta {{ Sim, Nao, NaoInformado }};\n");
    fmt::print("        → 3 estados nomeados, impossível confundir\n");
    fmt::print("      Se mantiver o optional<bool>, escreva sempre:\n");
    fmt::print("        if (o.has_value() && *o)   // explícito nos dois testes\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Onde optional cabe — e onde não
// ─────────────────────────────────────────────────────────────────────────────
void demo_onde_usar() {
    fmt::print("\n── 1.6 Onde optional cabe ──\n");

    fmt::print("      ✓ RETORNO de função que pode não achar nada\n");
    fmt::print("        (é o uso dominante — busca, parse, lookup)\n");
    fmt::print("      ✓ MEMBRO que é genuinamente opcional\n");
    fmt::print("        (um campo de formulário que o usuário pode deixar vazio)\n");
    fmt::print("      ✓ inicialização TARDIA de um membro sem default ctor\n");
    fmt::print("      ~ PARÂMETRO opcional — funciona, mas um overload ou um\n");
    fmt::print("        argumento default costuma ser mais legível\n");
    fmt::print("      ✗ como substituto de `expected` quando o PORQUÊ importa\n");
    fmt::print("        (optional perde o motivo do erro — ver src/errors/)\n");
    fmt::print("      ✗ optional<optional<T>>, optional<bool> — modele melhor\n");
    fmt::print("      ✗ optional<unique_ptr<T>> — o ptr já tem estado nulo\n");

    fmt::print("\n      Regra de ouro: `optional` diz \"pode não haver\".\n");
    fmt::print("      Se você precisa dizer \"não houve, e o motivo foi X\",\n");
    fmt::print("      a ferramenta é `std::expected` (src/errors/demo_expected.hpp).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_layout();
    demo_monadico();
    demo_sem_referencia();
    demo_optional_bool();
    demo_onde_usar();
}

} // namespace demo_optional
