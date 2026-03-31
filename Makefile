.PHONY: clean configure build install package run test all help
.PHONY: run
.PHONY: python

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
DIST_DIR := $(PWD)/dist

# Build configuration
BUILD_TYPE := Debug
CONAN_BUILD_TYPE := Debug

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# ============================================
# C++ Build Targets
# ============================================
all: clean configure build ## install package ## Full pipeline: clean, configure, build, install and package.

configure: ## Configure the project for building.
	mkdir -p $(BUILD_DIR)/
	conan install ./ \
		--build=missing \
		--settings=build_type=$(CONAN_BUILD_TYPE) \
		--remote=conancenter
		
	meson setup --reconfigure \
		--backend ninja \
		--buildtype debug \
		--prefix=$(DIST_DIR) \
		--libdir=$(DIST_DIR)/lib \
		-Dpkg_config_path=$(DIST_DIR)/lib/pkgconfig:$(BUILD_DIR) \
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
	$(BUILD_DIR)/src/main

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'