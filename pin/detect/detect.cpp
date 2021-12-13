// Takes in pinatrace.out
// Output list of interferences {addr1, addr2, [priority]}

#include <iostream>
#include <fstream>
#include <sstream>
#include <string> 
#include <cstdint>

#include "InterferenceDetector.h"

void process_pinatrace(const std::string& pinatrace_file, uint64_t cacheline_size);

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [path to pinatrace.out file] [cache line size in bytes]" << std::endl;
        exit(1);
    }

    std::string pinatrace_file(argv[1]);
    uint64_t cacheline_size;
    try {
        cacheline_size = std::stoll(argv[2]);
    } catch (...) {
        std::cout << "Exception thrown, could not convert cache line size to long long: " 
                  << argv[2] << std::endl;
        exit(1);
    }
    if (std::to_string(cacheline_size) != argv[2]) {
        std::cout << "Could not entirely parse cache line size to long long: "
                  << argv[2] << std::endl;
        exit(1);
    }
    std::cout << "Reading pinatrace file: " << pinatrace_file;
    std::cout << ", with cache line size: " << cacheline_size << std::endl;

    process_pinatrace(pinatrace_file, cacheline_size);
}

void process_pinatrace(const std::string& pinatrace_file, uint64_t cacheline_size) {
    std::ifstream infile(pinatrace_file);
    std::string output_file = pinatrace_file + ".interferences";
    std::ofstream outfile(output_file);
    if (!outfile.is_open()) {
        std::cout << "Could not open output file: " << output_file << std::endl;
        exit(1);
    }

    std::string line;

    // Columns of the pinatrace file:
    // program counter, read or write, dest addr, size of access, thread id, value
    std::string pc, rw, dest, sz, tid, val;

    InterferenceDetector detector(cacheline_size);

    uint64_t linenum = 0;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);

        bool parseError = !(iss >> pc >> rw >> dest >> sz >> tid >> val);
        ++linenum;
        if (!pc.empty() && pc[0] == '#') 
            continue; // filter out comments
        if (parseError) {
            std::cout << "Line #" << (linenum - 1) << " formatted incorrectly:" << std::endl;
            std::cout << '\t' << pc << '\t' << rw << '\t' << dest << '\t' << sz << '\t' << tid << '\t' << val << std::endl;
            continue;
        }
        
        try {
            detector.recordAccess(rw, dest, sz, tid);
        } catch (std::runtime_error& e) {
            std::cout << "Error processing line #" << (linenum - 1) << ": " << e.what() << std::endl;
            continue; // ignore bad access
        }

        if (linenum % 100000 == 0) {
            std::cout << "Processed " << linenum << " lines" << std::endl;
        }
    }

    detector.outputInterferences(outfile);
    std::cout << "Outputted interferences to file: " << output_file << std::endl;
}

