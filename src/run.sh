#!/bin/bash
SRC_DIR="$(dirname -- "${BASH_SOURCE[0]}")"

set -Eeuo pipefail
# set -x

if [ $# -ne 1 ]; then 
    echo "Usage: ./run.sh [path to benchmark, without the file extension .cpp]"
    exit 1
fi

BENCH=${1}.cpp
NAME="$(basename "${1}")"

 # Specify your build directory in the project
PATH2PROFILE=${SRC_DIR}/build/profile/LLVMFALSEPROFILE.so
PATH2FIX=${SRC_DIR}/build/fix/LLVMFALSEFIX.so
PASSPROFILE=-false-sharing-profile    
PASSFIX=-false-sharing-fix   

RUN_DIR="${SRC_DIR}/build/run"
# Remove old files 
rm -rf "${RUN_DIR}"
mkdir -p "${RUN_DIR}"

# Convert source code to bitcode (IR)
clang -emit-llvm "${BENCH}" -c -o "${RUN_DIR}/${NAME}.bc"

opt -load "${PATH2PROFILE}" "${PASSPROFILE}" "${RUN_DIR}/${NAME}.bc" -o "${RUN_DIR}/${NAME}.prof.bc"
clang -pthread "${RUN_DIR}/${NAME}.prof.bc" -o "${RUN_DIR}/${NAME}_prof"

"${RUN_DIR}/${NAME}_prof"