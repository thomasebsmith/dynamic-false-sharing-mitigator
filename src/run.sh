#!/bin/bash
SRC_DIR="$(dirname -- "${BASH_SOURCE[0]}")"

set -Eeuo pipefail
# set -x

usage() {
    >&2 echo "Usage: ./run.sh <path to benchmark, without the file extension .cpp> [globals|fix]"
    exit 1
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    usage
fi

PASS=globals
if [ $# -gt 1 ]; then
    PASS=${2}
fi

BENCH=${1}.cpp
NAME="$(basename "${1}")"

 # Specify your build directory in the project
PATH2GLOBALS=${SRC_DIR}/build/globals/LLVMGLOBALS.so
PATH2FIX=${SRC_DIR}/build/fix/LLVMFALSEFIX.so
PASSGLOBALS=-false-sharing-globals
PASSFIX=-false-sharing-fix   

case "${PASS}" in
    globals) PASSARG="${PASSGLOBALS}"; PASSPATH="${PATH2GLOBALS}";;
    fix)     PASSARG="${PASSFIX}";     PASSPATH="${PATH2FIX}";;
    *) usage
esac

RUN_DIR="${SRC_DIR}/build/run"
# Remove old files 
rm -rf "${RUN_DIR}"
mkdir -p "${RUN_DIR}"

# Convert source code to bitcode (IR)
echo 'Compiling benchmark to bitcode...'
clang -O3 -emit-llvm -I/usr/include/llvm-c-10 -I/usr/include/llvm-10 "${BENCH}" -c -o "${RUN_DIR}/${NAME}.bc"

echo 'Running pass...'
opt -load "${PASSPATH}" "${PASSARG}" "${RUN_DIR}/${NAME}.bc" -o "${RUN_DIR}/${NAME}.${PASS}.bc"

echo 'Generating final executable...'
clang -O3 -pthread -lstdc++ "${RUN_DIR}/${NAME}.${PASS}.bc" -o "${RUN_DIR}/${NAME}_${PASS}"

echo 'Running final executable...'
"${RUN_DIR}/${NAME}_${PASS}" || true # Ignore return code of actual executable
