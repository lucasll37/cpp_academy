// =============================================================================
//  src/cmake/demo_geracao.hpp
//
//  Protobuf: código gerado durante o build
//  ───────────────────────────────────────
//  O assunto não é o Protobuf — é o padrão "um arquivo que não é C++ vira
//  C++ antes da compilação". Vale igual para Qt (moc/uic), FlatBuffers,
//  gRPC, Cap'n Proto, bison/flex e geradores caseiros.
//
//  O CMake precisa de três coisas, e errar qualquer uma dá um build que
//  funciona hoje e quebra amanhã:
//    1. saber que `medida.pb.cc` DEPENDE de `medida.proto`;
//    2. rodar o gerador quando a entrada mudar, e SÓ então;
//    3. pôr a saída onde o alvo consiga incluí-la.
//
//  Tópicos:
//  • add_custom_command(OUTPUT ...) — a forma certa
//  • ⚠ a confusão com add_custom_command(TARGET ...)
//  • O Protobuf em si, e o que ele resolve
//  • ⚠ A armadilha de versão entre protoc e libprotobuf
// =============================================================================
#pragma once
#include "config.hpp"

#include <cstdio>
#include <string>

#if TEM_PROTOBUF
#  include "medida.pb.h"
#endif

namespace demo_geracao {

inline void demo_regra() {
    std::printf("\n── 3.1 A regra de geração ──\n");

    std::printf("        add_custom_command(\n");
    std::printf("            OUTPUT  ${PROTO_CC} ${PROTO_H}\n");
    std::printf("            COMMAND protobuf::protoc\n");
    std::printf("                    --cpp_out=${PROTO_GERADO_DIR}\n");
    std::printf("                    -I${CMAKE_CURRENT_SOURCE_DIR}\n");
    std::printf("                    ${PROTO_FONTE}\n");
    std::printf("            DEPENDS ${PROTO_FONTE}      ← a dependência\n");
    std::printf("            VERBATIM)                   ← escapa caminhos\n");
    std::printf("        add_library(medida_proto STATIC ${PROTO_CC})\n\n");

    std::printf("      O que isso declara: \"estas SAÍDAS vêm destas ENTRADAS,\n");
    std::printf("      por este COMANDO\". O ninja/make passa a tratar os .pb.cc\n");
    std::printf("      como qualquer outro arquivo com dependência: toca no\n");
    std::printf("      .proto → regera; não toca → não regera.\n\n");

    std::printf("      ⚠ A CONFUSÃO CLÁSSICA: existe uma segunda forma,\n");
    std::printf("          add_custom_command(TARGET x POST_BUILD COMMAND ...)\n");
    std::printf("        que é OUTRA COISA — roda depois de um alvo, SEMPRE, sem\n");
    std::printf("        rastrear dependência nenhuma. Ela serve para copiar um\n");
    std::printf("        arquivo ou assinar um binário. Usá-la para GERAR código\n");
    std::printf("        produz um build que regenera tudo toda vez, ou que não\n");
    std::printf("        regenera nunca — depende da ordem dos alvos.\n\n");

    std::printf("      ⚠ `VERBATIM` não é opcional. Sem ele, um caminho com\n");
    std::printf("        espaço (C:/Program Files/...) vira dois argumentos.\n\n");

    std::printf("      ⚠ A saída vai para o diretório de BUILD, nunca para o de\n");
    std::printf("        fonte. Gerar dentro do fonte suja o `git status`, quebra\n");
    std::printf("        builds paralelos e cria conflito entre Debug e Release.\n");
    std::printf("        Daí o `target_include_directories(... ${PROTO_GERADO_DIR})`.\n");
}

inline void demo_protobuf() {
    std::printf("\n── 3.2 O Protobuf em si ──\n");

#if TEM_PROTOBUF
    // O schema está em medida.proto. Este código usa a classe GERADA.
    academy::cmake::Relatorio relatorio;
    relatorio.set_maquina(std::string{config::kSistema} + "/" +
                          config::kProcessador);

    auto* m1 = relatorio.add_medidas();
    m1->set_tecnologia("serial");
    m1->set_elementos(20'000'000);
    m1->set_milissegundos(7.01);
    m1->set_unidades(1);
    m1->set_erro_maximo(0.0);

    auto* m2 = relatorio.add_medidas();
    m2->set_tecnologia("openmp");
    m2->set_elementos(20'000'000);
    m2->set_milissegundos(5.50);
    m2->set_unidades(24);
    m2->set_erro_maximo(0.0);

    std::printf("      Um Relatorio com %d medida(s), construído a partir da\n",
                relatorio.medidas_size());
    std::printf("      classe que o protoc gerou:\n");
    for (const auto& m : relatorio.medidas())
        std::printf("        %-8s %10llu elementos  %7.2f ms  %2u unidade(s)\n",
                    m.tecnologia().c_str(),
                    static_cast<unsigned long long>(m.elementos()),
                    m.milissegundos(), m.unidades());

    // Serialização binária e volta
    std::string bytes;
    relatorio.SerializeToString(&bytes);

    academy::cmake::Relatorio recuperado;
    const bool ok = recuperado.ParseFromString(bytes);

    std::printf("\n      serializado  : %zu bytes\n", bytes.size());
    std::printf("      desserializado: %s, %d medida(s)\n",
                ok ? "ok" : "FALHOU", recuperado.medidas_size());
    std::printf("      texto (debug) : %zu bytes\n",
                relatorio.DebugString().size());

    std::printf("\n      Repare no tamanho: %zu bytes binários contra %zu de\n",
                bytes.size(), relatorio.DebugString().size());
    std::printf("      texto. O binário é compacto porque cada campo é\n");
    std::printf("      identificado por um NÚMERO (o `= 1`, `= 2` do .proto),\n");
    std::printf("      não pelo nome — e campos com valor padrão nem são\n");
    std::printf("      escritos.\n\n");

    std::printf("      É essa numeração que dá a COMPATIBILIDADE: acrescentar um\n");
    std::printf("      campo novo com número novo não quebra quem lê a versão\n");
    std::printf("      antiga (ele ignora o que não conhece).\n");
    std::printf("      ⚠ E é por isso que REUTILIZAR um número já usado é o\n");
    std::printf("        pecado capital do Protobuf: dados antigos passam a ser\n");
    std::printf("        lidos como o campo errado. Use `reserved 3, 7;`.\n");
#else
    std::printf("      ⚠ Protobuf não foi encontrado nesta configuração.\n");
    std::printf("        O alvo `medida_proto` não foi criado e este demo está\n");
    std::printf("        desligado. Ver o resumo impresso pelo `cmake`.\n");
#endif
}

inline void demo_armadilhas() {
    std::printf("\n── 3.3 ⚠ As armadilhas do Protobuf ──\n");

    std::printf("      (a) VERSÃO do `protoc` diferente da `libprotobuf`. O\n");
    std::printf("          código gerado por um protoc novo não compila contra\n");
    std::printf("          uma lib antiga, e o erro é críptico:\n");
    std::printf("            \"This file was generated by a newer version of\n");
    std::printf("             protoc which is incompatible with your\n");
    std::printf("             Protocol Buffer headers\"\n");
    std::printf("          A defesa é usar `protobuf::protoc` (o alvo IMPORTADO\n");
    std::printf("          que vem junto do ProtobufConfig.cmake) em vez de um\n");
    std::printf("          `protoc` achado no PATH — é o que este CMakeLists faz.\n\n");

    std::printf("      (b) O código gerado dispara AVISOS que você não pode\n");
    std::printf("          corrigir. Suprima no alvo dele, não no projeto:\n");
    std::printf("            target_compile_options(medida_proto PRIVATE -w)\n");
    std::printf("          (é o mesmo raciocínio dos #pragma locais em\n");
    std::printf("           src/layout/ e src/linkage/)\n\n");

    std::printf("      (c) Protobuf 4.x (o \"22.x\" em diante) passou a EXIGIR\n");
    std::printf("          Abseil e C++17, e mudou o esquema de versionamento.\n");
    std::printf("          Um `find_package(Protobuf)` que funcionava pode parar\n");
    std::printf("          de funcionar numa atualização.\n\n");

    std::printf("      (d) `proto2` vs `proto3`: no proto3 todo campo escalar tem\n");
    std::printf("          valor PADRÃO e não há como distinguir \"zero\" de \"não\n");
    std::printf("          informado\" — a menos que se use `optional` (que\n");
    std::printf("          voltou no 3.15) ou um wrapper. Isso já causou muito\n");
    std::printf("          bug de \"o campo sumiu\".\n");
    std::printf("          É o mesmo problema que `std::optional` resolve em C++;\n");
    std::printf("          ver src/vocabulary_types/demo_optional.hpp.\n");
}

inline void run() {
    demo_regra();
    demo_protobuf();
    demo_armadilhas();
}

}  // namespace demo_geracao
