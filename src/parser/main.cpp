#include <cstdio>
#include <cstring>

extern "C" {
    int yyparse();
    extern FILE* yyin;
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