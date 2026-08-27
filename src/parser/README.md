# parser — De texto a valor: um interpretador de expressões com Flex e Bison

## O que é este projeto?

Um programa que lê a **string** `2 + 3 * 4` e imprime o **número** `14`. Entre uma coisa e
outra existem duas etapas de qualquer compilador — **análise léxica** (quebrar o texto em
pedaços com significado) e **análise sintática** (descobrir a estrutura desses pedaços) — e
duas ferramentas que geram esse código para você: **Flex** e **Bison**.

```bash
make build                                 # gera dist/bin/parser
./dist/bin/parser src/parser/example.txt   # modo arquivo
./dist/bin/parser                          # modo interativo (Ctrl+D encerra)
```

O executável chama-se **`parser`** — veja o `executable('parser', ...)` em
[meson.build](meson.build) — e é instalado em `dist/bin/parser`.

---

## Antes de começar: os conceitos que você precisa

Nada aqui presume mais do que C++ básico.

### Caractere, lexema e token

O arquivo de entrada, para o sistema operacional, é só uma sequência de bytes: `'2' ' ' '+'
' ' '3' ' ' '*' ' ' '4' '\n'`. Ninguém calcula nada a partir disso. O primeiro passo é
agrupar caracteres em **lexemas** — trechos contíguos que formam uma unidade — e classificar
cada lexema. O resultado é um **token**.

> **Token** = um par **(tipo, valor opcional)**. O *tipo* diz "isto é um número", "isto é o
> operador de soma". O *valor* é o dado que sobrou depois de classificar — só alguns tipos
> têm valor.

```
texto:    "2"        " "     "+"     " "     "3"       " "  "*"      "4"     "\n"
token:  NUMBER(2)   (nada)   PLUS   (nada)  NUMBER(3) (nada) TIMES  NUMBER(4) NEWLINE
```

Três coisas a notar: `NUMBER` carrega um valor (`2.0`), `PLUS` não carrega nada — saber que é
um `+` já é a informação toda; os espaços **desaparecem**, pois só separavam lexemas; o `\n`
**não** desaparece, porque neste projeto ele marca o fim de uma expressão.

O programa que faz isso é o **analisador léxico** (*scanner*, *lexer*). É o que o **Flex**
gera a partir de [lexer.l](lexer.l).

### Gramática, terminal, não-terminal, produção

A fita de tokens ainda não basta: `NUMBER PLUS NUMBER` faz sentido, `PLUS PLUS RPAREN` não.
Quem decide é uma **gramática** — um conjunto finito de regras que descreve todas as
sequências válidas.

| Termo | Significado | Neste projeto |
|---|---|---|
| **terminal** | um token; folha da árvore, não se expande | `NUMBER`, `PLUS`, `LPAREN` |
| **não-terminal** | um nome para "um pedaço de estrutura"; se expande | `expr`, `term`, `factor` |
| **produção** | regra "este não-terminal pode ser isto" | `expr : expr PLUS term` |
| **símbolo inicial** | o não-terminal por onde a análise começa | `input` |

Uma produção em Bison se lê assim:

```
factor : NUMBER               ← alternativa 1
       | LPAREN expr RPAREN   ← alternativa 2   ('|' significa "ou")
       | MINUS factor         ← alternativa 3
       ;                      ← o ponto-e-vírgula fecha a regra
```

Em português: *"um `factor` é um `NUMBER`, **ou** um `expr` entre parênteses, **ou** um menos
seguido de outro `factor`"*. Essa notação — nome, dois-pontos, alternativas com `|`,
ponto-e-vírgula — é a **BNF** (*Backus–Naur Form*), de 1959.

Repare que `factor` aparece **dentro da própria definição**: isso é **recursão**, e é o que dá
poder infinito a um conjunto finito de regras. Com três linhas você descreve `-5`, `--5`,
`---5`, sem limite.

### Análise ascendente: pilha, *shift* e *reduce*

Bison gera parsers **ascendentes** (*bottom-up*): em vez de partir do símbolo inicial
adivinhando, parte dos tokens e vai **juntando** pedaços até sobrar só o símbolo inicial. O
mecanismo é uma **pilha** e duas operações: **`shift`** (tira o próximo token da fita e
empilha) e **`reduce`** (reconhece no topo da pilha o lado direito de uma produção,
desempilha esses símbolos e empilha o não-terminal do lado esquerdo).

```
   fita →  NUMBER PLUS NUMBER NEWLINE
   pilha: [ ]              shift NUMBER
   pilha: [ NUMBER ]       reduce  factor : NUMBER
   pilha: [ factor ]       reduce  term   : factor
   pilha: [ term ]         reduce  expr   : term
   pilha: [ expr ]         shift PLUS ...
```

Quando só o símbolo inicial resta na pilha e a fita acabou, a análise terminou com sucesso.

### LALR(1) e o *lookahead*

**LALR(1)** é a família de parsers que o Bison gera. **LR** = lê da esquerda para a direita
produzindo a derivação mais à direita — ou seja, é ascendente. **LA** = *Look-Ahead*, os
estados são compactados para gerar tabelas pequenas. **(1)** = em cada passo o parser decide
entre `shift` e `reduce` olhando **a pilha atual e exatamente UM token à frente**. Um token
de lookahead parece pouco, mas basta para quase toda linguagem de programação — e é o que
permite analisar em tempo linear no tamanho da entrada.

### Conflito *shift-reduce*

É quando pilha e lookahead **não bastam** para decidir. Com uma gramática de uma regra só —
`expr : expr PLUS expr | expr TIMES expr | NUMBER ;` — ao processar `2 + 3 * 4` o parser
chega a `pilha: [expr(2) PLUS expr(3)]`, `lookahead: TIMES`, e tem duas saídas legítimas:
**reduce** `expr PLUS expr` agora → `(2+3)*4 = 20`; ou **shift** o `TIMES` → `2+(3*4) = 14`.
A gramática é **ambígua** e o Bison denuncia. Medido nesta máquina, com essa gramática mais
`MINUS`, `DIVIDE` e parênteses:

```
$ bison -d -o amb.tab.c amb.y
amb.y: warning: 16 shift/reduce conflicts [-Wconflicts-sr]
```

### Precedência e associatividade

**Precedência**: entre operadores **diferentes**, quem agarra primeiro — `*` antes de `+`.
**Associatividade**: entre operadores de **mesma** precedência, para que lado agrupa.

```
100 / 4 / 5   à esquerda: (100 / 4) / 5 = 5     ← o que este parser faz
              à direita:   100 / (4 / 5) = 125
```

Quase toda a aritmética é associativa à esquerda; potenciação (`2^3^2`) é a exceção clássica.

### Ações semânticas: `$$`, `$1`, `$3`

Reconhecer a estrutura é metade do trabalho. Cada produção pode carregar um bloco de C entre
chaves — a **ação semântica** — que roda **no instante em que a produção é reduzida**. Os
símbolos são numerados **a partir de 1, da esquerda para a direita**, e `$$` é o valor que o
não-terminal da esquerda vai passar a carregar:

```
expr  :  expr  PLUS  term   { $$ = $1 + $3; }
         ────  ────  ────
          $1    $2    $3        ($2 é o PLUS: existe, mas não tem valor útil)
         ↑
        $$ = o expr resultante, à esquerda dos dois-pontos
```

### `%union` e o tipo semântico

Se `$1` é um valor, **de que tipo C ele é?** A `%union` lista todos os tipos possíveis:

```bison
%union { double dval; }        /* um único campo, chamado dval  */
%token <dval> NUMBER           /* NUMBER carrega um double      */
%token PLUS MINUS TIMES ...    /* estes não carregam nada       */
%type  <dval> expr term factor /* estes três produzem um double */
```

`%token` declara **terminais**; `%type` declara o tipo de **não-terminais**; o `<dval>`
escolhe o campo da união. Sem `%type <dval> expr`, escrever `$$ = $1 + $3;` numa regra de
`expr` é erro de compilação — o Bison não saberia qual campo usar. A união vira o tipo
`YYSTYPE` no código gerado, e a global `yylval` é uma instância dela: é por ali que o lexer
entrega o valor de um token ao parser.

### A família `yy*`

Flex e Bison nasceram em C nos anos 70/80, muito antes de namespaces. Tudo o que geram leva o
prefixo `yy` e mora em **globais** — herança, não elegância.

| Nome | Quem cria | O que é |
|---|---|---|
| `yylex()` | Flex | devolve **o próximo token** (um `int`); `0` no fim da entrada |
| `yyparse()` | Bison | roda a análise inteira; `0` = sucesso, `1` = erro de sintaxe |
| `yylval` | Bison (tipo), Flex (escrita) | a global do tipo `%union` — o **valor** do token atual |
| `yytext` | Flex | ponteiro para o **lexema** que acabou de casar (texto cru) |
| `yyin` | Flex | o `FILE*` de onde o lexer lê; **padrão: `stdin`** |
| `yyerror()` | **você** | o Bison a chama quando a análise falha |

Há um sétimo, `yywrap()`, tratado na seção do `%option noyywrap`.

### `extern "C"` — por que o `main.cpp` precisa disso

Flex e Bison geram **C**. As declarações que você escreve à mão em `main.cpp` precisam
concordar com os símbolos produzidos; `extern "C"` desliga o *name mangling* do C++, de modo
que o `yyparse` declarado no seu arquivo é o mesmo símbolo do objeto gerado. O subprojeto
[linkage](../linkage/README.md) trata `extern "C"`, ODR e ABI em detalhe.

---

## Por que estudar isto?

Porque "ler texto estruturado" reaparece a vida inteira:

| Sem gramática | Com gramática |
|---|---|
| `split(',')` que quebra na primeira vírgula dentro de aspas | a regra diz onde a string começa e acaba |
| contagem manual de parênteses com um `int` | recursão na produção — aninhamento sai de graça |
| erro "entrada inválida" | "linha 3: esperava `)`, encontrei `+`" |
| operador novo = reescrever o laço | operador novo = **uma linha** de produção |
| precedência por `if` aninhado | precedência é propriedade da gramática |

Onde isso aparece: compiladores, arquivos de configuração (JSON, YAML, TOML, `.ini`), SQL,
protocolos textuais (HTTP, SMTP), expressões de filtro (`status == 200 && bytes > 1024`),
*shaders*, `printf`, linguagens de domínio.

**E por que não resolver com regex?** Porque **é impossível**, não difícil. Uma expressão
regular equivale a um autômato finito, que tem um número **fixo** de estados e portanto **não
sabe contar sem limite**. Reconhecer `((((...))))` balanceado exige lembrar quantos foram
abertos, e esse número não tem teto — é o *lema do bombeamento* para linguagens regulares. A
divisão correta é **regex para os tokens** (número, identificador, operador: todos regulares)
e **gramática para a estrutura**. É exatamente a divisão Flex/Bison. Sobre o custo de
`std::regex` em C++, veja [strings](../strings/README.md).

---

## Estrutura de arquivos

```
src/parser/
├── lexer.l        ← 1 · Flex: 10 regras que transformam caracteres em tokens
├── parser.y       ← 2 · Bison: a gramática estratificada + as ações semânticas
├── main.cpp       ← 3 · abre o arquivo (ou usa stdin) e chama yyparse()
├── example.txt    ← 4 · 7 expressões de teste, incluindo uma divisão por zero
├── meson.build    ← 5 · os dois custom_target que rodam bison e flex
└── README.md      ← este arquivo
```

Nenhum `.c` aparece aqui: `lex.yy.c`, `parser.tab.c` e `parser.tab.h` são **gerados** durante
a build e vivem em `build/src/parser/`.

---

## Exemplo mínimo: 3 regras de lexer, 2 de gramática

Antes do projeto inteiro, construa o menor Flex+Bison possível — lê um número por linha e o
imprime de volta. Sem operadores, sem precedência.

```lex
/* mini.l */                        │  /* mini.y */
%{                                  │  %{
#include "mini.tab.h"               │  #include <stdio.h>
#include <stdlib.h>                 │  extern int yylex(void);
%}                                  │  void yyerror(const char* s)
%option noyywrap                    │  { fprintf(stderr, "Erro: %s\n", s); }
%%                                  │  %}
[0-9]+  { yylval.dval = atof(yytext);│  %union { double dval; }
          return NUMBER; }          │  %token <dval> NUMBER
\n      { return NEWLINE; }         │  %token NEWLINE
.       { /* ignora o resto */ }    │  %%
%%                                  │  input : /* vazio */
                                    │        | input NUMBER NEWLINE
                                    │          { printf("li o numero %g\n", $2); }
                                    │        ;
                                    │  %%
                                    │  int main(void) { return yyparse(); }
```

```bash
mkdir -p /tmp/mini && cd /tmp/mini     # crie os dois arquivos acima
bison -d -o mini.tab.c mini.y          # gera mini.tab.c + mini.tab.h
flex  -o lex.yy.c mini.l               # gera lex.yy.c
cc -o mini mini.tab.c lex.yy.c
printf '42\n7\n' | ./mini
```

Saída real: `li o numero 42` e `li o numero 7`. Três lições já estão todas aqui:

1. o **`-d`** do Bison é o que gera o `.tab.h` com os `#define NUMBER 258` — sem ele o lexer
   não compila, porque `NUMBER` não existiria;
2. `input : /* vazio */ | input NUMBER NEWLINE` é o idioma de **lista** ("nada, ou uma lista
   seguida de mais um item"); é o que permite ler **várias** linhas em vez de uma só;
3. `$2` é o `NUMBER` porque ele é o **segundo** símbolo de `input NUMBER NEWLINE`.

---

## Módulo 1 — `lexer.l`: o Flex, regra a regra

Um arquivo Flex tem três seções separadas por `%%`: declarações (com código C literal entre
`%{` e `%}`), regras (`padrão { ação }`) e código extra opcional — ausente aqui. O arquivo
completo, [lexer.l](lexer.l):

```lex
%{
#include "parser.tab.h"   /* os #define NUMBER 258, PLUS 259, ... vindos do Bison */
#include <stdlib.h>       /* atof */
%}
%option noyywrap
%option nounput
%option noinput
%%
[0-9]+(\.[0-9]+)?   { yylval.dval = atof(yytext); return NUMBER; }
"+"                 { return PLUS;  }
"-"                 { return MINUS; }
"*"                 { return TIMES; }
"/"                 { return DIVIDE; }
"("                 { return LPAREN; }
")"                 { return RPAREN; }
"\n"                { return NEWLINE; }
[ \t]               { /* ignorar espaços */ }
.                   { fprintf(stderr, "Caractere desconhecido: %s\n", yytext); }
%%
```

Como o Flex escolhe a regra: **casamento mais longo vence** (diante de `12.5`, a primeira
regra casa 4 caracteres e a última casaria 1, então a primeira ganha); em caso de empate no
comprimento, vence a regra **declarada primeiro**.

`[0-9]+(\.[0-9]+)?` = um ou mais dígitos, opcionalmente seguidos de ponto e mais dígitos.
Logo `2` e `2.5` são aceitos, mas `.5` **não**. Uma ação sem `return` descarta o lexema e o
lexer continua — é o caso de `[ \t]` e do `.` final.

### `%option noyywrap` e o erro que ele evita

Por padrão, ao chegar ao fim do arquivo, o Flex chama `yywrap()` para perguntar *"tem mais
entrada?"*. Essa função não é gerada — você teria de escrevê-la. `%option noyywrap` diz ao
Flex para assumir que o fim do arquivo é o fim de tudo.

> ⚠ **Sem essa linha, o programa compila mas não linka.** Medido, removendo o `%option
> noyywrap` do exemplo mínimo acima:
> ```
> lex.yy.c:(.text+0x4b0): undefined reference to `yywrap'
> ```
> A alternativa histórica era linkar com `-lfl`, biblioteca que existe basicamente para
> fornecer um `yywrap()` que devolve 1.

`%option nounput` e `noinput` suprimem duas funções auxiliares que este lexer não usa; sem
elas o GCC avisaria *"defined but not used"*, e o projeto compila com `-Wall -Wextra`.

---

## Módulo 2 — `parser.y`: a gramática estratificada

A gramática de [parser.y](parser.y), exatamente como está no disco:

```bison
input   : /* vazio */
        | input linha
        ;
linha   : NEWLINE
        | expr NEWLINE        { printf("  = %.6g\n", $1); }
        ;
expr    : expr PLUS  term     { $$ = $1 + $3; }
        | expr MINUS term     { $$ = $1 - $3; }
        | term                { $$ = $1; }
        ;
term    : term TIMES  factor  { $$ = $1 * $3; }
        | term DIVIDE factor  {
            if ($3 == 0.0) { yyerror("divisão por zero"); $$ = 0; }
            else           { $$ = $1 / $3; }
          }
        | factor              { $$ = $1; }
        ;
factor  : NUMBER              { $$ = $1; }
        | LPAREN expr RPAREN  { $$ = $2; }
        | MINUS factor %prec UMINUS  { $$ = -$2; }
        ;
```

### Por que três níveis, e não um só

Esta é a **gramática estratificada**, o coração do projeto. Ela codifica a precedência na
**forma das regras**, não em declarações à parte:

```
expr    ← soma e subtração                    (menor precedência, agarra por último)
  └ term    ← multiplicação e divisão
       └ factor  ← número, parênteses, menos unário   (maior precedência)
```

A regra é sempre a mesma: **um nível só pode conter níveis abaixo dele**. Como `expr PLUS
term` tem `term` à direita, tudo o que for multiplicação já terá sido agrupado antes de a soma
ser considerada. É impossível montar a árvore errada — a ambiguidade não existe **por
construção**.

A associatividade à esquerda também é estrutural: `expr : expr PLUS term` é **recursiva à
esquerda** (o próprio `expr` na ponta esquerda), o que faz `a - b - c` agrupar como
`(a - b) - c`. Se fosse `term PLUS expr`, seria à direita.

> **Medido:** `bison -d parser.y` nesta gramática produz saída vazia — **zero conflitos**. E,
> removendo as três linhas `%left`/`%right` e o `%prec`, a saída continua vazia: **zero
> conflitos**. As declarações de precedência deste arquivo são praticamente inertes; quem
> resolve tudo é a estratificação. O `%prec UMINUS` é uma reserva — passaria a importar se um
> operador de potência fosse acrescentado.

### A alternativa ambígua, e por que este projeto não a usa

O outro caminho é uma gramática de um nível só, com o Bison desempatando:

```bison
%left  PLUS MINUS      /* declarado primeiro = MENOR precedência */
%left  TIMES DIVIDE    /* declarado depois   = MAIOR precedência */
%right UMINUS          /* associativo à direita, precedência máxima */

expr : expr PLUS expr { $$ = $1 + $3; } | expr MINUS  expr { $$ = $1 - $3; }
     | expr TIMES expr{ $$ = $1 * $3; } | expr DIVIDE expr { $$ = $1 / $3; }
     | MINUS expr %prec UMINUS { $$ = -$2; }
     | LPAREN expr RPAREN { $$ = $2; } | NUMBER { $$ = $1; }
     ;
```

Aqui o `%left` **é** o mecanismo. Cada produção recebe a precedência do seu último terminal, e
diante de um conflito o Bison compara a precedência da **regra** com a do **lookahead**:
lookahead mais forte ⇒ `shift`; regra mais forte ⇒ `reduce`; empate ⇒ decide a
associatividade (`%left` reduz, `%right` desloca). Medido:

| Versão | Saída do `bison` |
|---|---|
| sem `%left`/`%right` | `warning: 16 shift/reduce conflicts [-Wconflicts-sr]` |
| com `%left`/`%right` | *(silêncio — 0 conflitos)* |

Vantagem: menos linhas. Desvantagem: a gramática **continua ambígua**, e o que a salva é uma
tabela de desempate declarada à parte — ler a gramática já não basta para saber o que o
programa faz.

> ⚠ Silenciar conflitos não é o mesmo que não tê-los. Numa gramática grande, os `%left`
> mascaram conflitos que você **não** pretendia resolver. Prefira a estratificação.

---

## Módulo 3 — Traço de execução de `2 + 3 * 4`

É o passo que torna LALR concreto. O Bison sabe emitir o traço: acrescente `%define
parse.trace` ao `.y` e ponha `yydebug = 1;` antes do `yyparse()`. A tabela abaixo é a saída
**real**, condensada, para `2 + 3 * 4` seguido de uma quebra de linha:

| # | Pilha (topo à direita) | Lookahead | Ação | Ação semântica |
|---|---|---|---|---|
| 1 | `input` | `NUMBER(2)` | shift | |
| 2 | `input NUMBER` | | reduce `factor : NUMBER` | `$$ = 2` |
| 3 | `input factor` | | reduce `term : factor` | `$$ = 2` |
| 4 | `input term` | `PLUS` | reduce `expr : term` | `$$ = 2` |
| 5 | `input expr` | `PLUS` | shift | |
| 6 | `input expr PLUS` | `NUMBER(3)` | shift | |
| 7 | `input expr PLUS NUMBER` | | reduce `factor : NUMBER` | `$$ = 3` |
| 8 | `input expr PLUS factor` | | reduce `term : factor` | `$$ = 3` |
| 9 | `input expr PLUS term` | `TIMES` | **shift** ← o momento decisivo | |
| 10 | `input expr PLUS term TIMES` | `NUMBER(4)` | shift | |
| 11 | `… TIMES NUMBER` | | reduce `factor : NUMBER` | `$$ = 4` |
| 12 | `input expr PLUS term TIMES factor` | `NEWLINE` | reduce `term : term TIMES factor` | **`$$ = 3*4 = 12`** |
| 13 | `input expr PLUS term` | `NEWLINE` | reduce `expr : expr PLUS term` | **`$$ = 2+12 = 14`** |
| 14 | `input expr` | | shift `NEWLINE` | |
| 15 | `input expr NEWLINE` | | reduce `linha : expr NEWLINE` | imprime `  = 14` |
| 16 | `input linha` | | reduce `input : input linha` | |

Leia o **passo 9** com atenção: a pilha termina em `expr PLUS term` — o lado direito
**completo** da produção `expr : expr PLUS term`. Reduzir agora seria legal quanto à forma. O
parser **não** reduz, porque o lookahead é `TIMES` e, no estado em que ele está, `TIMES` só
admite `shift`. Não há conflito: a tabela LALR tem **uma única** ação possível ali, e isso é
consequência direta de a produção exigir `term` (e não `expr`) à direita do `PLUS`.

Leia agora os passos 12 e 13: a multiplicação reduz **antes** da soma. **A ordem das reduções
é a ordem de avaliação** — é por isso que a árvore certa produz o número certo.

---

## Módulo 4 — Erro semântico: divisão por zero

A gramática aceita `10 / 0` sem reclamar: sintaticamente não há nada de errado. O problema é
**semântico**, e por isso o teste mora dentro da ação:

```bison
| term DIVIDE factor  {
    if ($3 == 0.0) { yyerror("divisão por zero"); $$ = 0; }
    else           { $$ = $1 / $3; }
  }
```

`yyerror` aqui é usada só como "imprima esta mensagem no `stderr`" — ela **não** aborta a
análise. Por isso a regra ainda precisa atribuir algo a `$$` (zero, por convenção): o valor
sobe a árvore de qualquer jeito. A última linha de [example.txt](example.txt) é exatamente
`10 / 0`, e produz `Erro: divisão por zero` no `stderr` mais `  = 0` no `stdout`.

> ⚠ **Por que "Erro:" costuma aparecer antes de tudo?** `stderr` não é bufferizado; `stdout`,
> quando redirecionado, é bufferizado em bloco. As duas saem na ordem certa no terminal, mas
> se embaralham ao passar por `|` ou `>`. Separe os canais: `… 2>/dev/null`.

> ⚠ `if ($3 == 0.0)` compara `double` com igualdade exata. Aqui é intencional e correto — o
> alvo é o zero exato, único valor proibido como divisor. Para comparar dois resultados de
> conta, igualdade exata seria um erro; veja `nearly_equal` em
> [../doxygen/math_utils.hpp](../doxygen/math_utils.hpp).

---

## Módulo 5 — Entrada inválida: o que acontece de fato

Há dois tipos de rejeição, vindos de camadas diferentes.

**Caractere que nenhuma regra do lexer reconhece** — a regra `.` avisa e o descarta:

```
$ echo "2 @ 3" | ./dist/bin/parser
Caractere desconhecido: @
Erro: syntax error
```

O `@` some, sobra a fita `NUMBER NUMBER NEWLINE` — que a gramática não aceita. Daí o segundo
erro.

**Tokens válidos em ordem inválida** — o parser chama `yyerror`:

```
$ echo "2 +" | ./dist/bin/parser ; echo $?
Erro: syntax error
1
```

`syntax error` é a mensagem padrão do Bison, passada à sua `yyerror`, que só a imprime
prefixada (`fprintf(stderr, "Erro: %s\n", s)`).

> ⚠ **Este parser não tem recuperação de erro.** Não existe nenhuma produção com o terminal
> especial `error`, então `yyparse()` desiste na primeira falha e devolve 1 — mesmo em modo
> interativo, e mesmo que as linhas seguintes fossem válidas. Um parser de produção
> acrescentaria `linha : error NEWLINE { yyerrok; }` para descartar até a próxima quebra de
> linha e continuar.

Duas melhorias baratas para experimentar: `%define parse.error verbose` troca `syntax error`
por *"syntax error, unexpected NEWLINE, expecting NUMBER"*; `%locations` acrescenta linha e
coluna.

---

## Módulo 6 — `main.cpp`: a ponte C++ → C

O arquivo é [main.cpp](main.cpp):

```cpp
extern "C" {
    int yyparse();       // gerada pelo Bison
    extern FILE* yyin;   // definida pelo Flex; padrão: stdin
}

int main(int argc, char* argv[]) {
    printf("=== Parser de Expressões Aritméticas ===\n");
    printf("Digite expressões (ex: 2 + 3 * (4 - 1))\n");
    printf("Pressione Ctrl+D para sair.\n\n");

    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            fprintf(stderr, "Não foi possível abrir: %s\n", argv[1]);
            return 1;
        }
    }
    return yyparse();
}
```

1. **`yyin` não é aberto quando não há argumento** — o Flex o inicializa com `stdin`, e é
   assim que o modo interativo funciona, sem nenhuma linha extra.
2. **O `fopen` é verificado.** Sem essa checagem, `yyin` ficaria nulo e o lexer leria de um
   `FILE*` inválido.
3. **O retorno do `main` é o do `yyparse()`**: 0 se a entrada inteira foi analisada, 1 se
   houve erro de sintaxe — o que torna o binário utilizável em scripts.

---

## Módulo 7 — `meson.build`: como os geradores entram na build

```meson
flex  = find_program('flex',  required: true)
bison = find_program('bison', required: true)

bison_out = custom_target('parser_bison',
    input   : 'parser.y',
    output  : ['parser.tab.c', 'parser.tab.h'],
    command : [bison, '-d', '-o', '@OUTPUT0@', '@INPUT@'])

flex_out = custom_target('parser_flex',
    input   : 'lexer.l',
    output  : 'lex.yy.c',
    command : [flex, '-o', '@OUTPUT@', '@INPUT@'])

executable('parser',
    sources : ['main.cpp', bison_out, flex_out],
    include_directories : include_directories('.'),
    implicit_include_directories : true,
    install: true)
```

- **`custom_target`** declara "rode este comando; ele consome estes arquivos e produz estes
  outros". `@INPUT@`/`@OUTPUT@` são substituídos pelos caminhos reais; `@OUTPUT0@` é o
  **primeiro** da lista `output` — o `.tab.c`. O `-d` produz o `.tab.h` **como efeito
  colateral**, ao lado do `.c`, e é por isso que os dois estão em `output:`.
- **A ordem entre os dois alvos não é declarada, e não precisa ser.** Ela é deduzida: como
  `bison_out` aparece em `sources`, o Ninja sabe que `parser.tab.h` existe antes de qualquer
  `.c` ser compilado.
- **`implicit_include_directories: true`** é o que faz o `#include "parser.tab.h"` do lexer
  funcionar. Esse header **não existe no diretório de fontes** — nasce em `build/src/parser/`;
  a opção acrescenta o diretório de build correspondente ao caminho de include.

Depois de `make build`, `build/src/parser/` contém `lex.yy.c`, `parser.tab.c`, `parser.tab.h`,
`parser.p/` (objetos intermediários) e `parser` (o binário antes do install).

> ⚠ `find_program(..., required: true)` roda no **`meson setup`**, não na compilação. Sem
> `flex` e `bison` disponíveis, quem falha é a **configuração**, com `Program 'bison' not
> found`. Neste repositório eles vêm do Conan (`tool_requires("flex/2.6.4")` e
> `tool_requires("bison/3.8.2")` no `conanfile.py`), e o `make configure` os coloca no `PATH`
> via `build/conanbuild.sh`. Fora desse fluxo: `sudo apt install flex bison`.

```
  lexer.l  ──flex────►  lex.yy.c ───────────┐
                                             ├──► g++ ──► parser ──► dist/bin/parser
  parser.y ──bison -d─► parser.tab.c ───────┤
                     └► parser.tab.h ──►(incluído por lex.yy.c)
  main.cpp ──────────────────────────────────┘
```

Sobre `custom_target` e o resto do sistema de build: [../meson/README.md](../meson/README.md).

---

## Como validar

```bash
make build
./dist/bin/parser src/parser/example.txt 2>/dev/null
```

Saída esperada, exatamente (o `%.6g` do `printf` explica os formatos):

```
=== Parser de Expressões Aritméticas ===
Digite expressões (ex: 2 + 3 * (4 - 1))
Pressione Ctrl+D para sair.

  = 3
  = 17
  = 12
  = 5
  = 4
  = 6.25
  = 0
```

Linha a linha, contra o [example.txt](example.txt):

| Entrada | Saída | O que está sendo testado |
|---|---|---|
| `1 + 2` | `= 3` | soma |
| `3 * 4 + 5` | `= 17` | precedência: `*` antes de `+` |
| `(10 - 4) * 2` | `= 12` | parênteses vencem a precedência |
| `100 / 4 / 5` | `= 5` | associatividade à esquerda (`(100/4)/5`, não `100/(4/5)=125`) |
| `-3 + 7` | `= 4` | menos unário via `factor : MINUS factor` |
| `2.5 * (1 + 1.5)` | `= 6.25` | ponto flutuante e aninhamento |
| `10 / 0` | `Erro: divisão por zero` no `stderr` e `= 0` | erro semântico tratado na ação |

Sem o `2>/dev/null` a linha de erro aparece antes do cabeçalho — veja o aviso do Módulo 4.
No modo interativo, `2 + 3 * (4 - 1)` dá `= 11`, `-2 * 3` dá `= -6`, `2 - 3 - 4` dá `= -5` e
`((3))` dá `= 3`.

---

## Extensões possíveis, em ordem de dificuldade

1. **Mensagens melhores** — `%define parse.error verbose` e `%locations`.
2. **Recuperação de erro** — `linha : error NEWLINE { yyerrok; }`, e verificar que o parser
   passa a continuar depois de uma linha inválida.
3. **Potenciação** — `%right POW` e uma produção; é o caso em que `%prec` e associatividade à
   direita passam a valer a pena.
4. **Variáveis** — um token `IDENT`, um `std::map<std::string,double>` e a produção
   `linha : IDENT ASSIGN expr NEWLINE`.
5. **Funções** — `sin(x)`, `sqrt(x)`: uma produção `factor : IDENT LPAREN expr RPAREN`.
6. **AST** — hoje as ações **avaliam direto**, sem construir árvore nenhuma: `$$` é um
   `double`. Para otimizar, imprimir ou compilar a expressão, troque o `%union` por um `Node*`
   e faça cada ação **construir um nó** em vez de calcular. É o passo que separa um
   interpretador de um compilador.

---

## Referências

Neste repositório: [../meson/README.md](../meson/README.md) (`custom_target`, geração de
código na build) · [../linkage/README.md](../linkage/README.md) (`extern "C"`, *name
mangling*, ODR, ABI) · [../strings/README.md](../strings/README.md) (`std::regex` e seu custo
real) · [../doxygen/README.md](../doxygen/README.md) (documentar a API que você escreveu) ·
[../../README.md](../../README.md) (índice do `cpp_academy`).

Fora dele:

- *Bison Manual* — <https://www.gnu.org/software/bison/manual/> — o capítulo *The Bison Parser
  Algorithm* é a fonte canônica sobre `shift`/`reduce` e precedência
- *Flex Manual* — <https://westes.github.io/flex/manual/> — veja *Matching* e *Options*
- *Crafting Interpreters*, Robert Nystrom — <https://craftinginterpreters.com/> — o melhor
  texto gratuito sobre o assunto; escreve tudo à mão, sem geradores
- *Compilers: Principles, Techniques, and Tools* (Aho, Lam, Sethi, Ullman) — o "Livro do
  Dragão"; capítulos 3 (análise léxica) e 4 (análise sintática)
