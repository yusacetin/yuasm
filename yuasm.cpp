#include "yuasm.hpp"
#include <cctype>
#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <memory>
#include <string>
#include <map>
#include <cmath>

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
                        if (eval_instr(instr, params) != 0) {
                            return 1;
                        }

                        buffer0.clear();
                        pc++;
                        state = SCAN_FIRST;
                        break;
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
                        if (get_no_of_params_for_instr(buffer_str) >= 0) { // means instruction is valid
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

                            expand_macro(&buffer1, macros); // Check if it's a macro expansion
                            std::string param(buffer1.begin(), buffer1.end() + offset);
                            params.push_back(param);
                            buffer1.clear();

                            if (DEBUG_LEVEL >= 2) {
                                std::cout << "Added parameter: " << param << "\n"; // DEBUG
                            }
                        }

                        std::string instr(buffer0.begin(), buffer0.end());
                        if (eval_instr(instr, params) != 0) {
                            return 1;
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
                            expand_macro(&buffer1, macros); // Check if it's a macro expansion
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
                        if (eval_instr(instr, params) != 0) {
                            return 1;
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

int Yuasm::eval_instr(std::string instr, std::vector<std::string> params) {
    if (DEBUG_LEVEL >= 1) {
        std::cout << "# Instruction Complete #\n";
        std::cout << "Instruction: " << instr << "\n";
        for (int i=0; i<params.size(); i++) {
            std::cout << "Parameter: " << params[i] << "\n";
        }
        std::cout << "\n";
    }

    int no_of_params = get_no_of_params_for_instr(instr);
    if (no_of_params < 0) {
        std::cerr << "Error: invalid instruction: " << instr << "\n";
        return 1;
    }

    if (no_of_params != params.size()) {
        std::cerr << "Error: expected " << no_of_params << " arguments, got " << params.size() << "\n";
        return 1;
    }

    // General Rules:
    // Valid register values are 0 to 255 (inclusively)
    // Rules are not being enforced at the moment

    if (instr == "loadm") {
        // Arguments: rd, val
        // val is 16 bits

        int rd = param_to_int(params[0]);
        int val = param_to_int(params[1]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Load Immediate, rd=" << rd << ", val=" << val << "\n";
        }
        
    } else if (instr == "loadd") {
        // Arguments: rd, addr
        // addr is 16 bits

        int rd = param_to_int(params[0]);
        int addr = param_to_int(params[1]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Load Direct, rd=" << rd << ", addr=" << addr << "\n";
        }
    } else if (instr == "storem") {
        // Arguments: addr, val
        // addr and val are both 12 bits

        int addr = param_to_int(params[0]);
        int val = param_to_int(params[1]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Store Immediate, addr=" << addr << ", val=" << val << "\n";
        }
    } else if (instr == "stored") {
        // Arguments: addr, rs
        // val is 16 bits

        int addr = param_to_int(params[0]);
        int rs = param_to_int(params[1]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Store Direct, addr=" << addr << ", rs=" << rs << "\n";
        }
    } else if (instr == "addmm") {
        // Arguments: rd, val1, val2
        // val is 16 bits

        int rd = param_to_int(params[0]);
        int val1 = param_to_int(params[1]);
        int val2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Add Immediate, rd=" << rd << ", val1=" << val1 << ", val2=" << val2 << "\n";
        }
    } else if (instr == "adddd") {
        // Arguments: rd, rs1, rs2
        // val is 16 bits

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Add Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "submm") {
        // Arguments: rd, val1, val2
        // val is 16 bits

        int rd = param_to_int(params[0]);
        int val1 = param_to_int(params[1]);
        int val2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Subtract Immediate, rd=" << rd << ", val1=" << val1 << ", val2=" << val2 << "\n";
        }
    } else if (instr == "subdd") {
        // Arguments: rd, rs1, rs2
        // val is 16 bits

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Subtract Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "ltd") {
        // Arguments: rd, rs1, rs2

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Less Than Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "lted") {
        // Arguments: rd, rs1, rs2

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Less Than or Equal To Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "gtd") {
        // Arguments: rd, rs1, rs2

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Greater Than Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "gted") {
        // Arguments: rd, rs1, rs2

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Greater Than or Equal To Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "eqd") {
        // Arguments: rd, rs1, rs2

        int rd = param_to_int(params[0]);
        int rs1 = param_to_int(params[1]);
        int rs2 = param_to_int(params[2]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Equal To Direct, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << "\n";
        }
    } else if (instr == "jumpm") {
        // Arguments: val
        // val might be a function name

        int val = 0;
        std::string val_str = params[0];
        if (!is_numeric(val_str[0])) {
            // See if it's a function
            int func_index = get_function_index(val_str);
            if (func_index < 0) {
                std::cerr << "Error: invalid function name\n";
                return 1;
            } else {
                val = func_index;
            }
        } else {
            val = param_to_int(params[0]);
        }

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump Immediate, val=" << val << "\n";
        }
    } else if (instr == "jumpd") {
        // Arguments: rs

        int rs = param_to_int(params[0]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump Direct, rs=" << rs << "\n";
        }
    } else if (instr == "jumpifm") {
        // Arguments: val, rcond

        int val = 0;
        std::string val_str = params[0];
        if (!is_numeric(val_str[0])) {
            // See if it's a function
            int func_index = get_function_index(val_str);
            if (func_index < 0) {
                std::cerr << "Error: invalid function name\n";
                return 1;
            } else {
                val = func_index;
            }
        } else {
            val = param_to_int(params[0]);
        }

        int rcond = param_to_int(params[1]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump If Immediate, val=" << val << ", rcond=" << rcond << "\n";
        }
    } else if (instr == "jumpifd") {
        // Arguments: rs, rcond

        int rs = param_to_int(params[0]);
        int rcond = param_to_int(params[1]);

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump If Direct, rs=" << rs << ", rcond=" << rcond << "\n";
        }
    } else if (instr == "end") {
        // Arguments: none

        if (DEBUG_LEVEL >= 0) {
            std::cout << "End\n";
        }
    }

    return 0;
}

int Yuasm::param_to_int(std::string param) {
    int res = 0;

    // Determine radix
    int radix = 10; // decimal, hex, binary (octal not supported)
    if (param.size() > 2) {
        if (param[0] == '0' && param[1] == 'x') {
            radix = 16;
            for (char& c : param) { // convert to uppercase
                c = std::toupper(static_cast<unsigned char>(c));
            }
        } else if (param[0] == '0' && param[1] == 'b') {
            radix = 2;
        }
    }

    int limit = 0;
    if (radix != 10) {
        limit = 2;
    }
    int di = 0; // digit index
    for (int i=param.size()-1; i>limit-1; i--) {
        char digit_char = param[i];

        int digit_value;
        if (radix == 10) {
            if (!is_numeric(digit_char)) {
                std::cerr << "Error: invalid decimal number: " << param << "\n";
                return 0; // TODO handle properly
            }
            digit_value = digit_char - '0';
        } else if (radix == 16) {
            if (!is_hex_digit(digit_char)) {
                std::cerr << "Error: invalid hexadecimal number: " << param << "\n";
                return 0; // TODO handle properly
            }
            digit_value = get_hex_value(digit_char);
        } else if (radix == 2) {
            if (digit_char != '0' && digit_char != '1') {
                std::cerr << "Error: invalid binary number: " << param << "\n";
                return 0; // TODO handle properly
            }
            digit_value = digit_char - '0';
        }

        res += digit_value * std::pow(radix, di);
        di++;
    }

    return res;
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

int Yuasm::get_function_index(std::string func) {
    if (functions.find(func) != functions.end()) {
        int index = functions[func];
        return index;
    }
    return -1;
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

int Yuasm::get_no_of_params_for_instr(std::string instr) {
    if (instr == "loadm") {
        return 2;
    } else if (instr == "loadd") {
        return 2;
    } else if (instr == "storem") {
        return 2;
    } else if (instr == "stored") {
        return 2;
    } else if (instr == "addmm") {
        return 3;
    } else if (instr == "adddd") {
        return 3;
    } else if (instr == "submm") {
        return 3;
    } else if (instr == "subdd") {
        return 3;
    } else if (instr == "ltd") {
        return 3;
    } else if (instr == "lted") {
        return 3;
    } else if (instr == "gtd") {
        return 3;
    } else if (instr == "gted") {
        return 3;
    } else if (instr == "eqd") {
        return 3;
    } else if (instr == "jumpm") {
        return 1;
    } else if (instr == "jumpd") {
        return 1;
    } else if (instr == "jumpifm") {
        return 2;
    } else if (instr == "jumpifd") {
        return 2;
    } else if (instr == "end") {
        return 0;
    }
    return -1;
}

bool Yuasm::is_hex_digit(char c) { // parameter must be uppercase
    return (is_numeric(c) || c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E' || c == 'F');
}

int Yuasm::get_hex_value(char c) { // parameter must be uppercase and a valid hex digit
    if (c == 'A') {return 10;}
    else if (c == 'B') {return 11;}
    else if (c == 'C') {return 12;}
    else if (c == 'D') {return 13;}
    else if (c == 'E') {return 14;}
    else if (c == 'F') {return 15;}
    else {
        return c - '0';
    }
}