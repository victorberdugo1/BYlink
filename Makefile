# BYlink Makefile
# Windows: builds directly with MinGW g++ through PowerShell. CMake is NOT required.
# Linux/macOS: use the CMake build defined below.

ifeq ($(OS),Windows_NT)

POWERSHELL ?= powershell.exe
CXX ?= g++

.PHONY: all build test opengl-demo pong-demo clean help

all: build

build:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Target build -Cxx "$(CXX)"

test:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Target test -Cxx "$(CXX)"

opengl-demo:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Target opengl-demo -Cxx "$(CXX)"

pong-demo:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Target pong-demo -Cxx "$(CXX)"

clean:
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1 -Target clean

help:
	@echo make                  Build build/bylc.exe and refresh root bylc.exe with MinGW
	@echo make test             Build and run platform-independent unit tests
	@echo make opengl-demo      Build and launch a static OpenGL scene
	@echo make pong-demo        Build and launch interactive BYGL 2 Pong
	@echo make clean            Remove build artifacts

else

BUILD_DIR ?= build
CMAKE ?= cmake
CTEST ?= ctest
PYTHON ?= python3
PYTEST ?= PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 $(PYTHON) -m pytest
JOBS ?= 2

.PHONY: all configure build test test-research validate-reference llm-smoke bygl-emit-cpp opengl-check opengl-demo pong-demo audit clean help

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

test: build
	$(CTEST) --test-dir $(BUILD_DIR) --output-on-failure

test-research:
	$(PYTEST) -q research/neural_bridge/tests/test_alignment.py research/neural_bridge/tests/test_accounting.py

validate-reference: build
	BYLC="$(CURDIR)/$(BUILD_DIR)/bylc" $(PYTHON) research/neural_bridge/validate_reference.py

llm-smoke: build
	$(BUILD_DIR)/bylc examples/add.byai -o $(BUILD_DIR)/llm-smoke
	@set +e; $(BUILD_DIR)/llm-smoke; result=$$?; set -e; \
	  if [ "$$result" -ne 2 ]; then echo "FAIL: expected exit code 2, got $$result"; exit 1; fi
	@echo "PASS: examples/add.byai compiled and returned 2"

bygl-emit-cpp: build
	$(BUILD_DIR)/bylc examples/rotating_scene.bygl --emit-cpp -o $(BUILD_DIR)/rotating_scene.cpp
	@echo "PASS: BYGL parsed and generated C++ at $(BUILD_DIR)/rotating_scene.cpp"

opengl-check:
	@pkg-config --exists glfw3 gl || { echo "Missing OpenGL development dependencies. On Debian/Ubuntu/WSL run: sudo apt install pkg-config libglfw3-dev libgl1-mesa-dev"; exit 1; }
	@echo "PASS: GLFW and OpenGL development packages found"

opengl-demo: build opengl-check
	$(BUILD_DIR)/bylc examples/rotating_scene.bygl -o $(BUILD_DIR)/rotating_scene
	$(BUILD_DIR)/rotating_scene

pong-demo: build opengl-check
	$(BUILD_DIR)/bylc examples/pong.bygl -o $(BUILD_DIR)/pong
	$(BUILD_DIR)/pong

audit:
	$(PYTHON) research/neural_bridge/audit_env.py

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR) .pytest_cache
	find research -type d -name __pycache__ -prune -exec rm -rf {} +

help:
	@echo "make                  Build bylc and tests"
	@echo "make test             Build and run the C++ compiler test suite"
	@echo "make test-research    Run alignment/accounting research unit tests"
	@echo "make opengl-demo      Build and run the rotating OpenGL example"
	@echo "make pong-demo        Build and run interactive BYGL 2 Pong"

endif
