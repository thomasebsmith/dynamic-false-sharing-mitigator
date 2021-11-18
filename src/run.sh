set -Eeuo pipefail
# set -x

if [ $# -ne 1 ]; then 
    echo "Usage: ./run.sh [path to benchmark, without the file extension .cpp]"
    exit 1
fi

BENCH=${1}.cpp

 # Specify your build directory in the project
PATH2PROFILE=~/group21/src/build/profile/LLVMFALSEPROFILE.so
PATH2FIX=~/group21/src/build/fix/LLVMFALSEFIX.so
PASSPROFILE=-false-sharing-profile    
PASSFIX=-false-sharing-fix   

# Remove old files 
rm -rf *.bc 
# Convert source code to bitcode (IR)
clang -emit-llvm ${BENCH} -c -o ${1}.bc

opt -load $PATH2PROFILE $PASSPROFILE < ${1}.bc > /dev/null

exit 0 

# Remove old files 
rm -rf *.bc 
# Convert source code to bitcode (IR)
clang -emit-llvm ${BENCH} -c -o ${1}.bc
# Instrument profiler
opt -pgo-instr-gen -instrprof ${1}.bc -o ${1}.prof.bc
# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.prof.bc -o ${1}.prof
# Collect profiling data
./${1}.prof ${INPUT}
# Translate raw profiling data into LLVM data format
llvm-profdata merge -output=pgo.profdata default.profraw

# Apply your pass to bitcode (IR)
opt -pgo-instr-use -pgo-test-profile-file=pgo.profdata -load ${PATH_MYPASS} ${NAME_MYPASS} < ${1}.bc > /dev/null

exit 0


# Delete outputs from previous run.
rm -f default.profraw ${1}_prof ${1}_fplicm ${1}_no_fplicm *.bc ${1}.profdata *_output *.ll

# Convert source code to bitcode (IR)
clang -emit-llvm -c ${1}.cpp -o ${1}.bc
# Canonicalize natural loops
opt -loop-simplify ${1}.bc -o ${1}.ls.bc
# Instrument profiler
opt -pgo-instr-gen -instrprof ${1}.ls.bc -o ${1}.ls.prof.bc
# Generate binary executable with profiler embedded
clang -fprofile-instr-generate ${1}.ls.prof.bc -o ${1}_prof

# Generate profiled data
./${1}_prof > correct_output
llvm-profdata merge -o ${1}.profdata default.profraw

# Apply FPLICM
opt -o ${1}.fplicm.bc -pgo-instr-use -pgo-test-profile-file=${1}.profdata -load ${PATH2LIB} ${PASS} < ${1}.ls.bc > /dev/null

# Generate binary excutable before FPLICM: Unoptimzied code
clang ${1}.ls.bc -o ${1}_no_fplicm
# Generate binary executable after FPLICM: Optimized code
clang ${1}.fplicm.bc -o ${1}_fplicm

# Produce output from binary to check correctness
./${1}_fplicm > fplicm_output

echo -e "\n=== Correctness Check ==="
if [ "$(diff correct_output fplicm_output)" != "" ]; then
    echo -e ">> FAIL\n"
else
    echo -e ">> PASS\n"
    # Measure performance
    echo -e "1. Performance of unoptimized code"
    time ./${1}_no_fplicm > /dev/null
    echo -e "\n\n"
    echo -e "2. Performance of optimized code"
    time ./${1}_fplicm > /dev/null
    echo -e "\n\n"
fi

# Cleanup
# rm -f default.profraw ${1}_prof ${1}_fplicm ${1}_no_fplicm *.bc ${1}.profdata *_output *.ll

# Copy fplicm.bc file to bonus
cp ${1}.fplicm.bc ${1}_bonus.bc
