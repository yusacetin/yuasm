#include "yuasm.hpp"
#include <cctype>
#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <memory>
#include <string>
#include <map>

Yuasm::Yuasm(std::string first_fname) {
    if (open_new_file(first_fname) == 0) {
        mainloop();
    }
}

int Yuasm::mainloop() {
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

        int category = get_category(ch);

        if (DEBUG_LEVEL >= 2) {
            std::cout << "Ch: " << ch << ", State: " << print_state() << ", Category: " << category << std::endl; // DEBUG
        }

        // Main FSM
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

                    case SLASH: {
                        state = COMMENT_SCAN_BEGIN;
                        state_before_block_comment = SCAN_FIRST; // won't be used if line comment
                        break;
                    }

                    case NUM:
                    case COMMA:
                    case COLON:
                    case AST: {
                        std::cerr << "Error: invalid character\n";
                        return 1;
                    }

                    // Don't Care: SP, LF, CR, EOF, SC
                    // (maybe) TODO make SC invalid unless specifically at the end of a line
                    // -> also maybe make only one SC valid per end of line
                }
                break;
            }



            case COMMENT_SCAN_BEGIN: {
                switch (category) {
                    case SLASH: {
                        state = LINE_COMMENT;

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "Beginning line comment\n";
                        }
                        break;
                    }

                    case AST: {
                        state = BLOCK_COMMENT;

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "Beginning block comment\n";
                        }
                        break;
                    }

                    default: {
                        std::cout << "Error: expected '/' or '*'\n";
                        return 1;
                    }
                }
                break;
            }



            case LINE_COMMENT: {
                switch (category) {
                    case LF: {
                        state = SCAN_FIRST;
                        break;
                    }

                    // Don't Care: everything else
                }
                break;
            }



            case BLOCK_COMMENT: {
                switch (category) {
                    case AST: {
                        state = BLOCK_COMMENT_END;
                        break;
                    }

                    // Don't Care: everything else
                }
                break;
            }



            case LINE_COMMENT_END: {
                switch (category) {
                    case SLASH: {
                        state = SCAN_FIRST;
                        state_before_block_comment = -1;

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "End of line comment\n";
                        }
                        break;
                    }

                    // Don't Care: everything else
                }
                break;
            }



            case BLOCK_COMMENT_END: {
                switch (category) {
                    case SLASH: {
                        state = state_before_block_comment;
                        state_before_block_comment = -1;

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "End of block comment\n";
                        }
                    }

                    // Dont' Care: everything else
                }
                break;
            }
            


            case SCAN_PREPROC_DEF: {
                switch (category) {
                    case LF:
                    case CR: { // these are invalid, we expect a keyword
                        std::cerr << "Error: expected keyword for preprocessing directive\n";
                        return 1;
                    }
                     
                    case AL: { // scan the keyword
                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first character, valid character
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cerr << "Error: identifiers can't begin with numbers\n";
                            return 1;
                        }
                        break;
                    }

                    // We can't have spaces before the keyword, it has to be connected to the '#'
                    case SP: {
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        if (buffer_str == "define") {
                            state = SCAN_PREPROC_SUB;
                            buffer0.clear();
                        } else if (buffer_str == "include") {
                            state = SCAN_INCLUDE_FPATH;
                            buffer0.clear();
                        } else {
                            std::cerr << "Error: invalid preprocessor directive\n";
                            return 1;
                        }
                        break;
                    }

                    default: {
                        std::cerr << "Error: invalid character for preprocessor directive key\n";
                        break;
                    }
                }
                break;
            }
        
            

            case SCAN_PREPROC_SUB: {
                switch (category) {
                    case LF:
                    case CR: { // these are invalid, we expect a parameter
                        std::cerr << "Error: expected macro name for preprocessing directive\n";
                        return 1;
                    }
                     
                    case AL: { // scan the name
                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first character, valid character
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cerr << "Error: identifiers can't begin with numbers\n";
                            return 1;
                        }
                        break;
                    }

                    // There can be as many spaces as desired before the parameter begins
                    // But after the parameter ends we switch to the next state
                    case SP: {
                        if (buffer0.size() == 0) {
                            // Ignore leading spaces
                            break;
                        }
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        state = SCAN_PREPROC_VAL;
                        break;
                    }

                    default: {
                        std::cerr << "Error: invalid character for macro name\n";
                        return 1;
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
                        if (category == SP) {
                            // There can be as many spaces as desired before the parameter begins
                            // But after the parameter ends we switch to the next state
                            if (buffer1.size() == 0) {
                                // Ignore leading spaces
                                break;
                            }
                        }

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
                        std::cerr << "Error: invalid character for macro value\n";
                        return 1;
                    }
                }
                break;
            }

            

            case SCAN_INCLUDE_FPATH: {
                switch (category) {
                    case AL:
                    case NUM:
                    case DOT:
                    case COMMA:
                    case COLON:
                    case SC: // No SC at the end of include lines
                    case AST:
                    case HASH: {
                        buffer0.push_back(ch);
                        break;
                    }

                    case SP:
                    case LF:
                    case CR: {
                        std::string fpath(buffer0.begin(), buffer0.end());
                        std::unique_ptr<std::ifstream> file = std::make_unique<std::ifstream>(fpath);
                        if (!(*file)) {
                            std::cerr << "Error: file not found: " << fpath << std::endl;
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

                    default: { // slash or EOF
                        std::cerr << "Error: invalid file name\n";
                        return -1;
                    }
                }
                break;
            }



            case SCAN_FUNC_HEAD: {
                switch (category) {
                    case SC:
                    case LF:
                    case CR: { // these are invalid, we expect a function name
                        std::cerr << "Error: expected function name\n";
                        return 1;
                    }
                    
                    case AL: { // scan the name
                        // Disallow characters after trailing spaces
                        if (trailing_spaces_only) {
                            std::cerr << "Error: expected only one identifier\n";
                            return 1;
                        }

                        buffer0.push_back(ch);
                        break;
                    }

                    case NUM: { // if not the first character, valid character
                        if (buffer0.size() > 0) {
                            // Disallow characters after trailing spaces
                            if (trailing_spaces_only) {
                                std::cerr << "Error: expected only one identifier\n";
                                return 1;
                            }

                            buffer0.push_back(ch);
                        } else {
                            std::cerr << "Error: identifiers can't begin with numbers\n";
                            return 1;
                        }
                        break;
                    }

                    case COLON: {
                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        functions.insert({buffer_str, pc});

                        buffer0.clear();
                        trailing_spaces_only = false;
                        state = SCAN_FIRST;

                        if (DEBUG_LEVEL >= 1) {
                            std::cout << "# Function Definition Complete #\n";
                            std::cout << "Function name: " << buffer_str << "\n";
                            std::cout << "Function address: " << pc << "\n\n";
                        }
                        break;
                    }

                    // Spaces before and after the function name are allowed
                    case SP: {
                        if (buffer0.size() == 0) {
                            // Ignore leading spaces
                            break;
                        }
                        trailing_spaces_only = true;
                        break;
                    }

                    default: {
                        std::cerr << "Error: invalid character for function name\n";
                        return 1;
                    }
                }
                break;
            }



            case SCAN_INSTR_OR_MACRO: {
                switch (category) {
                    case SC:
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

                    case NUM: { // if not the first character, valid character
                        if (buffer0.size() > 0) {
                            buffer0.push_back(ch);
                        } else {
                            std::cerr << "Error: identifiers can't begin with numbers\n";
                            return 1;
                        }
                        break;
                    }

                    case SP: {
                        // First check if it's a macro expansion
                        expand_macro(&buffer0, macros);

                        std::string buffer_str(buffer0.begin(), buffer0.end());
                        if (is_valid_instr(buffer_str)) {
                            state = SCAN_PARAMS;
                        } else {
                            std::cerr << "Error: invalid instruction\n";
                            return 1;
                        }
                        break;
                    }

                    // Note: parentheses must be connected to the function name when calling
                    // spaces after the function name are not allowed
                    case PAREN_OPEN: {
                        state = WAIT_PAREN_CLOSE;
                        break;
                    }

                    default: {
                        std::cerr << "Error: invalid character for instruction or macro\n";
                        return 1;
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

                    // No spaces or anything between the parentheses
                    default: {
                        std::cerr << "Error: expected ')'\n";
                        return 1;
                    }
                }
                break;
            }



            // TODO check for trailing spaces after the last parameter
            case SCAN_PARAMS: {
                switch (category) {
                    case INPUT_EOF:
                    case LF:
                    case CR:
                    case SC: {
                        // Trailing commas after the last parameter are not allowed
                        if (used_comma) {
                            std::cerr << "Error: trailing commas are not allowed\n";
                            return 1;
                        }

                        // Need to check if another parameter needs to be added
                        if (buffer1.size() > 0) {
                            int offset = 0; // to avoid reading the last letter in the file twice
                            if (category == INPUT_EOF) {
                                offset = -1;
                            }

                            // First check if it's a macro expansion
                            expand_macro(&buffer1, macros);

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
                        used_comma = false;
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
                        used_comma = false;
                        break;
                    }

                    case SP: { // end of current parameter or leading space
                        // Allow leading spaces
                        if (category == SP) {
                            if (buffer1.size () == 0) {
                                // Ignore leading spaces
                                break;
                            }
                        }

                        // Check if it's a macro expansion
                        expand_macro(&buffer1, macros);

                        std::string param(buffer1.begin(), buffer1.end());
                        params.push_back(param);
                        buffer1.clear();

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "Added param: " << param << "\n"; // DEBUG
                        }
                        break;
                    }

                    // Only one comma between parameters is allowed
                    // No comma before the first parameter is allowed
                    case COMMA: {
                        // If the comma is connected to the end of the parameter
                        if (buffer1.size() > 0) {
                            // Check if it's a macro expansion
                            expand_macro(&buffer1, macros);

                            std::string param(buffer1.begin(), buffer1.end());
                            params.push_back(param);
                            used_comma = true;
                            buffer1.clear();

                            if (DEBUG_LEVEL >= 2) {
                                std::cout << "Added param: " << param << "\n"; // DEBUG
                            }
                            break;
                        }

                        // Disallow multiple commas
                        if (used_comma) {
                            std::cerr << "Error: invalid parameter (only one comma between parameters is allowed)\n";
                            return 1;
                        }
                        used_comma = true;

                        // Disallow comma before first parameter
                        if (params.size() == 0) {
                            std::cerr << "Error: comma before the first parameter is not allowed\n";
                            return 1;
                        }

                        if (DEBUG_LEVEL >= 2) {
                            std::cout << "Used comma\n";
                        }

                        break;
                    }

                    // Terminate scanning and switch to line comments
                    case SLASH: {
                        // Check if another parameter needs to be added
                        if (buffer1.size() > 0) {
                            // Check if it's a macro expansion
                            expand_macro(&buffer1, macros);

                            std::string param(buffer1.begin(), buffer1.end());
                            params.push_back(param);
                            used_comma = true;
                            buffer1.clear();

                            if (DEBUG_LEVEL >= 2) {
                                std::cout << "Added param: " << param << "\n"; // DEBUG
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
                        used_comma = false;
                        pc++;
                        state = COMMENT_SCAN_BEGIN;
                        break;
                    }

                    default: {
                        std::cerr << "Error: invalid character\n";
                        return 1;
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

int Yuasm::open_new_file(std::string fname) {
    std::unique_ptr<std::ifstream> file = std::make_unique<std::ifstream>(fname);
    if (!(*file)) {
        std::cerr << "Error: file not found" << std::endl;
        return 1;
    }
    files.push(std::move(file));
    return 0;
}

void Yuasm::expand_macro(std::vector<char>* buffer, std::map<std::string, std::string> macro_list) {
    std::string buffer_str(buffer->begin(), buffer->end());
    if (macro_list.find(buffer_str) != macro_list.end()) {
        std::string expansion = macro_list[buffer_str];
        //std::vector<char> substituted_value_vector(expansion.begin(), expansion.end());
        //*buffer = substituted_value_vector;
        buffer->assign(expansion.begin(), expansion.end());
    }
}

const int Yuasm::get_category(char ch) {
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

bool Yuasm::is_alphabetic(char ch) {
    return (isalpha(ch) || (ch == '_' || ch == '-'));
}

bool Yuasm::is_numeric(char ch) {
    return isdigit(ch);
}

std::string Yuasm::print_state() {
    switch (state) {
        case SCAN_FIRST: return "SCAN_FIRST";
        case SCAN_INSTR_OR_MACRO: return "SCAN_INSTR_OR_MACRO";
        case SCAN_PARAMS: return "SCAN_PARAMS";
        case SCAN_PREPROC_DEF: return "SCAN_PREPROC_DEF";
        case SCAN_PREPROC_SUB: return "SCAN_PREPROC_SUB";
        case SCAN_INCLUDE_FPATH: return "SCAN_INCLUDE_FPATH";
        case SCAN_FUNC_HEAD: return "SCAN_FUNC_HEAD";
        case SCAN_PREPROC_VAL: return "SCAN_PREPROC_VAL";
        case WAIT_PAREN_CLOSE: return "WAIT_PAREN_CLOSE";
        case COMMENT_SCAN_BEGIN: return "COMMENT_SCAN_BEGIN";
        case LINE_COMMENT: return "LINE_COMMENT";
        case BLOCK_COMMENT: return "BLOCK_COMMENT";
        case LINE_COMMENT_END: return "LINE_COMMENT_END";
        case BLOCK_COMMENT_END: return "BLOCK_COMMENT_END";
        default: return "UNKNOWN";
    }
}

bool Yuasm::is_valid_instr(std::string instr) {
    return true;
}

int Yuasm::get_no_of_params_for_instr(std::string instr) {
    if (instr == "loadm") {
        return 2;
    } else if (instr == "muldd") {
        return 3;
    } else if (instr == "end") {
        return 0;
    }
    return 0;
}