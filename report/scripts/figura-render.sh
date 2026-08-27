#!/bin/sh
# =============================================================================
#  figura-render.sh -- renderiza uma figura HTML para PNG via Chromium headless
# =============================================================================
#
#  Uso:  scripts/figura-render.sh [--chrome CAMINHO] --out figures/cap/nome.png \
#                                 figures-src/cap/nome.html
#
#  Duas passadas, de proposito:
#
#    1. MEDIR  -- carrega a pagina numa janela deliberadamente alta e le
#                 body[data-medido], escrito por assets/guarda.js.
#    2. CAPTURAR -- rerenderiza com a altura exata e tira o screenshot.
#
#  Captura de pagina inteira e PROIBIDA aqui. Foi ela que transformou um
#  transbordo em artefato corrompido no corpus antigo: o Chromium cresce a
#  imagem para a direita e para baixo, mas descarta em silencio o que sai pela
#  esquerda. A geometria e declarada na figura (<meta name="figura">), conferida
#  por guarda.js e conferida de novo contra o IHDR do PNG.
#
#  O PNG so e instalado em figures/ depois de todas as assercoes passarem --
#  um render que falhe nunca deixa arquivo meio-escrito na arvore de artefatos.
# =============================================================================
set -eu

CHROME=""
OUT=""
SRC=""
MEDIR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --chrome) CHROME="$2"; shift 2 ;;
        --out)    OUT="$2";    shift 2 ;;
        --medir)  MEDIR=1;     shift ;;
        -*)       echo "opcao desconhecida: $1" >&2; exit 2 ;;
        *)        SRC="$1";    shift ;;
    esac
done

[ -n "$SRC" ] || { echo "erro: falta o arquivo .html de entrada" >&2; exit 2; }
[ "$MEDIR" -eq 1 ] || [ -n "$OUT" ] || { echo "erro: falta --out" >&2; exit 2; }
[ -f "$SRC" ] || { echo "erro: nao encontrei $SRC" >&2; exit 2; }

# --- localiza o Chromium -----------------------------------------------------
if [ -z "$CHROME" ]; then
    CHROME=$(command -v chrome-headless-shell 2>/dev/null || true)
fi
if [ -z "$CHROME" ]; then
    CHROME=$(ls -1d "$HOME"/.cache/ms-playwright/chromium_headless_shell-*/chrome-headless-shell-linux64/chrome-headless-shell 2>/dev/null | sort -V | tail -1 || true)
fi
if [ -z "$CHROME" ]; then
    CHROME=$(ls -1d "$HOME"/.cache/ms-playwright/chromium-*/chrome-linux64/chrome 2>/dev/null | sort -V | tail -1 || true)
fi
if [ -z "$CHROME" ]; then
    CHROME=$(command -v chromium chromium-browser google-chrome 2>/dev/null | head -1 || true)
fi
[ -n "$CHROME" ] || {
    echo "erro: nenhum Chromium encontrado. Rode 'make setup-figures'." >&2
    exit 3
}

NOME=$(basename "$SRC" .html)
RASCUNHO="figures-src/.render"
mkdir -p "$RASCUNHO"
ABS_SRC=$(readlink -f "$SRC")
LARGURA=1360

COMUNS="--headless --no-sandbox --disable-gpu --disable-dev-shm-usage
        --hide-scrollbars --force-device-scale-factor=2 --force-color-profile=srgb
        --font-render-hinting=none --disable-lcd-text
        --allow-file-access-from-files --virtual-time-budget=3000"

# --- passada 1: medir --------------------------------------------------------
DOM="$RASCUNHO/$NOME.dom.html"
# shellcheck disable=SC2086
timeout 120 "$CHROME" $COMUNS --window-size=$LARGURA,6000 \
        --dump-dom "file://$ABS_SRC" > "$DOM" 2> "$RASCUNHO/$NOME.log" || {
    echo "  ERRO: o Chromium falhou na passada de medicao (ver $RASCUNHO/$NOME.log)" >&2
    exit 4
}

# --medir: so informa o tamanho medido, para o autor preencher o <meta>
if [ "$MEDIR" -eq 1 ]; then
    M=$(sed -n 's/.*data-medido="\([0-9]*x[0-9]*\)".*/\1/p' "$DOM" | head -1)
    [ -n "$M" ] || { echo "erro: nao consegui medir (ver $DOM)" >&2; exit 6; }
    echo "$M"
    exit 0
fi

if grep -q 'FIGURA-INVALIDA' "$DOM"; then
    echo "  ERRO: guarda.js reprovou a figura:" >&2
    sed -n 's/.*<title>FIGURA-INVALIDA: \(.*\)<\/title>.*/    \1/p' "$DOM" \
        | head -1 | tr ';;' '\n' >&2
    echo "    (DOM completo em $DOM -- o PNG em figures/ NAO foi tocado)" >&2
    exit 5
fi

ALTURA=$(sed -n 's/.*data-medido="[0-9]*x\([0-9]*\)".*/\1/p' "$DOM" | head -1)
[ -n "$ALTURA" ] || {
    echo "  ERRO: nao consegui ler data-medido do DOM ($DOM)" >&2
    exit 6
}

# --- passada 2: capturar -----------------------------------------------------
TMP_PNG="$RASCUNHO/$NOME.png"
rm -f "$TMP_PNG"
# shellcheck disable=SC2086
timeout 120 "$CHROME" $COMUNS --window-size=$LARGURA,"$ALTURA" \
        --screenshot="$(readlink -f "$RASCUNHO")/$NOME.png" \
        "file://$ABS_SRC" >> "$RASCUNHO/$NOME.log" 2>&1 || {
    echo "  ERRO: o Chromium falhou na captura (ver $RASCUNHO/$NOME.log)" >&2
    exit 7
}
[ -f "$TMP_PNG" ] || { echo "  ERRO: o Chromium nao gerou $TMP_PNG" >&2; exit 8; }

# --- assercoes sobre o artefato ---------------------------------------------
ESPERADO_W=$((LARGURA * 2))
ESPERADO_H=$((ALTURA * 2))
DIM=$(python3 scripts/png_utils.py dim "$TMP_PNG" | cut -d' ' -f1)
if [ "$DIM" != "${ESPERADO_W}x${ESPERADO_H}" ]; then
    echo "  ERRO: PNG saiu $DIM, esperado ${ESPERADO_W}x${ESPERADO_H} (DPR 2)" >&2
    exit 9
fi

if ! python3 scripts/png_utils.py borda "$TMP_PNG" 8 > /dev/null 2>&1; then
    echo "  ERRO: ha conteudo a menos de 8px de alguma borda do PNG:" >&2
    python3 scripts/png_utils.py borda "$TMP_PNG" 8 >&2 || true
    exit 10
fi

# --- instalacao atomica ------------------------------------------------------
mkdir -p "$(dirname "$OUT")"
mv "$TMP_PNG" "$OUT"
echo "  ok  $OUT  (${ESPERADO_W}x${ESPERADO_H})"
