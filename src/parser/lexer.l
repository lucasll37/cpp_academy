%{
#include "parser.tab.h"
#include <stdlib.h>
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
