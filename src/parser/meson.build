flex  = find_program('flex',  required: true)
bison = find_program('bison', required: true)

# ── Bison: gera parser.tab.c + parser.tab.h ──────────────────────────────────
bison_out = custom_target('parser_bison',
    input   : 'parser.y',
    output  : ['parser.tab.c', 'parser.tab.h'],
    command : [bison, '-d', '-o', '@OUTPUT0@', '@INPUT@'],
)

# ── Flex: gera lex.yy.c ───────────────────────────────────────────────────────
flex_out = custom_target('parser_flex',
    input   : 'lexer.l',
    output  : 'lex.yy.c',
    command : [flex, '-o', '@OUTPUT@', '@INPUT@'],
)

# ── Executável final ──────────────────────────────────────────────────────────
executable('parser',
    sources : ['main.cpp', bison_out, flex_out],
    include_directories : include_directories('.'),
    # parser.tab.h fica no build dir; dizemos ao Meson onde procurar
    implicit_include_directories : true,
    install: true
)
