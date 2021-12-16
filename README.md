# Dynamically Detecting and Reducing False Sharing
*EECS 583 F21 &mdash; Group 21*

*Tony Bai, Daniel Hoekwater, Brandon Kayes, Thomas Smith*

## Organization
- `bench` - Benchmark programs that exhibit false sharing
- `src`   - Source code for the compiler passes
  - `profile` - First (analysis) pass
  - `globals` - Custom pass to output the names, locations,
                and sizes of all global variables at the
                beginning of program execution.
  - `fix`     - Second pass to fix false sharing

## Setup
*Prerequisites*: LLVM is installed on the machine
1. Clone this repo
2. Download Intel Pin and unzip it with `tar -xzvf`.
```
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz
```
3. Look at `run.sh` script. 
  - Set `PATH_TO_PIN`, `BENCHNAME`, `CACHELINESIZE`, etc., correctly.
