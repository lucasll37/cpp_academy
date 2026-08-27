// =============================================================================
//  src/meson/main.cpp
//  Ponto de entrada — o build se explicando a si próprio
//
//  Tudo o que este programa imprime chegou até aqui por uma via diferente:
//
//    build_info.hpp      ← configure_file()  : valores do build viraram macros
//    tabela_gerada.hpp   ← custom_target()   : um script gerou código C++
//    saudacao.hpp/.a     ← static_library()  : código compilado à parte e linkado
//    fmt                 ← dependency()      : biblioteca externa via pkg-config
//
//  Nenhum destes quatro arquivos existe no diretório de fontes já pronto para
//  uso: dois são gerados dentro de `build/src/meson/`, um vira `.a`, e o
//  quarto vem do Conan. O meson.build ao lado é o mapa dessas quatro rotas.
// =============================================================================
#include "build_info.hpp"       // gerado por configure_file()
#include "tabela_gerada.hpp"    // gerado por custom_target()
#include "saudacao.hpp"         // static_library() + declare_dependency()

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::turquoise) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · configure_file() — o build vira macro");

    fmt::print("      Estes valores foram escritos pelo Meson em build_info.hpp,\n");
    fmt::print("      a partir do template build_info.hpp.in:\n\n");
    fmt::print("        projeto      : {} {}\n", BUILD_PROJETO_NOME, BUILD_PROJETO_VERSAO);
    fmt::print("        buildtype    : {}\n",    BUILD_TIPO);
    fmt::print("        cpp_std      : {}\n",    BUILD_PADRAO_CPP);
    fmt::print("        warning_level: {}\n",    BUILD_NIVEL_WARNING);
    fmt::print("        optimization : {}\n",    BUILD_OTIMIZACAO);
    fmt::print("        prefix       : {}\n",    BUILD_PREFIX);
    fmt::print("        compilador   : {} {}\n", BUILD_COMPILADOR_ID, BUILD_COMPILADOR_VER);
    fmt::print("        máquina      : {} / {}\n", BUILD_MAQUINA_CPU, BUILD_MAQUINA_SO);

    fmt::print("\n      Recompile com outro buildtype e este bloco muda sozinho:\n");
    fmt::print("        meson configure build -Dbuildtype=release && meson compile -C build\n");

    section("2 · #mesondefine — opção do build no pré-processador");

    fmt::print("      A opção `test_option` vive em meson_options.txt (na raiz) e\n");
    fmt::print("      atravessou até aqui como um #define condicional:\n\n");
#ifdef COM_TEST_OPTION
    fmt::print("        COM_TEST_OPTION está DEFINIDA  → -Dtest_option=true\n");
#else
    fmt::print("        COM_TEST_OPTION NÃO está definida → -Dtest_option=false (padrão)\n");
#endif
    fmt::print("\n        experimente: meson configure build -Dtest_option=true\n");
    fmt::print("                     meson compile -C build meson_demo\n");

    fmt::print("\n      E a mesma ideia com `set10()`, que dá 0/1 em vez de definir/undef —\n");
    fmt::print("      útil porque funciona com `if constexpr` em vez de `#ifdef`:\n");
    if constexpr (BUILD_TEM_DOXYGEN != 0) {
        fmt::print("        doxygen encontrado na configuração deste build\n");
    } else {
        fmt::print("        doxygen NÃO encontrado na configuração deste build\n");
    }

    section("3 · custom_target() — código gerado no build");

    fmt::print("      O header abaixo não existe no repositório: o Ninja chamou\n");
    fmt::print("      `gerar_tabela.py tabela.txt tabela_gerada.hpp` porque o\n");
    fmt::print("      custom_target declarou essa dependência.\n\n");

    for (const auto& c : tabela_gerada::comandos) {
        fmt::print("        meson {:<11} {}\n", c.nome, c.descricao);
    }

    fmt::print("\n      O Meson contou {} itens no mesmo arquivo em tempo de CONFIGURAÇÃO\n",
               BUILD_QTD_ITENS_TABELA);
    fmt::print("      (com o módulo `fs`) e o script os gerou em tempo de BUILD.\n");
    fmt::print("      Os dois números batem: {} == {}.\n",
               BUILD_QTD_ITENS_TABELA, tabela_gerada::comandos.size());
    fmt::print("\n      Edite tabela.txt, rode `meson compile -C build` e veja o\n");
    fmt::print("      gerador rodar de novo — sem você pedir. É o grafo funcionando.\n");

    section("4 · static_library() — código de outro alvo");

    fmt::print("      {}\n", saudacao::cumprimentar("mundo"));
    fmt::print("      tamanho_da_saudacao(\"mundo\") = {}\n",
               saudacao::tamanho_da_saudacao("mundo"));
    fmt::print("\n      A mesma libsaudacao.a é linkada pelo teste `test_saudacao`.\n");
    fmt::print("      Compilada uma vez, usada duas — é isso que `declare_dependency()`\n");
    fmt::print("      empacota: link_with + include_directories + dependências, num objeto só.\n");

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Guia do Meson: exemplo executado com sucesso!\n\n");
}
