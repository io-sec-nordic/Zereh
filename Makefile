CC ?= cc
PKG_CONFIG ?= pkg-config
BUILD_DIR ?= build
BIN_DIR := $(BUILD_DIR)/bin
XDP_OUT_DIR := $(BUILD_DIR)/xdp
GEN_DIR := $(BUILD_DIR)/generated
VERSION_FILE ?= VERSION
ZEREH_VERSION ?= $(shell cat $(VERSION_FILE) 2>/dev/null || echo 0.1.0-dev)
ZEREH_REVISION ?= $(shell git rev-parse --short=12 HEAD 2>/dev/null || echo nogit)
ZEREH_BUILD_SCRIPT := scripts/gen_build_metadata.sh

LIBBPF_CFLAGS := $(shell $(PKG_CONFIG) --cflags libbpf 2>/dev/null)
LIBBPF_LIBS := $(shell $(PKG_CONFIG) --libs libbpf 2>/dev/null)
YAML_CFLAGS := $(shell $(PKG_CONFIG) --cflags yaml-0.1 2>/dev/null || $(PKG_CONFIG) --cflags yaml 2>/dev/null)
YAML_LIBS := $(shell $(PKG_CONFIG) --libs yaml-0.1 2>/dev/null || $(PKG_CONFIG) --libs yaml 2>/dev/null)

CFLAGS_COMMON := -O2 -g -Wall -Wextra -std=gnu11 -I./include -I./control -I./$(GEN_DIR)

CONTROL_SRCS := \
	control/main.c \
	control/config.c \
	control/hash.c \
	control/yaml_parser.c \
	control/codegen.c \
	control/compiler.c \
	control/loader.c

CONTROL_BIN := $(BIN_DIR)/zerehctl
APPS_BIN := $(BIN_DIR)/zereh_rx $(BIN_DIR)/mock_dns_server
BUILD_METADATA_HDR := $(GEN_DIR)/zereh_build_info.h
BUILD_LICENSE_TXT := $(GEN_DIR)/zereh_license.txt
CANARY_IFACE ?= eth0
PRIMARY_IFACE ?=
PROMOTE ?= 0

.PHONY: all build-metadata deps-check generate load unload generate-load deploy-dev deploy-prod deploy-canary health-check rollback-release format fmt-check lint clean

all: $(CONTROL_BIN) $(APPS_BIN)

$(BIN_DIR) $(XDP_OUT_DIR) $(GEN_DIR):
	mkdir -p $@

build-metadata: | $(GEN_DIR)
	@bash $(ZEREH_BUILD_SCRIPT) "$(GEN_DIR)" "$(ZEREH_VERSION)" "$(ZEREH_REVISION)"

$(CONTROL_BIN): $(CONTROL_SRCS) include/zereh_license.h build-metadata | $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(LIBBPF_CFLAGS) $(YAML_CFLAGS) -o $@ $(CONTROL_SRCS) $(LIBBPF_LIBS) $(YAML_LIBS)

$(BIN_DIR)/zereh_rx: apps/zereh_rx.c build-metadata | $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) $(LIBBPF_CFLAGS) -o $@ $< $(LIBBPF_LIBS)

$(BIN_DIR)/mock_dns_server: apps/mock_dns_server.c build-metadata | $(BIN_DIR)
	$(CC) $(CFLAGS_COMMON) -o $@ $<

deps-check:
	@set -e; \
	missing=0; \
	command -v $(CC) >/dev/null 2>&1 || { echo "[deps] missing C compiler: $(CC)"; missing=1; }; \
	command -v clang >/dev/null 2>&1 || { echo "[deps] missing clang"; missing=1; }; \
	command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo "[deps] missing pkg-config"; missing=1; }; \
	$(PKG_CONFIG) --exists libbpf || { echo "[deps] missing libbpf development package (pkg-config: libbpf)"; missing=1; }; \
	($(PKG_CONFIG) --exists yaml-0.1 || $(PKG_CONFIG) --exists yaml) || { echo "[deps] missing libyaml development package (pkg-config: yaml-0.1 or yaml)"; missing=1; }; \
	if [ $$missing -ne 0 ]; then \
		echo "[deps] install missing dependencies and retry"; \
		exit 1; \
	fi; \
	echo "[deps] dependency check passed"

generate: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	./$(CONTROL_BIN) generate -c config.yaml

load: deps-check $(CONTROL_BIN)
	sudo ./$(CONTROL_BIN) load -c config.yaml

unload: $(CONTROL_BIN)
	sudo ./$(CONTROL_BIN) unload -c config.yaml

generate-load: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	sudo ./$(CONTROL_BIN) generate-load -c config.yaml

deploy-dev: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	bash scripts/deploy_dev.sh config.yaml

deploy-prod: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	bash scripts/deploy_prod.sh config.yaml

deploy-canary: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	bash scripts/deploy_canary.sh config.yaml $(CANARY_IFACE) $(PRIMARY_IFACE) $(if $(filter 1,$(PROMOTE)),--promote,)

health-check: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	bash scripts/health_check.sh config.yaml

rollback-release: deps-check $(CONTROL_BIN) | $(XDP_OUT_DIR)
	bash scripts/rollback_release.sh previous

format:
	@set -e; \
	if command -v clang-format >/dev/null 2>&1; then \
		find control xdp apps include -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r clang-format -i; \
		echo "[format] formatted C/C headers with clang-format"; \
	else \
		echo "[format] clang-format not found; skipping C/C header formatting"; \
	fi; \
	if command -v shfmt >/dev/null 2>&1; then \
		find scripts -type f -name '*.sh' -print0 | xargs -0 -r shfmt -w -i 2 -bn -ci; \
		echo "[format] formatted shell scripts with shfmt"; \
	else \
		echo "[format] shfmt not found; skipping shell formatting"; \
	fi

fmt-check:
	@set -e; \
	status=0; \
	if command -v clang-format >/dev/null 2>&1; then \
		if ! find control xdp apps include -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -r clang-format --dry-run --Werror; then \
			echo "[fmt-check] C/C header formatting check failed"; \
			status=1; \
		fi; \
	else \
		echo "[fmt-check] clang-format not found; skipping C/C header check"; \
	fi; \
	if command -v shfmt >/dev/null 2>&1; then \
		if ! find scripts -type f -name '*.sh' -print0 | xargs -0 -r shfmt -d -i 2 -bn -ci >/dev/null; then \
			echo "[fmt-check] shell formatting check failed"; \
			status=1; \
		fi; \
	else \
		echo "[fmt-check] shfmt not found; skipping shell formatting check"; \
	fi; \
	if [ $$status -ne 0 ]; then \
		exit 1; \
	fi; \
	echo "[fmt-check] formatting checks passed"

lint: fmt-check
	@set -e; \
	if command -v shellcheck >/dev/null 2>&1; then \
		find scripts -type f -name '*.sh' -print0 | xargs -0 -r shellcheck; \
		echo "[lint] shellcheck passed"; \
	else \
		echo "[lint] shellcheck not found; skipping shell lint"; \
	fi

clean:
	rm -rf $(BUILD_DIR)
	rm -f control/zerehctl apps/zereh_rx apps/mock_dns_server xdp/router_generated.c xdp/router_generated.o
