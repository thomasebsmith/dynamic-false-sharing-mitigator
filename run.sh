#!/bin/bash 
set -Eeuo pipefail 

REPO_ROOT=$(pwd)
BENCHNAME=locks # Change if necessary
BENCH=${REPO_ROOT}/bench/${BENCHNAME}
BENCH=${REPO_ROOT}/bench/${BENCHNAME} 
CACHELINESIZE=64 # Change if necessary

# Set up Intel Pin pinatrace
PATH_TO_PIN=~/intel-pin/pin-3.21-98484-ge7cd811fd-gcc-linux/ # Change if necessary
echo "Path to pin set as: ${PATH_TO_PIN}. Change this if necessary."
echo

PINATRACE_DIR=$PATH_TO_PIN/source/tools/SimpleExamples/
if [ ! -f ${PINATRACE_DIR}/pinatrace.cpp ]; then 
    echo "pinatrace.cpp not found at ${PINATRACE_DIR}/pinatrace.cpp! Make sure PATH_TO_PIN is set correctly."
    exit 1
fi

# Copy over modified pinatrace, and build pinatrace
cp pin/pinatrace.cpp ${PINATRACE_DIR}
cd ${PINATRACE_DIR}
make obj-intel64/pinatrace.so
echo "Successfully compiled pinatrace.so"
echo

# Copy over modified mdcache, and build mdcache 
cd ${REPO_ROOT}
cp pin/mdcache.cpp ${PINATRACE_DIR}
cp pin/mdcache.H ${PINATRACE_DIR}
cp pin/mutex.PH ${PINATRACE_DIR}
cd ${PINATRACE_DIR}
make obj-intel64/mdcache.so
echo "Successfully compiled mdcache.so"
echo

# Clean up old files
cd ${REPO_ROOT}
rm -f *.out *.interferences fs_globals.txt
echo "Cleaned up old output files"
echo

# Run the globals pass
cd ${REPO_ROOT}
cd src/ 
./make.sh 
./run.sh ${BENCH} globals 
echo "Successfully instrumented the globals pass"
echo

# Run pinatrace on the global pass 
cd ${REPO_ROOT}
${PATH_TO_PIN}/pin -t ${PINATRACE_DIR}/obj-intel64/pinatrace.so -- ${REPO_ROOT}/src/build/run/${BENCHNAME}_globals
echo "Successfully ran pinatrace on the globals pass. Got pinatrace.out as well as fs_globals.txt."
echo

# Run detect on pinatrace.out to get a list of interferences
cd pin/detect 
make clean
make detect 
./detect ${REPO_ROOT}/pinatrace.out $CACHELINESIZE
DETECT_OUTPUT_FNAME=pinatrace.out.cacheline${CACHELINESIZE}.interferences
echo "Successfully ran detect to produce ${DETECT_OUTPUT_FNAME}"
echo

# Run mdcache on the global pass 
cd ${REPO_ROOT}
${PATH_TO_PIN}/pin -t ${PINATRACE_DIR}/obj-intel64/mdcache.so -- ${REPO_ROOT}/src/build/run/${BENCHNAME}_globals
MDCACHE_OUTPUT_FNAME=mdcache.out.cacheline64.interferences
echo "Successfully ran mdcache on the globals pass. Got mdcache.out as well as ${MDCACHE_OUTPUT_FNAME}"
echo

# Run MapAddr to get mapped_conflicts.out
cd pin/MapAddr 
make clean 
make all 
./MapAddr "${REPO_ROOT}/${MDCACHE_OUTPUT_FNAME}" "${REPO_ROOT}/${DETECT_OUTPUT_FNAME}" "${REPO_ROOT}/fs_globals.txt"
mv mapped_conflicts.out ${REPO_ROOT}
cd ${REPO_ROOT}
echo "Successfully ran MapAddr to get mapped_conflicts.out"
echo 

# Apply the fix LLVM pass
echo "Applying fix and running optimized binary"
./src/run.sh ${BENCH} fix
echo "Successfully applied fix"
echo

# Evaluate the fixed binary with mdcache
mv mdcache.out pre_mdcache.out
mv $MDCACHE_OUTPUT_FNAME "pre_${MDCACHE_OUTPUT_FNAME}"
${PATH_TO_PIN}/pin -t ${PINATRACE_DIR}/obj-intel64/mdcache.so -- ${REPO_ROOT}/src/build/run/${BENCHNAME}_fix
mv mdcache.out post_mdcache.out
mv $MDCACHE_OUTPUT_FNAME "post_${MDCACHE_OUTPUT_FNAME}"
echo "Evaluated fixed binary with mdcache, and renamed old mdcache files."
echo "See pre_mdcache.out, pre_${MDCACHE_OUTPUT_FNAME}, post_mdcache.out, post_${MDCACHE_OUTPUT_FNAME}"
echo

