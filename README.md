# Dynamically Detecting and Reducing False Sharing
*EECS 583 F21 &mdash; Group 21*

*Tony Bai, Daniel Hoekwater, Brandon Kayes, Thomas Smith*

## Organization
- `bench` - Benchmark programs that exhibit false sharing
- `docs` - pdfs explaining more about this project
  - [`demo.pdf`](docs/demo.pdf) - Visual overview of design and an example
  - [`report.pdf`](docs/report.pdf) - Detailed report on the system
- `pin` - Source code for false sharing detection
  - Intel Pin pinatrace: `pinatrace.cpp`
  - Intel Pin multicore cache simulator: `mdcache.H`, `mdcache.cpp`, `mutex.PH`
  - `detect` - Detects false sharing from `pinatrace` output
  - `MapAddr` - Matches variable names from LLVM globals pass with interferences
    outputted by `pinatrace`/`detect` and `mdcache`
- `src`   - Source code for the compiler passes
  - `globals` - First pass to output the names, locations,
                and sizes of all global variables at the
                beginning of program execution.
  - `fix`     - Second pass to fix false sharing by aligning global variables and
                padding structs.

## Setup
*Prerequisites*: LLVM is installed on the machine
1. Clone this repo
2. Download Intel Pin and unzip it with `tar -xzvf`.
```
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz
```
3. Look at `run.sh` script. 
  - Set `PATH_TO_PIN`, `BENCHNAME`, `CACHELINESIZE`, etc., correctly.

Mega-command to do all of the above steps on Linux:
```
cd ~ && mkdir intel-pin && cd intel-pin && wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz && tar -xvzf pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz && cd ~ && git clone git@github.com:thomasebsmith/eecs583-f21-group21.git && cd eecs583-f21-group21
```

