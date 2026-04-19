# parser — Parser de Expressões Aritméticas com Flex/Bison

## O que é este projeto?

Implementa um **parser de expressões aritméticas** usando as ferramentas clássicas de compiladores: **Flex** (analisador léxico) e **Bison** (gerador de parser). Aceita entrada interativa ou arquivo e avalia expressões como `2 + 3 * (4 - 1)`.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Análise léxica (tokenização) | Flex transforma texto em tokens |
| Análise sintática (parsing) | Bison verifica gramática e constrói AST |
| Gramática BNF | Regras formais de uma linguagem |
| Precedência de operadores | Como `*` tem prioridade sobre `+` |
| Associatividade | Como `a - b - c` é avaliado (esquerda para direita) |

---

## Estrutura de arquivos

```
parser/
├── main.cpp       ← ponto de entrada, chama yyparse()
├── lexer.l        ← definição do analisador léxico (Flex)
├── parser.y       ← gramática e ações semânticas (Bison)
└── meson.build
```

---

## O que Flex faz — análise léxica

Flex lê o arquivo `.l` e gera um arquivo C com a função `yylex()` que transforma fluxo de caracteres em **tokens**:

```
Entrada: "2 + 3 * (4 - 1)\n"

Tokens produzidos:
  NUMBER(2)  PLUS  NUMBER(3)  TIMES  LPAREN  NUMBER(4)  MINUS  NUMBER(1)  RPAREN  NEWLINE
```

```lex
/* lexer.l */
%{
#include "parser.tab.h"  /* tokens gerados pelo Bison */
%}

%%

[0-9]+(\.[0-9]+)?   { yylval.dval = atof(yytext); return NUMBER; }
"+"                 { return PLUS; }
"-"                 { return MINUS; }
"*"                 { return TIMES; }
"/"                 { return DIVIDE; }
"("                 { return LPAREN; }
")"                 { return RPAREN; }
"\n"                { return NEWLINE; }
[ \t]               { /* ignora espaços */ }
.                   { fprintf(stderr, "caractere inválido: %c\n", *yytext); }

%%
```

---

## O que Bison faz — análise sintática

Bison lê o arquivo `.y` e gera um parser LALR(1) que verifica se os tokens seguem a gramática e executa **ações semânticas**:

```bison
/* parser.y */
%union {
    double dval;
}

%token <dval> NUMBER
%token PLUS MINUS TIMES DIVIDE LPAREN RPAREN NEWLINE

%type <dval> expr

/* Precedência: PLUS/MINUS menor que TIMES/DIVIDE */
/* (declaradas de baixo para cima: menor prioridade primeiro) */
%left PLUS MINUS
%left TIMES DIVIDE
%right UMINUS      /* unário, maior precedência */

%%

linha : expr NEWLINE  { printf("= %g\n", $1); }
      | NEWLINE
      ;

/* Gramática recursiva — cada regra tem uma ação semântica */
expr : expr PLUS   expr  { $$ = $1 + $3; }  /* $$ = valor desta regra */
     | expr MINUS  expr  { $$ = $1 - $3; }  /* $1, $3 = valores filhos */
     | expr TIMES  expr  { $$ = $1 * $3; }
     | expr DIVIDE expr  { $$ = $1 / $3; }  /* TODO: divisão por zero */
     | MINUS expr %prec UMINUS { $$ = -$2; }  /* negação unária */
     | LPAREN expr RPAREN      { $$ = $2; }   /* parênteses */
     | NUMBER                  { $$ = $1; }
     ;

%%
```

---

## Gramática BNF — como funciona a precedência

Sem declaração de precedência, `2 + 3 * 4` seria ambíguo:

```
Opção A: (2 + 3) * 4 = 20
Opção B:  2 + (3 * 4) = 14  ← matematicamente correto
```

Bison resolve isso com declarações `%left` e `%right`:

```bison
%left PLUS MINUS      /* menor precedência */
%left TIMES DIVIDE    /* maior precedência */
```

Isso faz com que `*` tenha precedência sobre `+`, produzindo a interpretação matemática correta.

---

## O `main.cpp`

```cpp
extern "C" {
    int yyparse();    // função gerada pelo Bison
    extern FILE* yyin; // entrada do lexer (padrão: stdin)
}

int main(int argc, char* argv[]) {
    printf("=== Parser de Expressões Aritméticas ===\n");

    if (argc > 1) {
        yyin = fopen(argv[1], "r"); // lê de arquivo
    }
    // Sem argumento: lê de stdin (modo interativo)

    return yyparse(); // inicia o parsing
}
```

---

## Exemplos de uso

```bash
# Modo interativo
./bin/parser
2 + 3 * (4 - 1)
= 11
-5 + 2
= -3
10 / (2 + 3)
= 2

# Modo arquivo
echo "2 + 3 * 4" > expr.txt
./bin/parser expr.txt
= 14
```

---

## Processo de build

```
lexer.l   → flex  → lex.yy.c
parser.y  → bison → parser.tab.c + parser.tab.h
                         │
main.cpp ────────────────┤
                         └── g++ -o parser → executável
```

O Meson automatiza este processo com `custom_target` para flex e bison.

---

## Extensões possíveis

- Adicionar variáveis (`x = 5; x + 1`)
- Funções (`sin(x)`, `sqrt(x)`)
- Construir uma AST (Abstract Syntax Tree) para análise posterior
- Gerar código (compilador simples)

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja parser
./bin/parser
```

**Requer**: `flex` e `bison` instalados no sistema.

```bash
sudo apt install flex bison
```
