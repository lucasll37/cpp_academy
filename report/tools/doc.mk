# =============================================================================
#  Conversao do relatorio para .doc no modelo PTC do IEAv.
#  Incluido pelo Makefile principal.
#
#  Alvos:
#    make doc         gera build/doc/report.doc a partir do LaTeX
#    make doc-check   lista as pendencias e as ferramentas que faltam
#    make open-doc    abre o .doc no visualizador do Windows
#    make setup-doc   instala pandoc e LibreOffice
#    make clean-doc   apaga apenas os intermediarios da conversao
#
#  O .doc e SAIDA GERADA: editar o Word e recompilar sobrescreve tudo. O texto
#  continua morando em chapters/*.tex e os dados da capa em ieav-metadata.yaml.
# =============================================================================

TOOLS_DIR   := tools
DOC_DIR     := $(BUILD_DIR)/doc
DOC_META    := $(TOOLS_DIR)/ieav-metadata.yaml
DOCX        := $(DOC_DIR)/$(MAIN).docx
DOC         := $(DOC_DIR)/$(MAIN).doc

# O pandoc do sistema tem prioridade; o binario estatico em tools/vendor/ e o
# plano B para maquinas sem permissao de instalar pacotes (ver setup-doc e
# tools/vendor/README.md -- depois do apt, aquele diretorio pode ser apagado).
PANDOC := $(shell command -v pandoc 2>/dev/null || echo $(TOOLS_DIR)/vendor/pandoc/bin/pandoc)

# As imagens sao referenciadas ora pelo caminho completo, ora so pelo nome:
# esta lista espelha o \graphicspath do preambulo, mas gerada do disco.
DOC_RESOURCE_PATH := .:$(FIGURES_DIR):$(shell find $(FIGURES_DIR) -mindepth 1 -maxdepth 1 -type d | tr '\n' ':')

.PHONY: doc doc-check setup-doc open-doc clean-doc

# -----------------------------------------------------------------------------
#  Pipeline
# -----------------------------------------------------------------------------
# 1. corpo: capitulos concatenados, com as referencias cruzadas ja resolvidas
#    pelos numeros que o LaTeX gravou nos .aux -- por isso depende do PDF.
$(DOC_DIR)/body.tex: $(PDF) $(TEX_SOURCES) $(TOOLS_DIR)/tex_preprocess.py
	@echo "==> [1/5] preparando o corpo LaTeX"
	@python3 $(TOOLS_DIR)/tex_preprocess.py --root . --build $(BUILD_DIR) --out $(DOC_DIR)

# 2. formatacao: Arial 12, margens 3-2-3-2, numeracao progressiva, cabecalho
#    com o numero de pagina. Espelha a secao FORMATACAO do modelo do IEAv.
$(DOC_DIR)/reference.docx: $(DOC_META) $(TOOLS_DIR)/build_reference_docx.py
	@echo "==> [2/5] gerando a folha de estilos do Word"
	@mkdir -p $(DOC_DIR)
	@python3 $(TOOLS_DIR)/build_reference_docx.py --pandoc $(PANDOC) \
	    --metadata $(DOC_META) --out $@

# 3. capa, resumo, listas, sumario e referencias
$(DOC_DIR)/frontmatter.md $(DOC_DIR)/backmatter.md: $(DOC_META) references.bib \
                                                    $(MAIN).tex \
                                                    $(TOOLS_DIR)/gen_frontmatter.py
	@echo "==> [3/5] montando os elementos pre e pos-textuais"
	@mkdir -p $(DOC_DIR)
	@python3 $(TOOLS_DIR)/gen_frontmatter.py --metadata $(DOC_META) \
	    --bib references.bib --master $(MAIN).tex --out $(DOC_DIR)

# 4. conversao propriamente dita. Corpo e pre-textual sao lidos separadamente
#    (LaTeX e Markdown) e unidos no nivel da arvore sintatica.
$(DOCX): $(DOC_DIR)/body.tex $(DOC_DIR)/frontmatter.md $(DOC_DIR)/backmatter.md \
         $(DOC_DIR)/reference.docx $(TOOLS_DIR)/abnt.lua $(TOOLS_DIR)/merge_ast.py \
         $(FIGURE_FILES)
	@echo "==> [4/5] convertendo com o pandoc"
	@$(PANDOC) $(DOC_DIR)/body.tex -f latex -t json \
	    --lua-filter=$(TOOLS_DIR)/abnt.lua -M eqnums_file=$(DOC_DIR)/eqnums.json \
	    -o $(DOC_DIR)/body.json
	@$(PANDOC) $(DOC_DIR)/frontmatter.md -f markdown -t json -o $(DOC_DIR)/front.json
	@$(PANDOC) $(DOC_DIR)/backmatter.md  -f markdown -t json -o $(DOC_DIR)/back.json
	@python3 $(TOOLS_DIR)/merge_ast.py $(DOC_DIR)/front.json $(DOC_DIR)/body.json \
	    $(DOC_DIR)/back.json -o $(DOC_DIR)/report.json
	@$(PANDOC) $(DOC_DIR)/report.json -f json -o $@ \
	    --reference-doc=$(DOC_DIR)/reference.docx \
	    --highlight-style=tango \
	    --resource-path="$(DOC_RESOURCE_PATH)"

# 5. .docx -> .doc (Word 97-2003), via LibreOffice ou via o proprio Word.
#    O conversor tambem recalcula os campos, para que o sumario e as listas
#    cheguem preenchidos em vez de vazios.
$(DOC): $(DOCX) $(TOOLS_DIR)/office_convert.py
	@echo "==> [5/5] convertendo para .doc (Word 97-2003)"
	@python3 $(TOOLS_DIR)/office_convert.py $(DOCX) --to doc --outdir $(DOC_DIR)

doc: $(DOC)
	@echo
	@echo "==> pronto: $(DOC)"
	@echo "    (o .docx equivalente fica em $(DOCX))"
	@if [ -s $(DOC_DIR)/pendencias.txt ]; then \
	   echo; echo "    pendencias marcadas como [A PREENCHER] no documento:"; \
	   sed 's/^/      - /' $(DOC_DIR)/pendencias.txt; \
	   echo "    edite $(DOC_META) e rode 'make doc' de novo."; \
	 fi

# -----------------------------------------------------------------------------
#  Diagnostico
# -----------------------------------------------------------------------------
doc-check:
	@echo "== ferramentas de conversao =="
	@if [ -x "$(PANDOC)" ] || command -v $(PANDOC) >/dev/null 2>&1; then \
	   echo "  ok      pandoc   ($(PANDOC), $$($(PANDOC) --version | head -1))"; \
	 else echo "  FALTA   pandoc   -> rode: make setup-doc"; fi
	@if python3 -c "import yaml" 2>/dev/null; then echo "  ok      python3-yaml"; \
	 else echo "  FALTA   python3-yaml   -> rode: make setup-doc"; fi
	@engines=$$(python3 $(TOOLS_DIR)/office_convert.py x --check 2>/dev/null); \
	 if [ "$$engines" = "nenhum" ] || [ -z "$$engines" ]; then \
	   echo "  FALTA   conversor .doc (LibreOffice ou Word)  -> rode: make setup-doc"; \
	   if command -v powershell.exe >/dev/null 2>&1 && \
	      ! ls /proc/sys/fs/binfmt_misc/WSLInterop* >/dev/null 2>&1; then \
	     echo "          o Word esta instalado, mas o WSL perdeu o handler binfmt_misc"; \
	     echo "          que executa .exe. Para reativa-lo nesta sessao:"; \
	     echo "            sudo sh -c 'echo \":WSLInterop:M::MZ::/init:PF\" > /proc/sys/fs/binfmt_misc/register'"; \
	   fi; \
	 else echo "  ok      conversor .doc  ($$engines)"; fi
	@echo "== pendencias de conteudo =="
	@if [ -f $(DOC_DIR)/pendencias.txt ]; then \
	   sed 's/^/  - /' $(DOC_DIR)/pendencias.txt; \
	 else echo "  (rode 'make doc' para gerar a lista)"; fi

# LibreOffice e a opcao portatil; em WSL com Word instalado, o proprio Word ja
# serve de conversor e nada precisa ser instalado no Linux alem do pandoc.
setup-doc:
	@echo "==> instalando pandoc, LibreOffice e python3-yaml"
	sudo apt-get update
	sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
	    pandoc libreoffice-writer python3-yaml
	@$(MAKE) --no-print-directory doc-check

open-doc: $(DOC)
	@if command -v wslview >/dev/null 2>&1; then wslview $(DOC); \
	 elif grep -qi microsoft /proc/version 2>/dev/null; then \
	   explorer.exe "$$(wslpath -w $(DOC))" || true; \
	 elif command -v xdg-open >/dev/null 2>&1; then xdg-open $(DOC); \
	 else echo "nenhum visualizador encontrado; abra manualmente: $(DOC)"; fi

clean-doc:
	@rm -rf $(DOC_DIR)
	@echo "==> $(DOC_DIR)/ removido"
