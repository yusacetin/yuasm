#include "yulinker.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

Linker::Linker(std::vector<std::string> set_fpaths, bool set_standalone_mode) : standalone_mode(set_standalone_mode) {
    fpaths = set_fpaths;
    int no_of_files = fpaths.size();
    defs.resize(no_of_files);
    callers.resize(no_of_files);
    link();
}

int Linker::link() {
    if (save_defs_and_callers_and_instrs() != 0) {
        return 1;
    }

    if (place_symbols() != 0) {
        return 1;
    }

    if (write_binary() != 0) {
        return 1;
    }

    std::cout << "Created program binary\n";
    return 0;
}

int Linker::save_defs_and_callers_and_instrs() {
    for (int i=0; i<fpaths.size(); i++) {
        std::string fpath = fpaths[i];

        // Run through file

        std::ifstream file(fpath, std::ios::binary);

        // Step 0: get N_defs (32 bits)

        unsigned int N_defs = 0;
        char N_defs_char[4] = {'0'};
        file.get(N_defs_char[3]);
        file.get(N_defs_char[2]);
        file.get(N_defs_char[1]);
        file.get(N_defs_char[0]);

        N_defs += ((unsigned char) N_defs_char[0]);
        N_defs += ((unsigned char) N_defs_char[1]) << 8;
        N_defs += ((unsigned char) N_defs_char[2]) << 16;
        N_defs += ((unsigned char) N_defs_char[3]) << 24;

        if (DEBUG_LEVEL >= 12) {
            std::cout << "N_defs for " << fpath << ": " << N_defs << "\n";
        }

        // Step 1: run through the definitions

        for (int defi=0; defi<N_defs; defi++) {
            // Step 1.1: get length of the symbol name (16 bits)

            unsigned int len = 0;
            char len_char[2] = {'0'};
            file.get(len_char[1]);
            file.get(len_char[0]);

            len += ((unsigned char) len_char[0]);
            len += ((unsigned char) len_char[1]) << 8;

            if (DEBUG_LEVEL >= 12) {
                std::cout << "* len for " << fpath << " symbol no " << defi << ": " << len << "\n";
            }
            // Step 1.2: get symbol name

            std::vector<char> symbol_name_chars;
            for (int symi=0; symi<len; symi++) {
                char cur_char = '0';
                file.get(cur_char);
                symbol_name_chars.push_back(cur_char);

                if (DEBUG_LEVEL >= 12) {
                    std::cout << "symbol current char: " << cur_char  << "(" << (int) cur_char << ")" << "\n";
                }
            }
            std::string symbol_name(symbol_name_chars.data(), symbol_name_chars.size());

            if (DEBUG_LEVEL >= 12) {
                std::cout << "+ symbol name: " << symbol_name << "\n";
            }

            // Step 1.3: get symbol address (32 bits)

            unsigned int loc = 0;
            char loc_char[4] = {'0'};
            file.get(loc_char[3]);
            file.get(loc_char[2]);
            file.get(loc_char[1]);
            file.get(loc_char[0]);

            loc += ((unsigned char) loc_char[0]);
            loc += ((unsigned char) loc_char[1]) << 8;
            loc += ((unsigned char) loc_char[2]) << 16;
            loc += ((unsigned char) loc_char[3]) << 24;

            if (DEBUG_LEVEL >= 12) {
                std::cout << "+ symbol address: " << loc << "\n";
            }

            // Step 1.4: save symbol name and address to map

            defs[i].insert({symbol_name, loc});
        }

        // Step 2: get N_callers (32 bits)

        unsigned int N_callers = 0;
        char N_callers_char[4] = {'0'};
        file.get(N_callers_char[3]);
        file.get(N_callers_char[2]);
        file.get(N_callers_char[1]);
        file.get(N_callers_char[0]);

        N_callers += ((unsigned char) N_callers_char[0]);
        N_callers += ((unsigned char) N_callers_char[1]) << 8;
        N_callers += ((unsigned char) N_callers_char[2]) << 16;
        N_callers += ((unsigned char) N_callers_char[3]) << 24;

        if (DEBUG_LEVEL >= 12) {
            std::cout << "N_callers for " << fpath << ": " << N_callers << "\n";
        }
        // Step 3: run through the callers

        for (int calleri=0; calleri<N_callers; calleri++) {
            // Step 3.1: get length of the symbol name (16 bits)

            unsigned int len = 0;
            char len_char[2] = {'0'};
            file.get(len_char[1]);
            file.get(len_char[0]);

            len += ((unsigned char) len_char[0]);
            len += ((unsigned char) len_char[1]) << 8;

            if (DEBUG_LEVEL >= 12) {
                std::cout << "* len for " << fpath << " symbol no " << calleri << ": " << len << "\n";
            }

            // Step 3.2: get symbol name

            std::vector<char> symbol_name_chars;
            for (int symi=0; symi<len; symi++) {
                char cur_char = '0';
                file.get(cur_char);
                symbol_name_chars.push_back(cur_char);

                if (DEBUG_LEVEL >= 12) {
                    std::cout << "symbol current char: " << cur_char  << "(" << (int) cur_char << ")" << "\n";
                }
            }
            std::string symbol_name(symbol_name_chars.data(), symbol_name_chars.size());

            if (DEBUG_LEVEL >= 12) {
                std::cout << "+ symbol name: " << symbol_name << "\n";
            }

            // Step 3.3: get caller address (32 bits)

            unsigned int loc = 0;
            char loc_char[4] = {'0'};
            file.get(loc_char[3]);
            file.get(loc_char[2]);
            file.get(loc_char[1]);
            file.get(loc_char[0]);

            loc += ((unsigned char) loc_char[0]);
            loc += ((unsigned char) loc_char[1]) << 8;
            loc += ((unsigned char) loc_char[2]) << 16;
            loc += ((unsigned char) loc_char[3]) << 24;

            if (DEBUG_LEVEL >= 12) {
                std::cout << "+ caller address: " << loc << "\n";
            }

            // Step 3.4: save symbol name and address to map

            callers[i].insert({symbol_name, loc});
        }

        // Step 4: save instructions to instrs vector

        int count_bytes = 0;
        char byte;
        while (file.get(byte)) {
            instrs.push_back((unsigned char) byte);
            count_bytes++;

            std::stringstream ss;
            ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int) (unsigned char) byte;
            
            if (DEBUG_LEVEL >= 13) {
                std::cout << ss.str() << "\n";
            }
        }

        file.close();

        if (count_bytes % 4 != 0) {
            std::cerr << "Error: object file misalignment\n";
            return 1;
        }

        int count = count_bytes / 4;

        instr_count.push_back(count);
    }

    if (DEBUG_LEVEL >= 11) {
        std::cout << "defs\n";
        print_vmsi(defs);
        std::cout << "\ncallers\n";
        print_vmsi(callers);
        std::cout << "\ninstrs\n";
        print_vuc(instrs);
    }

    return 0;
}

int Linker::place_symbols() {
    for (int filei=0; filei<callers.size(); filei++) {
        std::multimap<std::string, int> cur_map = callers[filei];
        // Step 0: initiate loop

        for (std::map<std::string, int>::iterator it = cur_map.begin(); it != cur_map.end(); ++it) {
            std::string symbol_name = it->first;
            int caller_rel_loc = it->second;
            int caller_abs_loc = caller_rel_loc;
            for (int i=0; i<filei; i++) {
                caller_abs_loc += instr_count[i] * 4;
            }

            // now abs_loc points to the index of the control instruction that
            // we need to put the jump address to

            // step 1: find the symbol.

            int def_abs_loc = find_symbol(symbol_name);
            if (def_abs_loc < 0) {
                std::cerr << "Error: symbol not found: " << symbol_name << "\n";
                if (!standalone_mode) {
                    std::cerr << "Please call the linker manually with all object files\n";
                } else {
                    std::cerr << "Please make sure to call the linker with all object files\n";
                }
                return 1;
            }
            
            // step 2: calculate jump amount to reach symbol

            int loc_diff = def_abs_loc - caller_abs_loc;

            // step 3: calculate necessary bits

            // step 3.1
            // for jump and br: 24 bits
            // for jumpif and brif: 16 bits

            int val = 0;
            unsigned char instr_bytes[4];

            if (DEBUG_LEVEL >= 11) {
                std::cout << symbol_name << " found at " << def_abs_loc << "\n";
                std::cout << "loc_diff: " << loc_diff << "\n";
                std::cout << "caller_abs_loc: " << caller_abs_loc << ", value: " << (unsigned int) instrs[caller_abs_loc] << "\n";
                std::cout << "def_abs_loc: " << def_abs_loc << ", value: " << (unsigned int) instrs[def_abs_loc] << "\n";
            }

            if (instrs[caller_abs_loc] == 0x20 || instrs[caller_abs_loc] == 0x26) {
                val += loc_diff & 0xFFFFFF;
                unsigned int uint_val = (unsigned int) val;
                instr_bytes[0] = (uint_val) & 0xFF;
                instr_bytes[1] = (uint_val >> 8) & 0xFF;
                instr_bytes[2] = (uint_val >> 16) & 0xFF;


                // step 4: modify instrs 

                instrs[caller_abs_loc + 1] = instr_bytes[2];
                instrs[caller_abs_loc + 2] = instr_bytes[1];
                instrs[caller_abs_loc + 3] = instr_bytes[0];

            } else if (instrs[caller_abs_loc] == 0x22 || instrs[caller_abs_loc] == 0x27) {
                val += loc_diff & 0xFFFF;
                unsigned int uint_val = (unsigned int) val;
                instr_bytes[0] = (uint_val) & 0xFF;
                instr_bytes[1] = (uint_val >> 8) & 0xFF;

                // step 4: modify instrs 

                instrs[caller_abs_loc + 1] = instr_bytes[1];
                instrs[caller_abs_loc + 2] = instr_bytes[0];
            }


            if (DEBUG_LEVEL >= 12) {
                // for debug or sth
                std::stringstream ss;

                ss << "0x" << std::uppercase << std::hex
                << std::setw(2) << std::setfill('0') << (int)instr_bytes[3] 
                << std::setw(2) << std::setfill('0') << (int)instr_bytes[2]
                << std::setw(2) << std::setfill('0') << (int)instr_bytes[1]
                << std::setw(2) << std::setfill('0') << (int)instr_bytes[0];

                std::cout << ss.str() << "\n";  
            }      
        }
    }
    return 0;
}

int Linker::find_symbol(std::string symbol_name) {
    for (int filei=0; filei<defs.size(); filei++) {
        std::multimap<std::string, int> cur_map = defs[filei];

        for (std::map<std::string, int>::iterator it = cur_map.begin(); it != cur_map.end(); ++it) {
            std::string cur_symbol_name = it->first;

            if (cur_symbol_name == symbol_name) {
                // Match

                int rel_loc = it->second;
                int abs_loc = rel_loc;
                for (int i=0; i<filei; i++) {
                    abs_loc += instr_count[i] * 4;
                }

                return abs_loc;
            }
        }
    }
    return -1;
}

int Linker::write_binary() {
    if (DEBUG_LEVEL >= 11) {
        print_vuc(instrs);
    }

    std::ofstream bin_file("program.bin", std::ios::binary);

    for (int i=0; i<instrs.size(); i++) {
        bin_file.write(reinterpret_cast<const char*>(&instrs[i]), sizeof(instrs[i]));
    }

    bin_file.close();
    return 0;
}

// Static functions

void Linker::print_vmsi(std::vector<std::multimap<std::string, int>> vmsi) {
    for (int i=0; i<vmsi.size(); i++) {
        std::cout << "Vector index " << i << ": \n";

        for (std::map<std::string, int>::iterator it = vmsi[i].begin(); it != vmsi[i].end(); ++it) {
            std::cout << "* " << it->first << ": " << it->second << "\n";
        }
    }
}

void Linker::print_vuc(std::vector<unsigned char> vuc) {
    for (int i=0; i<vuc.size(); i++) {
        std::stringstream ss;
        ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int) vuc[i];
        std::cout << ss.str();
        if (i % 2 == 1) {
            std::cout << " ";
        }
    }
    std::cout << "\n";
}
