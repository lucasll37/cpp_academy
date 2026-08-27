// =============================================================================
//  src/testing/demo_dubles.hpp
//
//  Dublês de teste e a testabilidade do desenho
//  ────────────────────────────────────────────
//  A parte mais valiosa deste subprojeto não é o gmock. É perceber que
//  CÓDIGO DIFÍCIL DE TESTAR É CÓDIGO MAL DESENHADO — e que o teste é o
//  primeiro cliente a reclamar.
//
//  Tópicos:
//  • Os cinco dublês
//  • Injeção de dependência: a razão de o Carrinho ser testável
//  • ⚠ Os padrões que impedem o teste
//  • Quando mockar e quando não
// =============================================================================
#pragma once
#include <fmt/core.h>

namespace demo_dubles {

void demo_taxonomia() {
    fmt::print("\n── 4.1 Os cinco dublês ──\n");

    fmt::print("      | dublê | o que faz                        | verifica    |\n");
    fmt::print("      |-------|----------------------------------|-------------|\n");
    fmt::print("      | dummy | só preenche um parâmetro         | nada        |\n");
    fmt::print("      | stub  | devolve respostas prontas        | ESTADO      |\n");
    fmt::print("      | spy   | um stub que anota o que recebeu  | os dois     |\n");
    fmt::print("      | mock  | tem EXPECTATIVAS programadas     | INTERAÇÃO   |\n");
    fmt::print("      | fake  | implementação real, simplificada | ESTADO      |\n\n");

    fmt::print("      Neste subprojeto:\n");
    fmt::print("        `PrecosFixos` (test_basico.cpp)     → stub\n");
    fmt::print("        `FakeRepositorio` (test_mock.cpp)   → fake (map em memória)\n");
    fmt::print("        `MockPrecos` (test_mock.cpp)        → mock\n\n");

    fmt::print("      A escolha certa quase sempre é FAKE ou STUB. O mock existe\n");
    fmt::print("      para uma pergunta específica: \"o código chamou X?\" — e essa\n");
    fmt::print("      pergunta só importa quando a chamada é o EFEITO que o\n");
    fmt::print("      código produz (enviou o e-mail, gravou o log, publicou a\n");
    fmt::print("      mensagem). Se o efeito é observável no estado, verifique o\n");
    fmt::print("      estado.\n");
}

void demo_injecao() {
    fmt::print("\n── 4.2 Injeção de dependência ──\n");

    fmt::print("      Por que o `Carrinho` é testável em microssegundos, sem\n");
    fmt::print("      banco de dados? Por causa de UMA linha:\n\n");
    fmt::print("        explicit Carrinho(const RepositorioDePrecos& precos);\n\n");
    fmt::print("      A dependência entra por uma INTERFACE, pelo construtor.\n");
    fmt::print("      Compare com o que quase todo mundo escreve primeiro:\n\n");
    fmt::print("        class Carrinho {{\n");
    fmt::print("            BancoDeDados db_{{\"localhost:5432\"}};   // ❌\n");
    fmt::print("        }};\n\n");
    fmt::print("      Essa classe SÓ pode ser testada com um Postgres rodando.\n");
    fmt::print("      O teste deixa de ser unitário, passa a levar segundos,\n");
    fmt::print("      falha quando alguém mexe no banco, e não roda no CI.\n\n");

    fmt::print("      Não é sobre \"usar interfaces em tudo\". É sobre inverter\n");
    fmt::print("      QUEM decide a dependência concreta: não a classe, e sim\n");
    fmt::print("      quem a constrói. É o D de SOLID — src/solid/dip/.\n\n");

    fmt::print("      As três formas, em ordem de preferência:\n");
    fmt::print("        1. parâmetro de FUNÇÃO — o menor acoplamento possível;\n");
    fmt::print("        2. parâmetro de CONSTRUTOR — quando a dependência é\n");
    fmt::print("           usada por vários métodos (o caso do Carrinho);\n");
    fmt::print("        3. parâmetro de TEMPLATE — despacho estático, sem vtable\n");
    fmt::print("           nem alocação; ver src/templates/ e src/type_erasure/.\n");
}

void demo_impedimentos() {
    fmt::print("\n── 4.3 ⚠ O que impede o teste ──\n");

    fmt::print("      (a) SINGLETON. `Config::instancia()` amarra o código a UMA\n");
    fmt::print("          configuração global, compartilhada entre testes. Dois\n");
    fmt::print("          testes com configs diferentes passam a depender da\n");
    fmt::print("          ORDEM. Ver a discussão em\n");
    fmt::print("          src/design-of-pattern/singleton/.\n\n");

    fmt::print("      (b) `new` DENTRO da classe. Se o objeto testado constrói a\n");
    fmt::print("          própria dependência, não há como substituí-la.\n\n");

    fmt::print("      (c) O RELÓGIO. `std::chrono::system_clock::now()` dentro da\n");
    fmt::print("          lógica torna impossível testar \"o que acontece na\n");
    fmt::print("          virada do mês\". Injete o relógio:\n");
    fmt::print("            template <class Clock = std::chrono::system_clock>\n");
    fmt::print("          ou passe o `time_point` como parâmetro. Ver src/chrono/.\n\n");

    fmt::print("      (d) FUNÇÕES ESTÁTICAS LIVRES que fazem IO. Não há o que\n");
    fmt::print("          substituir — não existe ponto de extensão.\n\n");

    fmt::print("      (e) MÉTODOS PRIVADOS que você \"precisa\" testar. É sintoma:\n");
    fmt::print("          ou o comportamento é alcançável pela interface pública\n");
    fmt::print("          (e é por ela que se testa), ou aquilo é outra classe\n");
    fmt::print("          querendo nascer.\n");
    fmt::print("          ⚠ `#define private public` antes do include \"funciona\"\n");
    fmt::print("            e é UB — muda o layout, viola a ODR (ver\n");
    fmt::print("            src/linkage/demo_odr.hpp) e mascara o problema real.\n");
}

void demo_quando() {
    fmt::print("\n── 4.4 Quando mockar ──\n");

    fmt::print("      MOCKE quando a dependência:\n");
    fmt::print("        • é lenta (rede, disco, banco);\n");
    fmt::print("        • não é determinística (relógio, aleatório, UUID);\n");
    fmt::print("        • tem efeito colateral IRREVERSÍVEL (enviar e-mail,\n");
    fmt::print("          cobrar cartão, publicar mensagem);\n");
    fmt::print("        • é difícil de levar a um estado de erro (disco cheio,\n");
    fmt::print("          timeout de rede) — e esse caminho precisa de teste;\n");
    fmt::print("        • ainda não existe.\n\n");

    fmt::print("      NÃO MOCKE:\n");
    fmt::print("        • tipos de VALOR (uma struct, um std::string) — use o\n");
    fmt::print("          objeto de verdade;\n");
    fmt::print("        • a biblioteca padrão;\n");
    fmt::print("        • código seu que é rápido e determinístico;\n");
    fmt::print("        • tipos que você não possui — mocke a SUA camada de\n");
    fmt::print("          adaptação em volta deles, não a API de terceiros.\n");
    fmt::print("          (\"don't mock what you don't own\")\n\n");

    fmt::print("      A regra de bolso: um teste com mais de dois ou três\n");
    fmt::print("      EXPECT_CALL está testando implementação. Ele vai falhar na\n");
    fmt::print("      próxima refatoração correta — e um teste que falha quando\n");
    fmt::print("      nada quebrou treina a equipe a ignorar a suíte.\n");
}

inline void run() {
    demo_taxonomia();
    demo_injecao();
    demo_impedimentos();
    demo_quando();
}

}  // namespace demo_dubles
