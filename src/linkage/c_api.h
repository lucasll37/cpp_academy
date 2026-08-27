/* =============================================================================
 *  src/linkage/c_api.h
 *
 *  Um cabeçalho que serve para C e para C++ ao mesmo tempo.
 *  ────────────────────────────────────────────────────────
 *  O idioma abaixo é o que toda biblioteca C respeitável tem no topo. Sem
 *  ele, um programa C++ que incluísse este arquivo geraria chamadas com
 *  nomes MANGLADOS (`_Z8soma_cii`) e o linker não acharia o símbolo que o
 *  compilador C gerou (`soma_c`).
 *
 *  `__cplusplus` só é definido pelo compilador C++ — é assim que o mesmo
 *  arquivo se adapta aos dois.
 * ========================================================================== */
#ifndef CPP_ACADEMY_C_API_H
#define CPP_ACADEMY_C_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compilado por um compilador C DE VERDADE (c_api.c). */
int         soma_c(int a, int b);
const char* nome_da_linguagem_c(void);
size_t      tamanho_de_int_em_c(void);

/* Um callback: o C chama de volta uma função que o C++ forneceu.
 * A função do C++ precisa ter linkage C para que o ponteiro seja compatível. */
typedef int (*transformador_t)(int);
int aplica_c(transformador_t f, int v);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CPP_ACADEMY_C_API_H */
