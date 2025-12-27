#include "yuasm.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Please provide the source code file path as an argument\n";
        std::cout << "Note: only one source file is allowed at the moment\n";
        return 1;
    } else if (argc > 2) {
        std::cout << "Only one source file is allowed at the moment\n";
        return 1;
    }

    std::string fpath (argv[1]);
    Yuasm yuasm(fpath);
    return 0;
}
