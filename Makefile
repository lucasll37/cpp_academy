.ONESHELL:
SHELL = /bin/bash
.SHELLFLAGS = -ec

.PHONY: clean configure build install package all run test test-cmake smoke help
.PHONY: run
.PHONY: python

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := $(PWD)/build
DIST_DIR := $(PWD)/dist

# Build configuration
BUILD_TYPE       := debug   # meson: debug | release | minsize
CONAN_BUILD_TYPE := Debug   # conan: Debug | Release

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# ============================================
# C++ Build Targets
# ============================================
all: clean configure build install ## install package ## Full pipeline: clean, configure, build, install and package.

configure: ## Configure the project for building.
	mkdir -p $(BUILD_DIR)/

	conan install ./ \
		--profile:build=default \
		--profile:host=default \
		--build=missing \
		--settings=build_type=$(CONAN_BUILD_TYPE) \
		--remote=conancenter \
		-c tools.system.package_manager:mode=install \
		-c tools.system.package_manager:sudo=True

	source $(BUILD_DIR)/conanbuild.sh

	meson setup --reconfigure \
		--backend ninja \
		--buildtype=$(BUILD_TYPE) \
		--prefix=$(DIST_DIR) \
		--libdir=$(DIST_DIR)/lib \
		-Dpkg_config_path=$(BUILD_DIR) \
		$(BUILD_DIR)/ .


build: ## Build all targets in the project.
	source $(BUILD_DIR)/conanbuild.sh
	meson compile -C $(BUILD_DIR)

install: ## Install all targets in the project.
	meson install -C $(BUILD_DIR)

package: ## Package the project using conan.
	conan create ./ \
		--build=missing \
		--settings=compiler.cppstd=20 \
		--settings=build_type=Debug
		
	conan create ./ \
		--build=missing \
		--settings=compiler.cppstd=20 \
		--settings=build_type=Release

test: ## Run all tests (meson test: src/testing, src/meson).
	# ⚠ `--test-args` é repassado a TODOS os alvos de teste. As flags abaixo são
	#   do GoogleTest; hoje todos os alvos são binários gtest, então funciona —
	#   mas um alvo de teste que NÃO seja gtest passaria a receber flags que não
	#   entende e falharia. Se acrescentar um, mova as flags para `env:` no
	#   `test()` do subprojeto.
	#
	# ⚠ Os testes de src/cmake/ NÃO rodam aqui: aquele subprojeto tem build
	#   próprio, de propósito (é sobre CMake). Use `make test-cmake`.
	meson test -C $(BUILD_DIR) \
		--print-errorlogs \
		--verbose \
		--test-args '--gtest_print_time=1 --gtest_color=yes'

test-cmake: ## Run the CTest suite of src/cmake (separate build system).
	cd src/cmake && \
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
	      -DCMAKE_PREFIX_PATH="$$(sed -n 's|^prefix=||p' ../../build/gtest.pc);$$(sed -n 's|^prefix=||p' ../../build/protobuf.pc)" && \
	cmake --build build -j && \
	ctest --test-dir build --output-on-failure

smoke: ## Run every installed binary and report the ones that fail.
	# Teste de fumaça: só verifica que cada binário roda e sai com 0. Não checa
	# resultado nenhum — mas pega crash, abort e regressão de dependência, que é
	# exatamente o que um repositório com 97 binários precisa vigiar.
	#
	# Os que exigem servidor, GUI ou argumento ficam de fora, com o motivo à vista.
	@falhas=0; total=0; \
	for b in $(DIST_DIR)/bin/*; do \
	  n=$$(basename $$b); \
	  case $$n in \
	    qt_*|*sfml_bullet*|rabbitMQ_*|aws-s3) \
	      echo "  pula   $$n  (precisa de GUI ou de servidor)"; continue;; \
	    yolo) \
	      echo "  pula   $$n  (exige modelo .onnx e imagem como argumentos)"; continue;; \
	  esac; \
	  total=$$((total+1)); \
	  if timeout 120 $$b >/dev/null 2>&1; then echo "  ok     $$n"; \
	  else echo "  FALHOU $$n (exit $$?)"; falhas=$$((falhas+1)); fi; \
	done; \
	echo; echo "  $$((total-falhas))/$$total binários executaram com código 0"; \
	test $$falhas -eq 0

clean: ## Clean all generated build files in the project.
	rm -rf $(BUILD_DIR)/
	rm -rf $(DIST_DIR)/
	rm -rf ./subprojects/packagecache
	rm -rf ./tmp

# `make run BIN=nome` — roda um binário instalado.
#
# ⚠ Antes isto era `$(DIST_DIR)/bin/qt_academy` fixo: um aplicativo Qt, que num
#   ambiente sem servidor gráfico simplesmente trava. Num repositório com 97
#   binários, fixar UM — e logo um que não roda headless — é a escolha errada.
BIN ?= syntax

run: ## Run one installed binary:  make run BIN=concurrency2
	@test -x "$(DIST_DIR)/bin/$(BIN)" || { \
	  echo "não existe: $(DIST_DIR)/bin/$(BIN)"; \
	  echo "disponíveis:"; ls "$(DIST_DIR)/bin" | column -c 100; exit 1; }
	$(DIST_DIR)/bin/$(BIN)

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'