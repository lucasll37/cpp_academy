// =============================================================================
//  src/design-of-pattern/prototype/main.cpp
//
//  PROTOTYPE — clonar sem conhecer o tipo concreto
//  ───────────────────────────────────────────────
//  O problema: você tem um `Forma*` e quer uma cópia. Não dá para escrever
//  `new Forma(*f)` (a classe é abstrata), nem `new Circulo(*f)` (você não sabe
//  que é um círculo). O objeto precisa se copiar sozinho.
//
//  É o mesmo raciocínio do destrutor virtual: a operação depende do tipo
//  DINÂMICO, então precisa ser virtual.
// =============================================================================
#include "forma.hpp"
#include "formas_concretas.hpp"

#include <fmt/core.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A eliminação do boilerplate: CRTP
//
//  Todo `clonar()` é a mesma linha. Uma classe intermediária CRTP escreve por
//  todo mundo — ver src/templates/demo_crtp.hpp.
// ─────────────────────────────────────────────────────────────────────────────
namespace com_crtp {

class Forma {
public:
    virtual ~Forma() = default;
    virtual std::unique_ptr<Forma> clonar() const = 0;
    virtual std::string descrever() const = 0;
};

// A camada que escreve o `clonar()` para os derivados
template <typename Derivada, typename Base = Forma>
class Clonavel : public Base {
public:
    std::unique_ptr<Forma> clonar() const override {
        // `static_cast` para o tipo real, e o construtor de cópia dele
        return std::make_unique<Derivada>(static_cast<const Derivada&>(*this));
    }
};

// Os derivados não escrevem `clonar()` — herdam pronto
class Quadrado : public Clonavel<Quadrado> {
public:
    explicit Quadrado(double l) : lado_(l) {}
    std::string descrever() const override {
        return fmt::format("Quadrado({:.1f})", lado_);
    }
private:
    double lado_;
};

class Triangulo : public Clonavel<Triangulo> {
public:
    Triangulo(double b, double h) : base_(b), altura_(h) {}
    std::string descrever() const override {
        return fmt::format("Triângulo({:.1f}x{:.1f})", base_, altura_);
    }
private:
    double base_, altura_;
};

}  // namespace com_crtp

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: std::variant — cópia sem clonar
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

struct Circulo   { double raio; };
struct Retangulo { double largura, altura; };
struct Triangulo { double base, altura; };

using Forma = std::variant<Circulo, Retangulo, Triangulo>;

std::string descrever(const Forma& f) {
    return std::visit([](const auto& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Circulo>)
            return fmt::format("Círculo(r={:.1f})", x.raio);
        else if constexpr (std::is_same_v<T, Retangulo>)
            return fmt::format("Retângulo({:.1f}x{:.1f})", x.largura, x.altura);
        else
            return fmt::format("Triângulo({:.1f}x{:.1f})", x.base, x.altura);
    }, f);
}

}  // namespace moderno

static void secao(const char* titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│             cpp_academy — Prototype (GoF)                │\n");
    fmt::print("│   \"Especifique tipos de objetos usando uma instância     │\n");
    fmt::print("│    prototípica, e crie novos copiando-a.\"       — GoF     │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. O problema");

    fmt::print("  Você tem um `Forma*` e quer uma cópia:\n\n");
    fmt::print("    Forma* f = obter_de_algum_lugar();\n");
    fmt::print("    new Forma(*f);     // ✗ Forma é abstrata\n");
    fmt::print("    new Circulo(*f);   // ✗ e se não for um Círculo?\n");
    fmt::print("    f->clonar();       // ✓ o OBJETO sabe o que ele é\n\n");
    fmt::print("  É o mesmo raciocínio do destrutor virtual: a operação depende\n");
    fmt::print("  do tipo DINÂMICO, então tem que ser virtual.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. Clonagem polimórfica");

    std::vector<std::unique_ptr<prototype::Forma>> originais;
    auto c = std::make_unique<prototype::Circulo>(2.0);
    c->definir_cor("vermelho");
    originais.push_back(std::move(c));
    originais.push_back(std::make_unique<prototype::Retangulo>(3.0, 4.0));

    fmt::print("  originais:\n");
    for (const auto& f : originais) {
        fmt::print("    {:<34} área={:.2f}\n", f->descrever(), f->area());
    }

    // Clonar TODOS sem saber os tipos
    std::vector<std::unique_ptr<prototype::Forma>> copias;
    copias.reserve(originais.size());
    for (const auto& f : originais) copias.push_back(f->clonar());

    fmt::print("\n  clones (o laço não menciona nenhum tipo concreto):\n");
    for (const auto& f : copias) {
        fmt::print("    {:<34} área={:.2f}\n", f->descrever(), f->area());
    }

    // E são INDEPENDENTES
    static_cast<prototype::Circulo&>(*copias[0]).definir_raio(10.0);
    copias[0]->definir_cor("azul");
    fmt::print("\n  após modificar o CLONE:\n");
    fmt::print("    original: {}\n", originais[0]->descrever());
    fmt::print("    clone   : {}\n", copias[0]->descrever());
    fmt::print("  → cópias independentes. O estado herdado (`cor_`) também.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Clone profundo — onde fica difícil");

    auto grupo = std::make_unique<prototype::Grupo>();
    grupo->definir_cor("verde");
    grupo->adicionar(std::make_unique<prototype::Circulo>(1.0));
    grupo->adicionar(std::make_unique<prototype::Retangulo>(2.0, 2.0));

    auto sub = std::make_unique<prototype::Grupo>();
    sub->adicionar(std::make_unique<prototype::Circulo>(0.5));
    grupo->adicionar(std::move(sub));

    fmt::print("  original:\n    {}\n", grupo->descrever());

    const auto clone_grupo = grupo->clonar();
    fmt::print("\n  clone:\n    {}\n", clone_grupo->descrever());

    // Modificar um filho do clone não afeta o original
    auto& g = static_cast<prototype::Grupo&>(*clone_grupo);
    static_cast<prototype::Circulo&>(g.filho(0)).definir_raio(99.0);

    fmt::print("\n  após mudar o raio do 1º filho do CLONE:\n");
    fmt::print("    original: área total = {:.2f}\n", grupo->area());
    fmt::print("    clone   : área total = {:.2f}\n", clone_grupo->area());

    fmt::print("\n  O `Grupo` precisou de um construtor de cópia ESCRITO À MÃO:\n");
    fmt::print("    Grupo(const Grupo& o) : Forma(o) {{\n");
    fmt::print("        for (const auto& f : o.filhos_)\n");
    fmt::print("            filhos_.push_back(f->clonar());   // ← recursivo\n");
    fmt::print("    }}\n");
    fmt::print("  O gerado nem compilaria: `unique_ptr` não é copiável.\n");
    fmt::print("  É a diferença entre cópia RASA e PROFUNDA, e o Prototype só\n");
    fmt::print("  funciona se cada nó fizer a sua parte.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Eliminando o boilerplate com CRTP");

    fmt::print("  Todo `clonar()` é a MESMA linha:\n");
    fmt::print("    return std::make_unique<EuMesmo>(*this);\n\n");
    fmt::print("  Uma camada CRTP escreve por todos:\n");
    fmt::print("    template <typename Derivada, typename Base = Forma>\n");
    fmt::print("    class Clonavel : public Base {{\n");
    fmt::print("        std::unique_ptr<Forma> clonar() const override {{\n");
    fmt::print("            return std::make_unique<Derivada>(\n");
    fmt::print("                static_cast<const Derivada&>(*this));\n");
    fmt::print("        }}\n");
    fmt::print("    }};\n");
    fmt::print("    class Quadrado : public Clonavel<Quadrado> {{ ... }};\n\n");

    std::vector<std::unique_ptr<com_crtp::Forma>> crtp;
    crtp.push_back(std::make_unique<com_crtp::Quadrado>(3.0));
    crtp.push_back(std::make_unique<com_crtp::Triangulo>(4.0, 5.0));

    fmt::print("  nenhuma das classes abaixo escreveu `clonar()`:\n");
    for (const auto& f : crtp) {
        const auto copia = f->clonar();
        fmt::print("    {:<22} → clone: {}\n", f->descrever(), copia->descrever());
    }

    fmt::print("\n  ⚠  o CRTP aqui tem um custo: a hierarquia fica com uma camada\n");
    fmt::print("     a mais, e herdar de `Clonavel<X>` em vez de `Forma` é menos\n");
    fmt::print("     óbvio para quem lê. Vale quando há muitos derivados.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. A versão moderna: variant");

    std::vector<moderno::Forma> formas{
        moderno::Circulo{2.0},
        moderno::Retangulo{3.0, 4.0},
        moderno::Triangulo{5.0, 6.0},
    };

    // Copiar um vector<variant> é uma linha — e é cópia PROFUNDA de graça
    const auto copia = formas;

    fmt::print("  com std::variant:\n");
    for (const auto& f : copia) fmt::print("    {}\n", moderno::descrever(f));

    fmt::print("\n    auto copia = formas;      // ← isso é tudo\n\n");
    fmt::print("  Por que funciona: o `variant` guarda o valor INLINE, não um\n");
    fmt::print("  ponteiro. Copiar o variant copia o objeto. Não há posse\n");
    fmt::print("  compartilhada, logo não há distinção rasa/profunda.\n");

    fmt::print("\n  | critério                | Prototype   | variant       |\n");
    fmt::print("  |-------------------------|-------------|---------------|\n");
    fmt::print("  | hierarquia               | ABERTA      | fechada       |\n");
    fmt::print("  | escrever clonar()        | por classe  | nada          |\n");
    fmt::print("  | cópia profunda           | manual      | automática    |\n");
    fmt::print("  | alocação                 | uma por nó  | nenhuma       |\n");
    fmt::print("  | tipo novo de FORA        | sim         | não           |\n");
    fmt::print("  (o \"expression problem\" — src/vocabulary_types/, 2.7)\n");

    // ── 6 ────────────────────────────────────────────────────────────────────
    secao("6. Um registro de protótipos");

    // O uso que dá nome ao padrão: um catálogo de instâncias-modelo
    std::map<std::string, std::unique_ptr<prototype::Forma>> catalogo;
    auto modelo = std::make_unique<prototype::Circulo>(1.0);
    modelo->definir_cor("padrão");
    catalogo.emplace("circulo-unitario", std::move(modelo));
    catalogo.emplace("quadrado-1x1", std::make_unique<prototype::Retangulo>(1.0, 1.0));

    fmt::print("  um catálogo de INSTÂNCIAS-MODELO, criado em runtime:\n");
    for (const auto& [nome, proto] : catalogo) {
        auto novo = proto->clonar();
        fmt::print("    catalogo[\"{}\"]->clonar() → {}\n", nome, novo->descrever());
    }

    fmt::print("\n  É este uso que dá NOME ao padrão: em vez de uma fábrica com\n");
    fmt::print("  um `switch` sobre nomes de classe, você guarda um EXEMPLAR de\n");
    fmt::print("  cada tipo e clona sob demanda.\n");
    fmt::print("  Vantagem sobre a Factory: os protótipos podem ser CONFIGURADOS\n");
    fmt::print("  em runtime (carregados de um arquivo, montados pelo usuário) —\n");
    fmt::print("  uma fábrica só sabe criar o que foi COMPILADO nela.\n");
    fmt::print("  Ver design-of-pattern/factory/ e solid/ocp/demo_registry.hpp.\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do Prototype                         ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : copiar sem conhecer o tipo concreto         ║\n");
    fmt::print("║ Assinatura : virtual unique_ptr<Base> clonar() const     ║\n");
    fmt::print("║ Chave      : devolva unique_ptr, não Base*               ║\n");
    fmt::print("║ Cuidado    : nós compostos exigem clone PROFUNDO à mão   ║\n");
    fmt::print("║ Boilerplate: elimine com CRTP se houver muitos derivados ║\n");
    fmt::print("║ Alternativa: std::variant, se a hierarquia for FECHADA   ║\n");
    fmt::print("║ Uso típico : catálogo de modelos configurados em runtime ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ prototype — todos os demos concluídos\n\n");

    return 0;
}
