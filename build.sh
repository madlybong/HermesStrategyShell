#!/bin/bash
set -e

# ── ANSI color helpers ────────────────────────────────────────────────────────
if [ -t 1 ]; then
    C_RESET='\033[0m';  C_BOLD='\033[1m';    C_DIM='\033[2m'
    C_GREEN='\033[32m'; C_CYAN='\033[36m';  C_YELLOW='\033[33m'; C_RED='\033[31m'
else
    C_RESET=''; C_BOLD=''; C_DIM=''; C_GREEN=''; C_CYAN=''; C_YELLOW=''; C_RED=''
fi

# ==============================================================================
# Unified Build Script for Hermes Strategy Shell
# ==============================================================================

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
ROOT_DIR="$SCRIPT_DIR"

# ── Single source of truth: read from VERSION file ──────────────────────────
PROJECT_VERSION=$(cat "$ROOT_DIR/VERSION")
PROJECT_VERSION="${PROJECT_VERSION//[$'\r\n']}"   # strip Windows line endings
echo "[INFO] Project version: $PROJECT_VERSION"

# Stamp vcpkg.json with current project version
sed -i "s/\"version-string\": \"[^\"]*\"/\"version-string\": \"$PROJECT_VERSION\"/" \
    "$SCRIPT_DIR/vcpkg.json"

GIT_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "")
if [ -n "$GIT_TAG" ] && [ "$GIT_TAG" != "v$PROJECT_VERSION" ]; then
    echo -e "${C_YELLOW}[WARN] VERSION ($PROJECT_VERSION) does not match latest git tag ($GIT_TAG)${C_RESET}"
fi

OS_NAME=$(uname -s)
TRIPLET=""
TOOLCHAIN=""

if [[ "$OS_NAME" == MINGW* ]] || [[ "$OS_NAME" == CYGWIN* ]] || [[ "$OS_NAME" == MSYS* ]]; then
    echo "[INFO] Detected Windows Environment ($OS_NAME)"
    IS_WINDOWS=true
    TRIPLET="x64-windows"
    DIST_DIR="$SCRIPT_DIR/dist/hermes_strategy_shell_windows"
    if [ -n "$VCPKG_ROOT" ]; then
        TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    elif [ -f "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
        TOOLCHAIN="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    fi
else
    echo "[INFO] Detected Linux Environment ($OS_NAME)"
    IS_WINDOWS=false
    TRIPLET="x64-linux"
    DIST_DIR="$SCRIPT_DIR/dist/hermes_strategy_shell_linux"
    if [ -n "$VCPKG_ROOT" ]; then
        TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    elif [ -f "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
        TOOLCHAIN="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
    elif [ -f "/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
        TOOLCHAIN="/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake"
    fi
fi

if [ -n "$TOOLCHAIN" ]; then
    echo "[INFO] Toolchain: $TOOLCHAIN"
fi

# ──────────────────────────────────────────────────────────────────────────────
# Interactive Build Wizard
# ──────────────────────────────────────────────────────────────────────────────
run_wizard() {
    local _COMP_STRATEGY=1 _COMP_MONITOR=1

    _wiz_header() {
        printf "${C_CYAN}${C_BOLD}"
        printf "╬═══════════════════════════════════════════════════════════╪\n"
        printf "║  HermesStrategyShell Build Wizard                      ║\n"
        printf "╚═══════════════════════════════════════════════════════════╝\n"
        printf "${C_RESET}\n"
    }

    _tick() { [ "$1" -eq 1 ] && printf "${C_GREEN}✓${C_RESET}" || printf "${C_DIM}✗${C_RESET}"; }

    local _sel
    while true; do
        printf '\033[2J\033[H'
        _wiz_header
        printf "${C_BOLD}Step 1/3${C_RESET} — Components to build\n"
        printf "${C_DIM}  Number = toggle  ·  a = all on  ·  n = all off  ·  Enter = confirm${C_RESET}\n\n"
        printf "  [1]  $(_tick $_COMP_STRATEGY)  ${C_BOLD}Strategy${C_RESET}   — HermesStrategyShell + deps\n"
        printf "  [2]  $(_tick $_COMP_MONITOR)  ${C_BOLD}Monitor${C_RESET}    — React UI + WS Server\n"
        echo ""
        printf "  ${C_DIM}>${C_RESET} "; read -r _sel
        case "$_sel" in
            1) [ "$_COMP_STRATEGY" -eq 1 ] && _COMP_STRATEGY=0 || _COMP_STRATEGY=1 ;;
            2) [ "$_COMP_MONITOR"  -eq 1 ] && _COMP_MONITOR=0  || _COMP_MONITOR=1  ;;
            a|A) _COMP_STRATEGY=1; _COMP_MONITOR=1 ;;
            n|N) _COMP_STRATEGY=0; _COMP_MONITOR=0 ;;
            "") break ;;
        esac
    done

    [ "$_COMP_STRATEGY" -eq 1 ] && BUILD_STRATEGY=true || BUILD_STRATEGY=false
    [ "$_COMP_MONITOR"  -eq 1 ] && BUILD_MONITOR=true  || BUILD_MONITOR=false

    printf '\033[2J\033[H'
    _wiz_header
    printf "${C_BOLD}Step 2/3${C_RESET} — Build options\n\n"

    local _ans
    printf "  Force clean build?       ${C_DIM}[y/N]${C_RESET} : "; read -r _ans
    [[ "$_ans" =~ ^[Yy]$ ]] && CLEAN=true  || CLEAN=false

    printf "  Enable MSMQ support?     ${C_DIM}[Y/n]${C_RESET} : "; read -r _ans
    [[ "$_ans" =~ ^[Nn]$ ]] && ENABLE_MSMQ=false || ENABLE_MSMQ=true

    printf "  Build type               ${C_DIM}[R=Release / d=Debug]${C_RESET} : "; read -r _ans
    [[ "$_ans" =~ ^[Dd]$ ]] && BUILD_TYPE="Debug" || BUILD_TYPE="Release"

    printf '\033[2J\033[H'
    _wiz_header
    printf "${C_BOLD}Step 3/3${C_RESET} — Confirm\n\n"

    local _built=""
    [ "$BUILD_STRATEGY" = true ] && _built="${_built}${C_CYAN}Strategy${C_RESET} "
    [ "$BUILD_MONITOR"  = true ] && _built="${_built}${C_CYAN}Monitor${C_RESET} "
    [ -z "$_built" ] && _built="${C_RED}(nothing selected — will exit)${C_RESET}"

    printf "  ${C_BOLD}%-14s${C_RESET}: " "Components"; printf "${_built}\n"
    if [ "$CLEAN" = true ]; then printf "  ${C_BOLD}%-14s${C_RESET}: ${C_YELLOW}Yes — full clean${C_RESET}\n" "Clean"; else printf "  ${C_BOLD}%-14s${C_RESET}: ${C_DIM}No (incremental)${C_RESET}\n" "Clean"; fi
    if [ "$ENABLE_MSMQ" = true ]; then printf "  ${C_BOLD}%-14s${C_RESET}: ${C_GREEN}Enabled${C_RESET}\n" "MSMQ"; else printf "  ${C_BOLD}%-14s${C_RESET}: ${C_DIM}Disabled${C_RESET}\n" "MSMQ"; fi
    if [ "$BUILD_TYPE" = "Debug" ]; then printf "  ${C_BOLD}%-14s${C_RESET}: ${C_YELLOW}Debug${C_RESET}\n" "Build type"; else printf "  ${C_BOLD}%-14s${C_RESET}: ${C_GREEN}Release${C_RESET}\n" "Build type"; fi

    echo ""
    printf "  ${C_BOLD}Proceed?${C_RESET} ${C_DIM}[Y/n]${C_RESET} : "; read -r _ans
    if [[ "$_ans" =~ ^[Nn]$ ]]; then
        printf '\033[2J\033[H'
        printf "\n  ${C_DIM}Build cancelled.${C_RESET}\n\n"
        exit 0
    fi
    printf '\033[2J\033[H'
}

CLEAN=false
ENABLE_MSMQ=true
ENABLE_LOCAL=true
BUILD_TYPE="Release"
BUILD_STRATEGY=true
BUILD_MONITOR=false

for arg in "$@"; do
  case $arg in
    --clean) CLEAN=true; shift ;;
    --no-msmq) ENABLE_MSMQ=false; shift ;;
    --no-local) ENABLE_LOCAL=false; shift ;;
    --debug) BUILD_TYPE="Debug"; shift ;;
    --monitor) BUILD_MONITOR=true; shift ;;
    --no-monitor) BUILD_MONITOR=false; shift ;;
    --no-strategy) BUILD_STRATEGY=false; shift ;;
    *) ;;
  esac
done

if [ $# -eq 0 ] && [ -t 0 ]; then
    run_wizard
fi

if [ -f "build/CMakeCache.txt" ]; then
    CACHE_SOURCE=$(grep "CMAKE_HOME_DIRECTORY:INTERNAL=" build/CMakeCache.txt | cut -d'=' -f2)
    if [ -n "$CACHE_SOURCE" ]; then
        CURRENT_SOURCE="$SCRIPT_DIR"
        if [[ "$IS_WINDOWS" = true && "$CACHE_SOURCE" == /mnt/* ]] || \
           [[ "$IS_WINDOWS" = false && "$CACHE_SOURCE" == *:* ]]; then
            echo "[WARN] Detected cross-platform CMake cache conflict!"
            echo "[WARN] Cache was created on different OS. Cleaning build directory..."
            rm -rf build
        fi
    fi
fi

if [ "$CLEAN" = true ]; then
    echo "[INFO] Cleaning build directories..."
    rm -rf "$DIST_DIR"
    rm -rf "$SCRIPT_DIR/build"
    rm -rf "$SCRIPT_DIR/extern/HermesPortal/build"
    rm -rf "$SCRIPT_DIR/extern/HermesTrader/build"
    rm -rf "$SCRIPT_DIR/extern/hermes_common/build"
    rm -rf "$SCRIPT_DIR/tools/monitor/build"
    rm -rf "$SCRIPT_DIR/tools/monitor/ui/dist"
    rm -rf "$SCRIPT_DIR/tools/monitor/ui/node_modules"
    rm -f "$SCRIPT_DIR/tools/monitor/ui/package-lock.json"
    rm -f "$SCRIPT_DIR/tools/monitor/ui/bun.lockb"
fi

mkdir -p extern

echo ">>> Initializing Git Submodules (if applicable)..."
git submodule update --init || true

ensure_repo() {
    local NAME=$1
    local URL=$2
    if [ ! -d "extern/$NAME" ]; then
        echo ">>> Cloning $NAME..."
        git clone "$URL" "extern/$NAME"
    fi
}

ensure_repo "HermesPortal" "https://github.com/madlybong/HermesPortal.git"
ensure_repo "HermesTrader" "https://github.com/madlybong/HermesTrader.git"
ensure_repo "hermes_common" "https://github.com/madlybong/HermesCommon.git"

if [ -d "extern/HermesTrader" ]; then
    echo ">>> Initializing Submodules for HermesTrader..."
    (cd extern/HermesTrader && git submodule update --init extern/HermesCommon)
    (cd extern/HermesTrader && git submodule update --init extern/HermesPortal 2>/dev/null || true)
    (cd extern/HermesTrader && git submodule update --init extern/HermesTraderMSMQ 2>/dev/null || true)
fi

build_cmake_project() {
    local PROJECT_NAME=$1
    local BUILD_DIR=$2
    local IS_STRATEGY=$3
    local EXTRA_ARGS=("${@:4}")

    echo ""
    echo ">>> Building $PROJECT_NAME..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    local CMAKE_CMD=(cmake .. "-DCMAKE_BUILD_TYPE=$BUILD_TYPE")
    if [ "$IS_STRATEGY" = true ]; then
        CMAKE_CMD+=("-DCMAKE_INSTALL_PREFIX=$DIST_DIR")
    fi
    if [ -n "$TRIPLET" ]; then CMAKE_CMD+=("-DVCPKG_TARGET_TRIPLET=$TRIPLET"); fi
    if [ -n "$TOOLCHAIN" ]; then CMAKE_CMD+=("-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN"); fi
    
    CMAKE_CMD+=("${EXTRA_ARGS[@]}")

    echo "[EXEC] ${CMAKE_CMD[@]}"
    "${CMAKE_CMD[@]}"

    echo "[EXEC] cmake --build . --config $BUILD_TYPE --parallel"
    cmake --build . --config "$BUILD_TYPE" --parallel

    echo "[EXEC] cmake --install . --config $BUILD_TYPE"
    cmake --install . --config "$BUILD_TYPE"
}

if [ "$BUILD_STRATEGY" = true ]; then
    EXTRA_STRATEGY=()
    if [ "$ENABLE_MSMQ" = true ]; then EXTRA_STRATEGY+=("-DENABLE_MSMQ=ON"); else EXTRA_STRATEGY+=("-DENABLE_MSMQ=OFF"); fi
    if [ "$ENABLE_LOCAL" = true ]; then EXTRA_STRATEGY+=("-DENABLE_LOCAL=ON"); else EXTRA_STRATEGY+=("-DENABLE_LOCAL=OFF"); fi
    if [ "$BUILD_MONITOR" = true ]; then EXTRA_STRATEGY+=("-DBUILD_MONITOR=ON"); else EXTRA_STRATEGY+=("-DBUILD_MONITOR=OFF"); fi
    build_cmake_project "HermesStrategyShell" "$SCRIPT_DIR/build" true "${EXTRA_STRATEGY[@]}"
fi

# Note: The monitor could also be built as a separate project if desired, but here we just pass the flag.

echo ""
echo "=========================================================="
echo " BUILD SUCCESSFUL"
echo " Artifacts: $DIST_DIR"
echo "=========================================================="
