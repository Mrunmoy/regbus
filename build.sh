#!/usr/bin/env bash
# regbus build helper
# Usage examples:
#   ./build.sh                      # configure + build (RelWithDebInfo, tests/examples ON)
#   ./build.sh --debug              # Debug build
#   ./build.sh --release            # Release build
#   ./build.sh --no-tests --no-examples
#   ./build.sh --run-tests
#   ./build.sh --run-example        # runs examples/minimal_registry after building
#   ./build.sh --clean              # delete build dir
#   ./build.sh --install ~/.local   # install headers + CMake package

set -euo pipefail

# -------- defaults --------
BUILD_DIR="build"
BUILD_TYPE="RelWithDebInfo"
WITH_TESTS="ON"
WITH_EXAMPLES="ON"
RUN_TESTS="OFF"
RUN_EXAMPLE="OFF"
INSTALL_PREFIX=""
GENERATOR=""

# parallel jobs
if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
elif command -v getconf >/dev/null 2>&1; then
  JOBS="$(getconf _NPROCESSORS_ONLN)"
else
  JOBS="4"
fi

# prefer Ninja if present
if command -v ninja >/dev/null 2>&1; then
  GENERATOR="-G Ninja"
fi

print_help() {
  cat <<EOF
regbus build.sh

Options:
  --dir <path>         Build directory (default: build)
  --debug              CMAKE_BUILD_TYPE=Debug
  --release            CMAKE_BUILD_TYPE=Release
  --relwithdebinfo     CMAKE_BUILD_TYPE=RelWithDebInfo (default)
  --minsizerel         CMAKE_BUILD_TYPE=MinSizeRel

  --no-tests           Disable building tests (default: ON)
  --no-examples        Disable building examples (default: ON)
  --run-tests          Run ctest after build
  --run-example        Run the minimal example after build

  --install <prefix>   Install to prefix (headers + CMake package)
  --ninja              Force Ninja generator
  --make               Force Unix Makefiles generator
  --clean              Remove build directory and exit
  -h, --help           Show this help

Environment:
  JOBS: override parallel build jobs (default: detected CPUs = ${JOBS})
EOF
}

# -------- arg parse --------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --dir) BUILD_DIR="${2?}"; shift 2 ;;
    --debug) BUILD_TYPE="Debug"; shift ;;
    --release) BUILD_TYPE="Release"; shift ;;
    --relwithdebinfo) BUILD_TYPE="RelWithDebInfo"; shift ;;
    --minsizerel) BUILD_TYPE="MinSizeRel"; shift ;;
    --no-tests) WITH_TESTS="OFF"; shift ;;
    --no-examples) WITH_EXAMPLES="OFF"; shift ;;
    --run-tests) RUN_TESTS="ON"; shift ;;
    --run-example) RUN_EXAMPLE="ON"; shift ;;
    --install) INSTALL_PREFIX="${2?}"; shift 2 ;;
    --ninja) GENERATOR="-G Ninja"; shift ;;
    --make) GENERATOR="-G Unix Makefiles"; shift ;;
    --clean) echo "Removing ${BUILD_DIR}"; rm -rf "${BUILD_DIR}"; exit 0 ;;
    -h|--help) print_help; exit 0 ;;
    *) echo "Unknown arg: $1"; print_help; exit 1 ;;
  esac
done

mkdir -p "${BUILD_DIR}"

echo "==> Configure"
set -x
cmake -S . -B "${BUILD_DIR}" ${GENERATOR} \
  -DREGBUS_BUILD_TESTS="${WITH_TESTS}" \
  -DREGBUS_BUILD_EXAMPLES="${WITH_EXAMPLES}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  ${INSTALL_PREFIX:+-DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"}
set +x

echo "==> Build (${BUILD_TYPE})"
set -x
cmake --build "${BUILD_DIR}" -j "${JOBS}"
set +x

if [[ "${RUN_TESTS}" == "ON" && "${WITH_TESTS}" == "ON" ]]; then
  echo "==> Run tests"
  set -x
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
  set +x
elif [[ "${RUN_TESTS}" == "ON" ]]; then
  echo "(!) --run-tests requested but tests are disabled; re-run without --no-tests"
fi

if [[ "${RUN_EXAMPLE}" == "ON" ]]; then
  EXE="${BUILD_DIR}/example_minimal"
  if [[ -x "${EXE}" ]]; then
    echo "==> Running example_minimal"
    "${EXE}"
  else
    echo "(!) example_minimal not built; enable with --no-examples omitted (examples ON)"
  fi
fi

if [[ -n "${INSTALL_PREFIX}" ]]; then
  echo "==> Install to ${INSTALL_PREFIX}"
  set -x
  cmake --build "${BUILD_DIR}" --target install -j "${JOBS}"
  set +x
fi

echo "✅ Done."
