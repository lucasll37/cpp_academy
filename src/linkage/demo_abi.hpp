// =============================================================================
//  src/linkage/demo_abi.hpp
//
//  ABI — o contrato binário
//  ────────────────────────
//  API é o contrato de CÓDIGO-FONTE: quais funções existem, com que
//  assinatura. Recompilar resolve qualquer quebra de API — o compilador
//  reclama e você conserta.
//
//  ABI é o contrato BINÁRIO: como os símbolos são nomeados, como os
//  argumentos são passados, qual o layout das classes, onde ficam os vptr.
//  Quebrar a ABI sem quebrar a API é o pior dos mundos: recompila, linka,
//  e corrompe memória em runtime.
//
//  Tópicos:
//  • O que faz parte da ABI (mais do que se imagina)
//  • ⚠ O que quebra a ABI sem quebrar a API
//  • Pimpl — o firewall de ABI
//  • Inline namespaces e versionamento
//  • Por que a std não conserta `unordered_map`
// =============================================================================
#pragma once
#include "widget.hpp"        // ← ver o cabeçalho do arquivo: por que existe
#include <fmt/core.h>
#include <string>

namespace demo_abi {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O que faz parte da ABI
// ─────────────────────────────────────────────────────────────────────────────
void demo_o_que_e() {
    fmt::print("\n── 5.1 O que faz parte da ABI ──\n");

    fmt::print("      Muito além dos nomes das funções:\n\n");
    fmt::print("        • o nome MANGLADO de cada símbolo (seção 4);\n");
    fmt::print("        • o sizeof e o LAYOUT de cada classe exposta;\n");
    fmt::print("        • a ordem das entradas na VTABLE;\n");
    fmt::print("        • a convenção de chamada (registradores vs pilha);\n");
    fmt::print("        • quem destrói cada argumento;\n");
    fmt::print("        • o formato das exceções e do unwinding;\n");
    fmt::print("        • o layout de TODO tipo da std que apareça na interface.\n\n");

    fmt::print("      Este último é o que pega: se a sua biblioteca tem\n");
    fmt::print("        void configura(const std::string& s);\n");
    fmt::print("      então o layout de `std::string` faz parte da SUA ABI. Ela\n");
    fmt::print("      não pode ser usada por um programa compilado com uma\n");
    fmt::print("      libstdc++ de layout diferente.\n\n");

    fmt::print("      Foi exatamente isso o \"grande rompimento\" do GCC 5, com o\n");
    fmt::print("      `_GLIBCXX_USE_CXX11_ABI`: `std::string` deixou de ser\n");
    fmt::print("      copy-on-write, o layout mudou, e todo o ecossistema teve\n");
    fmt::print("      que recompilar. O símbolo passou a carregar `__cxx11` no\n");
    fmt::print("      nome, justamente para que a incompatibilidade virasse um\n");
    fmt::print("      erro de LINKER em vez de corrupção silenciosa.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ O que quebra a ABI sem quebrar a API
// ─────────────────────────────────────────────────────────────────────────────
void demo_quebras() {
    fmt::print("\n── 5.2 ⚠ Quebra a ABI, não quebra a API ──\n");

    fmt::print("      | mudança                          | API | ABI |\n");
    fmt::print("      |----------------------------------|-----|-----|\n");
    fmt::print("      | acrescentar membro de DADO       | ok  | ✗   |\n");
    fmt::print("      | reordenar membros de dado        | ok  | ✗   |\n");
    fmt::print("      | acrescentar método VIRTUAL       | ok  | ✗   |\n");
    fmt::print("      | reordenar métodos virtuais       | ok  | ✗   |\n");
    fmt::print("      | tornar virtual um método comum   | ok  | ✗   |\n");
    fmt::print("      | mudar tipo de parâmetro          | ok* | ✗   |\n");
    fmt::print("      | acrescentar parâmetro com default| ok  | ✗   |\n");
    fmt::print("      | mudar um `enum` de int para char | ok  | ✗   |\n");
    fmt::print("      | tornar `inline` uma função       | ok  | ✗   |\n");
    fmt::print("      |----------------------------------|-----|-----|\n");
    fmt::print("      | acrescentar método NÃO virtual   | ok  | ok  |\n");
    fmt::print("      | acrescentar sobrecarga nova      | ok  | ok  |\n");
    fmt::print("      | mudar o CORPO de uma função      | ok  | ok  |\n");
    fmt::print("      | acrescentar membro `static`      | ok  | ok  |\n\n");

    fmt::print("      Repare em \"acrescentar parâmetro com default\". O código\n");
    fmt::print("      cliente continua compilando — mas o VALOR PADRÃO é gravado\n");
    fmt::print("      no lado do CHAMADOR, não da função. Um cliente antigo, não\n");
    fmt::print("      recompilado, chama um símbolo que não existe mais.\n\n");

    fmt::print("      E \"acrescentar método virtual\": a vtable muda de tamanho e\n");
    fmt::print("      os índices das entradas seguintes se deslocam. Um cliente\n");
    fmt::print("      antigo chamando `->f()` pega o slot errado — e executa\n");
    fmt::print("      outra função, com os argumentos errados.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Pimpl — o firewall de ABI
// ─────────────────────────────────────────────────────────────────────────────
void demo_pimpl() {
    fmt::print("\n── 5.3 Pimpl ──\n");

    fmt::print("      A classe pública tem UM membro: um ponteiro.\n");
    fmt::print("        sizeof(Widget) = {} bytes, e continuará {} para sempre,\n",
               sizeof(Widget), sizeof(Widget));
    fmt::print("        independentemente do que a Impl ganhe ou perca.\n\n");

    Widget w{"exemplo"};
    fmt::print("      w.descricao() = \"{}\"\n", w.descricao());
    w.renomear("renomeado");
    fmt::print("      após renomear = \"{}\"\n", w.descricao());

    fmt::print("\n      Os dois ganhos:\n");
    fmt::print("        (1) ABI ESTÁVEL — o layout público nunca muda;\n");
    fmt::print("        (2) BUILD RÁPIDO — o cabeçalho não precisa incluir nada\n");
    fmt::print("            do que a Impl usa. Ver a medição em 1.2.\n\n");

    fmt::print("      Os dois custos:\n");
    fmt::print("        (1) uma ALOCAÇÃO por objeto, e uma indireção por acesso;\n");
    fmt::print("        (2) o compilador não consegue inlinar através dela.\n\n");

    fmt::print("      ⚠  A armadilha do destrutor: `~Widget()` NÃO pode ser\n");
    fmt::print("         `= default` no cabeçalho. `unique_ptr<Impl>` precisa do\n");
    fmt::print("         tipo COMPLETO para destruir, e ali `Impl` é incompleta.\n");
    fmt::print("         O erro é críptico:\n");
    fmt::print("           static assertion failed: can't delete an incomplete type\n");
    fmt::print("         A solução: DECLARE no .hpp, `= default` no .cpp — depois\n");
    fmt::print("         da definição de Impl. O mesmo vale para o move.\n\n");

    fmt::print("      ⚠  A segunda armadilha custou um erro de linker na primeira\n");
    fmt::print("         versão deste arquivo. `Widget` estava declarada AQUI, e\n");
    fmt::print("         widget.cpp incluía demo_abi.hpp para alcançá-la — o que\n");
    fmt::print("         fez as funções `demo_*()`, que não são `inline`, serem\n");
    fmt::print("         definidas em DUAS TUs:\n");
    fmt::print("           multiple definition of `demo_abi::demo_o_que_e()'\n");
    fmt::print("         É o erro de 1.4, ao vivo. A correção foi separar\n");
    fmt::print("         widget.hpp: um cabeçalho com só o que precisa ser\n");
    fmt::print("         compartilhado, e nada definido.\n\n");

    fmt::print("      Quem usa: Qt (o macro Q_D/Q_DECLARE_PRIVATE é pimpl),\n");
    fmt::print("      praticamente toda biblioteca com compromisso de ABI.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Inline namespaces
// ─────────────────────────────────────────────────────────────────────────────
namespace biblioteca {
inline namespace v2 {
    inline const char* versao() { return "v2 (a atual)"; }
}
namespace v1 {
    inline const char* versao() { return "v1 (antiga, ainda acessível)"; }
}
}  // namespace biblioteca

void demo_inline_namespace() {
    fmt::print("\n── 5.4 Inline namespaces ──\n");

    fmt::print("      biblioteca::versao()      = \"{}\"\n", biblioteca::versao());
    fmt::print("      biblioteca::v1::versao()  = \"{}\"\n", biblioteca::v1::versao());
    fmt::print("      biblioteca::v2::versao()  = \"{}\"\n", biblioteca::v2::versao());

    fmt::print("\n      `inline namespace v2` faz os nomes de dentro aparecerem\n");
    fmt::print("      como se estivessem em `biblioteca` — mas o SÍMBOLO continua\n");
    fmt::print("      carregando o `v2` no mangling. Resultado:\n");
    fmt::print("        • o usuário escreve `biblioteca::versao()`, sem versão;\n");
    fmt::print("        • o binário liga a `_ZN11biblioteca2v26versaoEv`;\n");
    fmt::print("        • um binário antigo, que pedia `..2v1..`, continua\n");
    fmt::print("          encontrando o símbolo de v1 se ele ainda existir.\n\n");

    fmt::print("      Duas versões incompatíveis podem COEXISTIR no mesmo\n");
    fmt::print("      processo. É como a libstdc++ faz — `std::__cxx11::string`\n");
    fmt::print("      é exatamente isto.\n\n");

    fmt::print("      Também é o mecanismo dos literais de usuário do padrão:\n");
    fmt::print("      `std::literals::chrono_literals` é um inline namespace.\n");
    fmt::print("      Ver src/operators/demo_literais.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Por que a std não conserta o que está ruim
// ─────────────────────────────────────────────────────────────────────────────
void demo_std_travada() {
    fmt::print("\n── 5.5 A std travada pela própria ABI ──\n");

    fmt::print("      Todo mundo sabe que estes têm problema:\n\n");
    fmt::print("        std::unordered_map  → encadeamento por listas, um nó\n");
    fmt::print("                              alocado por elemento. Uma tabela\n");
    fmt::print("                              com open addressing é várias vezes\n");
    fmt::print("                              mais rápida (absl::flat_hash_map).\n");
    fmt::print("                              O padrão EXIGE estabilidade de\n");
    fmt::print("                              referências e buckets — e isso\n");
    fmt::print("                              está na interface, não no layout.\n\n");
    fmt::print("        std::regex          → ordens de grandeza mais lento que\n");
    fmt::print("                              RE2 ou PCRE (medido em\n");
    fmt::print("                              src/strings/demo_regex.hpp).\n\n");
    fmt::print("        std::deque (MSVC)   → blocos de 16 bytes; patológico.\n\n");
    fmt::print("        std::vector<bool>   → a especialização que todo mundo\n");
    fmt::print("                              lamenta (src/bits/demo_bitset.hpp).\n\n");

    fmt::print("      Consertar qualquer um muda o layout → quebra a ABI → toda\n");
    fmt::print("      biblioteca binária do planeta que exponha um desses na\n");
    fmt::print("      interface para de funcionar.\n\n");

    fmt::print("      As saídas em discussão no comitê:\n");
    fmt::print("        • `std2::` — um namespace novo, e a std antiga fica;\n");
    fmt::print("        • uma quebra coordenada num release marcado;\n");
    fmt::print("        • epochs / módulos com versão.\n");
    fmt::print("      Nada foi decidido. Enquanto isso, use as alternativas de\n");
    fmt::print("      terceiros nos pontos quentes e a std no resto.\n\n");

    fmt::print("      A lição para o SEU código: se você publica uma biblioteca\n");
    fmt::print("      binária, decida CEDO o que é ABI. Depois de publicado, a\n");
    fmt::print("      escolha é irreversível — pergunte à std.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_o_que_e();
    demo_quebras();
    demo_pimpl();
    demo_inline_namespace();
    demo_std_travada();
}

}  // namespace demo_abi
