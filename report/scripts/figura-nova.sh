#!/bin/sh
# =============================================================================
#  figura-nova.sh -- ajuda a autorar: mede, preenche o <meta> e renderiza
# =============================================================================
#
#  Uso:  sh scripts/figura-nova.sh figures-src/<cap>/<nome>.html [--instalar]
#
#  Sem --instalar, renderiza para figures-src/.render/ (rascunho, para
#  conferir a olho). Com --instalar, escreve em figures/<cap>/<nome>.png.
#
#  Existe porque a altura de uma figura so se sabe depois de renderizar, e o
#  <meta name="figura"> tem de declara-la. Este script fecha esse laco: mede,
#  reescreve o meta e renderiza -- mantendo a assercao de geometria intacta
#  (o guarda.js continua conferindo declarado x medido na hora do render).
# =============================================================================
set -eu

SRC=""
INSTALAR=0
for arg in "$@"; do
    case "$arg" in
        --instalar) INSTALAR=1 ;;
        *)          SRC="$arg" ;;
    esac
done

[ -n "$SRC" ] || { echo "uso: sh scripts/figura-nova.sh figures-src/<cap>/<nome>.html [--instalar]" >&2; exit 2; }
[ -f "$SRC" ] || { echo "erro: nao encontrei $SRC" >&2; exit 2; }

MEDIDO=$(sh scripts/figura-render.sh --medir "$SRC")
echo "  medido: $MEDIDO"

python3 - "$SRC" "$MEDIDO" <<'PYEOF'
import io, re, sys
caminho, medido = sys.argv[1], sys.argv[2]
s = io.open(caminho, encoding='utf-8').read()
novo, n = re.subn(r'<meta name="figura" content="[0-9]+x[0-9]+">',
                  '<meta name="figura" content="%s">' % medido, s)
if n != 1:
    sys.stderr.write('erro: esperava exatamente um <meta name="figura">, achei %d\n' % n)
    sys.exit(1)
io.open(caminho, 'w', encoding='utf-8').write(novo)
PYEOF

if [ "$INSTALAR" -eq 1 ]; then
    OUT=$(echo "$SRC" | sed 's|^figures-src/|figures/|; s|\.html$|.png|')
else
    OUT="figures-src/.render/$(basename "$SRC" .html).previa.png"
fi

sh scripts/figura-render.sh --out "$OUT" "$SRC"
