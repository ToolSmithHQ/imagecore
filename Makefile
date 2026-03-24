
.PHONY: help build test test-samples clean build-ios build-android

CORE_DIR = packages/core
BUILD_DIR = $(CORE_DIR)/build
SAMPLES_DIR = $(CORE_DIR)/tests/samples
NPROC = $(shell sysctl -n hw.logicalcpu 2>/dev/null || nproc)

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-25s\033[0m %s\n", $$1, $$2}'

# ── Build ────────────────────────────────────────────────────────────── #

build: ## Build C++ core for macOS (with tests)
	cd $(CORE_DIR) && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIC_BUILD_TESTS=ON
	cd $(CORE_DIR) && cmake --build build --config Release -j$(NPROC)

build-ios: ## Build prebuilt .a libraries for iOS (device + simulator)
	bash scripts/build-ios.sh

build-android: ## Build prebuilt .a libraries for Android (arm64-v8a)
	bash scripts/build-android.sh

# ── Test ─────────────────────────────────────────────────────────────── #

test: build ## Run C++ tests (without sample images)
	$(BUILD_DIR)/test_imagecore

test-samples: build ## Run C++ tests with sample images (63 tests)
	$(BUILD_DIR)/test_imagecore "$$(cd $(CORE_DIR) && pwd)/tests/samples/"

# ── Clean ────────────────────────────────────────────────────────────── #

clean: ## Remove all build directories
	rm -rf $(CORE_DIR)/build $(CORE_DIR)/build-*
