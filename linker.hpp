#include <string>
#include <vector>
#include <map>

class Linker{
public:
    Linker(std::vector<std::string> set_fpaths);

private:
    const int DEBUG_LEVEL = 10;

    std::vector<std::string> fpaths;
    std::vector<std::multimap<std::string, int>> defs; // should be map but gotta change the print function
    std::vector<std::multimap<std::string, int>> callers;
    std::vector<int> instr_count;
    std::vector<unsigned char> instrs;

    int link();
    int save_defs_and_callers_and_instrs();
    std::multimap<std::string, int> get_defs(std::string fpath); // should be map but gotta change the print function
    std::multimap<std::string, int> get_callers(std::string fpath);
    int place_symbols();
    int find_symbol(std::string symbol_name);
    int write_binary();

    static void print_vmsi(std::vector<std::multimap<std::string, int>> vmsi);
    static void print_vuc(std::vector<unsigned char> vuc);
};