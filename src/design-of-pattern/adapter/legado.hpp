#pragma once
// =============================================================================
//  legado.hpp — a biblioteca que você NÃO pode mudar
//
//  Este é o cenário que dá sentido ao Adapter: código de terceiros, ou legado,
//  cuja interface não é a que o seu sistema espera. Você não pode editá-lo —
//  ou não deve, porque a próxima versão sobrescreveria.
//
//  Repare nas diferenças em relação ao que o sistema novo quer:
//    • nomes em inglês, estilo C
//    • devolve códigos de erro em vez de exceções ou expected
//    • trabalha com `char*` e tamanho, não com std::string
//    • exige inicializar/finalizar explicitamente
// =============================================================================
#include <cstddef>

namespace adapter::legado {

// Uma API típica de biblioteca C: struct opaco + funções livres
struct LegacyLogger;

enum LegacyResult {
    LEGACY_OK          = 0,
    LEGACY_NOT_OPEN    = 1,
    LEGACY_BUFFER_FULL = 2,
    LEGACY_BAD_LEVEL   = 3,
};

LegacyLogger* legacy_logger_create(const char* name);
void          legacy_logger_destroy(LegacyLogger* h);

// Níveis como int mágico: 0=debug, 1=info, 2=warn, 3=error
LegacyResult  legacy_logger_write(LegacyLogger* h, int level,
                                  const char* msg, std::size_t len);

LegacyResult  legacy_logger_flush(LegacyLogger* h);
std::size_t   legacy_logger_count(const LegacyLogger* h);
const char*   legacy_result_string(LegacyResult r);

}  // namespace adapter::legado
