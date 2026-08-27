// =============================================================================
//  src/design-of-pattern/mediator/main.cpp
//
//  MEDIATOR — trocar uma teia por uma estrela
//  ──────────────────────────────────────────
//  Quando N componentes precisam reagir uns aos outros, cada um acaba
//  conhecendo os outros — N×(N-1) referências, e acrescentar um toca todos.
//
//  O Mediator centraliza a INTERAÇÃO: cada componente conhece só o mediador.
// =============================================================================
#include "componentes.hpp"
#include "formulario.hpp"
#include "mediador.hpp"

#include <fmt/core.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

static void secao(const char* titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

static void mostrar(const mediator::FormularioDeCadastro& f) {
    auto& nc = const_cast<mediator::FormularioDeCadastro&>(f);
    fmt::print("      email='{}' termos={} empresa={} cnpj={}(visível={}) | enviar={} | {}\n",
               nc.email().texto(),
               nc.termos().marcada() ? "x" : " ",
               nc.empresa().marcada() ? "x" : " ",
               nc.cnpj().texto().empty() ? "vazio" : nc.cnpj().texto(),
               nc.cnpj().visivel() ? "sim" : "não",
               nc.enviar().habilitado() ? "ATIVO" : "inativo",
               f.status().texto());
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│              cpp_academy — Mediator (GoF)                │\n");
    fmt::print("│   \"Defina um objeto que encapsula como um conjunto de    │\n");
    fmt::print("│    objetos interage.\"                           — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. A teia que ele desfaz");

    fmt::print("  Um formulário com 6 componentes e estas regras:\n");
    fmt::print("    • \"Enviar\" só ativa com e-mail válido E termos aceitos\n");
    fmt::print("    • marcar \"empresa\" revela o CNPJ e o torna obrigatório\n");
    fmt::print("    • o status reflete o que falta\n\n");

    fmt::print("  SEM mediador, quem conhece quem:\n");
    fmt::print("    email   → botão, status\n");
    fmt::print("    termos  → botão, status\n");
    fmt::print("    empresa → cnpj, botão, status\n");
    fmt::print("    cnpj    → botão, status\n");
    fmt::print("    6 componentes → até 30 referências\n");
    fmt::print("    e acrescentar \"aceito newsletter\" toca vários deles\n\n");

    fmt::print("  COM mediador:\n");
    fmt::print("    cada componente → o mediador. Só isso.\n");
    fmt::print("    6 referências, e a regra num lugar só.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. A interação em ação");

    mediator::FormularioDeCadastro form;

    fmt::print("  estado inicial:\n");
    mostrar(form);

    fmt::print("\n  digitando um e-mail inválido:\n");
    form.email().digitar("ada");
    mostrar(form);

    fmt::print("\n  corrigindo o e-mail:\n");
    form.email().digitar("ada@exemplo.com");
    mostrar(form);

    fmt::print("\n  aceitando os termos:\n");
    form.termos().alternar();
    mostrar(form);

    fmt::print("\n  marcando \"empresa\" (revela o CNPJ e o torna obrigatório):\n");
    form.empresa().alternar();
    mostrar(form);

    fmt::print("\n  preenchendo o CNPJ:\n");
    form.cnpj().digitar("12.345.678/0001-90");
    mostrar(form);

    fmt::print("\n  clicando em Enviar:\n");
    form.enviar().clicar();
    mostrar(form);

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Nenhum componente conhece outro");

    fmt::print("  O log das notificações que o mediador recebeu:\n");
    for (const auto& e : form.registro()) fmt::print("    {}\n", e);

    fmt::print("\n  Cada linha é um componente dizendo \"algo aconteceu comigo\".\n");
    fmt::print("  Nenhum deles disse a QUEM isso interessa — o mediador decide.\n\n");

    fmt::print("  A base `Componente` tem exatamente UMA referência externa:\n");
    fmt::print("    Mediador* mediador_ = nullptr;\n\n");
    fmt::print("  E o método que a usa:\n");
    fmt::print("    void emitir(const std::string& evento) {{\n");
    fmt::print("        if (mediador_) mediador_->notificar(this, evento);\n");
    fmt::print("    }}\n\n");
    fmt::print("  `componentes.hpp` não menciona `Botao` dentro de `CampoDeTexto`,\n");
    fmt::print("  nem vice-versa. Eles são independentes e REUTILIZÁVEIS.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Mediator vs Observer");

    fmt::print("  Os dois desacoplam quem avisa de quem escuta. A diferença:\n\n");
    fmt::print("  | critério              | Mediator         | Observer        |\n");
    fmt::print("  |-----------------------|------------------|-----------------|\n");
    fmt::print("  | direção               | ESTRELA          | broadcast 1→N   |\n");
    fmt::print("  | quem tem a lógica     | o MEDIADOR       | cada observador |\n");
    fmt::print("  | o emissor sabe algo?  | conhece o mediad.| nada dos obs.   |\n");
    fmt::print("  | acoplamento           | ao mediador      | zero            |\n");
    fmt::print("  | coordenação complexa  | SIM              | não             |\n");
    fmt::print("  | N-para-N              | SIM              | difícil         |\n\n");

    fmt::print("  A pergunta que separa:\n");
    fmt::print("    \"existe uma REGRA que depende de VÁRIOS componentes?\"\n");
    fmt::print("      sim → Mediator (a regra mora nele)\n");
    fmt::print("      não, é só notificação → Observer\n\n");
    fmt::print("  Aqui, \"ativar o botão se e-mail OK E termos OK E cnpj OK\"\n");
    fmt::print("  depende de TRÊS componentes. Com Observer, essa regra teria\n");
    fmt::print("  que morar no botão — e aí ele conheceria os três.\n");
    fmt::print("  Ver design-of-pattern/observer/.\n\n");

    fmt::print("  ⚠  os dois se COMBINAM com frequência: o mediador costuma\n");
    fmt::print("     ser um observador de todos os componentes. É o que a\n");
    fmt::print("     arquitetura MVC/MVP faz — o Controller/Presenter é o\n");
    fmt::print("     mediador entre a View e o Model.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. O risco: o mediador vira um God Object");

    fmt::print("  É o defeito estrutural do padrão, e ele é inerente:\n");
    fmt::print("  ao centralizar a interação, o mediador acumula TODA a lógica\n");
    fmt::print("  que estava distribuída.\n\n");

    fmt::print("  Os sinais de alarme:\n");
    fmt::print("    • `notificar` vira um switch de 200 linhas\n");
    fmt::print("    • o mediador ganha estado próprio que não é de ninguém\n");
    fmt::print("    • acrescentar um componente exige entender o mediador inteiro\n");
    fmt::print("    • o mediador conhece detalhes INTERNOS dos componentes\n\n");

    fmt::print("  As saídas quando isso acontece:\n");
    fmt::print("    1. QUEBRAR em mediadores menores, por área de\n");
    fmt::print("       responsabilidade (o formulário de endereço tem o seu)\n");
    fmt::print("    2. um mapa evento→handler em vez do switch:\n");
    fmt::print("         std::map<std::string, std::function<void()>>\n");
    fmt::print("    3. extrair as REGRAS para objetos próprios, e deixar o\n");
    fmt::print("       mediador só roteando\n\n");

    fmt::print("  Repare que `revalidar()` neste demo já é a saída 3 em\n");
    fmt::print("  miniatura: a regra de validação está numa função separada, e\n");
    fmt::print("  `notificar` só decide QUANDO chamá-la.\n");
    fmt::print("  Ver src/solid/srp/ — é o mesmo raciocínio da God Class.\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║                Resumo do Mediator                        ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : centralizar a interação entre componentes   ║\n");
    fmt::print("║ Troca      : teia N×(N-1) por estrela N                  ║\n");
    fmt::print("║ Ganho      : os componentes ficam REUTILIZÁVEIS          ║\n");
    fmt::print("║ Ganho      : a regra de interação mora num lugar só      ║\n");
    fmt::print("║ vs Observer: aqui há uma REGRA que depende de vários     ║\n");
    fmt::print("║ Risco      : o mediador vira God Object                  ║\n");
    fmt::print("║ Saída      : quebrar por área, ou extrair as regras      ║\n");
    fmt::print("║ Aparece em : MVC/MVP, GUIs, orquestração de serviços     ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ mediator — todos os demos concluídos\n\n");

    return 0;
}
