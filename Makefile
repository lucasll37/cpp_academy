SHELL := /bin/bash

.PHONY: clean setup configure build install package run test help
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

# setup: ## Configure the project for building.
# 	mkdir -p $(BUILD_DIR)/
# 	conan install ./ \
# 		--profile:build=default \
# 		--profile:host=default \
# 		--build=missing \
# 		--settings=build_type=$(CONAN_BUILD_TYPE) \
# 		--remote=conancenter

configure: ## Configure the project for building.
	mkdir -p $(BUILD_DIR)/

	conan install ./ \
		--profile:build=default \
		--profile:host=default \
		--build=missing \
		--settings=build_type=$(CONAN_BUILD_TYPE) \
		--remote=conancenter

	meson setup --reconfigure \
		--backend ninja \
		--buildtype=$(BUILD_TYPE) \
		--prefix=$(DIST_DIR) \
		--libdir=$(DIST_DIR)/lib \
		-Dpkg_config_path=$(BUILD_DIR) \
		$(BUILD_DIR)/ .


build: ## Build all targets in the project.
	meson compile -C $(BUILD_DIR)

install: ## Install all targets in the project.
	meson install -C $(BUILD_DIR)

package: ## Package the project using conan.
	conan create ./ \
		--build=missing \
		--settings=compiler.cppstd=17 \
		--settings=build_type=Debug
		
	conan create ./ \
		--build=missing \
		--settings=compiler.cppstd=17 \
		--settings=build_type=Release

test: ## Run all tests.
	meson test -C $(BUILD_DIR) \
 		--print-errorlogs \
		--verbose \
		--test-args '--gtest_output=json:test_results.json --gtest_print_time=1 --gtest_color=yes'

clean: ## Clean all generated build files in the project.
	rm -rf $(BUILD_DIR)/
	rm -rf $(DIST_DIR)/
	rm -rf ./subprojects/packagecache

run: ## Run the code.
	$(DIST_DIR)/bin/qt_academy

release: ## Create and push a release tag (usage: make release VERSION=1.2.3).
	@test -n "$(VERSION)" || (echo "Usage: make release VERSION=x.y.z"; exit 1)
	git tag v$(VERSION)
	git push origin v$(VERSION)

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'