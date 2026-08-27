# ==============================================================================
# Python 3.12 Environment Manager
# ==============================================================================

PYTHON        := python3.12
VENV_DIR      := .venv
VENV_PYTHON   := $(VENV_DIR)/bin/python
VENV_PIP      := $(VENV_DIR)/bin/pip
SENTINEL      := $(VENV_DIR)/.setup_done

REQUIREMENTS  := requirements.txt

.PHONY: all setup clean reset run wheel help

all: setup

$(SENTINEL): $(REQUIREMENTS)
	@echo "[1/3] Criando ambiente virtual com $(PYTHON)..."
	$(PYTHON) -m venv $(VENV_DIR)
	@echo "[2/3] Atualizando pip, setuptools, wheel..."
	$(VENV_PIP) install --upgrade pip setuptools wheel
	@echo "[3/3] Instalando dependências de $(REQUIREMENTS)..."
	$(VENV_PIP) install -r $(REQUIREMENTS)
	@touch $(SENTINEL)
	@echo "✓ Ambiente pronto. Ative com: source $(VENV_DIR)/bin/activate"

setup: $(SENTINEL) ## Cria o venv e instala dependências (idempotente via sentinel)

clean: ## Remove o ambiente virtual e o sentinel
	@echo "Removendo $(VENV_DIR)..."
	rm -rf $(VENV_DIR)
	@echo "✓ Ambiente removido."

reset: clean setup ## Limpa e recria o ambiente do zero

# ------------------------------------------------------------------------------
# Onde os .so ficam depois de `make build` na RAIZ do repositorio.
# Este caminho e derivado, nunca escrito a mao: a versao anterior tinha
# /home/lucas/... cravado e nao rodava em nenhuma outra maquina.
# ------------------------------------------------------------------------------
REPO_ROOT   := $(abspath $(CURDIR)/../..)
EXT_DIR     := $(REPO_ROOT)/build/src/python

run: setup ## Roda os dois scripts contra os .so de build/src/python
	@test -d $(EXT_DIR) || { echo "$(EXT_DIR) nao existe — rode 'make build' na raiz"; exit 1; }
	PYTHONPATH=$(EXT_DIR) $(VENV_PYTHON) ./main_cpython.py
	PYTHONPATH=$(EXT_DIR) $(VENV_PYTHON) ./main_pybind.py

wheel: setup ## Instala standalone/ como wheel no venv (nao precisa de PYTHONPATH)
	$(VENV_PIP) install --no-build-isolation ./standalone
	$(VENV_PYTHON) -c "import academy_cp; print(academy_cp.__file__)"

help: ## Exibe esta mensagem de ajuda
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'