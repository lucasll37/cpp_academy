/* =============================================================================
 *  src/linkage/c_api.c
 *
 *  Este arquivo é compilado pelo compilador C (`cc`), não pelo `c++`.
 *  O Meson decide isso pela extensão .c — veja o meson.build.
 *
 *  É por isso que o experimento vale: os símbolos aqui são gerados por um
 *  compilador que não sabe o que é sobrecarga, namespace ou template, e
 *  portanto não mangla nada.
 * ========================================================================== */
#include "c_api.h"

int soma_c(int a, int b) { return a + b; }

const char* nome_da_linguagem_c(void) { return "C (compilado por cc)"; }

size_t tamanho_de_int_em_c(void) { return sizeof(int); }

int aplica_c(transformador_t f, int v) { return f(v); }
