#!/usr/bin/env bash
# =============================================================================
#  scripts/site.sh — monta o site estático do C++ Academy (cpp.lucaslima.ai) em _site/
#
#  O Render roda isto (render.yaml) e publica _site/. Localmente:  make site
#
#    _site/            ← a aula de src/syntax (a raiz do site é a primeira aula)
#    _site/<proj>/     ← as demais aulas: src/<proj>/apresentacao, se existirem
#    _site/ll/ …       ← public/: a camada da casa (sincronizada pela landing-page
#                        com `make design-sync`), o selo do autor e academia.css
#
#  O deck do repositório continua sem nada da casa (abre offline via make open);
#  a cortina, a serifa e o selo são injetados só na cópia publicada.
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

SAIDA=_site
rm -rf "$SAIDA"
mkdir -p "$SAIDA"

injetar() {   # $1 = index.html da cópia; $2 = prefixo até a raiz ("" ou "../")
  local p="$2"
  python3 - "$1" "$p" <<'PY'
import sys, re, pathlib
arq, p = pathlib.Path(sys.argv[1]), sys.argv[2]
h = arq.read_text(encoding='utf-8')
casa = (f'<script src="{p}ll/curtain.js"></script>\n'
        f'<link rel="preload" href="{p}ll/source-serif-4.woff2" as="font" type="font/woff2" crossorigin>\n'
        f'<link rel="stylesheet" href="{p}ll/house.css">\n'
        f'<link rel="stylesheet" href="{p}academia.css">\n')
h, n = re.subn(r'(<meta name="viewport"[^>]*>\n)', lambda m: m.group(1) + casa, h, count=1)
assert n == 1, 'viewport não encontrado'
selo = (f'<a class="ll-author academia" href="https://www.lucaslima.ai/"><img class="ll-author-avatar" '
        f'src="{p}autor.webp" width="22" height="22" alt=""><span class="ll-author-name">Feito por Lucas Lima</span></a>\n')
h, n = re.subn(r'<body>\n', lambda m: '<body data-ll-name="C++ Academy">\n' + selo, h, count=1)
assert n == 1, '<body> não encontrado'
arq.write_text(h, encoding='utf-8')
PY
}

# a raiz: a aula de sintaxe
cp -r src/syntax/apresentacao/. "$SAIDA/"
cp -r public/. "$SAIDA/"
injetar "$SAIDA/index.html" ""

# as outras aulas, quando existirem
for deck in src/*/apresentacao/index.html; do
  proj="$(basename "$(dirname "$(dirname "$deck")")")"
  [ "$proj" = syntax ] && continue
  cp -r "src/$proj/apresentacao" "$SAIDA/$proj"
  injetar "$SAIDA/$proj/index.html" "../"
done

echo "site montado em $SAIDA/ ($(find "$SAIDA" -type f | wc -l) arquivos)"
