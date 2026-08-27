# =============================================================================
#  figuras.mk -- regeracao das figuras a partir de figures-src/*.html
# =============================================================================
#
#  Fica em arquivo separado, como tools/doc.mk, por depender de uma ferramenta
#  externa ao TeX Live (Chromium headless).
#
#  DECISAO CENTRAL, e ela surpreende quem le o Makefile pela primeira vez:
#
#      `figures` NUNCA e pre-requisito de `pdf`.
#
#  Os PNG em figures/ sao artefatos VERSIONADOS, nao gerados pelo build. Dois
#  motivos, os dois praticos:
#    1. um colaborador sem Chromium precisa continuar compilando o PDF;
#    2. o git nao preserva mtime -- depois de um clone a ordem de checkout e
#       arbitraria, e um PNG pode facilmente parecer mais velho que o seu HTML.
#       Se `pdf` dependesse de `figures`, todo clone novo tentaria regerar.
#
#  Regerar e, portanto, um ato explicito:  make figures
# =============================================================================

FIGSRC_DIR   := figures-src
FIG_SOURCES  := $(shell find $(FIGSRC_DIR) -mindepth 2 -name '*.html' 2>/dev/null | grep -v '/_legado/' | grep -v '/\.render/' | sort)
FIG_TARGETS  := $(patsubst $(FIGSRC_DIR)/%.html,$(FIGURES_DIR)/%.png,$(FIG_SOURCES))
FIG_ASSETS   := $(wildcard $(FIGSRC_DIR)/assets/*)
FIG_RENDER   := scripts/figura-render.sh
FIG_PNGTOOL  := scripts/png_utils.py

# Lista de TODAS as figuras -- tools/doc.mk referencia FIGURE_FILES mas nunca a
# define, entao `make doc` nao rebuildava quando uma figura mudava.
FIGURE_FILES := $(shell find $(FIGURES_DIR) -name '*.png' 2>/dev/null | sort)

# Busca do Chromium: glob da revisao (nao fixa -1234) e o binario vive no cache
# GLOBAL do usuario, nao dentro de node_modules de projeto nenhum.
FIG_CHROME := $(shell command -v chrome-headless-shell 2>/dev/null \
  || ls -1d $(HOME)/.cache/ms-playwright/chromium_headless_shell-*/chrome-headless-shell-linux64/chrome-headless-shell 2>/dev/null | sort -V | tail -1 \
  || ls -1d $(HOME)/.cache/ms-playwright/chromium-*/chrome-linux64/chrome 2>/dev/null | sort -V | tail -1 \
  || command -v chromium chromium-browser google-chrome 2>/dev/null | head -1)

.PHONY: figures figure figures-status figures-diff setup-figures clean-figures

$(FIGURES_DIR)/%.png: $(FIGSRC_DIR)/%.html $(FIG_ASSETS) $(FIG_RENDER) $(FIG_PNGTOOL)
	@echo "==> figura $*"
	@sh $(FIG_RENDER) --chrome "$(FIG_CHROME)" --out $@ $<

figures: ## Regera os PNG cujo HTML em figures-src/ mudou.
ifeq ($(strip $(FIG_CHROME)),)
	@echo "==> nenhum Chromium encontrado; figuras NAO regeradas"
	@echo "    os PNG versionados em $(FIGURES_DIR)/ continuam validos"
	@echo "    e o PDF compila normalmente. Para regerar: make setup-figures"
else
	@$(MAKE) --no-print-directory $(FIG_TARGETS)
	@echo "==> figuras em dia"
endif

figure: ## Regera uma unica figura:  make figure FIG=<nome>
	@test -n "$(FIG)" || { echo "uso: make figure FIG=<nome-sem-extensao>"; exit 1; }
	@src=$$(find $(FIGSRC_DIR) -mindepth 2 -name '$(FIG).html' ! -path '*/_legado/*' | head -1); \
	 test -n "$$src" || { echo "nao achei $(FIG).html em $(FIGSRC_DIR)/"; exit 1; }; \
	 out=$$(echo "$$src" | sed 's|^$(FIGSRC_DIR)/|$(FIGURES_DIR)/|; s|\.html$$|.png|'); \
	 echo "==> figura $(FIG)"; \
	 sh $(FIG_RENDER) --chrome "$(FIG_CHROME)" --out "$$out" "$$src"

figures-status: ## Classifica cada PNG: com fonte, desatualizada, ou legado sem fonte.
	@echo "== figuras com fonte =="
	@for s in $(FIG_SOURCES); do \
	   png=$$(echo "$$s" | sed 's|^$(FIGSRC_DIR)/|$(FIGURES_DIR)/|; s|\.html$$|.png|'); \
	   if [ ! -f "$$png" ]; then st="SEM PNG      "; \
	   elif [ "$$s" -nt "$$png" ]; then st="DESATUALIZADA"; \
	   else st="atualizada   "; fi; \
	   printf "  %s  %s\n" "$$st" "$$png"; \
	 done
	@echo "== figuras SEM fonte (legado -- nao regeraveis nem verificaveis) =="
	@n=0; for p in $$(find $(FIGURES_DIR) -name '*.png' | sort); do \
	   src=$$(echo "$$p" | sed 's|^$(FIGURES_DIR)/|$(FIGSRC_DIR)/|; s|\.png$$|.html|'); \
	   [ -f "$$src" ] || { n=$$((n+1)); }; \
	 done; echo "  $$n figuras"

figures-diff: ## Gera build/figuras/comparacao.pdf -- antiga vs nova, lado a lado.
	@sh scripts/figuras-diff.sh

setup-figures: ## Explica como obter o Chromium e instala a fonte Lato.
	@echo "Fonte Lato (obrigatoria -- e a fonte das figuras):"
	@echo "    sudo apt-get install fonts-lato"
	@echo
	@echo "Chromium headless. O caminho mais simples e o cache do Playwright:"
	@echo "    npx playwright install chromium"
	@echo "ou aponte um binario existente:"
	@echo "    make figures FIG_CHROME=/caminho/para/chrome"

clean-figures: ## Remove apenas os rascunhos do renderizador (nunca figures/).
	rm -rf $(FIGSRC_DIR)/.render
