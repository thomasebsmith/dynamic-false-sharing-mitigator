#!/bin/bash 
set -Eeuo pipefail 

REPO_ROOT=$(pwd)
BENCHNAME=sharedStruct # Change if necessary
BENCH=${REPO_ROOT}/bench/${BENCHNAME}
BENCH=${REPO_ROOT}/bench/${BENCHNAME} 
CACHELINESIZE=64 # Change if necessary

# Set up Intel Pin pinatrace
PATH_TO_PIN=~/intel-pin/pin-3.21-98484-ge7cd811fd-gcc-linux/ # Change if necessary
echo "Path to pin set as: ${PATH_TO_PIN}. Change this if necessary."

PINATRACE_DIR=$PATH_TO_PIN/source/tools/SimpleExamples/
if [ ! -f ${PINATRACE_DIR}/pinatrace.cpp ]; then 
    echo "pinatrace.cpp not found at ${PINATRACE_DIR}/pinatrace.cpp! Make sure PATH_TO_PIN is set correctly."
    exit 1
fi

cp pin/pinatrace.cpp ${PINATRACE_DIR}
cd ${PINATRACE_DIR}
make obj-intel64/pinatrace.so
echo "Successfully compiled pinatrace.so"

# Run the globals pass
cd ${REPO_ROOT}
cd src/ 
./make.sh 
./run.sh ${BENCH} globals 
echo "Successfully instrumented the globals pass"

# Run pinatrace on the global pass 
cd ${REPO_ROOT}
${PATH_TO_PIN}/pin -t ${PINATRACE_DIR}/obj-intel64/pinatrace.so -- ${REPO_ROOT}/src/build/run/${BENCHNAME}_globals
echo "Successfully ran pinatrace on the globals pass. Got pinatrace.out as well as fs_globals.txt."

cd pin/detect 
make detect 
./detect ${REPO_ROOT}/pinatrace.out $CACHELINESIZE
DETECT_OUTPUT_FNAME=pinatrace.out.cacheline${CACHELINESIZE}.interferences
echo "Successfully ran detect to produce ${DETECT_OUTPUT_FNAME}"


