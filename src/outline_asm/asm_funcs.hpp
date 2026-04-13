#pragma once

// ============================================================
// asm_funcs.hpp
//
// Declarações das funções implementadas em asm_funcs.s.
//
// Por que extern "C"?
//   C++ aplica "name mangling" nas funções: _Z8asm_somaiz,
//   _Z15asm_fatorialRi etc. O assembler .s não faz isso —
//   o símbolo exportado é o nome literal "asm_soma_int".
//   extern "C" desabilita o mangling e alinha os nomes.
//
// O header é opcional (você pode declarar direto no .cpp),
// mas centralizar aqui é a forma correta em projetos reais.
// ============================================================

#include <cstdint>

extern "C" {

// 1. Soma dois inteiros de 32 bits
int      asm_soma_int(int a, int b);

// 2. Soma dois inteiros de 64 bits
int64_t  asm_soma_int64(int64_t a, int64_t b);

// 3. Valor absoluto sem branch
int      asm_valor_absoluto(int n);

// 4. Fatorial iterativo (n <= 20 para não overflow)
int64_t  asm_fatorial(int n);

// 5. Máximo de dois inteiros (branchless via CMOV)
int      asm_maximo(int a, int b);

// 6. Popcount — conta bits 1 (usa instrução POPCNT)
int      asm_popcount(uint32_t x);

// 7. Strlen — comprimento de string com REPNE SCASB
int64_t  asm_strlen(const char* s);

// 8. Memcpy de N blocos de 64 bits com REP MOVSQ
void     asm_memcpy_64(void* dst, const void* src, int64_t n);

// 9. Soma todos os elementos de um array int32
int64_t  asm_soma_array(const int* arr, int n);

// 10. Teste de primalidade (trial division)
int      asm_eh_primo(int n);

} // extern "C"
