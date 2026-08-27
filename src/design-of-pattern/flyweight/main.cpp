// =============================================================================
//  src/design-of-pattern/flyweight/main.cpp
//
//  FLYWEIGHT — compartilhar o que é igual, passar o que é único
//  ────────────────────────────────────────────────────────────
//  O único padrão do catálogo cuja motivação é puramente de MEMÓRIA. E é o
//  único cujo benefício dá para MEDIR — que é o que este demo faz.
//
//  A divisão que define o padrão:
//    INTRÍNSECO — compartilhável, imutável   (o desenho da letra 'a')
//    EXTRÍNSECO — único, passado por parâmetro (onde essa 'a' aparece)
// =============================================================================
#include "documento.hpp"
#include "fabrica.hpp"
#include "glifo.hpp"

#include <fmt/core.h>

#include <string>

static void secao(const char* titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

// Um texto de tamanho realista, com repetição natural
static std::string gerar_texto(int repeticoes) {
    const std::string base =
        "O padrao Flyweight separa o estado intrinseco do extrinseco. ";
    std::string s;
    s.reserve(base.size() * static_cast<std::size_t>(repeticoes));
    for (int i = 0; i < repeticoes; ++i) s += base;
    return s;
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│             cpp_academy — Flyweight (GoF)                │\n");
    fmt::print("│   \"Use compartilhamento para suportar eficientemente     │\n");
    fmt::print("│    um grande número de objetos pequenos.\"       — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. A divisão do estado");

    fmt::print("  Um caractere na tela tem dois tipos de estado:\n\n");
    fmt::print("    INTRÍNSECO (compartilhável, imutável):\n");
    fmt::print("      • o caractere ('a')\n");
    fmt::print("      • a fonte (\"Arial\")\n");
    fmt::print("      • o tamanho (12pt)\n");
    fmt::print("      • o BITMAP rasterizado  ← é isto que pesa\n\n");
    fmt::print("    EXTRÍNSECO (único por instância):\n");
    fmt::print("      • a posição (x, y)\n");
    fmt::print("      • a cor\n\n");
    fmt::print("  Um documento com 100.000 caracteres precisa de 100.000\n");
    fmt::print("  POSIÇÕES, mas de apenas ~100 glifos distintos.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. A fábrica garante o compartilhamento");

    flyweight::Glifo::construcoes = 0;
    flyweight::FabricaDeGlifos fabrica;

    const auto& a1 = fabrica.obter('a', "Arial", 12);
    const auto& a2 = fabrica.obter('a', "Arial", 12);
    const auto& a3 = fabrica.obter('a', "Arial", 14);   // tamanho diferente
    const auto& b1 = fabrica.obter('b', "Arial", 12);

    fmt::print("  4 pedidos de glifo:\n");
    fmt::print("    obter('a', Arial, 12)  →  &{}\n", static_cast<const void*>(&a1));
    fmt::print("    obter('a', Arial, 12)  →  &{}   ← MESMO objeto\n",
               static_cast<const void*>(&a2));
    fmt::print("    obter('a', Arial, 14)  →  &{}   ← outro (tamanho difere)\n",
               static_cast<const void*>(&a3));
    fmt::print("    obter('b', Arial, 12)  →  &{}   ← outro\n",
               static_cast<const void*>(&b1));

    fmt::print("\n    a1 e a2 são o mesmo objeto? {}\n", &a1 == &a2);
    fmt::print("    glifos distintos criados : {}\n", fabrica.distintos());
    fmt::print("    construções de Glifo     : {}\n", flyweight::Glifo::construcoes);

    fmt::print("\n  A chave do mapa é a TUPLA do estado intrínseco inteiro:\n");
    fmt::print("    std::tuple<char, std::string, int>\n");
    fmt::print("  Mudar QUALQUER parte dela produz um glifo diferente — foi o\n");
    fmt::print("  que aconteceu com o tamanho 14.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. A economia, medida");

    const auto texto = gerar_texto(200);

    flyweight::Glifo::construcoes = 0;
    flyweight::FabricaDeGlifos f2;
    flyweight::Documento doc{f2};
    doc.escrever(texto, "Arial", 12);

    flyweight::DocumentoGordo gordo;
    gordo.escrever(texto, "Arial", 12);

    const auto bytes_fly   = doc.bytes_do_documento() + f2.bytes_totais();
    const auto bytes_gordo = gordo.bytes_totais();

    fmt::print("  documento de {} caracteres:\n\n", doc.caracteres());

    fmt::print("  COM flyweight:\n");
    fmt::print("    glifos distintos          : {:>10}\n", f2.distintos());
    fmt::print("    memória dos glifos        : {:>10} B\n", f2.bytes_totais());
    fmt::print("    memória do documento      : {:>10} B  ({} × {} B)\n",
               doc.bytes_do_documento(), doc.caracteres(),
               sizeof(flyweight::CaractereNaTela));
    fmt::print("    TOTAL                     : {:>10} B\n", bytes_fly);

    fmt::print("\n  SEM flyweight:\n");
    fmt::print("    objetos completos         : {:>10}\n", gordo.caracteres());
    fmt::print("    TOTAL                     : {:>10} B\n", bytes_gordo);

    fmt::print("\n  economia: {:.1f}× menos memória ({} B → {} B)\n",
               static_cast<double>(bytes_gordo) / static_cast<double>(bytes_fly),
               bytes_gordo, bytes_fly);

    fmt::print("\n  E o `sizeof` de cada elemento conta a história:\n");
    fmt::print("    sizeof(CaractereNaTela) = {:>3} B  (ponteiro + x,y + cor)\n",
               sizeof(flyweight::CaractereNaTela));
    fmt::print("    sizeof(CaractereGordo)  = {:>3} B  + as duas strings na heap\n",
               sizeof(flyweight::CaractereGordo));

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Renderizando — o extrínseco chega por parâmetro");

    fmt::print("  os 4 primeiros caracteres:\n      {}",
               doc.renderizar(4));

    fmt::print("\n  Repare na assinatura do glifo:\n");
    fmt::print("    std::string desenhar(int x, int y, const std::string& cor) const;\n\n");
    fmt::print("  O `x`, `y` e `cor` são PARÂMETROS — o glifo não os guarda.\n");
    fmt::print("  É essa assinatura que permite o compartilhamento: um objeto\n");
    fmt::print("  que guardasse a posição não poderia ser reusado.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. As armadilhas");

    fmt::print("  ⚠  1. O flyweight tem que ser IMUTÁVEL.\n");
    fmt::print("        Se um cliente pudesse modificá-lo, a mudança apareceria\n");
    fmt::print("        em todos os outros que compartilham. Por isso a fábrica\n");
    fmt::print("        devolve `const Glifo&`, e o glifo não tem setters.\n\n");

    fmt::print("  ⚠  2. O container da fábrica não pode INVALIDAR referências.\n");
    fmt::print("        Este demo usa `std::deque`, não `std::vector`:\n");
    fmt::print("          vector realoca ao crescer → as referências já\n");
    fmt::print("          entregues viram ponteiros pendurados\n");
    fmt::print("          deque nunca invalida referências ao crescer no fim\n");
    fmt::print("        (ver src/stl/demo_containers.hpp, 1.3)\n\n");

    fmt::print("  ⚠  3. Tempo de vida: a fábrica tem que sobreviver a TODOS os\n");
    fmt::print("        clientes. Se ela morrer antes, todos os ponteiros ficam\n");
    fmt::print("        pendurados. Aqui `fabrica` está no escopo externo de\n");
    fmt::print("        propósito.\n\n");

    fmt::print("  ⚠  4. Em código MULTITHREAD, a fábrica precisa de sincronização\n");
    fmt::print("        na inserção — duas threads pedindo o mesmo glifo novo ao\n");
    fmt::print("        mesmo tempo é uma corrida. Os glifos JÁ criados são\n");
    fmt::print("        seguros de ler concorrentemente, por serem imutáveis.\n\n");

    fmt::print("  ⚠  5. Não vale a pena se o estado intrínseco for PEQUENO.\n");
    fmt::print("        Um ponteiro custa 8 bytes. Compartilhar um objeto de 4\n");
    fmt::print("        bytes GASTA memória em vez de economizar.\n");
    fmt::print("        A conta: compensa se sizeof(intrínseco) > sizeof(ptr) e\n");
    fmt::print("        houver repetição real.\n");

    // ── 6 ────────────────────────────────────────────────────────────────────
    secao("6. Onde ele já está no seu código");

    fmt::print("  | mecanismo                | o que compartilha            |\n");
    fmt::print("  |--------------------------|------------------------------|\n");
    fmt::print("  | string interning          | strings iguais → um buffer   |\n");
    fmt::print("  | std::shared_ptr           | um objeto entre N donos      |\n");
    fmt::print("  | COW (copy-on-write)       | até alguém escrever          |\n");
    fmt::print("  | atlas de texturas (jogos) | um bitmap para N sprites     |\n");
    fmt::print("  | pool de conexões          | N conexões para M requisições|\n");
    fmt::print("  | tabela de símbolos        | um nó por identificador      |\n\n");

    fmt::print("  O SSO da `std::string` é o inverso interessante: ele evita a\n");
    fmt::print("  indireção para objetos PEQUENOS, exatamente pelo motivo da\n");
    fmt::print("  armadilha 5 (ver src/strings/demo_string.hpp, 1.1).\n");

    fmt::print("\n  E uma versão moderna do compartilhamento, sem fábrica:\n");
    fmt::print("    std::shared_ptr<const Glifo>\n");
    fmt::print("  Ganha a contagem de referência automática (a fábrica pode\n");
    fmt::print("  liberar glifos sem uso); perde a garantia de unicidade —\n");
    fmt::print("  nada impede criar dois `shared_ptr` para glifos idênticos.\n");
    fmt::print("  Por isso o padrão exige a FÁBRICA: é ela que garante que\n");
    fmt::print("  \"mesmo estado intrínseco\" implica \"mesmo objeto\".\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║               Resumo do Flyweight                        ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : compartilhar o intrínseco, passar o extrínseco║\n");
    fmt::print("║ Motivação  : MEMÓRIA — o único padrão puramente disso    ║\n");
    fmt::print("║ Exige      : uma FÁBRICA que garanta a unicidade         ║\n");
    fmt::print("║ Exige      : o flyweight ser IMUTÁVEL                    ║\n");
    fmt::print("║ Cuidado    : container que não invalide referências      ║\n");
    fmt::print("║ Cuidado    : a fábrica sobrevive a todos os clientes     ║\n");
    fmt::print("║ Não use se : o intrínseco for menor que um ponteiro      ║\n");
    fmt::print("║ Já existe  : interning, shared_ptr, COW, atlas de textura║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ flyweight — todos os demos concluídos\n\n");

    return 0;
}
