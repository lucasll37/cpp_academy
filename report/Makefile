# =============================================================================
#  C++ Academy -- Relatorio de Topicos de C++ -- automacao de build
#
#  Alvos principais:
#    make setup     instala tudo que falta num WSL/Ubuntu recem-criado
#    make           compila o PDF completo em build/report.pdf
#    make watch     recompila automaticamente a cada gravacao de arquivo
#    make view      abre o PDF no visualizador do Windows
#    make live      watch + visualizador aberto (fluxo de edicao recomendado)
#    make draft CHAPTER=05-nos-de-controle   compila so um capitulo (rapido)
#    make new-chapter NAME=14-meu-capitulo   cria capitulo + pasta de figuras
#    make check     verifica a instalacao e lista o que falta
#    make clean     apaga os arquivos intermediarios
#    make overleaf  empacota o .zip para importar no Overleaf
#    make doc       converte para .doc no modelo PTC do IEAv (ver tools/doc.mk)
#    make figures   regera as figuras a partir de figures-src/ (ver tools/figuras.mk)
#    make figures-status   quais figuras tem fonte, quais estao desatualizadas
# =============================================================================

MAIN        := report
BUILD_DIR   := build
CHAPTERS_DIR:= chapters
FIGURES_DIR := figures
PDF         := $(BUILD_DIR)/$(MAIN).pdf

LATEXMK     := latexmk
# -interaction=nonstopmode: nao para pedindo input quando ha erro
# -file-line-error        : erros no formato arquivo:linha, clicaveis no editor
# -synctex=1              : permite saltar do PDF para o .tex e vice-versa
LATEXMKOPTS := -pdf -interaction=nonstopmode -file-line-error -synctex=1 \
               -outdir=$(BUILD_DIR)

# O \include gera um .aux por capitulo dentro do diretorio de saida; a arvore
# de subdiretorios precisa existir antes do primeiro latexmk.
AUX_DIRS    := $(BUILD_DIR)/$(CHAPTERS_DIR)

# Marca que o ultimo build foi parcial, para que o proximo `make` refaca tudo.
DRAFT_FLAG  := $(BUILD_DIR)/.draft-mode
# Injeta \onlychapter no documento; report.tex traduz isso num \includeonly.
DRAFT_PDFLATEX = -pdflatex='pdflatex %O "\def\onlychapter{$(CHAPTER)}\input{%S}"'

.DEFAULT_GOAL := pdf
.PHONY: pdf all watch live view open draft quick setup setup-full check clean distclean \
        new-chapter figures-report overleaf help

# -----------------------------------------------------------------------------
#  Compilacao
# -----------------------------------------------------------------------------
# Quem decide o que precisa ser refeito e o proprio latexmk (ele acompanha
# .aux, .toc, figuras e o numero de passagens necessarias). O alvo apenas
# garante os diretorios e desfaz um eventual estado de `draft` anterior.
pdf all: | $(AUX_DIRS)
	@echo "==> compilando $(MAIN).tex"
	@force=""; \
	 if [ -f $(DRAFT_FLAG) ]; then \
	   echo "    (build anterior era um draft de $$(cat $(DRAFT_FLAG)); refazendo tudo)"; \
	   rm -f $(DRAFT_FLAG); force="-g"; \
	 fi; \
	 $(LATEXMK) $(LATEXMKOPTS) $$force $(MAIN).tex
	@echo "==> pronto: $(PDF)"

$(AUX_DIRS):
	@mkdir -p $@

# Uma unica passagem do pdflatex. Nao atualiza sumario nem referencias
# cruzadas, mas devolve o PDF em poucos segundos -- util para conferir
# rapidamente um ajuste de texto ou de figura.
quick: | $(AUX_DIRS)
	@pdflatex -interaction=nonstopmode -file-line-error -synctex=1 \
	          -output-directory=$(BUILD_DIR) $(MAIN).tex >/dev/null; \
	 echo "==> $(PDF) (passagem unica, sumario/referencias podem estar desatualizados)"

# Compila apenas um capitulo, mantendo numeracao e referencias cruzadas
# corretas atraves do .aux ja existente:  make draft CHAPTER=06-camada-models
draft: | $(AUX_DIRS)
ifndef CHAPTER
	@echo "uso: make draft CHAPTER=<nome-do-capitulo-sem-.tex>"; \
	 echo "capitulos disponiveis:"; \
	 ls $(CHAPTERS_DIR)/*.tex | grep -v '/00-' \
	   | sed 's|$(CHAPTERS_DIR)/|  |; s|\.tex$$||'; \
	 exit 1
else
	@test -f $(CHAPTERS_DIR)/$(CHAPTER).tex || \
	  { echo "erro: $(CHAPTERS_DIR)/$(CHAPTER).tex nao existe"; exit 1; }
	@echo "==> compilando apenas $(CHAPTER)"
	@$(LATEXMK) $(LATEXMKOPTS) -g $(DRAFT_PDFLATEX) $(MAIN).tex
	@echo "$(CHAPTER)" > $(DRAFT_FLAG)
	@echo "==> pronto: $(PDF) (apenas $(CHAPTER); rode 'make' para o documento inteiro)"
endif

# -----------------------------------------------------------------------------
#  Edicao ao vivo
# -----------------------------------------------------------------------------
# Fica residente e recompila a cada gravacao de qualquer .tex ou figura.
# Com CHAPTER=<nome> observa em modo parcial, que e bem mais rapido:
#   make watch CHAPTER=06-camada-models
watch: | $(AUX_DIRS)
ifdef CHAPTER
	@test -f $(CHAPTERS_DIR)/$(CHAPTER).tex || \
	  { echo "erro: $(CHAPTERS_DIR)/$(CHAPTER).tex nao existe"; exit 1; }
	@echo "==> observando alteracoes, apenas $(CHAPTER) (Ctrl-C para sair)"
	@echo "$(CHAPTER)" > $(DRAFT_FLAG)
	@$(LATEXMK) $(LATEXMKOPTS) -pvc -view=none $(DRAFT_PDFLATEX) $(MAIN).tex
else
	@echo "==> observando alteracoes (Ctrl-C para sair)"
	@rm -f $(DRAFT_FLAG)
	@$(LATEXMK) $(LATEXMKOPTS) -pvc -view=none $(MAIN).tex
endif

# Compila, abre o PDF e entra em modo de observacao -- o fluxo de edicao
# recomendado. Aceita CHAPTER=<nome> para trabalhar num capitulo so.
# Os visualizadores do Windows se comportam de formas diferentes: Edge e
# SumatraPDF recarregam sozinhos a cada recompilacao; o Acrobat trava o
# arquivo e precisa ser fechado antes do proximo build.
live:
ifdef CHAPTER
	@$(MAKE) --no-print-directory draft CHAPTER=$(CHAPTER)
else
	@$(MAKE) --no-print-directory pdf
endif
	@$(MAKE) --no-print-directory open
	@$(MAKE) --no-print-directory watch CHAPTER=$(CHAPTER)

# Compila (se preciso) e abre o PDF no visualizador padrao.
view: pdf open

# Abre o PDF que ja existe, sem recompilar. Em WSL o caminho e convertido
# para o formato do Windows, o que faz o Edge/Acrobat abri-lo corretamente.
open:
	@test -f $(PDF) || { echo "erro: $(PDF) nao existe; rode 'make' primeiro"; exit 1; }
	@if command -v wslview >/dev/null 2>&1; then \
	   wslview $(PDF); \
	 elif grep -qi microsoft /proc/version 2>/dev/null; then \
	   explorer.exe "$$(wslpath -w $(PDF))" || true; \
	 elif command -v xdg-open >/dev/null 2>&1; then \
	   xdg-open $(PDF); \
	 else \
	   echo "nenhum visualizador encontrado; abra manualmente: $(PDF)"; \
	 fi

# -----------------------------------------------------------------------------
#  Preparacao do ambiente (WSL/Ubuntu ou Debian recem-instalado)
# -----------------------------------------------------------------------------
# Conjunto minimo que compila ESTE documento. Cerca de 1,5 GB.
APT_PACKAGES := \
	texlive-latex-base \
	texlive-latex-recommended \
	texlive-latex-extra \
	texlive-fonts-recommended \
	texlive-fonts-extra \
	texlive-lang-portuguese \
	texlive-science \
	texlive-pictures \
	texlive-plain-generic \
	latexmk \
	wslu

setup:
	@echo "==> instalando a cadeia LaTeX (pode levar varios minutos)"
	sudo apt-get update
	sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $(APT_PACKAGES)
	@echo
	@$(MAKE) --no-print-directory check

# Alternativa: instala a distribuicao TeX Live inteira (~5 GB). So vale a pena
# se voce pretende usar pacotes alem dos exigidos por este documento.
setup-full:
	sudo apt-get update
	sudo DEBIAN_FRONTEND=noninteractive apt-get install -y texlive-full latexmk wslu
	@$(MAKE) --no-print-directory check

# Verifica se cada ferramenta e cada pacote LaTeX exigido esta presente.
REQUIRED_STY := algorithm.sty algorithmic.sty montserrat.sty wallpaper.sty \
                tocbibind.sty makecell.sty silence.sty datetime.sty sectsty.sty \
                lipsum.sty tabularx.sty rotating.sty placeins.sty subcaption.sty \
                newunicodechar.sty hyperref.sty listings.sty pdfpages.sty \
                titlesec.sty enumitem.sty changepage.sty multirow.sty \
                tcolorbox.sty glossaries.sty makeidx.sty cleveref.sty tikz.sty \
                booktabs.sty pifont.sty

check:
	@echo "== ferramentas =="
	@for t in pdflatex latexmk bibtex; do \
	   if command -v $$t >/dev/null 2>&1; then echo "  ok      $$t"; \
	   else echo "  FALTA   $$t   -> rode: make setup"; fi; \
	 done
	@echo "== pacotes LaTeX =="
	@missing=0; \
	 for s in $(REQUIRED_STY); do \
	   if kpsewhich $$s >/dev/null 2>&1; then echo "  ok      $$s"; \
	   else echo "  FALTA   $$s"; missing=1; fi; \
	 done; \
	 if [ $$missing -eq 1 ]; then \
	   echo; echo "  pacotes ausentes -> rode: make setup"; \
	 fi
	@echo "== visualizador de PDF =="
	@if command -v wslview >/dev/null 2>&1; then echo "  ok      wslview"; \
	 elif grep -qi microsoft /proc/version 2>/dev/null; then echo "  ok      explorer.exe (fallback do WSL)"; \
	 elif command -v xdg-open >/dev/null 2>&1; then echo "  ok      xdg-open"; \
	 else echo "  FALTA   wslu   -> rode: make setup"; fi
	@$(MAKE) --no-print-directory check-fonte

# -----------------------------------------------------------------------------
#  Conferencia do texto contra o codigo-fonte dos subprojetos
#
#  check-fontes.py le ../src/ (a arvore do cpp_academy) como fonte de verdade:
#  todo bloco cuja legenda traz um caminho `src/...` precisa ser mesmo uma
#  transcricao daquele arquivo, e nao uma parafrase.
#
#  E pulado em silencio quando ../src/ nao esta presente (o caso de quem
#  recebeu so a pasta report/), para que o `make check` continue util a quem
#  so quer compilar o PDF.
# -----------------------------------------------------------------------------
.PHONY: check-fonte
check-fonte:
	@echo "== conferencia contra o codigo-fonte =="
	@if [ ! -d ../src ]; then \
	   echo "  pulado  (../src/ ausente)"; \
	 elif ! command -v python3 >/dev/null 2>&1; then \
	   echo "  FALTA   python3"; \
	 else \
	   python3 scripts/check-fontes.py --quiet || exit 1; \
	 fi

# -----------------------------------------------------------------------------
#  Autoria
# -----------------------------------------------------------------------------
# Cria o arquivo do capitulo, a pasta de figuras correspondente e lembra de
# acrescentar o \include:   make new-chapter NAME=12-meu-capitulo
new-chapter:
ifndef NAME
	@echo "uso: make new-chapter NAME=12-nome-do-capitulo"; exit 1
else
	@test ! -f $(CHAPTERS_DIR)/$(NAME).tex || \
	  { echo "erro: $(CHAPTERS_DIR)/$(NAME).tex ja existe"; exit 1; }
	@mkdir -p $(FIGURES_DIR)/$(NAME)
	@touch $(FIGURES_DIR)/$(NAME)/.gitkeep
	@printf '%% =============================================================================\n%%  $(NAME)\n%%  Figuras deste capitulo: $(FIGURES_DIR)/$(NAME)/\n%% =============================================================================\n\n\\section{TITULO}\n\\label{sec:$(NAME)}\n\n' \
	   > $(CHAPTERS_DIR)/$(NAME).tex
	@echo "==> criados $(CHAPTERS_DIR)/$(NAME).tex e $(FIGURES_DIR)/$(NAME)/"
	@echo "    falta acrescentar a $(MAIN).tex:  \\include{$(CHAPTERS_DIR)/$(NAME)}"
endif

# Relatorio de figuras: quantas ha por capitulo, quais estao referenciadas no
# texto mas nao existem em disco e quais existem mas nunca sao usadas.
figures-report:
	@echo "== figuras por capitulo =="
	@for d in $(FIGURES_DIR)/*/; do \
	   n=$$(find "$$d" -type f ! -name '.gitkeep' | wc -l); \
	   printf "  %-34s %s\n" "$$d" "$$n"; \
	 done
	@echo "== referenciadas no texto mas ausentes em disco =="
	@cat $(CHAPTERS_DIR)/*.tex | sed 's/^%.*//' \
	   | grep -o '\\includegraphics\(\[[^]]*\]\)\?{[^}]*}' \
	   | sed 's/.*{\(.*\)}/\1/' | sort -u | while read f; do \
	       [ -f "$$f" ] || [ -f "$$f.png" ] || [ -f "$$f.pdf" ] || echo "  AUSENTE  $$f"; \
	   done
	@echo "== presentes em disco mas nunca referenciadas =="
	@find $(FIGURES_DIR) -type f ! -name '.gitkeep' | while read f; do \
	     grep -qF "$$(basename $$f)" $(CHAPTERS_DIR)/*.tex || echo "  SEM USO  $$f"; \
	   done

# -----------------------------------------------------------------------------
#  Exportacao para o Overleaf
#
#  Empacota so o que o Overleaf precisa para compilar. Fica de fora tudo que
#  serve apenas ao fluxo local -- build/, tmp/, _legado-mixr/, tools/,
#  scripts/, templates/ -- e tambem o .latexmkrc, de proposito: ele
#  fixa $out_dir='build', e o Overleaf nao encontraria o PDF resultante.
#
#  O .zip leva na raiz o LEIA-ME-OVERLEAF.txt, que explica o unico ajuste
#  manual necessario apos a importacao (definir report.tex como documento
#  principal, ja que o \documentclass mora em assets/MASAfrontmatter.tex).
# -----------------------------------------------------------------------------
OVERLEAF_ZIP  := $(BUILD_DIR)/$(MAIN)-overleaf.zip
OVERLEAF_SRC  := $(MAIN).tex references.bib new-aiaa.bst \
                 assets $(CHAPTERS_DIR) $(FIGURES_DIR)
OVERLEAF_NOTE := tools/LEIA-ME-OVERLEAF.txt

overleaf:
	@command -v zip >/dev/null 2>&1 || \
	  { echo "erro: 'zip' nao encontrado -> sudo apt-get install -y zip"; exit 1; }
	@for f in $(OVERLEAF_SRC); do \
	   test -e "$$f" || { echo "erro: $$f nao existe"; exit 1; }; \
	 done
	@mkdir -p $(BUILD_DIR)
	@rm -f $(OVERLEAF_ZIP)
	@zip -q -r $(OVERLEAF_ZIP) $(OVERLEAF_SRC) \
	     -x '*.gitkeep' '*.aux' '*.log' '*.synctex.gz' '*/.DS_Store'
	@zip -q -j $(OVERLEAF_ZIP) $(OVERLEAF_NOTE)
	@echo "==> pronto: $(OVERLEAF_ZIP) ($$(du -h $(OVERLEAF_ZIP) | cut -f1), $$(zipinfo -t $(OVERLEAF_ZIP) | cut -d' ' -f1) arquivos)"
	@echo "    Overleaf: New Project > Upload Project"
	@echo "    depois:   Menu > Settings > Main document = $(MAIN).tex"

# -----------------------------------------------------------------------------
#  Limpeza
# -----------------------------------------------------------------------------
# Remove os intermediarios (.aux, .log, .toc, ...) e mantem o PDF.
clean:
	@$(LATEXMK) -c -outdir=$(BUILD_DIR) $(MAIN).tex >/dev/null 2>&1 || true
	@echo "==> intermediarios removidos (PDF preservado)"

# Remove o diretorio de build inteiro, PDF incluido.
distclean:
	@rm -rf $(BUILD_DIR)
	@echo "==> $(BUILD_DIR)/ removido"

help:
	@sed -n '2,17p' Makefile | sed 's/^#\s\?//'

# -----------------------------------------------------------------------------
#  Conversao para .doc no modelo PTC do IEAv (make doc / make doc-check)
#  Fica em arquivo separado por depender de ferramentas externas ao TeX Live.
# -----------------------------------------------------------------------------
include tools/doc.mk
include tools/figuras.mk
