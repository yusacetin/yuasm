#include "yuasm.hpp"
#include <cctype>
#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <memory>
#include <string>
#include <map>

const int DEBUG_LEVEL = 1; // 0: nothing, 1: state completions, 2: full info

enum State {
    SCAN_FIRST,
    SCAN_INSTR_OR_MACRO,
    SCAN_PARAMS,
    SCAN_PREPROC_DEF,
    SCAN_PREPROC_SUB,
    SCAN_INCLUDE_FPATH,
    SCAN_FUNC_HEAD,
    SCAN_PREPROC_VAL,
    WAIT_PAREN_CLOSE
};

enum Input {
    UNKNOWN,
    AL,
    NUM,
    HASH,
    COMMA,
    DOT,
    COLON,
    LF,
    CR,
    SP,
    SC,
    SLASH,
    AST,
    INPUT_EOF,
    PAREN_OPEN,
    PAREN_CLOSE
};

int state = SCAN_FIRST;

std::vector<char> buffer0; // for instructions and function names and macro names
std::vector<char> buffer1; // for macro values and instruction parameters
std::vector<std::string> params; // for instruction parameters

std::stack<std::unique_ptr<std::ifstream>> files;
std::map<std::string, std::string> macros;
std::map<std::string, int> functions;
int pc = 0; // program counter

int main() {
    int first_file_result = init_first_file();
    if (first_file_result == 1) {
        return 1;
    }

    return mainloop();
}

int mainloop() {
    while (!files.empty()) {
        // Quit the program if all files are scanned
        // TODO there's a serious problem with state switching after EOF
        if ((*(files.top())).eof()) {
            buffer0.clear();
            buffer1.clear();
            params.clear();
            files.pop();
            continue;
        }

        // Get the next character in line
        char ch;
        (*(files.top())).get(ch);

        // Main FSM
        int category = get_category(ch);

        if (DEBUG_LEVEL >= 2) {
            std::cout << "Ch: " << ch << ", State: " << print_state(state) << ", Category: " << category << std::endl; // DEBUG
        }

        switch (state) {
            case SCAN_FIRST: {
                switch (category) {
                    case AL: {
                        buffer0.push_back(ch);
                        state = SCAN_INSTR_OR_MACRO;
                        break;
                    }

                    case DOT: {
                        state = SCAN_FUNC_HEAD;
                        break;
                    }

                    case HASH: {
                        state = SCAN_PREPROC_DEF;
                        break;
                    }
                }
                break;
            }
            


            case SCAN_PREPROC_DEF: {
                switch (category) {
                    case LF:
                    case CR: { // these are invalid, we expect a parameter
                        std::cout << "Error: expected parameter for preprocessing directive\n";
                        return 1;
                    }
                     
                    case AL: { // scan the instruction
                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first letter, valid letter
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cout << "Identifiers can't begin with numbers\n";
                            return -1;
                        }
                        break;
                    }

                    case SP: {
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        if (buffer_str == "define") {
                            state = SCAN_PREPROC_SUB;
                            buffer0.clear();
                        } else if (buffer_str == "include") {
                            state = SCAN_INCLUDE_FPATH;
                            buffer0.clear();
                        } else {
                            std::cout << "Error: invalid preprocessor directive\n";
                            return -1;
                        }
                        break;
                    }

                    default: {
                        std::cout << "Error: invalid character 0\n";
                        break;
                    }
                }
                break;
            }
        
            

            case SCAN_PREPROC_SUB: {
                switch (category) {
                    case LF:
                    case CR: { // these are invalid, we expect a parameter
                        std::cout << "Error: expected parameter for preprocessing directive\n";
                        return 1;
                    }
                     
                    case AL: { // scan the instruction
                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first letter, valid letter
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cout << "Identifiers can't begin with numbers\n";
                            return -1;
                        }
                        break;
                    }

                    case SP: {
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        state = SCAN_PREPROC_VAL;
                        break;
                    }

                    default: {
                        std::cout << "Error: invalid character 1\n";
                        break;
                    }
                }
                break;
            }



            case SCAN_PREPROC_VAL: {
                switch (category) {
                    case NUM:
                    case AL: { // scan the instruction
                        buffer1.push_back(ch);
                        break;
                    }

                    case LF:
                    case CR:
                    case SC:
                    case SP: {
                        std::string macro_name(buffer0.begin(), buffer0.end());
                        std::string macro_val(buffer1.begin(), buffer1.end());
                        macros.insert({macro_name, macro_val});
                        buffer0.clear();
                        buffer1.clear();
                        state = SCAN_FIRST;

                        if (DEBUG_LEVEL >= 1) {
                            std::cout << "# Macro Definition Complete #\n"; // DEBUG
                            std::cout << "Key: " << macro_name << ", Value: " << macro_val << "\n\n";
                        }

                        break;
                    }

                    default: {
                        std::cout << "Error: invalid character 2\n";
                        break;
                    }
                }
                break;
            }

            

            case SCAN_INCLUDE_FPATH: {
                switch (category) {
                    case AL:
                    case NUM:
                    case DOT: // TODO add all because file names can contain almost anything
                    case HASH: {
                        buffer0.push_back(ch);
                        break;
                    }

                    case SP:
                    case LF:
                    case CR:
                    case SC: {
                        std::string fpath(buffer0.begin(), buffer0.end());
                        std::unique_ptr<std::ifstream> file = std::make_unique<std::ifstream>(fpath);
                        if (!(*file)) {
                            std::cout << "Error: file not found" << std::endl;
                            return 1;
                        }
                        files.push(std::move(file));
                        buffer0.clear();
                        state = SCAN_FIRST;

                        if (DEBUG_LEVEL >= 1) {
                            std::cout << "# Include Complete #\n"; // DEBUG
                            std::cout << "File name: " << fpath << "\n\n";
                        }
                        break;
                    }

                }
                break;
            }



            case SCAN_FUNC_HEAD: {
                switch (category) {
                    case SC: // TODO add this to other similar places too
                    case LF:
                    case CR: { // these are invalid, we expect a parameter
                        std::cout << "Error: expected function name\n";
                        return 1;
                    }
                    
                    case AL: { // scan the name
                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first letter, valid letter
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cout << "Identifiers can't begin with numbers\n";
                            return -1;
                        }
                        break;
                    }

                    case COLON: {
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        functions.insert({buffer_str, pc});
                        buffer0.clear();
                        state = SCAN_FIRST;

                        if (DEBUG_LEVEL >= 1) { 
                            std::cout << "# Function Definition Complete #\n"; // DEBUG
                            std::cout << "Function name: " << buffer_str << "\n";
                            std::cout << "Function address: " << pc << "\n\n";
                        }
                        break;
                    }

                    default: {
                        std::cout << "Error: invalid character 6\n";
                        break;
                    }
                }
                break;
            }



            case SCAN_INSTR_OR_MACRO: {
                switch (category) {
                    case SC: // TODO add this to other similar places too
                    case LF:
                    case CR: { // these are invalid, we expect a parameter
                        std::string instr(buffer0.begin(), buffer0.end());
                        if (get_no_of_params_for_instr(instr) == 0) {

                            if (DEBUG_LEVEL >= 1) {
                                std::cout << "# Instruction Complete #\n";
                                std::cout << "Instruction: " << instr << "\n\n";
                            }
                            
                            buffer0.clear();
                            pc++;
                            state = SCAN_FIRST;
                            break;
                        } else {
                            std::cout << "Error: expected identifier\n";
                            return 1;
                        }
                        
                    }
                    
                    case AL: { // scan the name
                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first letter, valid letter
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cout << "Identifiers can't begin with numbers\n";
                            return -1;
                        }
                        break;
                    }

                    case SP: {
                        // First check if it's a macro expansion
                        std::string temp_buffer_str(buffer0.begin(), buffer0.end());
                        if (macros.find(temp_buffer_str) != macros.end()) {
                            std::string expansion = macros[temp_buffer_str];
                            std::vector<char> substituted_value_vector(expansion.begin(), expansion.end());
                            buffer0 = substituted_value_vector;
                        }
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        if (is_valid_instr(buffer_str)) {
                            state = SCAN_PARAMS;
                        } else {
                            std::cout << "Invalid instruction\n";
                            return 1;
                        }
                        break;
                        
                    }

                    case PAREN_OPEN: {
                        state = WAIT_PAREN_CLOSE;
                        break;
                    }

                    default: {
                        std::cout << "Error: invalid character 6\n";
                        break;
                    }
                }
                break;
            }



            case WAIT_PAREN_CLOSE: {
                switch (category) {
                    case PAREN_CLOSE: {
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        int func_pc = functions[buffer_str];
                        
                        if (DEBUG_LEVEL >= 1) {
                            std::cout << "# Calling function " << buffer_str << " at address " << func_pc << " #\n\n";
                        }

                        buffer0.clear();
                        state = SCAN_FIRST;
                        break;
                    }
                }
                break;
            }



            case SCAN_PARAMS: {
                switch (category) {
                    case INPUT_EOF:
                    case LF:
                    case CR:
                    case SC: {
                        // Need to check if another parameter needs to be added
                        if (buffer1.size() > 0) {
                            int offset = 0; // to avoid reading the last letter in the file twice
                            if (category == INPUT_EOF) {
                                offset = -1;
                            }

                            // First check if it's a macro expansion
                            std::string temp_buffer_str(buffer1.begin(), buffer1.end() + offset);
                            if (macros.find(temp_buffer_str) != macros.end()) {
                                std::string expansion = macros[temp_buffer_str];
                                std::vector<char> substituted_value_vector(expansion.begin(), expansion.end());
                                buffer1 = substituted_value_vector;
                            }

                            std::string param(buffer1.begin(), buffer1.end() + offset);
                            params.push_back(param);
                            buffer1.clear();

                            if (DEBUG_LEVEL >= 2) {
                                std::cout << "Added parameter: " << param << "\n"; // DEBUG
                            }
                        }

                        // Moving on
                        std::string instr(buffer0.begin(), buffer0.end());

                        if (DEBUG_LEVEL >= 1) {
                            std::cout << "# Instruction Complete #\n";
                            std::cout << "Instruction: " << instr << "\n";
                            for (int i=0; i<params.size(); i++) {
                                std::cout << "Parameter: " << params[i] << "\n";
                            }
                            std::cout << "\n";
                        }

                        buffer0.clear();
                        buffer1.clear();
                        params.clear();
                        pc++;
                        state = SCAN_FIRST;

                        if (DEBUG_LEVEL >= 2) {
                            if (category == INPUT_EOF) {
                                std::cout << "Category EOF\n"; // DEBUG
                            }
                        }

                        break;
                    }

                    case AL:
                    case NUM: {
                        buffer1.push_back(ch);
                        break;
                    }

                    case COMMA:
                    case SP: { // end of current parameter
                        // First check if it's a macro expansion
                        std::string temp_buffer_str(buffer1.begin(), buffer1.end());
                        if (macros.find(temp_buffer_str) != macros.end()) {
                            std::string expansion = macros[temp_buffer_str];
                            std::vector<char> substituted_value_vector(expansion.begin(), expansion.end());
                            buffer1 = substituted_value_vector;
                        }

                        std::string param(buffer1.begin(), buffer1.end());
                        params.push_back(param);
                        buffer1.clear();

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "Added param: " << param << "\n"; // DEBUG
                        }
                        break;
                    }
                }
                break;
            }
        }

    }

    if (DEBUG_LEVEL >= 1) {
        std::cout << "########\n\n";
        std::cout << "List of Macros:\n";
        for (auto it = macros.begin(); it != macros.end(); ++it) {
            std::cout << "Key: " << it->first << ", Value: " << it->second << "\n";
        }

        std::cout << "\n";
        std::cout << "List of Functions:\n";
        for (auto it = functions.begin(); it != functions.end(); ++it) {
            std::cout << "Function: " << it->first << ", Address: " << it->second << "\n";
        }
        std::cout << "\n";
    }

    return 0;
}

int init_first_file() {
    std::unique_ptr<std::ifstream> file = std::make_unique<std::ifstream>("test.yuasm");
    if (!(*file)) {
        std::cout << "Error: file not found" << std::endl;
        return 1;
    }
    files.push(std::move(file));
    return 0;
}

const int get_category(char ch) {
    switch (ch) {
        case '#': return HASH;
        case ',': return COMMA;
        case '.': return DOT;
        case ':': return COLON;
        case '\n': return LF;
        case '\r': return CR;
        case ' ': return SP;
        case ';': return SC;
        case '/': return SLASH;
        case '*': return AST;
        case '(': return PAREN_OPEN;
        case ')': return PAREN_CLOSE; 
        default:
            if (is_alphabetic(ch)) {
                return AL;
            } else if (is_numeric(ch)) {
                return NUM;
            }
    }
    return UNKNOWN;
}

bool is_alphabetic(char ch) {
    return (isalpha(ch) || (ch == '_' || ch == '-'));
}

bool is_numeric(char ch) {
    return isdigit(ch);
}

std::string print_state(State state_value) {
    switch (state_value) {
        case SCAN_FIRST: return "SCAN_FIRST";
        case SCAN_INSTR_OR_MACRO: return "SCAN_INSTR_OR_MACRO";
        case SCAN_PARAMS: return "SCAN_PARAMS";
        case SCAN_PREPROC_DEF: return "SCAN_PREPROC_DEF";
        case SCAN_PREPROC_SUB: return "SCAN_PREPROC_SUB";
        case SCAN_INCLUDE_FPATH: return "SCAN_INCLUDE_FPATH";
        case SCAN_FUNC_HEAD: return "SCAN_FUNC_HEAD";
        case SCAN_PREPROC_VAL: return "SCAN_PREPROC_VAL";
        case WAIT_PAREN_CLOSE: return "WAIT_PAREN_CLOSE";
        default: return "UNKNOWN";
    }
}

bool is_valid_instr(std::string instr) {
    return true;
}

int get_no_of_params_for_instr(std::string instr) {
    if (instr == "loadimm") {
        return 2;
    } else if (instr == "muldirdir") {
        return 3;
    } else if (instr == "end") {
        return 0;
    }

    return 0;
}