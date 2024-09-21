#include <fstream>
#include <string>

int init_first_file();
char get_next_char(std::ifstream file);
int mainloop();
bool is_alphabetic(char ch);
bool is_numeric(char ch);
const int get_category(char ch);
std::string print_state(const int state_value);
bool is_valid_instr(std::string instr);
int get_no_of_params_for_instr(std::string instr);