#include "yuasm.hpp"
#include "linker.hpp"

#include <cctype>
#include <iostream>
#include <fstream>
#include <vector>
#include <stack>
#include <memory>
#include <string>
#include <map>
#include <cmath>
#include <iomanip>

Yuasm::Yuasm(std::string first_fname) {
    ofname = generate_ofname(first_fname);
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
                    case AL: { // scan value
                        buffer1.push_back(ch);
                        break;
                    }

                    case DASH: {
                        if (buffer1.size() == 0) { // only the first character can be dash
                            buffer1.push_back(ch);
                        } else {
                            std::cerr << "Error: invalid character for macro value: " << ch << "\n";
                            return 1;
                        }
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
                        std::cerr << "Error: invalid character for macro value: " << ch << "\n";
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
                    case SLASH:
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

                    default: { // EOF
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
                        pc += 4;
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
                            std::cerr << "Error: invalid instruction: " << buffer_str << "\n";
                            return 1;
                        }
                        break;
                    }

                    // Note: parentheses must be connected to the function name when calling
                    // spaces after the function name are not allowed
                    // More important note: this is redundant due to the jump instruction
                    // TODO maybe actually implement func() calls
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
                            if (used_dash) {
                                param = "-" + param;
                                used_dash = false;
                            }
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
                        pc += 4;
                        state = SCAN_FIRST;

                        if (DEBUG_LEVEL >= 2) {
                            if (category == INPUT_EOF) {
                                std::cout << "Category EOF\n"; // DEBUG
                            }
                        }

                        break;
                    }

                    case DASH: {
                        if (used_dash) {
                            std::cerr << "Error: double negation is not allowed\n";
                            return 1;
                        }
                        used_dash = true;
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
                        if (used_dash) {
                            param = "-" + param;
                            used_dash = false;
                        }
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
                        // Disallow comma after negative sign
                        if (used_dash) {
                            std::cerr << "Error: comma after negative sign is not allowed\n";
                            return 1;
                        }

                        // If the comma is connected to the end of the parameter
                        if (buffer1.size() > 0) {
                            // Check if it's a macro expansion
                            expand_macro(&buffer1, macros);

                            std::string param(buffer1.begin(), buffer1.end());
                            if (used_dash) {
                                param = "-" + param;
                                used_dash = false;
                            }
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
                            if (used_dash) {
                                param = "-" + param;
                                used_dash = false;
                            }
                            params.push_back(param);
                            used_comma = true; // TODO check why I put this here
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
                        pc += 4;
                        state = COMMENT_SCAN_BEGIN;
                        state_before_block_comment = SCAN_FIRST; // TODO maybe implement a way to put block comments between parameters
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

    write_object();
    link_object();
    // write_binary();

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

    for (int i=0; i<params.size(); i++) { // check for illegal negatives
        if (params[i][0] == '-') {
            if (instr != "loadm") {
                std::cerr << "Error: parameter can not be negative: " << params[i] << "\n";
                return 1;
            }
        }
    }

    // General Rules:
    // Valid register values are 0 to 255 (inclusively)
    // Not all rules are being enforced at the moment so the source code should make sense

    unsigned int instr_int = 0;

    if (instr == "loadm") {
        // Arguments: rd, val

        if (params[0][0] == '-') {
            std::cerr << "Error: rd value can not be negative\n";
            return 1;
        }

        bool neg = false;
        if (params[1][0] == '-') {
            neg = true;
            //std::cout << "old params 1: " << params[1] << "\n";
            params[1] = params[1].substr(1);
            //std::cout << "new params 1: " << params[1] << "\n";
        }

        unsigned int rd = param_to_int(params[0]);
        unsigned int val = param_to_int(params[1]);
        if (neg) {
            val = twos_complement(val) & 0xFFFF;
        }

        instr_int += rd << 16;
        instr_int += val;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Load Immediate, rd=" << rd << ", val=" << val << " --> " << get_instr_as_hex(instr_int) << "\n";
        }

    } else if (instr == "loadr") {
        // Arguments: rd, raddr

        unsigned int rd = param_to_int(params[0]);
        unsigned int raddr = param_to_int(params[1]);

        instr_int += rd << 16;
        instr_int += raddr << 8;
        instr_int |= 0x01 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Load Direct, rd=" << rd << ", raddr=" << raddr << " --> " << get_instr_as_hex(instr_int) << "\n";
        }

    } else if (instr == "storen") {
        // Arguments: raddr, rs

        unsigned int raddr = param_to_int(params[0]);
        unsigned int rs = param_to_int(params[1]);

        instr_int += raddr << 16;
        instr_int += rs << 8;
        instr_int |= 0x02 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Store Indirect, raddr=" << raddr << ", rs=" << rs << " --> " << get_instr_as_hex(instr_int) << "\n";
        }

    } else if (instr == "add") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x10 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Add, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "sub") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x11 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Subtract, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "lt") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x50 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Less Than, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "lte") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x51 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Less Than or Equal To, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "gt") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);
        
        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x52 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Greater Than, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "gte") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x53 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Greater Than or Equal To, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "eq") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x54 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Equal To, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "jump") {
        // Arguments: val
        // val should be a function name

        unsigned int val = 0;
        std::string val_str = params[0];
        if (!is_numeric(val_str[0])) {
            // It's a function name
            /*
            int func_index = get_function_index(val_str);
            if (func_index < 0) {
                std::cerr << "Error: invalid function name\n";
                return 1;
            } else {
                int diff = func_index - pc;
                val = diff & 0xFFFF;
            }
            */

            callers.insert({val_str, pc});
            val = 0;
        } else {
            val = param_to_int(params[0]);
        }

        instr_int += val;
        instr_int |= 0x20 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump To Section, val=" << val << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "jumpd") {
        // Arguments: rs

        unsigned int rs = param_to_int(params[0]);

        instr_int += rs << 16;
        instr_int |= 0x21 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump Direct, rs=" << rs << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "jumpif") {
        // Arguments: val, rcond
        // val should be a function name

        unsigned int val = 0;
        std::string val_str = params[0];
        if (!is_numeric(val_str[0])) {
            // It's a function name
            /*
            int func_index = get_function_index(val_str);
            if (func_index < 0) {
                std::cerr << "Error: invalid function name\n";
                return 1;
            } else {
                int diff = func_index - pc;
                val = diff & 0xFFFF;
            }
            */

            callers.insert({val_str, pc});
            val = 0;
        } else {
            val = param_to_int(params[0]);
        }

        unsigned int rcond = param_to_int(params[1]);

        instr_int += val << 8;
        instr_int += rcond;
        instr_int |= 0x22 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump If Immediate, val=" << val << ", rcond=" << rcond << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "jumpifd") {
        // Arguments: rs, rcond

        unsigned int rs = param_to_int(params[0]);
        unsigned int rcond = param_to_int(params[1]);

        instr_int += rs << 16;
        instr_int += rcond;
        instr_int |= 0x23 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Jump If Direct, rs=" << rs << ", rcond=" << rcond << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "ret") {
        // Arguments: none

        instr_int |= 0x24 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Return" << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "end") {
        // Arguments: none

        instr_int |= 0x25 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "End" << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "br") {
        // Arguments: val
        // val should be a function name

        unsigned int val = 0;
        std::string val_str = params[0];
        if (!is_numeric(val_str[0])) {
            // It's a function name
            /*
            int func_index = get_function_index(val_str);
            if (func_index < 0) {
                std::cerr << "Error: invalid function name\n";
                return 1;
            } else {
                int diff = func_index - pc;
                val = diff & 0xFFFF;
            }
            */

            callers.insert({val_str, pc});
            val = 0;
        } else {
            val = param_to_int(params[0]);
        }

        instr_int += val;
        instr_int |= 0x26 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Branch, val=" << val << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "brif") {
        // Arguments: val, rcond
        // val should be a function name

        unsigned int val = 0;
        std::string val_str = params[0];
        if (!is_numeric(val_str[0])) {
            // It's a function name
            /*
            int func_index = get_function_index(val_str);
            if (func_index < 0) {
                std::cerr << "Error: invalid function name\n";
                return 1;
            } else {
                int diff = func_index - pc;
                val = diff & 0xFFFF;
            }
            */

            callers.insert({val_str, pc});
            val = 0;
        } else {
            val = param_to_int(params[0]);
        }

        unsigned int rcond = param_to_int(params[1]);

        instr_int += val << 8;
        instr_int += rcond;
        instr_int |= 0x27 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Branch If, val=" << val << ", rcond=" << rcond << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "stored") { // 0000_0011_16-bits-addr_8-bits-rs
        // Arguments: addr, rs

        unsigned int addr = param_to_int(params[0]);
        unsigned int rs = param_to_int(params[1]);

        instr_int += addr << 8;
        instr_int += rs;
        instr_int |= 0x03 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Store Direct, addr=" << addr << ", rs=" << rs << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "loadd") { // 0000_0100_8-bits-rd_16-bits-addr
        // Arguments: rd, addr

        unsigned int rd = param_to_int(params[0]);
        unsigned int addr = param_to_int(params[1]);

        instr_int += rd << 16;
        instr_int += addr;
        instr_int |= 0x04 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Load Direct, rd=" << rd << ", addr=" << addr << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "lshift") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x40 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Left Shift, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "rshift") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x41 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Right Shift, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "mul") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x12 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Multiply, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "div") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x13 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Divide, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "and") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x30 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "And, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "or") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x31 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Or, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "nand") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x32 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Nand, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "nor") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x33 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Nor, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    } else if (instr == "xor") {
        // Arguments: rd, rs1, rs2

        unsigned int rd = param_to_int(params[0]);
        unsigned int rs1 = param_to_int(params[1]);
        unsigned int rs2 = param_to_int(params[2]);

        instr_int += rd << 16;
        instr_int += rs1 << 8;
        instr_int += rs2;
        instr_int |= 0x34 << 24;

        if (DEBUG_LEVEL >= 0) {
            std::cout << "Xor, rd=" << rd << ", rs1=" << rs1 << ", rs2=" << rs2 << " --> " << get_instr_as_hex(instr_int) << "\n";
        }
    }

    instructions.push_back(instr_int);

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

/*
int Yuasm::write_binary() {
    std::ofstream bin_file("program.bin", std::ios::binary);

    for (int i=0; i<instructions.size(); i++) {
        unsigned int instr_int = instructions[i];
        unsigned char instr_bytes[4];
        instr_bytes[0] = (instr_int) & 0xFF;
        instr_bytes[1] = (instr_int >> 8) & 0xFF;
        instr_bytes[2] = (instr_int >> 16) & 0xFF;
        instr_bytes[3] = (instr_int >> 24) & 0xFF;

        bin_file.write(reinterpret_cast<const char*>(&instr_bytes[3]), sizeof(instr_bytes[0]));
        bin_file.write(reinterpret_cast<const char*>(&instr_bytes[2]), sizeof(instr_bytes[0]));
        bin_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
        bin_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));
    }

    bin_file.close();
    return 0;
}
*/

int Yuasm::write_object() {
    std::ofstream obj_file("objects/" + ofname, std::ios::binary);
    unsigned char instr_bytes[4];

    // Write N_defs

    unsigned int N_defs = functions.size();

    instr_bytes[0] = (N_defs) & 0xFF;
    instr_bytes[1] = (N_defs >> 8) & 0xFF;
    instr_bytes[2] = (N_defs >> 16) & 0xFF;
    instr_bytes[3] = (N_defs >> 24) & 0xFF;

    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[3]), sizeof(instr_bytes[0]));
    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[2]), sizeof(instr_bytes[0]));
    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));

    // Write DEFs

    for (std::map<std::string, int>::iterator it = functions.begin(); it != functions.end(); ++it) {
        std::string symbol_name = it->first;
        int len = symbol_name.size();
        int loc = it->second;
        
        if (len > 65535) {
            std::cerr << "Error: symbol name length must be at most 16 bits\n";
            return 1;
        }

        // write len
        instr_bytes[0] = (len) & 0xFF;
        instr_bytes[1] = (len >> 8) & 0xFF;
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));

        // write symbol_name
        for (int i=0; i<len; i++) {
            unsigned char cur_char = symbol_name[i];
            obj_file.write(reinterpret_cast<const char*>(&cur_char), sizeof(cur_char));
        }

        // write loc

        instr_bytes[0] = (loc) & 0xFF;
        instr_bytes[1] = (loc >> 8) & 0xFF;
        instr_bytes[2] = (loc >> 16) & 0xFF;
        instr_bytes[3] = (loc >> 24) & 0xFF;

        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[3]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[2]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));
    }

    // Write N_callers

    unsigned int N_callers = callers.size();

    instr_bytes[0] = (N_callers) & 0xFF;
    instr_bytes[1] = (N_callers >> 8) & 0xFF;
    instr_bytes[2] = (N_callers >> 16) & 0xFF;
    instr_bytes[3] = (N_callers >> 24) & 0xFF;

    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[3]), sizeof(instr_bytes[0]));
    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[2]), sizeof(instr_bytes[0]));
    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
    obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));

    // Write CALLs

    for (std::map<std::string, int>::iterator it = callers.begin(); it != callers.end(); ++it) {
        std::string symbol_name = it->first;
        int len = symbol_name.size();
        int loc = it->second;
        
        if (len > 65535) {
            std::cerr << "Error: symbol name length must be at most 16 bits\n";
            return 1;
        }

        // write len
        instr_bytes[0] = (len) & 0xFF;
        instr_bytes[1] = (len >> 8) & 0xFF;
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));

        // write symbol_name
        for (int i=0; i<len; i++) {
            unsigned char cur_char = symbol_name[i];
            obj_file.write(reinterpret_cast<const char*>(&cur_char), sizeof(cur_char));
        }

        // write loc

        instr_bytes[0] = (loc) & 0xFF;
        instr_bytes[1] = (loc >> 8) & 0xFF;
        instr_bytes[2] = (loc >> 16) & 0xFF;
        instr_bytes[3] = (loc >> 24) & 0xFF;

        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[3]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[2]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));
    }

    // Write instructions

    for (int i=0; i<instructions.size(); i++) {
        unsigned int instr_int = instructions[i];
        unsigned char instr_bytes[4];
        instr_bytes[0] = (instr_int) & 0xFF;
        instr_bytes[1] = (instr_int >> 8) & 0xFF;
        instr_bytes[2] = (instr_int >> 16) & 0xFF;
        instr_bytes[3] = (instr_int >> 24) & 0xFF;

        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[3]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[2]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[1]), sizeof(instr_bytes[0]));
        obj_file.write(reinterpret_cast<const char*>(&instr_bytes[0]), sizeof(instr_bytes[0]));
    }

    obj_file.close();
    return 0;
}

int Yuasm::link_object() {
    std::vector<std::string> obj_vec;
    obj_vec.push_back("objects/" + ofname);
    Linker linker(obj_vec);
    return 0;
}

int Yuasm::get_function_index(std::string func) {
    if (functions.find(func) != functions.end()) {
        int index = functions[func];
        return index;
    }
    return -1;
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

// Static functions

unsigned int Yuasm::param_to_int(std::string param) {
    unsigned int res = 0;

    // Determine radix
    unsigned int radix = 10; // decimal, hex, binary (octal not supported)
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
    unsigned int di = 0; // digit index
    for (int i=param.size()-1; i>limit-1; i--) {
        char digit_char = param[i];

        unsigned int digit_value;
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
        case '-': return DASH;
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
    return (isalpha(ch) || (ch == '_'));
}

bool Yuasm::is_numeric(char ch) {
    return isdigit(ch);
}

unsigned int Yuasm::twos_complement(unsigned int val) {
    return ~val + 1;
}

int Yuasm::get_no_of_params_for_instr(std::string instr) {
    if (instr == "loadm") {
        return 2;
    } else if (instr == "loadr") {
        return 2;
    } else if (instr == "storen") {
        return 2;
    } else if (instr == "add") {
        return 3;
    } else if (instr == "sub") {
        return 3;
    } else if (instr == "lt") {
        return 3;
    } else if (instr == "lte") {
        return 3;
    } else if (instr == "gt") {
        return 3;
    } else if (instr == "gte") {
        return 3;
    } else if (instr == "eq") {
        return 3;
    } else if (instr == "jump") {
        return 1;
    } else if (instr == "jumpd") {
        return 1;
    } else if (instr == "jumpif") {
        return 2;
    } else if (instr == "jumpifd") {
        return 2;
    } else if (instr == "end") {
        return 0;
    } else if (instr == "stored") {
        return 2;
    } else if (instr == "loadd") {
        return 2;
    } else if (instr == "lshift") {
        return 3;
    } else if (instr == "rshift") {
        return 3;
    } else if (instr == "mul") {
        return 3;
    } else if (instr == "div") {
        return 3;
    } else if (instr == "and") {
        return 3;
    } else if (instr == "or") {
        return 3;
    } else if (instr == "nand") {
        return 3;
    } else if (instr == "nor") {
        return 3;
    } else if (instr == "xor") {
        return 3;
    } else if (instr == "ret") {
        return 0;
    } else if (instr == "br") {
        return 1;
    } else if (instr == "brif") {
        return 2;
    }
    return -1;
}

bool Yuasm::is_hex_digit(char c) { // parameter must be uppercase
    return (is_numeric(c) || c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E' || c == 'F');
}

unsigned int Yuasm::get_hex_value(char c) { // parameter must be uppercase and a valid hex digit
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

std::string Yuasm::get_instr_as_hex(unsigned int instr_int) {
    unsigned char instr_bytes[4];
    instr_bytes[0] = (instr_int) & 0xFF;
    instr_bytes[1] = (instr_int >> 8) & 0xFF;
    instr_bytes[2] = (instr_int >> 16) & 0xFF;
    instr_bytes[3] = (instr_int >> 24) & 0xFF;

    std::stringstream ss;

    ss << "0x" << std::uppercase << std::hex
    << std::setw(2) << std::setfill('0') << (int)instr_bytes[3] 
    << std::setw(2) << std::setfill('0') << (int)instr_bytes[2]
    << std::setw(2) << std::setfill('0') << (int)instr_bytes[1]
    << std::setw(2) << std::setfill('0') << (int)instr_bytes[0];

    return ss.str();
}

std::string Yuasm::generate_ofname(std::string fpath) {
    size_t last_slash_pos = fpath.find_last_of("/\\");
    std::string fname = (last_slash_pos == std::string::npos) ? fpath : fpath.substr(last_slash_pos + 1);
    size_t last_dot_pos = fname.find_last_of('.');
    std::string fname_noext = (last_dot_pos == std::string::npos) ? fname : fname.substr(0, last_dot_pos);
    return fname_noext + ".o";
}
