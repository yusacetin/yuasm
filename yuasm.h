#ifndef YUASM_H
#define YUASM_H

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <memory>
#include <cstdint>

using uint32_t = std::uint32_t;

inline constexpr char newl[] = "\n";

class Yuasm {
    public:
        Yuasm(std::string first_fname);

        enum State {
            SCAN_FIRST,
            SCAN_INSTR_OR_MACRO,
            SCAN_PREPROC_DEF,
            SCAN_PREPROC_SUB,
            SCAN_INCLUDE_LEAD,
            SCAN_INCLUDE_FPATH,
            SCAN_FUNC_LEAD,
            SCAN_FUNC_NAME,
            SCAN_FUNC_TRAIL,
            SCAN_PREPROC_VAL,
            WAIT_PAREN_CLOSE,
            COMMENT_SCAN_BEGIN,
            LINE_COMMENT,
            BLOCK_COMMENT,
            LINE_COMMENT_END,
            BLOCK_COMMENT_END,
            SC_OR_COMMENT_UNTIL_LF,
            NOTHING_OR_COMMENT_UNTIL_LF,
            SCAN_PARAM_YES_COMMA_YES_DASH,
            SCAN_PARAM_YES_COMMA_NO_DASH,
            SCAN_PARAM_NO_COMMA_YES_DASH,
            SCAN_PARAM_NO_COMMA_NO_DASH,
            INVALID_STATE
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
            PAREN_CLOSE,
            DASH,
            QUOTE
        };

    private:
        static constexpr int DEBUG_LEVEL = 0; // 0: instr info, 1: state completions, 2: full info

        State state = SCAN_FIRST;

        std::vector<char> buffer0; // for instructions and function names and macro names
        std::vector<char> buffer1; // for macro values and instruction parameters
        std::vector<std::string> params; // for instruction parameters
        std::vector<uint32_t> instructions;
        std::vector<char> line_buffer; // used in error messages
        std::stack<int> line_counters; // used in error messages (a counter per file)
        std::stack<std::string> fnames; // used in error messages

        std::string ofname;
        std::stack<std::unique_ptr<std::ifstream>> files;
        std::map<std::string, std::string> macros;
        std::map<std::string, int> functions; // should be called sections really
        std::multimap<std::string, int> callers; // caller positions
        uint32_t pc = 0; // program counter

        State state_before_block_comment; // TODO not properly implemented

        bool open_new_file(std::string fname);
        bool mainloop();
        std::string print_state();
        bool eval_instr(std::string instr, std::vector<std::string> params);
        bool get_function_index(std::string func);
        bool write_object();
        bool link_object();
        void print_line_to_std_err();
        Input get_next_char_category();

        static void expand_macro(std::vector<char>* buffer, std::map<std::string, std::string> macro_list);
        static const Input get_category(char ch);
        static bool is_alphabetic(char ch);
        static bool is_numeric(char ch);
        static uint32_t get_no_of_params_for_instr(std::string instr); // returns -1 if instruction is invalid
        static uint32_t param_to_int(std::string param);
        static bool is_hex_digit(char c);
        static bool is_binary_digit(char c);
        static uint32_t get_hex_value(char c);
        static std::string get_instr_as_hex(uint32_t instr_int);
        static uint32_t twos_complement(uint32_t val);
        static std::string generate_ofname(std::string fpath);
};

#endif