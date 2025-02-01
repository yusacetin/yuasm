#include "linker.hpp"
#include <vector>
#include <string>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Please provide the object file paths as arguments\n";
        return 1;
    }

    std::vector<std::string> files;
    for (int i=1; i<argc; i++) {
        std::string fpath (argv[i]);
        files.push_back(fpath);
    }
    Linker linker(files, true);
    return 0;
}
