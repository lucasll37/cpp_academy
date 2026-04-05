%{
#include <stdio.h>
#include <math.h>

extern int yylex();
void yyerror(const char* s);
%}

%union {
    double dval;
}

%token <dval> NUMBER
%token PLUS MINUS TIMES DIVIDE LPAREN RPAREN NEWLINE

%type  <dval> expr term factor

/* Precedência: PLUS/MINUS menor, TIMES/DIVIDE maior (declarados depois = maior) */
%left PLUS MINUS
%left TIMES DIVIDE
%right UMINUS

%%

input
    : /* vazio */
    | input linha
    ;

linha
    : NEWLINE
    | expr NEWLINE  { printf("  = %.6g\n", $1); }
    ;

expr
    : expr PLUS  term   { $$ = $1 + $3; }
    | expr MINUS term   { $$ = $1 - $3; }
    | term              { $$ = $1; }
    ;

term
    : term TIMES  factor  { $$ = $1 * $3; }
    | term DIVIDE factor  {
        if ($3 == 0.0) { yyerror("divisão por zero"); $$ = 0; }
        else           { $$ = $1 / $3; }
      }
    | factor              { $$ = $1; }
    ;

factor
    : NUMBER                 { $$ = $1; }
    | LPAREN expr RPAREN     { $$ = $2; }
    | MINUS factor %prec UMINUS  { $$ = -$2; }
    ;

%%

void yyerror(const char* s) {
    fprintf(stderr, "Erro: %s\n", s);
}
